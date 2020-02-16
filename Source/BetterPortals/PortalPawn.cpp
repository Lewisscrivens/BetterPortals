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

DEFINE_LOG_CATEGORY(LogPortalPawn);

APortalPawn::APortalPawn()
{
	PrimaryActorTick.bCanEverTick = true;// Tick enabled.

	// Create sub-components.
	playerMesh = CreateDefaultSubobject<USkeletalMeshComponent>("Char");
	playerMesh->SetCollisionObjectType(ECC_Pawn);
	playerMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// Setup movement capsule around the player.
	playerCapsule = CreateDefaultSubobject<UCapsuleComponent>("Capsule");
	playerCapsule->SetCollisionObjectType(ECC_Pawn);
	playerCapsule->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	playerCapsule->SetCollisionResponseToAllChannels(ECR_Block);
	playerCapsule->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);
	playerCapsule->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	playerCapsule->SetCapsuleHalfHeight(90.0f);
	playerCapsule->SetCapsuleRadius(40.0f);
	playerCapsule->SetMassOverrideInKg(NAME_None, characterSettings.mass);
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

	// Setup component order.
	RootComponent = playerCapsule;
	playerMesh->SetupAttachment(playerCapsule);

	// Setup default variables.
	jumpCount = 0;
}

void APortalPawn::BeginPlay()
{
	Super::BeginPlay();

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

	// Update physics handle location if something is being held.
	if (physicsHandle->GetGrabbedComponent() != nullptr)
	{
		FVector newLoc = camera->GetComponentTransform().TransformPositionNoScale(originalRelativeLocation);
		FRotator newRot = camera->GetComponentTransform().TransformRotation(originalRelativeRotation.Quaternion()).Rotator();
		physicsHandle->SetTargetLocationAndRotation(newLoc, newRot);
	}

	// Update movement velocity.
	characterSettings.linVelocity = playerCapsule->GetPhysicsLinearVelocity();
	characterSettings.rotVelocity = playerCapsule->GetPhysicsAngularVelocityInDegrees();
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
		if (characterSettings.doubleJump ? jumpCount < 2 || characterSettings.IsGrounded() : characterSettings.IsGrounded())
		{
			playerCapsule->AddImpulse(1000.0f * characterSettings.jumpForce * playerCapsule->GetUpVector());
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
		crouchLerp.endingHeight = 60.0f;

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
		crouchLerp.endingHeight = 90.0f;

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
	// Crouch or uncrouch the player.
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
		FHitResult interactHit;
		FVector startLocation = camera->GetComponentLocation();
		FVector endLocation = startLocation + (camera->GetForwardVector() * characterSettings.interactionDistance);
		FCollisionObjectQueryParams collObjParams;
		collObjParams.AddObjectTypesToQuery(ECC_Interactable);
		FCollisionQueryParams collParams;
		collParams.AddIgnoredActor(this);
		bool anything = GetWorld()->LineTraceSingleByObjectType(interactHit, startLocation, endLocation, collObjParams, collParams);
		characterSettings.lastInteractHit = interactHit;

		// If anything was hit run the interact function.
		if (anything)
		{
			Interact();
		}
	}
	// Interact Action - Released
	else
	{
		// Drop physics handle comp.
		if (UPrimitiveComponent* primComp = physicsHandle->GetGrabbedComponent())
		{
			primComp->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block); // TODO: Dont enable collision until its outside of the player capsule.
			physicsHandle->ReleaseComponent();
		}
		
		// Reset interact struct.
		characterSettings.lastInteractHit = FHitResult();
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
	capsuleBottom.Z -= playerCapsule->GetScaledCapsuleHalfHeight();
	GetWorld()->LineTraceSingleByChannel(groundHit, capsuleBottom, capsuleBottom - (playerCapsule->GetUpVector() * 2.0f),//<-- Ground trace distance
		ECC_Pawn, collParams);
	characterSettings.lastGroundHit = groundHit;

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
	FVector direction = FVector(characterSettings.movementDir.X, -characterSettings.movementDir.Y, 0.0f);

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

	//  NOTE: Fix too much force being added when moving vertically.
	if (characterSettings.movementDir.X != 0 && characterSettings.movementDir.Y != 0) force *= 0.5f;

	// Apply movement through physics to the capsule.
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

	// Capsule yaw movement.
	FRotator newYaw = playerMesh->GetComponentRotation();
	newYaw.Yaw += (mouseX * characterSettings.mouseSpeed);
	playerCapsule->SetWorldRotation(newYaw, false, nullptr, ETeleportType::TeleportPhysics);

	// Camera roll movement.
	FRotator newRelPitch = camera->GetRelativeTransform().Rotator();
	newRelPitch.Pitch += (mouseY * characterSettings.mouseSpeed); 
	newRelPitch.Pitch = FMath::Clamp(newRelPitch.Pitch, -characterSettings.cameraPitch, characterSettings.cameraPitch);
	camera->SetRelativeRotation(newRelPitch);
	
#if CHAR_DEBUG
	// Print any enabled debug messages.
	if (debugSettings.debugMouseMovement)
	{
		UE_LOG(LogPortalPawn, Log, TEXT("Mouse X-Value: %f"), mouseX);
		UE_LOG(LogPortalPawn, Log, TEXT("Mouse Y-Value: %f"), mouseY);
		UE_LOG(LogPortalPawn, Log, TEXT("New Yaw: %s"), *newYaw.ToString());
		UE_LOG(LogPortalPawn, Log, TEXT("New Pitch: %s"), *newRelPitch.ToString());
	}
#endif
}

void APortalPawn::Interact()
{
	// Only pickup with physics handle if there is a component and it is simulating physics.
	UPrimitiveComponent* primComp = characterSettings.lastInteractHit.GetComponent();
	if (primComp != nullptr && primComp->IsSimulatingPhysics())
	{
		originalRelativeLocation = camera->GetComponentTransform().InverseTransformPositionNoScale(primComp->GetComponentLocation());
		originalRelativeRotation = camera->GetComponentTransform().TransformRotation(primComp->GetComponentQuat()).Rotator();
		physicsHandle->GrabComponentAtLocationWithRotation(primComp, NAME_None, primComp->GetComponentLocation(), primComp->GetComponentRotation());
		primComp->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	}
}

void APortalPawn::PortalTeleport(APortal* targetPortal)
{

}
