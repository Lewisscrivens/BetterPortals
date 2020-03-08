// Fill out your copyright notice in the Description page of Project Settings.
#include "LimitVelocity.h"
#include "GameFramework/Actor.h"
#include "Components/PrimitiveComponent.h"

DEFINE_LOG_CATEGORY(LogLimitVelocity);

ULimitVelocity::ULimitVelocity()
{
	// We will want to do the tick check post physics for adjusting velocity straight after it is calculated.
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = ETickingGroup::TG_PostPhysics;

}

void ULimitVelocity::BeginPlay()
{
	Super::BeginPlay();

	// Find component to start tracking.
	for (UActorComponent* currentComp : GetOwner()->GetComponents())
	{
		if (currentComp->GetFName() == trackedCompName)
		{
			trackedComponent = Cast<UPrimitiveComponent>(currentComp);
			break;
		}
	}

	// Check if not found if so destroy and print log message.
	CHECK_DESTROY_COMP(LogLimitVelocity, !trackedComponent, "The tracked component name %s couldn't be found in actor %s. Actor component destroyed...", *trackedCompName.ToString(), *GetOwner()->GetName());

	// If there is no maxVelocity print warning to log.
	CHECK_WARNING(LogLimitVelocity, maxVelocity <= 0.0f, "ULimitVelocity has no maxVelocity set owned by %s.", *GetOwner()->GetName());
}

void ULimitVelocity::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// Check if found components velocity is over the limit if so clamp and reset it.
	if (trackedComponent->IsSimulatingPhysics())
	{
		// Only adjust if we are over the max.
		FVector currentVelocity = trackedComponent->GetPhysicsLinearVelocity();
		if (currentVelocity.Size() > maxVelocity)
		{
			FVector clampedVelocity = currentVelocity.GetClampedToSize(0.0f, maxVelocity);
			trackedComponent->SetPhysicsLinearVelocity(clampedVelocity);
		}
	}
}

