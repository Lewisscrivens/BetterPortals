// Fill out your copyright notice in the Description page of Project Settings.
#include "Portal.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/BoxComponent.h"
#include "PhysicsEngine/PhysicsHandleComponent.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/CanvasRenderTarget2D.h"
#include "Engine/Engine.h"
#include "Camera/CameraComponent.h"
#include "Engine/LocalPlayer.h"
#include "Plane.h"
#include "PortalPawn.h"
#include "PortalPlayer.h"

DEFINE_LOG_CATEGORY(LogPortal);

APortal::APortal()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = ETickingGroup::TG_PostUpdateWork;

	// Create class default sub-objects.
	RootComponent = CreateDefaultSubobject<USceneComponent>("RootComponent");
	RootComponent->Mobility = EComponentMobility::Static; // Portals are static objects.
	portalMesh = CreateDefaultSubobject<USkeletalMeshComponent>("PortalMesh");

	// Setup portal overlap box for detecting actors.
	portalBox = CreateDefaultSubobject<UBoxComponent>(TEXT("PortalBox"));
	
	// Setup default scene capture comp.
	portalCapture = CreateDefaultSubobject<USceneCaptureComponent2D>("PortalCapture");
	portalCapture->AttachToComponent(GetRootComponent(), FAttachmentTransformRules::SnapToTargetIncludingScale);
	portalCapture->bCaptureEveryFrame = false;
	portalCapture->bCaptureOnMovement = false;
	portalCapture->LODDistanceFactor = 3;
	portalCapture->TextureTarget = nullptr;
	portalCapture->bEnableClipPlane = true;
	portalCapture->bUseCustomProjectionMatrix = true;
	portalCapture->CaptureSource = ESceneCaptureSource::SCS_SceneColorSceneDepth;
	FPostProcessSettings CaptureSettings;
	CaptureSettings.bOverride_AmbientOcclusionQuality = true;
	CaptureSettings.bOverride_MotionBlurAmount = true;
	CaptureSettings.bOverride_SceneFringeIntensity = true;
	CaptureSettings.bOverride_GrainIntensity = true;
	CaptureSettings.bOverride_ScreenSpaceReflectionQuality = true;
	CaptureSettings.AmbientOcclusionQuality = 50.0f;
	CaptureSettings.MotionBlurAmount = 0.0f; 
	CaptureSettings.SceneFringeIntensity = 0.0f; 
	CaptureSettings.GrainIntensity = 0.0f; 
	CaptureSettings.ScreenSpaceReflectionQuality = 0.0f; // Set to one to enable SSR.
	CaptureSettings.bOverride_ScreenPercentage = true;
	CaptureSettings.ScreenPercentage = 100.0f;
	portalCapture->PostProcessSettings = CaptureSettings;

	// Set default class variables.
	active = false;
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
}

void APortal::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

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

			// TODO: Check if the players camera has passed through the portal between frames...

		}

		// NOTE: GET ALL OVERLAPPING PHYSICS ACTORS WITH THE PORTAL BOX AND DECIDE TO TELEPORT OR NOT...



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

bool APortal::IsInfront(FVector location)
{
	FPlane plane = FPlane(portalMesh->GetComponentLocation(), portalMesh->GetForwardVector());
	float dotProduct = plane.PlaneDot(location);
	return (dotProduct >= 0); // Returns true if the location is in-front of this portal.
}

void APortal::CreatePortalTexture()
{
	// Create default texture size.
	int32 viewportX, viewportY;
	portalController->GetViewportSize(viewportX, viewportY);

	// Cleanup existing Render textures stuck in memory.
	if (renderTarget != nullptr && renderTarget->IsValidLowLevel())
	{
		renderTarget->BeginDestroy();
		renderTarget = nullptr;
		GEngine->ForceGarbageCollection();
	}

	// Create new render texture.
	renderTarget = nullptr;
	renderTarget = NewObject<UCanvasRenderTarget2D>(this, UCanvasRenderTarget2D::StaticClass(), *FString("PortalRenderTarget"));
	renderTarget->SizeX = viewportX;
	renderTarget->SizeY = viewportY;

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

	// Setup clip plane to cut out objects between the camera and the back of the portal.
	portalCapture->ClipPlaneNormal = targetPortal->GetActorForwardVector();
	portalCapture->ClipPlaneBase = targetPortal->GetActorLocation() + (portalCapture->ClipPlaneNormal * -1.5f);

	// Assign the Render Target
	portalCapture->TextureTarget = renderTarget;

	// Get the Projection Matrix from the players camera view settings.
	UPortalPlayer* portalPlayer = Cast<UPortalPlayer>(portalController->GetLocalPlayer());
	CHECK_DESTROY(LogPortal, !portalPlayer, "UpdatePortalView: Portal player class couldnt be found in the portal %s.", *GetName());
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

void APortal::TeleportObject(AActor* actor)
{
	// Return if object is null.
	if (actor == nullptr) return;

	// Perform a camera cut so the teleportation is seamless with the render functions.
	UPortalPlayer* portalPlayer = Cast<UPortalPlayer>(portalController->GetLocalPlayer());
	CHECK_DESTROY(LogPortal, !portalPlayer, "TeleportObject: Portal player class couldnt be found in the portal %s.", *GetName());
	portalPlayer->CameraCut();

	// Teleport the physics object.
	FTransform convertedTrans = ConvertTransformToTarget(actor->GetActorLocation(), actor->GetActorRotation());
	actor->SetActorLocationAndRotation(convertedTrans.GetLocation(), convertedTrans.GetRotation(), false, nullptr, ETeleportType::TeleportPhysics);

	// If its a player handle any extra teleporting functionality in the player class.
	APortal* pTargetPortal = Cast<APortal>(targetPortal);
	if (APortalPawn* portalPawn = Cast<APortalPawn>(actor))
	{
		portalPawn->PortalTeleport(pTargetPortal);
	}

	// Update the portal view and world offset for the target portal.
	pTargetPortal->UpdateWorldOffset();
	pTargetPortal->UpdatePortalView();
}

FTransform APortal::ConvertTransformToTarget(FVector location, FRotator rotation)
{
	FTransform newTransform;

	// Translate location to target portal location.
	FVector direction = location - GetActorLocation();
	FVector newWorldLocation = targetPortal->GetActorLocation();
	FVector dot;
	dot.X = FVector::DotProduct(direction, GetActorForwardVector());
	dot.Y = FVector::DotProduct(direction, GetActorRightVector());
	dot.Z = FVector::DotProduct(direction, GetActorUpVector());
	FVector newDirection = dot.X * targetPortal->GetActorForwardVector() 
		+ dot.Y * targetPortal->GetActorRightVector()
		+ dot.Z * targetPortal->GetActorUpVector();

	// Translate rotation to target portal rotation.
	FQuat relativeRotation = GetActorTransform().GetRotation().Inverse() * rotation.Quaternion();
	FQuat newWorldRotation = targetPortal->GetActorTransform().GetRotation() * relativeRotation;

	// Set location and rotation variables to return.
	newTransform.SetLocation(newWorldLocation + newDirection);
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

