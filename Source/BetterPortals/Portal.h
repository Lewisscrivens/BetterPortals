// Fill out your copyright notice in the Description page of Project Settings.
#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Portal.generated.h"

/* Logging category for this class. */
DECLARE_LOG_CATEGORY_EXTERN(LogPortal, Log, All);

/* Checks and returns. */
#define CHECK_DESTROY( LogCategory, Condition, Message, ...) if(Condition) { UE_LOG( LogCategory, Warning, TEXT(Message), ##__VA_ARGS__ ); Destroy(); return;}

/* Portal class to handle visualizing a portal to its target portal as well as teleportation of the players
 * or any other physics objects that could move through the portal. */
UCLASS()
class BETTERPORTALS_API APortal : public AActor
{
	GENERATED_BODY()
	
public:	
	
	/* The portal mesh. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Portal")
	class USkeletalMeshComponent* portalMesh;

	/* Box overlap component for teleporting actors. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Portal")
	class UBoxComponent* portalBox;

	/* Scene capture component for creating the portal render target. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Portal")
	class USceneCaptureComponent2D* portalCapture;

	/* The other portal actor to target. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Portal")
	class AActor* targetPortal;

	/* The portal material instance to create the dynamic material from to update the render texture. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Portal")
	class UMaterialInterface* portalMaterialInstance;

private:

	bool active; /* Is this portal active? NOTE: Currently does nothing but is true when its being updated. */
	class APlayerController* portalController; /* The player controller. */
	class APortalPawn* portalPawn; /* The portal pawn. */
	class UCanvasRenderTarget2D* renderTarget; /* The portals render target texture. */
	class UMaterialInstanceDynamic* portalMaterial; /* The portals dynamic material instance. */

	/* Function to teleport a given actor. */
	void TeleportObject(AActor* actor);

	/* Create a render texture target for this portal. */
	void CreatePortalTexture();

protected:

	/* Level start. */
	virtual void BeginPlay() override;

public:	

	/* Constructor. */
	APortal();
	
	/* Frame. */
	virtual void Tick(float DeltaTime) override;

	/* Is this portal active. */
	UFUNCTION(BlueprintCallable, Category = "Portal")
	bool IsActive();

	/* Set if the portal is active or in-active. */
	UFUNCTION(BlueprintCallable, Category = "Portal")
	void SetActive(bool activate);

	/* Is the location infront of this portal? */
	UFUNCTION(BlueprintCallable, Category = "Portal")
	bool IsInfront(FVector location);

	/* Update the render texture for this portal using the scene capture component. */
	UFUNCTION(BlueprintCallable, Category = "Portal")
	void UpdatePortalView();

	/* Clears the current render texture on the portal meshes dynamic material instance. */
	UFUNCTION(BlueprintCallable, Category = "Portal")
	void ClearPortalView();

	/* Updates the world offset in the dynamic material instance for the vertexes on the portal mesh when the camera gets too close.
	 * NOTE: Fix for near clipping plane clipping with the portal plane mesh. */
	void UpdateWorldOffset();

	/* Convert a given location and rotation to the target portal. */
	UFUNCTION(BlueprintCallable, Category = "Portal")
	FTransform ConvertTransformToTarget(FVector location, FRotator rotation);

	/* Is a given location inside of this portals box. */
	UFUNCTION(BlueprintCallable, Category = "Portal")
	bool LocationInsidePortal(FVector location);
};
