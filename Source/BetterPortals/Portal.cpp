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

DEFINE_LOG_CATEGORY(LogPortal);

APortal::APortal()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = ETickingGroup::TG_PostUpdateWork;

	// Create class default sub-objects.
	RootComponent = CreateDefaultSubobject<USceneComponent>("RootComponent");
	RootComponent->Mobility = EComponentMobility::Static; // Portals are static objects.
	portalMesh = CreateDefaultSubobject<UStaticMeshComponent>("PortalMesh");
	portalMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	portalMesh->SetupAttachment(RootComponent);
	portalMesh->CastShadow = false; // Dont want it casting shadows.

	// Setup portal overlap box for detecting actors.
	portalBox = CreateDefaultSubobject<UBoxComponent>(TEXT("PortalBox"));
	portalBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	portalBox->SetCollisionProfileName("Portal");
	portalBox->SetupAttachment(portalMesh);
	portalBox->OnComponentBeginOverlap.AddDynamic(this, &APortal::OnPortalOverlap);
	portalBox->OnComponentEndOverlap.AddDynamic(this, &APortal::OnOverlapEnd);
	
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

	// Set active by default. 
    // NOTE: If performant portals is enabled in game mode portals will be deactivated until needed to be activated...
	active = true;
	initialised = false;
	debugCameraTransform = false;
	actorsBeingTracked = 0;
}

void APortal::BeginPlay()
{
	Super::BeginPlay();

	// If there is no target destroy and print log message.
	CHECK_DESTROY(LogPortal, (!targetPortal || !Cast<APortal>(targetPortal)), "Portal %s, was destroyed as there was no target portal or it wasnt a type of APortal class.", *GetName());

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

	// Bind the overlapping functions if not yet bound.
	if (!portalBox->OnComponentBeginOverlap.IsAlreadyBound(this, &APortal::OnPortalOverlap)) portalBox->OnComponentBeginOverlap.AddDynamic(this, &APortal::OnPortalOverlap);
	if (!portalBox->OnComponentEndOverlap.IsAlreadyBound(this, &APortal::OnOverlapEnd)) portalBox->OnComponentEndOverlap.AddDynamic(this, &APortal::OnOverlapEnd);

	// Begin play ran.
	initialised = true;
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

			// Loop through tracked actors and update the target portal relative to the actors locations and rotations.
			UpdateTrackedActors();
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
		if (IsInfront(portalHit.Location))
		{
			AddTrackedActor(overlappedActor);
		}
	}
}

void APortal::OnOverlapEnd(UPrimitiveComponent* portalMeshHit, AActor* overlappedActor, UPrimitiveComponent* overlappedComp, int32 otherBodyIndex)
{
	// Remove the actor. Does nothing if the actor isnt being tracked. Will check this.
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
}

void APortal::AddTrackedActor(AActor* actorToAdd)
{
	// Create visual copy of the tracked actor.

	// Create tracked actor struct.
	FTrackedActor* track = new FTrackedActor();
	track->lastTrackedOrigin = actorToAdd->GetActorLocation();

	// Add to tracked actors.
	trackedActors.Add(actorToAdd, track);
	actorsBeingTracked++;
}

void APortal::RemoveTrackedActor(AActor* actorToRemove)
{
	// Remove visual copy of the tracked actor.


	// Remove tracked actor.
	trackedActors.Remove(actorToRemove);
	actorsBeingTracked--;
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
}

void APortal::ClearPortalView()
{
	portalMaterial->SetTextureParameterValue("RT_Portal", nullptr);
}

void APortal::UpdatePortalView()
{
	// Get reference to player camera.
	UCameraComponent* playerCamera = portalPawn->camera;

	// Position the scene capture relative to the other portal to get the render texture.
	FTransform newCamTransform = ConvertTransformToTarget(playerCamera->GetComponentLocation(), playerCamera->GetComponentRotation());
	portalCapture->SetWorldLocationAndRotation(newCamTransform.GetLocation(), newCamTransform.GetRotation());

	// Use-full for debugging convert transform to target function on the camera.
	if (debugCameraTransform) DrawDebugBox(GetWorld(), newCamTransform.GetLocation(), FVector(10.0f), newCamTransform.GetRotation(), FColor::Red, false, 0.05f, 0.0f, 2.0f);

	// Get cameras post-processing settings.
	portalCapture->PostProcessSettings = portalPawn->camera->PostProcessSettings;

	// Setup clip plane to cut out objects between the camera and the back of the portal.
	APortal* portalTarget = Cast<APortal>(targetPortal);
	portalCapture->bEnableClipPlane = true;
	portalCapture->bOverride_CustomNearClippingPlane = true;
	portalCapture->ClipPlaneNormal = portalTarget->portalMesh->GetForwardVector();
	
	// Set clip plane 5 units behind portal so in VR if one eye goes through the portal it will not be noticeable.
	// NOTE: In VR it might need to be adjusted even more.
	portalCapture->ClipPlaneBase = portalTarget->portalMesh->GetComponentLocation() - (portalCapture->ClipPlaneNormal * 5.0f);

	// Assign the Render Target
	portalCapture->TextureTarget = renderTarget;

	// Get the Projection Matrix from the players camera view settings.
	UPortalPlayer* portalPlayer = Cast<UPortalPlayer>(portalController->GetLocalPlayer());
	CHECK_DESTROY(LogPortal, !portalPlayer, "UpdatePortalView: Portal player class couldn't be found in the portal %s.", *GetName());
	portalCapture->bUseCustomProjectionMatrix = true;
	portalCapture->CustomProjectionMatrix = portalPlayer->GetCameraProjectionMatrix();

	// Update the portal scene capture to render it to the RT.
	portalCapture->CaptureScene();
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

void APortal::UpdateTrackedActors()
{
	// Loop through all tracked actors. Key is the actor and Value is the tracking structure.
	if (actorsBeingTracked > 0)
	{
		TArray<AActor*> teleportedActors;
		for (TMap<AActor*, FTrackedActor*>::TIterator trackedActor = trackedActors.CreateIterator(); trackedActor; ++trackedActor)
		{
			// Update the positions for the duplicate tracked actors at the target portal.

			// Check for when the actors origin passes to the other side of the portal between frames.
			FVector currLocation = trackedActor->Key->GetActorLocation();
			if (IsInfront(trackedActor->Value->lastTrackedOrigin) && !IsInfront(currLocation) && LocationInsidePortal(currLocation))
			{
				// Teleport the actor.
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
			RemoveTrackedActor(actor);
		}
	}	
}

void APortal::TeleportObject(AActor* actor)
{
	// Return if object is null.
	if (actor == nullptr) return;

	// Perform a camera cut so the teleportation is seamless with the render functions.
	APortal* pTargetPortal = Cast<APortal>(targetPortal);
	UPortalPlayer* portalPlayer = Cast<UPortalPlayer>(portalController->GetLocalPlayer());
	CHECK_DESTROY(LogPortal, !portalPlayer, "TeleportObject: Portal player class couldnt be found in the portal %s.", *GetName());
	portalPlayer->CameraCut();

	// Teleport the physics object. Teleport both position and relative velocity.
	UPrimitiveComponent* primComp = Cast<UPrimitiveComponent>(actor->GetRootComponent());
	FVector newLinearVelocity = ConvertVectorToTarget(primComp->GetPhysicsLinearVelocity());
	FVector newAngularVelocity = ConvertVectorToTarget(primComp->GetPhysicsAngularVelocityInDegrees());
	FTransform convertedTrans = ConvertTransformToTarget(actor->GetActorLocation(), actor->GetActorRotation());
	primComp->SetWorldLocationAndRotation(convertedTrans.GetLocation(), convertedTrans.GetRotation(), false, nullptr, ETeleportType::TeleportPhysics);
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

bool APortal::IsInfront(FVector location)
{
	FPlane plane = FPlane(portalMesh->GetComponentLocation(), portalMesh->GetForwardVector());
	float dotProduct = plane.PlaneDot(location);
	return (dotProduct >= 0); // Returns true if the location is in-front of this portal.
}

FVector APortal::ConvertVectorToTarget(FVector velocity)
{
	// Get pointer to target.
	APortal* pTargetPortal = Cast<APortal>(targetPortal);

	// Flip the given velocity to the target portal.
	FVector flippedVel;
	flippedVel.X = FVector::DotProduct(velocity, portalMesh->GetForwardVector());
	flippedVel.Y = FVector::DotProduct(velocity, portalMesh->GetRightVector());
	flippedVel.Z = FVector::DotProduct(velocity, portalMesh->GetUpVector());
	FVector newVelocity = flippedVel.X * -pTargetPortal->portalMesh->GetForwardVector()
					    + flippedVel.Y * -pTargetPortal->portalMesh->GetRightVector()
						+ flippedVel.Z * pTargetPortal->portalMesh->GetUpVector();
	
	// Return flipped vector for velocity.
	return newVelocity;
}

FTransform APortal::ConvertTransformToTarget(FVector location, FRotator rotation, bool flip)
{
	// Get target portal actor.
	APortal* portalTarget = Cast<APortal>(targetPortal);
 
	// Convert location to new portal.
	FVector posRelativeToPortal = portalMesh->GetComponentTransform().InverseTransformPositionNoScale(location);
	if (flip)
	{
		posRelativeToPortal.X *= -1; // Flip forward axis.
		posRelativeToPortal.Y *= -1; // Flip right axis.
	}
	FVector newWorldLocation = portalTarget->portalMesh->GetComponentTransform().TransformPositionNoScale(posRelativeToPortal);

	// Convert rotation to new portal.
	FRotator relativeRotation = portalMesh->GetComponentTransform().InverseTransformRotation(rotation.Quaternion()).Rotator();
	if (flip)
	{
		relativeRotation.Yaw += 180.0f;
	}
	FQuat newWorldRotation = portalTarget->portalMesh->GetComponentTransform().TransformRotation(relativeRotation.Quaternion());

	// Set location and rotation variables to return.
	FTransform newTransform;
	newTransform.SetLocation(newWorldLocation);
	newTransform.SetRotation(newWorldRotation);

	// Return the converted location and rotation.
	return newTransform;
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

