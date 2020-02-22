// Fill out your copyright notice in the Description page of Project Settings.
#include "Portal.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/BoxComponent.h"
#include "PhysicsEngine/PhysicsHandleComponent.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/UObjectGlobals.h"
#include "Engine/CanvasRenderTarget2D.h"
#include "Engine/Engine.h"
#include "Camera/CameraComponent.h"
#include "Engine/LocalPlayer.h"
#include "Plane.h"
#include "PortalPawn.h"
#include "PortalPlayer.h"
#include "DrawDebugHelpers.h"
#include "Kismet/KismetRenderingLibrary.h"
#include "Engine/StaticMesh.h"

DEFINE_LOG_CATEGORY(LogPortal);

APortal::APortal()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = ETickingGroup::TG_PostUpdateWork;

	// Create class default sub-objects.
	RootComponent = CreateDefaultSubobject<USceneComponent>("RootComponent");
	RootComponent->Mobility = EComponentMobility::Static; // Portals are static objects.
	portalMesh = CreateDefaultSubobject<UStaticMeshComponent>("PortalMesh");
	portalMesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	portalMesh->SetCollisionObjectType(ECC_Portal);
	portalMesh->SetupAttachment(RootComponent);
	portalMesh->CastShadow = false; // Dont want it casting shadows.

	// Setup portal overlap box for detecting actors.
	portalBox = CreateDefaultSubobject<UBoxComponent>(TEXT("PortalBox"));
	portalBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	portalBox->SetUseCCD(true);
	portalBox->SetCollisionProfileName("Portal");
	portalBox->SetupAttachment(portalMesh);
	
	// Setup default scene capture comp.
	portalCapture = CreateDefaultSubobject<USceneCaptureComponent2D>("PortalCapture");
	portalCapture->SetupAttachment(RootComponent);
	portalCapture->bEnableClipPlane = true;
	portalCapture->bUseCustomProjectionMatrix = false;
	portalCapture->bCaptureEveryFrame = false;
	portalCapture->bCaptureOnMovement = false;
	portalCapture->LODDistanceFactor = 3;
	portalCapture->TextureTarget = nullptr;	
	portalCapture->CaptureSource = ESceneCaptureSource::SCS_SceneColorSceneDepth;// Stores Scene Depth in A channel.

	// Add post physics ticking function to this actor.
	physicsTick.bCanEverTick = false;
	physicsTick.Target = this;
	physicsTick.TickGroup = TG_PostPhysics;

	// Set active by default. 
    // NOTE: If performant portals is enabled in game mode portals will be deactivated until needed to be activated...
	active = true;
	initialised = false;
	latePortalUpdate = true;
	debugCameraTransform = false;
	debugTrackedActors = false;
	actorsBeingTracked = 0;
	recursionAmount = 5;
}

void APortal::BeginPlay()
{
	Super::BeginPlay();

	// If there is no target destroy and print log message.
	pTargetPortal = Cast<APortal>(targetPortal);
	CHECK_DESTROY(LogPortal, (!targetPortal || !pTargetPortal), "Portal %s, was destroyed as there was no target portal or it wasnt a type of APortal class.", *GetName());
	
	// Save a reference to the player controller.
	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	CHECK_DESTROY(LogPortal, !PC, "Player controller could not be found in the portal class %s.", *GetName());
	portalController = PC;
	APortalPawn* pawn = Cast<APortalPawn>(PC->GetPawn());
	CHECK_DESTROY(LogPortal, !pawn, "Player portal pawn could not be found in the portal class %s.", *GetName());
	portalPawn = pawn;

	// Create a render target for this portal and its dynamic material instance. Then check if it has been successfully created.
	CreatePortalTexture();
	CHECK_DESTROY(LogPortal, (!renderTarget || !portalMaterial), "render target or portal material was null and could not be created in the portal class %s.", *GetName());

	// Register the secondary post physics tick function in the world on level start.
	physicsTick.bCanEverTick = true;
	physicsTick.RegisterTickFunction(GetWorld()->PersistentLevel);

	// Begin play ran.
	initialised = true;
}

void APortal::PostLoad()
{
	Super::PostLoad();

	// Bind the portals overlap events.
	portalBox->OnComponentBeginOverlap.AddDynamic(this, &APortal::OnPortalOverlap);
	portalBox->OnComponentEndOverlap.AddDynamic(this, &APortal::OnOverlapEnd);
}

void APortal::PostPhysicsTick(float DeltaTime)
{
	// If the portal is currently active.
	if (active)
	{
		// Loop through tracked actors and update the target portal relative to the actors locations and rotations.
		UpdatePawnTracking();
		UpdateTrackedActors();
	}
}

void APortal::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// If begin play has been ran.
	if (initialised)
	{
		// Clear portal information.
		portalMaterial->SetScalarParameterValue("ScaleOffset", 0.0f);
		ClearPortalView();

		// If the portal is active.
		if (active)
		{
			// Update the portals view.
			UpdatePortalView();

			// Check if the player needs teleporting through this portal.
			if (LocationInsidePortal(portalPawn->camera->GetComponentLocation()))
			{
				// Update world offset to prevent clipping.
				portalMaterial->SetScalarParameterValue("ScaleOffset", 1.0f);
			}
		}
	}
}

void APortal::OnPortalOverlap(UPrimitiveComponent* portalMeshHit, AActor* overlappedActor, UPrimitiveComponent* overlappedComp, int32 otherBodyIndex, bool fromSweep, const FHitResult& portalHit)
{
	// If a physics enabled actor passes through the portal from the correct direction duplicate and track said object at the target portal.
	USceneComponent* overlappedRootComponent = overlappedActor->GetRootComponent();
	if (overlappedRootComponent && overlappedRootComponent->IsSimulatingPhysics())
	{
		// Ensure that the item thats entering the portal is in-front.
		if (!trackedActors.Contains(overlappedActor) && IsInfront(portalHit.Location))
		{
			AddTrackedActor(overlappedActor);
		}
	}
}

void APortal::OnOverlapEnd(UPrimitiveComponent* portalMeshHit, AActor* overlappedActor, UPrimitiveComponent* overlappedComp, int32 otherBodyIndex)
{
	// Remove the actor. Does nothing if the actor isn't being tracked. Will check this.
	if (trackedActors.Contains(overlappedActor))
	{
		RemoveTrackedActor(overlappedActor);
	}
}

bool APortal::IsActive()
{
	return active;
}

void APortal::SetActive(bool activate)
{
	active = activate;
	currentFrameCount = 0;
}

void APortal::AddTrackedActor(AActor* actorToAdd)
{
	// Ensure the actor is not null.
	if (actorToAdd == nullptr) return;

	// Create tracked actor struct.
	// NOTE: If its the pawn track the camera otherwise track the root component...
	FTrackedActor* track;
	if (APortalPawn* isPawn = Cast<APortalPawn>(actorToAdd)) track = new FTrackedActor(isPawn->camera);
	else track = new FTrackedActor(actorToAdd->GetRootComponent());
	track->lastTrackedOrigin = actorToAdd->GetActorLocation();

	// Add to tracked actors.
	trackedActors.Add(actorToAdd, track);
	actorsBeingTracked++;

	// Debug tracked actor.
	if (debugTrackedActors) UE_LOG(LogPortal, Log, TEXT("Added new tracked actor %s."), *actorToAdd->GetName());

	// Create visual copy of the tracked actor.
	CopyActor(actorToAdd);
}

void APortal::RemoveTrackedActor(AActor* actorToRemove)
{
	// Ensure the actor is not null.
	if (actorToRemove == nullptr) return;

	// Destroy visual copy of the tracked actor.
	if (trackedActors.FindRef(actorToRemove))
	{
		if (AActor* isValid = trackedActors.FindRef(actorToRemove)->trackedDuplicate)
		{
			// Also remove from duplicate map.
			duplicateMap.Remove(isValid);
			isValid->Destroy();
		}
	}

	// Remove tracked actor.
	trackedActors.Remove(actorToRemove);
	actorsBeingTracked--;

	// Debug tracked actor.
	if (debugTrackedActors) UE_LOG(LogPortal, Log, TEXT("Removed tracked actor %s."), *actorToRemove->GetName());
}

void APortal::CreatePortalTexture()
{
	// Create default texture size.
	int32 viewportX, viewportY;
	portalController->GetViewportSize(viewportX, viewportY);

	// Create new render texture.
	renderTarget = nullptr;
	renderTarget = UCanvasRenderTarget2D::CreateCanvasRenderTarget2D(GetWorld(), UCanvasRenderTarget2D::StaticClass(), viewportX, viewportY);

	// Create the dynamic material instance for the portal mesh to show the render texture.
	portalMaterial = portalMesh->CreateDynamicMaterialInstance(0, portalMaterialInstance);
	portalMaterial->SetTextureParameterValue("RT_Portal", renderTarget);

	// Assign the Render Target
	portalCapture->TextureTarget = renderTarget;
}

void APortal::ClearPortalView()
{
	// Force portal to be a random color that can be found as mask.
	UKismetRenderingLibrary::ClearRenderTarget2D(GetWorld(), renderTarget);
}

void APortal::UpdatePortalView()
{
	// Increase current frame count.
	currentFrameCount++;
	if (currentFrameCount > recursionAmount) currentFrameCount = 0;

	// Get cameras post-processing settings.
	portalCapture->PostProcessSettings = portalPawn->camera->PostProcessSettings;

	// Setup clip plane to cut out objects between the camera and the back of the portal.
	portalCapture->bEnableClipPlane = true;
	portalCapture->bOverride_CustomNearClippingPlane = true;
	portalCapture->ClipPlaneNormal = pTargetPortal->portalMesh->GetForwardVector();
	
	// Set clip plane 5 units behind portal so in VR if one eye goes through the portal it will not be noticeable.
	// NOTE: In VR it might need to be adjusted even more.
	portalCapture->ClipPlaneBase = pTargetPortal->portalMesh->GetComponentLocation() - (portalCapture->ClipPlaneNormal * 5.0f);

	// Get the Projection Matrix from the players camera view settings.
	UPortalPlayer* portalPlayer = Cast<UPortalPlayer>(portalController->GetLocalPlayer());
	CHECK_DESTROY(LogPortal, !portalPlayer, "UpdatePortalView: Portal player class couldn't be found in the portal %s.", *GetName());
	portalCapture->bUseCustomProjectionMatrix = true;
	portalCapture->CustomProjectionMatrix = portalPlayer->GetCameraProjectionMatrix();

	// Get reference to player camera.
	UCameraComponent* playerCamera = portalPawn->camera;

	// Get the position of the main camera transform to the target portal.
	FVector newCameraLocation = ConvertLocationToPortal(playerCamera->GetComponentLocation(), this, pTargetPortal);
	FRotator newCameraRotation = ConvertRotationToPortal(playerCamera->GetComponentRotation(), this, pTargetPortal);

	// Update each portal a frame later than each other to prevent N^2 performance complexity.
	if (latePortalUpdate)
	{

		
	}
	// Recurse backwards for the max number of recursions and render to the texture each time overlaying each portal view.
	else
	{
		for (int i = recursionAmount; i >= 0; i--)
		{
			// Update location of the scene capture.
			FVector recursiveCamLoc = newCameraLocation;
			FRotator recursiveCamRot = newCameraRotation;
			for (int p = 0; p < i; p++)
			{
				recursiveCamLoc = ConvertLocationToPortal(recursiveCamLoc, this, pTargetPortal);
				recursiveCamRot = ConvertRotationToPortal(recursiveCamRot, this, pTargetPortal);
			}
			portalCapture->SetWorldLocationAndRotation(recursiveCamLoc, recursiveCamRot);

			// Use-full for debugging convert transform to target function on the camera.
			if (debugCameraTransform) DrawDebugBox(GetWorld(), recursiveCamLoc, FVector(10.0f), recursiveCamRot.Quaternion(), FColor::Red, false, 0.05f, 0.0f, 2.0f);

			// Set portal to not be rendered if its the first recursion event.
			// NOTE: Caps off the end so theres no visual glitches.
			if (i == recursionAmount) portalMesh->SetVisibility(false);

			// Update the portal scene capture to render it to the RT.
			portalCapture->CaptureScene();

			// Set portal to be rendered for next recursion.
			if (i == recursionAmount) portalMesh->SetVisibility(true);
		}
	}	
}

void APortal::UpdateWorldOffset()
{
	// If the camera is within the portal box.
	if (LocationInsidePortal(portalPawn->camera->GetComponentLocation()))
	{
		portalMaterial->SetScalarParameterValue("ScaleOffset", 1.0f);
	}
	else
	{
		portalMaterial->SetScalarParameterValue("ScaleOffset", 0.0f);
	}
}

void APortal::UpdatePawnTracking()
{
	// Check for when the pawn has passed through this portal between frames.
	FVector currLocation = portalPawn->camera->GetComponentLocation();
	FVector pointInterscetion;
	FPlane portalPlane = FPlane(portalMesh->GetComponentLocation(), portalMesh->GetForwardVector());
	bool passedThroughPlane = FMath::SegmentPlaneIntersection(portalPawn->lastLocation, currLocation, portalPlane, pointInterscetion);
	FVector relativeIntersection = portalMesh->GetComponentTransform().InverseTransformPositionNoScale(pointInterscetion);
	FVector portalSize = portalBox->GetScaledBoxExtent();// NOTE: Ensure portal box is setup correctly for this to work.
	bool passedWithinPortal = FMath::Abs(relativeIntersection.Z) <= portalSize.Z &&
							  FMath::Abs(relativeIntersection.Y) <= portalSize.Y;
	
	// If the pawn has passed through the plane within the portals boundaries teleport.
	if (passedThroughPlane && passedWithinPortal)
	{
		// Teleport the actor.
		TeleportObject(portalPawn);

		// Remove the pawn from tracked actors if inside.
		if (trackedActors.Contains(portalPawn)) RemoveTrackedActor(portalPawn);
	}
}

void APortal::UpdateTrackedActors()
{
	// Loop through all tracked actors. Key is the actor and Value is the tracking structure.
	if (actorsBeingTracked > 0)
	{
		TArray<AActor*> teleportedActors;
		for (TMap<AActor*, FTrackedActor*>::TIterator trackedActor = trackedActors.CreateIterator(); trackedActor; ++trackedActor)
		{
			// Update the positions for the duplicate tracked actors at the target portal. NOTE: Only if it isn't null.
			AActor* isValid = trackedActor->Value->trackedDuplicate;
			if (isValid && isValid->IsValidLowLevel())
			{
				FVector convertedLoc = ConvertLocationToPortal(trackedActor->Key->GetActorLocation(), this, pTargetPortal);
				FRotator convertedRot = ConvertRotationToPortal(trackedActor->Key->GetActorRotation(), this, pTargetPortal);
				isValid->SetActorLocationAndRotation(convertedLoc, convertedRot); 
				// Duplicates are static and should disable physics.
				// NOTE: Location updates are driven through root components the way the code is set out atm.
			}

			// If its the player skip this next part as its handled in UpdatePawnTracking.
			// NOTE: Moved into its own function as the overlap events was effecting valid teleporting on the pawn.
			if (APortalPawn* isPlayer = Cast<APortalPawn>(trackedActor->Key)) continue;

			// Check for when the actors origin passes to the other side of the portal between frames.
			FVector currLocation = trackedActor->Value->trackedComp->GetComponentLocation();
			FVector pointInterscetion;
			FPlane portalPlane = FPlane(portalMesh->GetComponentLocation(), portalMesh->GetForwardVector());
			bool passedThroughPortal = FMath::SegmentPlaneIntersection(trackedActor->Value->lastTrackedOrigin, currLocation, portalPlane, pointInterscetion);
			if (passedThroughPortal)
			{
				// Teleport the actor.
				// NOTE: If actor is simulating physics and has 
				// CCD it will effect physics objects around it when moved.
				TeleportObject(trackedActor->Key);

				// Add to be removed.
				teleportedActors.Add(trackedActor->Key);

				// Skip to next actor once removed.
				continue;
			}

			// Update last tracked origin.
			trackedActor->Value->lastTrackedOrigin = currLocation;
		}

		// Remove any teleported actors.
		// NOTE: Do post to avoid errors in for loop.
		for (AActor* actor : teleportedActors)
		{
			if (trackedActors.Contains(actor)) RemoveTrackedActor(actor);
		}
	}	
}

void APortal::TeleportObject(AActor* actor)
{
	// Return if object is null.
	if (actor == nullptr) return;

	// Perform a camera cut so the teleportation is seamless with the render functions.
	UPortalPlayer* portalPlayer = Cast<UPortalPlayer>(portalController->GetLocalPlayer());
	CHECK_DESTROY(LogPortal, !portalPlayer, "TeleportObject: Portal player class couldnt be found in the portal %s.", *GetName());
	portalPlayer->CameraCut();

	// Teleport the physics object. Teleport both position and relative velocity.
	UPrimitiveComponent* primComp = Cast<UPrimitiveComponent>(actor->GetRootComponent());
	FVector newLinearVelocity = ConvertDirectionToTarget(primComp->GetPhysicsLinearVelocity());
	FVector newAngularVelocity = ConvertDirectionToTarget(primComp->GetPhysicsAngularVelocityInDegrees());
	FVector convertedLoc = ConvertLocationToPortal(actor->GetActorLocation(), this, pTargetPortal);
	FRotator convertedRot = ConvertRotationToPortal(actor->GetActorRotation(), this, pTargetPortal);
	primComp->SetWorldLocationAndRotation(convertedLoc, convertedRot, false, nullptr, ETeleportType::TeleportPhysics);
	primComp->SetPhysicsLinearVelocity(newLinearVelocity);
	primComp->SetPhysicsAngularVelocityInDegrees(newAngularVelocity);

	// If its a player handle any extra teleporting functionality in the player class.
	if (APortalPawn* portalPawn = Cast<APortalPawn>(actor))
	{
		portalPawn->PortalTeleport(pTargetPortal);
	}

	// Update the portal view and world offset for the target portal.
	pTargetPortal->UpdateWorldOffset();
	pTargetPortal->UpdatePortalView();
}

void APortal::CopyActor(AActor* actorToCopy)
{
	// NOTE: CODE IS ONLY TESTED FOR STATIC MESHES AND THE PLAYER... CHECKS ROOT COMPONENT ONLY.
	// Create copy of the given actor.
	FName newActorName = MakeUniqueObjectName(this, AActor::StaticClass(), "CoppiedActor");
	AActor* newActor = NewObject<AActor>(this, newActorName, RF_NoFlags, actorToCopy);
	TArray<UActorComponent*> foundStaticMeshes = newActor->GetComponentsByClass(UStaticMeshComponent::StaticClass());
	newActor->RegisterAllComponents();

	// If its the player disable any important functionality.
	if (APortalPawn* isPawn = Cast<APortalPawn>(newActor))
	{
		isPawn->playerCapsule->SetCollisionResponseToChannel(ECC_PortalBox, ECR_Ignore);
		isPawn->playerCapsule->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
		isPawn->playerCapsule->SetSimulatePhysics(false);
		isPawn->PrimaryActorTick.SetTickFunctionEnable(false);
	}
	// Else assume its a static mesh or handle other objects HERE...
	else
	{
		// Make sure static meshes ignore portal to avoid duplicates of duplicates being made...
		// NOTE: Causes an issue where the duplicate when ran into will not be moved by the player
		// FIXED: Fix for this was to duplicate the player as-well and drive its position...
		for (UActorComponent* comp : foundStaticMeshes)
		{
			UStaticMeshComponent* staticComp = (UStaticMeshComponent*)comp;
			staticComp->SetCollisionResponseToChannel(ECC_PortalBox, ECR_Ignore);
			staticComp->SetCollisionResponseToChannel(ECC_Interactable, ECR_Ignore);
			staticComp->SetSimulatePhysics(false);
		}
	}
	
	// Add it to the actors tracking information.
	trackedActors.FindRef(actorToCopy)->trackedDuplicate = newActor;

	// Setup location and rotation for this frame.
	FVector newLoc = ConvertLocationToPortal(newActor->GetActorLocation(), this, pTargetPortal);
	FRotator newRot = ConvertRotationToPortal(newActor->GetActorRotation(), this, pTargetPortal);
	newActor->SetActorLocationAndRotation(newLoc, newRot);

	// Create duplicate map to original actor.
	duplicateMap.Add(newActor, actorToCopy);
}

bool APortal::IsInfront(FVector location)
{
	FPlane plane = FPlane(portalMesh->GetComponentLocation(), portalMesh->GetForwardVector());
	float dotProduct = plane.PlaneDot(location);
	return (dotProduct >= 0); // Returns true if the location is in-front of this portal.
}

FVector APortal::ConvertDirectionToTarget(FVector direction)
{
	// Flip the given direction to the target portal.
	FVector flippedVel;
	flippedVel.X = FVector::DotProduct(direction, portalMesh->GetForwardVector());
	flippedVel.Y = FVector::DotProduct(direction, portalMesh->GetRightVector());
	flippedVel.Z = FVector::DotProduct(direction, portalMesh->GetUpVector());
	FVector newVelocity = flippedVel.X * -pTargetPortal->portalMesh->GetForwardVector()
					    + flippedVel.Y * -pTargetPortal->portalMesh->GetRightVector()
						+ flippedVel.Z * pTargetPortal->portalMesh->GetUpVector();
	
	// Return flipped vector for the given direction.
	return newVelocity;
}

FVector APortal::ConvertLocationToPortal(FVector location, APortal* currentPortal, APortal* endPortal, bool flip)
{
	// Convert location to new portal.
	FVector posRelativeToPortal = currentPortal->portalMesh->GetComponentTransform().InverseTransformPositionNoScale(location);
	if (flip)
	{
		posRelativeToPortal.X *= -1; // Flip forward axis.
		posRelativeToPortal.Y *= -1; // Flip right axis.
	}
	FVector newWorldLocation = endPortal->portalMesh->GetComponentTransform().TransformPositionNoScale(posRelativeToPortal);

	// Return the new location.
	return newWorldLocation;
}

FRotator APortal::ConvertRotationToPortal(FRotator rotation, APortal* currentPortal, APortal* endPortal, bool flip)
{
	// Convert rotation to new portal.
	FRotator relativeRotation = currentPortal->portalMesh->GetComponentTransform().InverseTransformRotation(rotation.Quaternion()).Rotator();
	if (flip)
	{
		relativeRotation.Yaw += 180.0f;
	}
	FRotator newWorldRotation = endPortal->portalMesh->GetComponentTransform().TransformRotation(relativeRotation.Quaternion()).Rotator();

	// Return the new rotation.
	return newWorldRotation;
}

bool APortal::LocationInsidePortal(FVector location)
{
	// Get variables from portals box component.
	FVector halfHeight = portalBox->GetScaledBoxExtent();
	FVector direction = location - portalBox->GetComponentLocation();

	// Check all axis.
	bool withinX = FMath::Abs(FVector::DotProduct(direction, portalBox->GetForwardVector())) <= halfHeight.X;
	bool withinY = FMath::Abs(FVector::DotProduct(direction, portalBox->GetRightVector())) <= halfHeight.Y;
	bool withinZ = FMath::Abs(FVector::DotProduct(direction, portalBox->GetUpVector())) <= halfHeight.Z;

	// True if the location is within the box components span.
	return withinX && withinY && withinZ;
}

int APortal::GetNumberOfTrackedActors()
{
	return actorsBeingTracked;
}

TMap<AActor*, AActor*>& APortal::GetDuplicateMap()
{
	return duplicateMap;
}

void FPostPhysicsTick::ExecuteTick(float DeltaTime, enum ELevelTick TickType, ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
{
	// Call the Portals second tick function for running post tick.
	if (Target) Target->PostPhysicsTick(DeltaTime);
}
