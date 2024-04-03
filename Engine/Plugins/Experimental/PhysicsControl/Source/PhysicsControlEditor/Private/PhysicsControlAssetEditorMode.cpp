// Copyright Epic Games, Inc. All Rights Reserved.

#include "PhysicsControlAssetEditorMode.h"
#include "PhysicsControlEditorModule.h"
#include "PhysicsControlAssetEditorToolkit.h"
#include "AssetEditorModeManager.h"

FName FPhysicsControlAssetEditorMode::ModeName("PhysicsControlAssetEditMode");

//======================================================================================================================
bool FPhysicsControlAssetEditorMode::GetCameraTarget(FSphere& OutTarget) const
{
	return false;
}

//======================================================================================================================
IPersonaPreviewScene& FPhysicsControlAssetEditorMode::GetAnimPreviewScene() const
{
	return *static_cast<IPersonaPreviewScene*>(static_cast<FAssetEditorModeManager*>(Owner)->GetPreviewScene());
}

//======================================================================================================================
void FPhysicsControlAssetEditorMode::Render(const FSceneView* View, FViewport* Viewport, FPrimitiveDrawInterface* PDI)
{
	FEdMode::Render(View, Viewport, PDI);
}

//======================================================================================================================
void FPhysicsControlAssetEditorMode::Tick(FEditorViewportClient* ViewportClient, float DeltaTime)
{
	FEdMode::Tick(ViewportClient, DeltaTime);
}

//======================================================================================================================
void FPhysicsControlAssetEditorMode::DrawHUD(FEditorViewportClient* ViewportClient, FViewport* Viewport, const FSceneView* View, FCanvas* Canvas)
{
	FEdMode::DrawHUD(ViewportClient, Viewport, View, Canvas);
}
