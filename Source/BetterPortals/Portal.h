// Fill out your copyright notice in the Description page of Project Settings.
#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Portal.generated.h"

/* Logging category for this class. */
DECLARE_LOG_CATEGORY_EXTERN(LogPortal, Log, All);

/* Checks and returns. */
#define CHECK_DESTROY( LogCategory, Condition, Message, ...) if(Condition) { UE_LOG( LogCategory, Warning, TEXT(Message), ##__VA_ARGS__ ); Destroy(); return;}

/* Structure to hold important tracking information with each overlapping actor. */
USTRUCT(BlueprintType)
struct FTrackedActor
{
	GENERATED_BODY()

public:

	FVector lastTrackedOrigin;

public:

	/* Constructor. */
	FTrackedActor()
	{
		lastTrackedOrigin = FVector::ZeroVector;
	}
};

/* Portal class to handle visualizing a portal to its target portal as well as teleportation of the players
 * or any other physics objects that could move through the portal. */
UCLASS()
class BETTERPORTALS_API APortal : public AActor
{
	GENERATED_BODY()
	
public:	
	
	/* The portal mesh. */
	UPROPERTY(BlueprintReadWrite, VisibleAnywhere, Category = "Portal")
	class UStaticMeshComponent* portalMesh;

	/* Box overlap component for teleporting actors. */
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

	/* Debug the duplicated camera position and rotation relative to the other portal by drawing debug cube based of scenecapture2D transform. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Portal")
	bool debugCameraTransform;

private:

	bool initialised; /* Has begin play been ran. */
	bool active; /* Is this portal active? NOTE: Currently does nothing but is true when its being updated. */
	class APlayerController* portalController; /* The player controller. */
	class APortalPawn* portalPawn; /* The portal pawn. */
	class UCanvasRenderTarget2D* renderTarget; /* The portals render target texture. */
	class UMaterialInstanceDynamic* portalMaterial; /* The portals dynamic material instance. */
	TMap<AActor*, FTrackedActor*> trackedActors; /* Tracked actor map where each tracked actor has tracked settings like last location etc. */
	int actorsBeingTracked; /* Number of actors currently being tracked. */

	/* Function to teleport a given actor. */
	void TeleportObject(AActor* actor);

	/* Create a render texture target for this portal. */
	void CreatePortalTexture();

	/* Update the tracked actors relative to the target portal. */
	void UpdateTrackedActors();

protected:

	/* Level start. */
	virtual void BeginPlay() override;

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
	FVector ConvertVelocityToTarget(FVector velocity);

	/* Convert a given location and rotation to the target portal. */
	UFUNCTION(BlueprintCallable, Category = "Portal")
	FTransform ConvertTransformToTarget(FVector location, FRotator rotation, bool flip = true);

	/* Is a given location inside of this portals box. */
	UFUNCTION(BlueprintCallable, Category = "Portal")
	bool LocationInsidePortal(FVector location);

	/* Number of actors currently being tracked and duplicated at the target portal. */
	UFUNCTION(BlueprintCallable, Category = "Portal")
	int GetNumberOfTrackedActors();
};
