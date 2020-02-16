// Fill out your copyright notice in the Description page of Project Settings.
#pragma once
#include "CoreMinimal.h"
#include "Portal.h"
#include "PortalPawn.h"
#include "GameFramework/GameModeBase.h"
#include "BetterPortalsGameModeBase.generated.h"

/* Logging category for this class. */
DECLARE_LOG_CATEGORY_EXTERN(LogPortalGamemode, Log, All);

/* Manages updating portals and when to set them active and inactive. */
UCLASS()
class BETTERPORTALS_API ABetterPortalsGameModeBase : public AGameModeBase
{
	GENERATED_BODY()

public:

	/* Pointers to keep track of which portals to update. */
	class APortalPawn* pawn;
	TArray<class APortal*> portals;

public:

	/* Constructor. */
	ABetterPortalsGameModeBase();
	
protected:

	/* Level start. */
	virtual void BeginPlay() override;

	/* Frame. */
	virtual void Tick(float DeltaTime) override;
	
	/* Function to update portals in the world based off player location relative to each of them. */
	void UpdatePortals();
};
