// Fill out your copyright notice in the Description page of Project Settings.
#include "BetterPortalsGameModeBase.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "TimerManager.h"
#include "GameFramework/Actor.h"
#include "PortalPawn.h"
#include "Camera/CameraComponent.h"

DEFINE_LOG_CATEGORY(LogPortalGamemode);

ABetterPortalsGameModeBase::ABetterPortalsGameModeBase()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = ETickingGroup::TG_PostUpdateWork;

	// Defaults.
	performantPortals = true;
	checkDirection = false;
	portalUpdateRate = 0.1f;
	maxPortalRenderDistance = 500.0f;
}

void ABetterPortalsGameModeBase::BeginPlay()
{
	Super::BeginPlay();

	// Save reference to the player and all portals in the scene.
	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	CHECK_DESTROY(LogPortalGamemode, !PC, "Player controller could not be found in the gamemode class %s.", *GetName());
	APortalPawn* foundPawn = Cast<APortalPawn>(PC->GetPawn());
	CHECK_DESTROY(LogPortalGamemode, !foundPawn, "Player portal pawn could not be found in the portal class %s.", *GetName());
	pawn = foundPawn;

	// Set off timer to update the portals in the world.
	if (performantPortals)
	{
		FTimerDelegate timer;
		timer.BindUFunction(this, "UpdatePortals");
		GetWorld()->GetTimerManager().SetTimer(portalsTimer, timer, portalUpdateRate, true);
	}
}

void ABetterPortalsGameModeBase::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// NOT IN USE USING TIMER INSTEAD.
}

void ABetterPortalsGameModeBase::UpdatePortals()
{
	// Get all portals in the scene.
	for (TActorIterator<AActor> portal(GetWorld(), APortal::StaticClass()); portal; ++portal)
	{
		APortal* foundPortal = Cast<APortal>(*portal);

		// Get portal info.
		FVector portalLoc = foundPortal->GetActorLocation();
		FVector portalNorm = -1 * foundPortal->GetActorForwardVector();

		// Get the pawns facing direction.
		FVector pawnLoc = pawn->GetActorLocation();
		FVector pawnDirection = pawn->camera->GetForwardVector();
		FVector portalDirection = portalLoc - pawnLoc;

		// Get the angle difference in their directions. In Degrees.
		float angleDifference = FMath::Abs(FMath::Acos(FVector::DotProduct(pawnDirection, portalNorm))) * (180.0f / PI);
		UE_LOG(LogTemp, Warning, TEXT("Angle Diff: %f"), angleDifference);

		// Get distance from portal.
		float portalDistance = FMath::Abs(portalDirection.Size());
		float angleDiffAmount = portalDistance <= 1000.0f ? 130.0f : 90.0f;

		// Activate the portals based on distance, if the camera is in-front and facing...
		// NOTE: This is only an example of how the portals can be made less of an impact to performance.
		// NOTE: It would be better to find a way of checking if portals are being rendered.
		// to take it further you could check recursions to see if the portal actually needs to recurse itself.
		bool looking = checkDirection ? angleDifference < angleDiffAmount : true;
		if (foundPortal->IsInfront(pawnLoc) && looking && portalDistance <= maxPortalRenderDistance)
		{
			foundPortal->SetActive(true);
		}
		else
		{
			foundPortal->SetActive(false);
		}
	}
}