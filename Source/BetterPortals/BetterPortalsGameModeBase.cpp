// Fill out your copyright notice in the Description page of Project Settings.
#include "BetterPortalsGameModeBase.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"

DEFINE_LOG_CATEGORY(LogPortalGamemode);

ABetterPortalsGameModeBase::ABetterPortalsGameModeBase()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = ETickingGroup::TG_PostUpdateWork;

	// Defaults.
	performantPortals = true;
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

	// Get all portals in the scene.
	for (TActorIterator<AActor> portal (GetWorld(), APortal::StaticClass()); portal; ++portal)
	{
		APortal* foundPortal = Cast<APortal>(*portal);
		portals.Add(foundPortal);
	}
}

void ABetterPortalsGameModeBase::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// Run function to update the portals in the scene.
	if (performantPortals && portals.Num() > 0)
	{
		UpdatePortals();
	}
}

void ABetterPortalsGameModeBase::UpdatePortals()
{
	// Loop through each portal until the closest max updates are found.
	TArray<APortal*> portalsToUpdate;
	for (APortal* portal : portals)
	{
		// Get portal info.
		FVector portalLoc = portal->GetActorLocation();
		FVector portalNorm = -1 * portal->GetActorForwardVector();

		// Reset Portals.
		portal->SetActive(false);

		// TODO:
		// Find the next closest Portal when the player is Standing in front of.
		// Find if the players camera is facing the portal also.
		bool closest = false;
		bool infront = false;
		if (closest && infront)
		{
			portalsToUpdate.Add(portal);
		}
	}

	// Update the found portals.
	for (APortal* portal : portalsToUpdate)
	{
		portal->SetActive(true);
	}
}