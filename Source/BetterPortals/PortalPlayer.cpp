// Fill out your copyright notice in the Description page of Project Settings.
#include "PortalPlayer.h"
#include "Engine/LocalPlayer.h"
#include "SceneView.h"

UPortalPlayer::UPortalPlayer()
{
	camCut = false;
}

FSceneView* UPortalPlayer::CalcSceneView(class FSceneViewFamily* ViewFamily, FVector& OutViewLocation, FRotator& OutViewRotation, FViewport* Viewport, class FViewElementDrawer* ViewDrawer, EStereoscopicPass StereoPass)
{
	FSceneView* view = Super::CalcSceneView(ViewFamily, OutViewLocation, OutViewRotation, Viewport, ViewDrawer, StereoPass);
	if (camCut) view->bCameraCut = true;
	return view;
}

void UPortalPlayer::CameraCut()
{
	camCut = true;
}

FMatrix UPortalPlayer::GetCameraProjectionMatrix()
{
	FMatrix projMatrix;
	FSceneViewProjectionData projData;
	GetProjectionData(ViewportClient->Viewport, EStereoscopicPass::eSSP_FULL, projData);//EStereoscopicPass::eSSP_RIGHT_EYE VR matrix copy...
	projMatrix = projData.ProjectionMatrix;
	return projMatrix;
}
