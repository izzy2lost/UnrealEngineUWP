// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/WeakObjectPtrTemplates.h"

class AWaterZone;
class UWaterBodyComponent;
class FSceneInterface;
class UTextureRenderTarget2DArray;
class UPrimitiveComponent;
class FSceneView;
class FSceneViewFamily;


template <typename KeyType, typename ValueType>
using TWeakObjectPtrKeyMap = TMap<TWeakObjectPtr<KeyType>, ValueType, FDefaultSetAllocator, TWeakObjectPtrMapKeyFuncs<TWeakObjectPtr<KeyType>, ValueType>>;

namespace UE::WaterInfo
{

struct FRenderingContext
{
	AWaterZone* ZoneToRender = nullptr;
	UTextureRenderTarget2DArray* TextureRenderTarget;
	TArray<TWeakObjectPtr<UWaterBodyComponent>> WaterBodies;
	TArray<TWeakObjectPtr<UPrimitiveComponent>> GroundPrimitiveComponents;
	float CaptureZ;
};
	
void UpdateWaterInfoRendering(
	FSceneInterface* Scene,
	const FRenderingContext& Context);

void UpdateWaterInfoRendering2(
	FSceneView& InView, 
	const TWeakObjectPtrKeyMap<AWaterZone, UE::WaterInfo::FRenderingContext> WaterInfoContexts);

void UpdateWaterInfoRendering_CustomRenderPass(
	FSceneInterface* Scene,
	const FSceneViewFamily& ViewFamily,
	const FRenderingContext& Context);

const FName& GetWaterInfoDepthPassName();
const FName& GetWaterInfoColorPassName();
const FName& GetWaterInfoDilationPassName();

} // namespace UE::WaterInfo
