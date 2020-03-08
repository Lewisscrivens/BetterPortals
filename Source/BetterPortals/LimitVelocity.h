// Fill out your copyright notice in the Description page of Project Settings.

#pragma once
#include "CoreMinimal.h"
#include "HelperMacros.h"
#include "Components/ActorComponent.h"
#include "LimitVelocity.generated.h"

/* Logging category for this class. */
DECLARE_LOG_CATEGORY_EXTERN(LogLimitVelocity, Log, All);

/* Class that will track a specified component of an actor if simulating physics to keep it bellow a given velocity. */
/* FOR SOME REASON IT JUST BREAKS THE PLAYER WHEN ADDED TO IT? */
UCLASS( ClassGroup=(Portals), meta=(BlueprintSpawnableComponent), Blueprintable, BlueprintType )
class BETTERPORTALS_API ULimitVelocity : public UActorComponent
{
	GENERATED_BODY()

public:

	/* The component to tracks name. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics")
	FName trackedCompName;

	/* Max linear velocity the specified component can reach. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics")
	float maxVelocity;

private:

	UPrimitiveComponent* trackedComponent;/* The component to track. */

public:	
	
	/* Constructor. */
	ULimitVelocity();

protected:
	
	/* Level start. */
	virtual void BeginPlay() override;

public:	
	
	/* Frame. */
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;		
};
