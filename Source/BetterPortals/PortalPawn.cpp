// Fill out your copyright notice in the Description page of Project Settings.
#include "PortalPawn.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/InputComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "PhysicsEngine/PhysicsHandleComponent.h"
#include "Camera/CameraComponent.h"
#include "Engine/EngineTypes.h"
#include "DrawDebugHelpers.h"
#include "TimerManager.h"
#include "Portal.h"
#include "Kismet/KismetMathLibrary.h"

DEFINE_LOG_CATEGORY(LogPortalPawn);

APortalPawn::APortalPawn()
{
	PrimaryActorTick.bCanEverTick = true;// Tick enabled.
	PrimaryActorTick.TickGroup = ETickingGroup::TG_PostPhysics;

	// Create sub-components.
	playerMesh = CreateDefaultSubobject<USkeletalMeshComponent>("Char");
	playerMesh->SetCollisionObjectType(ECC_Pawn);
	playerMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// Setup movement capsule around the player.
	playerCapsule = CreateDefaultSubobject<UCapsuleComponent>("Capsule");
	playerCapsule->SetCollisionObjectType(ECC_Pawn);
	playerCapsule->SetCollisionProfileName("PortalPawn");
	playerCapsule->SetCapsuleHalfHeight(characterSettings.standingHeight);
	playerCapsule->SetCapsuleRadius(40.0f);
	playerCapsule->SetSimulatePhysics(true);

	// Setup camera holding component for rotation.
	cameraHolder = CreateDefaultSubobject<USceneComponent>(TEXT("CameraHolder"));
	cameraHolder->SetupAttachment(playerMesh);

	// Setup camera default settings.
	camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	camera->SetupAttachment(cameraHolder);

	// Setup physics handle for interacting with physics objects in the world.
	physicsHandle = CreateDefaultSubobject<UPhysicsHandleComponent>(TEXT("PhysicsHandle"));
	physicsHandle->LinearDamping = 200.0f;
	physicsHandle->AngularDamping = 200.0f;
	physicsHandle->LinearStiffness = 5000.0f;
	physicsHandle->AngularStiffness = 3000.0f;
	physicsHandle->bSoftAngularConstraint = true;
	physicsHandle->bSoftLinearConstraint = true;
	physicsHandle->bInterpolateTarget = true;
	physicsHandle->InterpolationSpeed = 100.0f;
	orientation = false;

	// Setup component order.
	RootComponent = playerCapsule;
	playerMesh->SetupAttachment(playerCapsule);

	// Setup default variables.
	jumpCount = 0;
}

void APortalPawn::BeginPlay()
{
	Super::BeginPlay();

	// Ensure standing height is correct.
	playerCapsule->SetCapsuleHalfHeight(characterSettings.standingHeight);

	// Setup mass override.
	playerCapsule->SetMassOverrideInKg(NAME_None, characterSettings.mass);

	// IMPORTANT BUG FIXES FOR PHYSICS LOCKING OF DIFFERENT AXIS.
	FVector intertia = playerCapsule->BodyInstance.GetBodyInertiaTensor();
	intertia.Z = 1000.0f; // Override Yaw intertia.
	playerCapsule->BodyInstance.InertiaTensorScale = intertia;

	// Check for any missing variables that are needed for the class to function as intended.
	if (characterSettings.IsValid())
	{
		UE_LOG(LogPortalPawn, Warning, TEXT("NULL ERROR: Ragdoll Pawn has been destroyed as some character settings was not set correctly."));
		PrimaryActorTick.bCanEverTick = false;
		Destroy();
	}
}

void APortalPawn::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// Check ground to update ground values.
	GroundCheck();

	// Check should update movement before updating.
	if (characterSettings.IsInputtingMovement()) UpdateMovement(DeltaTime);
	if (characterSettings.IsInputtingMouseMovement()) UpdateMouseMovement(DeltaTime);

	// Update orientation if need be.
	// NOTE: Apply this before any user input that will apply movement through rotation...
	if (orientation) ReturnToOrientation();

	// Update physics handle location if something is being held.
	if (physicsHandle->GetGrabbedComponent() != nullptr)
	{
		// Update handle offset.
		FVector newLoc = camera->GetComponentTransform().TransformPositionNoScale(originalRelativeLocation);
		FRotator newRot = camera->GetComponentTransform().TransformRotation(originalRelativeRotation.Quaternion()).Rotator();
		physicsHandle->SetTargetLocationAndRotation(newLoc, newRot);
	}

	// Update movement velocity.
	characterSettings.linVelocity = playerCapsule->GetPhysicsLinearVelocity();
	characterSettings.rotVelocity = playerCapsule->GetPhysicsAngularVelocityInDegrees();

	// Update last location.
	lastLocation = camera->GetComponentLocation();
}

void APortalPawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	APawn::SetupPlayerInputComponent(PlayerInputComponent);

	// Setup action bindings.
	PlayerInputComponent->BindAction("Jump", IE_Pressed, this, &APortalPawn::JumpAction<true>);
	PlayerInputComponent->BindAction("Jump", IE_Released, this, &APortalPawn::JumpAction<false>);
	PlayerInputComponent->BindAction("Run", IE_Pressed, this, &APortalPawn::RunAction<true>);
	PlayerInputComponent->BindAction("Run", IE_Released, this, &APortalPawn::RunAction<false>);
	PlayerInputComponent->BindAction("Crouch", IE_Pressed, this, &APortalPawn::CrouchAction<true>);
	PlayerInputComponent->BindAction("Crouch", IE_Released, this, &APortalPawn::CrouchAction<false>);
	PlayerInputComponent->BindAction("Interact", IE_Pressed, this, &APortalPawn::InteractAction<true>);
	PlayerInputComponent->BindAction("Interact", IE_Released, this, &APortalPawn::InteractAction<false>);
	PlayerInputComponent->BindAction("Fire", IE_Pressed, this, &APortalPawn::FireAction<true>);
	PlayerInputComponent->BindAction("Fire", IE_Released, this, &APortalPawn::FireAction<false>);

	// Setup axis bindings.
	PlayerInputComponent->BindAxis("MoveForward", this, &APortalPawn::Forward);
	PlayerInputComponent->BindAxis("MoveRight", this, &APortalPawn::Right);
	PlayerInputComponent->BindAxis("Turn", this, &APortalPawn::Turn);
	PlayerInputComponent->BindAxis("LookUp", this, &APortalPawn::LookUp);
}

void APortalPawn::JumpAction(bool pressed)
{
	// Jump Action - Pressed
	if (pressed)
	{
		// If double jump is enabled and only jumped once up to now allow another jump mid air or if the character is grounded.
		if (characterSettings.doubleJump ? jumpCount <= 1 || characterSettings.IsGrounded() : characterSettings.IsGrounded())
		{
			FVector newVel = playerCapsule->GetPhysicsLinearVelocity();
			newVel.Z = 0.0f;// Zero out the capsule velocity in the Z direction to ensure upwards force feels the same when pressed.
			playerCapsule->SetPhysicsLinearVelocity(newVel);

			// Add impulse upwards and towards the direction the player was moving.
			FVector jumpDirection = (playerCapsule->GetUpVector() + FVector(characterSettings.movementDir.X / -2, characterSettings.movementDir.Y / 2, 0.0f)).GetSafeNormal();
			playerCapsule->AddImpulse(1000.0f * characterSettings.jumpForce * jumpDirection);
			jumpCount++;
		}
	}
	// Jump Action - Released
	else
	{
		// ...
	}
}

void APortalPawn::RunAction(bool pressed)
{
	// If not crouching do the following.
	if (characterSettings.GetCurrentMovementState() != EMovementState::CROUCHING)
	{
		// Run Action - Pressed
		if (pressed) characterSettings.SetMovement(EMovementState::RUNNING);
		// Run Action - Released
		else characterSettings.SetMovement(EMovementState::WALKING);
	}
}

void APortalPawn::CrouchAction(bool pressed)
{
	// Clear any timers.
	GetWorld()->GetTimerManager().ClearTimer(crouchLerp.crouchTimerHandle);

	// Setup starting variables.
	crouchLerp.timeCrouchStarted = GetWorld()->GetTimeSeconds();
	crouchLerp.timeToCrouch = characterSettings.crouchTime;
	crouchLerp.startingHeight = playerCapsule->GetScaledCapsuleHalfHeight();
	characterSettings.crouching = false;
	characterSettings.uncrouching = false;

	// Crouch Action - Pressed
	if (pressed)
	{
		// Set crouched height.
		crouchLerp.endingHeight = characterSettings.crouchingHeight;

		// Start new crouch timer lerp.
		FTimerDelegate crouchTimerDelegate;
		crouchTimerDelegate.BindUFunction(this, "CrouchLerp");
		GetWorld()->GetTimerManager().SetTimer(crouchLerp.crouchTimerHandle, crouchTimerDelegate, 0.01f, true);
		characterSettings.crouching = true;

		// Set new movement mode.
		characterSettings.SetMovement(EMovementState::CROUCHING);
	}
	// Crouch Action - Released
	else
	{
		// Set un-crouched height.
		crouchLerp.endingHeight = characterSettings.standingHeight;

		// Start new crouch timer lerp.
		FTimerDelegate crouchTimerDelegate;
		crouchTimerDelegate.BindUFunction(this, "CrouchLerp");
		GetWorld()->GetTimerManager().SetTimer(crouchLerp.crouchTimerHandle, crouchTimerDelegate, 0.01f, true);
		characterSettings.uncrouching = true;

		// Set new movement mode.
		characterSettings.SetMovement(EMovementState::WALKING);
	}
}

void APortalPawn::CrouchLerp()
{
	// Crouch or un-crouch the player.
	float crouchAlpha = (GetWorld()->GetTimeSeconds() - crouchLerp.timeCrouchStarted) / crouchLerp.timeToCrouch;
	float lastHeight = playerCapsule->GetScaledCapsuleHalfHeight();
	float lerpedHalfHeight = FMath::Lerp(crouchLerp.startingHeight, crouchLerp.endingHeight, crouchAlpha);
	playerCapsule->SetCapsuleHalfHeight(lerpedHalfHeight, false);
	playerCapsule->AddWorldOffset(FVector(0.0f, 0.0f, lerpedHalfHeight - lastHeight), false, nullptr, ETeleportType::TeleportPhysics);
	
	// End timer.
	if (crouchAlpha >= 1.0f)
	{
		GetWorld()->GetTimerManager().ClearTimer(crouchLerp.crouchTimerHandle);
		characterSettings.crouching = false;
		characterSettings.uncrouching = false;
	}
}

void APortalPawn::InteractAction(bool pressed)
{
	// Interact Action - Pressed
	if (pressed)
	{
		// Perform interact line trace and fill relevant information in character settings.
		// Doesn't support picking up through portals but the PortalTraceSingleExample can be used to achieve this.
		FHitResult interactHit;
		FVector startLocation = camera->GetComponentLocation();
		FVector endLocation = startLocation + (camera->GetForwardVector() * characterSettings.interactionDistance);
		FCollisionObjectQueryParams collObjParams;
		collObjParams.AddObjectTypesToQuery(ECC_Interactable);
		FCollisionQueryParams collParams;
		collParams.AddIgnoredActor(this);
		bool wentThroughPortal = GetWorld()->LineTraceSingleByObjectType(interactHit, startLocation, endLocation, collObjParams, collParams);
		characterSettings.lastInteractHit = interactHit;

		// If debug is enabled debug what happened with the interaction trace.
		if (debugSettings.debugInteractionTrace)
		{
			if (interactHit.bBlockingHit)
			{
				DrawDebugLine(GetWorld(), interactHit.TraceStart, interactHit.Location, FColor::Green, false, 10.0f, 0.0f, 1.0f);
				DrawDebugPoint(GetWorld(), interactHit.Location, 10.0f, FColor::Red, false, 10.0f, 0.0f);
			}
			else
			{
				DrawDebugLine(GetWorld(), startLocation, endLocation, FColor::Red, false, 10.0f, 0.0f, 1.0f);
			}
		}

		// If something was hit grab it with the physics handle.
		if (interactHit.bBlockingHit)
		{
			// Create component to grab here.
			UPrimitiveComponent* primComp = interactHit.GetComponent();

			// Only pickup with physics handle if there is a component and it is simulating physics.
			if (primComp != nullptr && primComp->IsSimulatingPhysics())
			{
				originalRelativeLocation = camera->GetComponentTransform().InverseTransformPositionNoScale(primComp->GetComponentLocation());
				originalRelativeRotation = camera->GetComponentTransform().TransformRotation(primComp->GetComponentQuat()).Rotator();
				physicsHandle->GrabComponentAtLocationWithRotation(primComp, NAME_None, primComp->GetComponentLocation(), primComp->GetComponentRotation());
				primComp->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
			}
		}
	}
	// Interact Action - Released
	else ReleaseInteractable();
}

void APortalPawn::ReleaseInteractable()
{
	// Drop physics handle comp.
	if (UPrimitiveComponent* primComp = physicsHandle->GetGrabbedComponent())
	{
		primComp->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
		physicsHandle->ReleaseComponent();
	}
	// Nothing needs releasing.
	else return;

	// Reset interact struct.
	characterSettings.lastInteractHit = FHitResult();
}

void APortalPawn::FireAction(bool pressed)
{
	// Fire Pressed...
	if (pressed)
	{
		if (UPrimitiveComponent* grabbedComp = physicsHandle->GetGrabbedComponent())
		{
			ReleaseInteractable();
			grabbedComp->AddImpulse(5000.0f * characterSettings.throwForce * camera->GetForwardVector());
		}
	}
	// Fire Released...
	else
	{
		// ....
	}
}

void APortalPawn::Forward(float val)
{
	characterSettings.movementDir.Y = val;
}

void APortalPawn::Right(float val)
{
	characterSettings.movementDir.X = val;
}

void APortalPawn::Turn(float val)
{
	characterSettings.mouseMovement.X = val;
}

void APortalPawn::LookUp(float val)
{
	characterSettings.mouseMovement.Y = val;
}

bool APortalPawn::GroundCheck()
{
	// Perform a line trace from the hip location to check if this pawn is grounded.
	// NOTE: Line trace will return block if anything that would block the pawn is blocked.
	FHitResult groundHit;
	FCollisionQueryParams collParams;
	collParams.AddIgnoredActor(this);
	FVector capsuleBottom = playerCapsule->GetComponentLocation();
	capsuleBottom.Z -= characterSettings.GetCurrentMovementState() == EMovementState::CROUCHING ? characterSettings.crouchingHeight : characterSettings.standingHeight;
	float radius = playerCapsule->GetScaledCapsuleRadius();
	capsuleBottom.Z += radius;
	FCollisionShape sphere = FCollisionShape::MakeSphere(radius);
	GetWorld()->SweepSingleByChannel(groundHit, capsuleBottom, capsuleBottom - FVector(0.0f, 0.0f, characterSettings.groundCheckDistance), FQuat::Identity, ECC_Pawn, sphere, collParams);
	characterSettings.lastGroundHit = groundHit;

	// If debugging is enabled for the ground check draw it on screen.
	if (debugSettings.debugGroundTrace)
	{
		if (groundHit.bBlockingHit) DrawDebugSphere(GetWorld(), groundHit.TraceStart, radius, 10.0f, FColor::Green, false, 0.05f, 0.0f, 0.5f);
		else  DrawDebugSphere(GetWorld(), groundHit.TraceStart, radius, 10.0f, FColor::Red, false, 0.05f, 0.0f, 0.5f);
	}

	// Act on the character being grounded or in the air.
	if (groundHit.bBlockingHit)
	{
		// If grounded update jumped again value if its still true.
		if (characterSettings.doubleJump && jumpCount != 0) jumpCount = 0;

		// Check if the character needs its physics material changing to grounded.
		if (playerCapsule->BodyInstance.GetSimplePhysicalMaterial() != characterSettings.physicsMaterialGrounded)
		{
			playerCapsule->BodyInstance.SetPhysMaterialOverride(characterSettings.physicsMaterialGrounded);
		}
	}
	else
	{
		// Check if the character needs its physics material changing to air.
		if (playerCapsule->BodyInstance.GetSimplePhysicalMaterial() != characterSettings.physicsMaterialAir)
		{
			playerCapsule->BodyInstance.SetPhysMaterialOverride(characterSettings.physicsMaterialAir);
		}
	}

	// Return ground hit result.
	return groundHit.bBlockingHit;
}

void APortalPawn::UpdateMovement(float deltaTime)
{
	// Get current force from current velocity.
	FVector currentForce = characterSettings.linVelocity;
	currentForce.Z = 0.0f;

	// Get intended direction from input and current camera forward rotation.
	// NOTE: Rotate through rotator function to avoid lerping the long way round...
	FVector direction = FVector(characterSettings.movementDir.X, -characterSettings.movementDir.Y, 0.0f);
	lastDirection = direction;

	// Multiply direction by current movement speed if on ground use groundSpeed and if in air use airSpeed.
	if (characterSettings.IsGrounded()) direction *= (characterSettings.movementSpeedMul * characterSettings.GetCurrentMovementSpeed());
	else
	{
		// Get 3D speed vector for air movement.
		FVector airSpeed3D = FVector(characterSettings.airSpeed.X * characterSettings.movementSpeedMul, 
									 characterSettings.airSpeed.Y * characterSettings.movementSpeedMul, 0.0f);

		// Multiply new speed along direction vector.
		direction *= airSpeed3D;	
	}

	// Rotate the directional force in the camera facing direction.
	FVector currentCameraForward = camera->GetRightVector();
	currentCameraForward.Z = 0.0f;
	currentCameraForward.Normalize();
	FVector force = currentCameraForward.Rotation().RotateVector(direction);

	// NOTE: Fix too much force being added. (Fake air drag);
	FVector dragForce = currentForce * characterSettings.movementDragMul;
	force -= dragForce;

	// If on the ground add upwards force also to help going up slopes.
	if (characterSettings.lastGroundHit.bBlockingHit) force.Z = (characterSettings.upForceMultiplier * 0.01f) * (55000.0f - (characterSettings.linVelocity.Z * characterSettings.maxUpForce));

	//  NOTE: Fix too much force being added when moving vertically.
	if (characterSettings.movementDir.X != 0 && characterSettings.movementDir.Y != 0) force *= 0.7f;

	// Apply movement.
	playerCapsule->AddForce(force / deltaTime);

	// Remove any yaw velocity.
	FVector currVel = playerCapsule->GetPhysicsAngularVelocityInDegrees();
	currVel.Z = 0.0f;
	playerCapsule->SetPhysicsAngularVelocityInDegrees(currVel);
}

void APortalPawn::UpdateMouseMovement(float deltaTime)
{
	// Get current mouse axis values.
	float mouseX = InputComponent->GetAxisValue("Turn");
	float mouseY = InputComponent->GetAxisValue("LookUp");

	// Camera movement.
	FRotator newRelativeCameraRot = camera->GetRelativeTransform().Rotator();
	newRelativeCameraRot.Pitch += (mouseY * characterSettings.mouseSpeed);
	newRelativeCameraRot.Yaw += (mouseX * characterSettings.mouseSpeed);
	newRelativeCameraRot.Roll = 0.0f;
	newRelativeCameraRot.Pitch = FMath::Clamp(newRelativeCameraRot.Pitch, -characterSettings.cameraPitch, characterSettings.cameraPitch);
	camera->SetRelativeRotation(newRelativeCameraRot);
	
#if CHAR_DEBUG
	// Print any enabled debug messages.
	if (debugSettings.debugMouseMovement)
	{
		UE_LOG(LogPortalPawn, Log, TEXT("Mouse X-Value: %f"), mouseX);
		UE_LOG(LogPortalPawn, Log, TEXT("Mouse Y-Value: %f"), mouseY);
		UE_LOG(LogPortalPawn, Log, TEXT("New Relative Camera Rotation: %s"), *newRelativeCameraRot.ToString());
	}
#endif
}

void APortalPawn::UpdatePhysicsHandleOffset()
{
	FVector newLoc = camera->GetComponentTransform().TransformPositionNoScale(originalRelativeLocation);
	FRotator newRot = camera->GetComponentTransform().TransformRotation(originalRelativeRotation.Quaternion()).Rotator();
	FHitResult holdingHit;
	FCollisionObjectQueryParams collObjParams;
	collObjParams.AddObjectTypesToQuery(ECC_Portal);
	FCollisionQueryParams collParams;
	collParams.AddIgnoredActor(this);
	collParams.AddIgnoredActor(physicsHandle->GetGrabbedComponent()->GetOwner());
	bool anything = GetWorld()->LineTraceSingleByObjectType(holdingHit, camera->GetComponentLocation(), newLoc, collObjParams, collParams);
	if (anything)
	{
		if (APortal* isAPortal = Cast<APortal>(holdingHit.GetActor()))
		{
			FVector newerLoc = isAPortal->ConvertLocationToPortal(newLoc, isAPortal, isAPortal->pTargetPortal);
			FRotator newerRot = isAPortal->ConvertRotationToPortal(newRot, isAPortal, isAPortal->pTargetPortal);
			physicsHandle->SetTargetLocationAndRotation(newerLoc, newerRot);
		}
	}
	else physicsHandle->SetTargetLocationAndRotation(newLoc, newRot);
}

void APortalPawn::PortalTeleport(APortal* targetPortal)
{
	// If the target portal is at the same orientation upwards as its target then return as orientation does not need to be adjusted.
	if (targetPortal->pTargetPortal->GetActorUpVector() == targetPortal->GetActorUpVector()) return;

	// Start timer to return the player to the correct orientation relative to the world.
	orientationStart = GetWorld()->GetTimeSeconds();
	orientationAtStart = playerCapsule->GetComponentRotation();
	orientation = true;
}

void APortalPawn::ReturnToOrientation()
{
	float alpha = (GetWorld()->GetTimeSeconds() - orientationStart) / characterSettings.orientationCorrectionTime;
	FRotator currentOrientation = playerCapsule->GetComponentRotation();
	FQuat target = FRotator(0.0f, currentOrientation.Yaw, 0.0f).Quaternion();
	FQuat newOrientation = FQuat::Slerp(currentOrientation.Quaternion(), target, alpha);
	playerCapsule->SetWorldRotation(newOrientation.Rotator(), false, nullptr, ETeleportType::TeleportPhysics);
	if (alpha >= 1.0f) orientation = false;
}

bool APortalPawn::PortalTraceSingleExample(struct FHitResult& outHit, const FVector& start, const FVector& end, ECollisionChannel objectType, int maxPortalTrace)
{
	// Perform first trace.
	bool beenThroughPortal = false;
	FCollisionObjectQueryParams collObjParams;
	collObjParams.AddObjectTypesToQuery(ECC_Portal);
	collObjParams.AddObjectTypesToQuery(objectType);
	FCollisionQueryParams collParams;
	collParams.AddIgnoredActor(this);
	GetWorld()->LineTraceSingleByObjectType(outHit, start, end, collObjParams, collParams);

	// If debug is enabled debug this line.
	if (debugSettings.debugInteractionTrace)
	{
		FVector endToUse = end;
		if (outHit.bBlockingHit) endToUse = outHit.Location;
		DrawDebugLine(GetWorld(), start, endToUse, FColor::Red, false, 15.0f, 0.0f, 2.0f);
	}

	// If a portal was hit perform another trace from said portal with converted start and end positions.
	if (outHit.bBlockingHit)
	{
		if (APortal* wasPortal = Cast<APortal>(outHit.Actor))
		{
			beenThroughPortal = true;
			APortal* lastPortal = wasPortal;
			for (int i = 0; i < maxPortalTrace; i++)
			{
				FVector newStart = wasPortal->ConvertLocationToPortal(outHit.Location, wasPortal, wasPortal->pTargetPortal);
				FVector newEnd = wasPortal->ConvertLocationToPortal(end, wasPortal, wasPortal->pTargetPortal);
				outHit = FHitResult();

				// Ignore target portal to avoid returning its blocking hit result.
				collParams.AddIgnoredActor(lastPortal->targetPortal);

				// Line trace from portal exit.
				GetWorld()->LineTraceSingleByObjectType(outHit, newStart, newEnd, collObjParams, collParams);
	
				// If another portal was hit continue otherwise exit.
				if (!Cast<APortal>(outHit.Actor)) return outHit.bBlockingHit;
			}
		}
	}

	// Return if the trace passed through a portal in the world.
	return beenThroughPortal;
}
