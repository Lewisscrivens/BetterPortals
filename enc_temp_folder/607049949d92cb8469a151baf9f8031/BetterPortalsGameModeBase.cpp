// Fill out your copyright notice in the Description page of Project Settings.
#include "BetterPortalsGameModeBase.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "TimerManager.h"
#include "GameFramework/Actor.h"

DEFINE_LOG_CATEGORY(LogPortalGamemode);

ABetterPortalsGameModeBase::ABetterPortalsGameModeBase()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = ETickingGroup::TG_PostUpdateWork;

	// Defaults.
	performantPortals = true;
	portalUpdateRate = 0.1f;
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
	FTimerDelegate timer;
	timer.BindUFunction(this, "UpdatePortals");
	GetWorld()->GetTimerManager().SetTimer(portalsTimer, timer, portalUpdateRate, true);
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
		FVector portalLoc = portal->GetActorLocation();
		FVector portalNorm = -1 * portal->GetActorForwardVector();

		// TODO: Check for if the player is in-front also.
		// Activate given portals based on if they was previously rendered.
		if (portal->WasRecentlyRendered(GetWorld()->GetDeltaSeconds()))
		{
			foundPortal->SetActive(true);
		}
		else
		{
			foundPortal->SetActive(false);
		}
	}
}