// Fill out your copyright notice in the Description page of Project Settings.
#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HelperMacros.h"
#include "Portal.generated.h"

/* Logging category for this class. */
DECLARE_LOG_CATEGORY_EXTERN(LogPortal, Log, All);

/* Structure to hold important tracking information with each overlapping actor. */
USTRUCT(BlueprintType)
struct FTrackedActor
{
	GENERATED_BODY()

public:

	FVector lastTrackedOrigin;
	USceneComponent* trackedComp;
	AActor* trackedDuplicate;

public:

	/* Default Constructor. */
	FTrackedActor()
	{
		lastTrackedOrigin = FVector::ZeroVector;
		trackedComp = nullptr;
		trackedDuplicate = nullptr;
	}

	/* Main Constructor. */
	FTrackedActor(USceneComponent* trackingComponent)
	{
		lastTrackedOrigin = trackingComponent->GetComponentLocation();
		trackedComp = trackingComponent;
		trackedDuplicate = nullptr;
	}
};

/* Post physics update tick for updating position as my pawn position is physics driven. 
 * NOTE: This is irrelevant for a pawn that is not physics driven.
 * NOTE: This is always relevant way of tracking actors that are moving via physics.
 * NOTE: Important for checking the tracking state of the HMD and hands. 
 */
USTRUCT()
struct FPostPhysicsTick : public FActorTickFunction
{
	GENERATED_BODY()

	/* Target actor. */
	class APortal* Target;

	/* Declaration of the new ticking function for this class. */
	virtual void ExecuteTick(float DeltaTime, enum ELevelTick TickType, ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent) override;
};

template <>
struct TStructOpsTypeTraits<FPostPhysicsTick> : public TStructOpsTypeTraitsBase2<FPostPhysicsTick>
{
	enum { WithCopy = false };
};

/* Portal class to handle visualizing a portal to its target portal as well as teleportation of the players
 * or any other physics objects that could move through the portal. */
UCLASS()
class BETTERPORTALS_API APortal : public AActor
{
	GENERATED_BODY()
	
	/* Make post physics friend so it can access the tick function. */
	friend FPostPhysicsTick;

public:	
	
	/* The portal mesh. */
	UPROPERTY(BlueprintReadWrite, VisibleAnywhere, Category = "Portal")
	class UStaticMeshComponent* portalMesh;

	/* Box overlap component for teleporting actors. 
	 * NOTE: Used instead of overlaps with the portalMesh because the physics doesn't call overlaps if the mesh passed through something that doesn't block.
	 *       This also still happens with CCD enabled as its mainly for blocking physics between frames not calling overlaps... */
	UPROPERTY(BlueprintReadWrite, VisibleAnywhere, Category = "Portal")
	class UBoxComponent* portalBox;

	/* Scene capture component for creating the portal render target. */
	UPROPERTY(BlueprintReadWrite, VisibleAnywhere, Category = "Portal")
	class USceneCaptureComponent2D* portalCapture;

	/* The other portal actor to target. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Portal")
	class AActor* targetPortal;

	/* The portal material instance to create the dynamic material from to update the render texture. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Portal")
	class UMaterialInterface* portalMaterialInstance;

	/* The max number of times a portal can recurse any portals. NOTE: Only target portal is supported for now. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Portal")
	int recursionAmount;

	/* Debug the duplicated camera position and rotation relative to the other portal by drawing debug cube based of scenecapture2D transform. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Portal|Debugging")
	bool debugCameraTransform;

	/* Log when a new actor is added to the trackedActors map and when one is removed. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Portal|Debugging")
	bool debugTrackedActors;

	/* Post ticking declaration. */
	FPostPhysicsTick physicsTick;

	/* Portal class target portal. */
	APortal* pTargetPortal;

	/* Is this portal active? NOTE: Currently does nothing but is true when its being updated. */
	bool active; 

private:

	bool initialised; /* Has begin play been ran. */
	class APlayerController* portalController; /* The player controller. */
	class APortalPawn* portalPawn; /* The portal pawn. */
	class UCanvasRenderTarget2D* renderTarget; /* The portals render target texture. */
	TArray<UCanvasRenderTarget2D*> renderTargets; /* The performance render targets used when latePortalUpdate is being used. */
	class UMaterialInstanceDynamic* portalMaterial; /* The portals dynamic material instance. */
	TMap<AActor*, FTrackedActor*> trackedActors; /* Tracked actor map where each tracked actor has tracked settings like last location etc. */
	TMap<AActor*, AActor*> duplicateMap; /* Map to find an original actor from a tracked duplicate actor from a hit result for example on a duplicate. */
	int actorsBeingTracked; /* Number of actors currently being tracked. */
	int currentFrameCount; /* Frame count for updating the portals one frame late. */
	FVector lastPawnLoc; /* The pawns last tracked location for calculating when to teleport the player. */

	/* Function to teleport a given actor. */
	void TeleportObject(AActor* actor);

	/* Deletes a copied version of another actor. */
	void DeleteCopy(AActor* actorToDelete);

	/* copies a given actors static mesh root component and sets it in the tracked actor struct.
	 * NOTE: Only static meshes are duplicated but this is easily added. */
	void CopyActor(AActor* actorToCopy);

	/* Create a render texture target for this portal. */
	void CreatePortalTexture();

	/* Updates the pawns tracking for going through portals. Cannot rely on detecting overlaps. */
	void UpdatePawnTracking();

	/* Update the tracked actors relative to the target portal. 
	 * NOTE: Takes care of teleporting physics objects as well as duplicating them and the pawn if overlapping... */
	void UpdateTrackedActors();

protected:

	/* Level start. */
	virtual void BeginPlay() override;

	/* Post initialization. */
	virtual void PostInitializeComponents() override;

	/* Post physics ticking function. */
	void PostPhysicsTick(float DeltaTime);

public:

	/* Constructor. */
	APortal();
	
	/* Frame. */
	virtual void Tick(float DeltaTime) override;

	/* Called when the portal is overlapped. */
	UFUNCTION(Category = "Portal")
	void OnPortalOverlap(UPrimitiveComponent* portalMeshHit, AActor* overlappedActor, UPrimitiveComponent* overlappedComp, int32 otherBodyIndex, bool fromSweep, const FHitResult& portalHit);
	
	/* Called when the portal ends one of its overlap events. */
	UFUNCTION(Category = "Portal")
	void OnOverlapEnd(UPrimitiveComponent* portalMeshHit, AActor* overlappedActor, UPrimitiveComponent* overlappedComp, int32 otherBodyIndex);

	/* Is this portal active. */
	UFUNCTION(BlueprintCallable, Category = "Portal")
	bool IsActive();

	/* Set if the portal is active or in-active. */
	UFUNCTION(BlueprintCallable, Category = "Portal")
	void SetActive(bool activate);

	/* Adds a tracked actor to the tracked actor array updated in tick. */
	UFUNCTION(BlueprintCallable, Category = "Portal")
	void AddTrackedActor(AActor* actorToAdd);

	/* Removes a tracked actor and its duplicate. */
	UFUNCTION(BlueprintCallable, Category = "Portal")
	void RemoveTrackedActor(AActor* actorToRemove);

	/* Update the render texture for this portal using the scene capture component. */
	UFUNCTION(BlueprintCallable, Category = "Portal")
	void UpdatePortalView();

	/* Clears the current render texture on the portal meshes dynamic material instance. */
	UFUNCTION(BlueprintCallable, Category = "Portal")
	void ClearPortalView();

	/* Updates the world offset in the dynamic material instance for the vertexes on the portal mesh when the camera gets too close.
	 * NOTE: Fix for near clipping plane clipping with the portal plane mesh. */
	void UpdateWorldOffset();

	/* Is the location in-front of this portal? */
	UFUNCTION(BlueprintCallable, Category = "Portal")
	bool IsInfront(FVector location);

	/* Convert a given velocity vector to the target portal. */
	UFUNCTION(BlueprintCallable, Category = "Portal")
	FVector ConvertDirectionToTarget(FVector direction);

	/* Convert a given location to the target portal. */
	UFUNCTION(BlueprintCallable, Category = "Portal")
	FVector ConvertLocationToPortal(FVector location, APortal* currentPortal, APortal* endPortal, bool flip = true);

	/* Convert a given rotation to the target portal. */
	UFUNCTION(BlueprintCallable, Category = "Portal")
	FRotator ConvertRotationToPortal(FRotator rotation, APortal* currentPortal, APortal* endPortal, bool flip = true);

	/* Is a given location inside of this portals box. */
	UFUNCTION(BlueprintCallable, Category = "Portal")
	bool LocationInsidePortal(FVector location);

	/* Number of actors currently being tracked and duplicated at the target portal. */
	UFUNCTION(BlueprintCallable, Category = "Portal")
	int GetNumberOfTrackedActors();

	/* Returns the current duplicate map for this portal. All static meshes that are duplicated and tracked are added to this list. */
	UFUNCTION(BlueprintCallable, Category = "Portal")
	TMap<AActor*, AActor*>& GetDuplicateMap();
};
