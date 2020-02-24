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
	
	/* Should the game mode class activate/deactivate portals based on player location and distance. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Portal")
	bool performantPortals;

	/* Check for portals being rendered on screen every portalUpdateRate seconds. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Portal")
	float portalUpdateRate;

	/* Pointers to keep track of which portals to update. */
	class APortalPawn* pawn;

	/* Timer handle for the portals udpate function. */
	FTimerHandle portalsTimer;

public:

	/* Constructor. */
	ABetterPortalsGameModeBase();

	/* Function to update portals in the world based off player location relative to each of them. */
	UFUNCTION(Category = "Portals")
	void UpdatePortals();
	
protected:

	/* Level start. */
	virtual void BeginPlay() override;

	/* Frame. */
	virtual void Tick(float DeltaTime) override;
};
