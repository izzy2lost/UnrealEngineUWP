// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/WeakObjectPtrTemplates.h"

class AWaterZone;
class UWaterBodyComponent;
class FSceneInterface;
class UTextureRenderTarget2D;
class UPrimitiveComponent;
class FSceneView;

namespace UE::WaterInfo
{

struct FRenderingContext
{
	AWaterZone* ZoneToRender = nullptr;
	UTextureRenderTarget2D* TextureRenderTarget;
	TArray<UWaterBodyComponent*> WaterBodies;
	TArray<TWeakObjectPtr<UPrimitiveComponent>> GroundPrimitiveComponents;
	float CaptureZ;
};
	
void UpdateWaterInfoRendering(
	FSceneInterface* Scene,
	const FRenderingContext& Context);

void UpdateWaterInfoRendering2(
	FSceneView& InView, 
	const TMap<AWaterZone*, UE::WaterInfo::FRenderingContext>& WaterInfoContexts);

void UpdateWaterInfoRendering_CustomRenderPass(
	FSceneInterface* Scene,
	const FRenderingContext& Context);

const FName& GetWaterInfoDepthPassName();
const FName& GetWaterInfoColorPassName();
const FName& GetWaterInfoDilationPassName();

} // namespace UE::WaterInfo
