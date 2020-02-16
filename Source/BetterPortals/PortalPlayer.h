// Fill out your copyright notice in the Description page of Project Settings.
#pragma once
#include "CoreMinimal.h"
#include "Engine/LocalPlayer.h"
#include "PortalPlayer.generated.h"

/* New local player class to add camera cutting to the view settings and projection matrix retrieval functions. */
UCLASS()
class BETTERPORTALS_API UPortalPlayer : public ULocalPlayer
{
	GENERATED_BODY()

private:

	bool camCut; /* Camera cut enabled in FSceneView. */
	
public:

	/* Constructor. */
	UPortalPlayer();

	/* Override calculate scene view. */
	virtual FSceneView* CalcSceneView(class FSceneViewFamily* ViewFamily, FVector& OutViewLocation, FRotator& OutViewRotation, FViewport* Viewport, class FViewElementDrawer* ViewDrawer, EStereoscopicPass StereoPass) override;

	/* Cut the camera's frame. */
	void CameraCut();

	/* Get the cameras projection matrix. */
	FMatrix GetCameraProjectionMatrix();
};
