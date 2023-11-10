// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "LandscapeComponent.h"
#include "MeshMaterialShader.h"
#include "LandscapeAsyncTextureReadback.h"

class ALandscapeProxy;
class FLandscapeComponentSceneProxy;
class FTextureRenderTarget2DResource;
class UTextureRenderTarget2D;

// data also accessible by render thread
class FLandscapeGrassWeightExporter_RenderThread
{
	FLandscapeGrassWeightExporter_RenderThread(const TArray<int32>& InHeightMips, bool bInUseAsyncReadback)
		: HeightMips(InHeightMips)
	{
		if (bInUseAsyncReadback)
		{
			AsyncReadbackPtr = new FLandscapeAsyncTextureReadback();
		}
	}

	friend class FLandscapeGrassWeightExporter;

public:
	virtual ~FLandscapeGrassWeightExporter_RenderThread()
	{
		if (AsyncReadbackPtr != nullptr)
		{
			AsyncReadbackPtr->QueueDeletionFromGameThread();
			AsyncReadbackPtr = nullptr;
		}
	}

	struct FComponentInfo
	{
		TObjectPtr<ULandscapeComponent> Component = nullptr;
		TArray<TObjectPtr<ULandscapeGrassType>> RequestedGrassTypes;
		FVector2D ViewOffset = FVector2D::ZeroVector;
		int32 PixelOffsetX = 0;
		FLandscapeComponentSceneProxy* SceneProxy = nullptr;
		int32 NumPasses = 0;
		int32 FirstHeightMipsPassIndex = MAX_int32;

		FComponentInfo(ULandscapeComponent* InComponent, bool bInNeedsGrassmap, bool bInNeedsHeightmap, const TArray<int32>& InHeightMips)
			: Component(InComponent)
			, SceneProxy((FLandscapeComponentSceneProxy*)InComponent->SceneProxy)
		{
			if (bInNeedsGrassmap)
			{
				RequestedGrassTypes = InComponent->GetGrassTypes();
			}
			int32 NumGrassMaps = RequestedGrassTypes.Num();
			if (bInNeedsHeightmap || NumGrassMaps > 0)
			{
				NumPasses += FMath::DivideAndRoundUp(2 /* heightmap */ + NumGrassMaps, 4);
			}
			if (InHeightMips.Num() > 0)
			{
				FirstHeightMipsPassIndex = NumPasses;
				NumPasses += InHeightMips.Num();
			}
		}
	};

	FSceneInterface* SceneInterface = nullptr;
	FTextureRenderTarget2DResource* RenderTargetResource = nullptr;
	TArray<FComponentInfo, TInlineAllocator<1>> ComponentInfos;
	FIntPoint TargetSize;
	TArray<int32> HeightMips;
	float PassOffsetX;
	FVector ViewOrigin;
	FMatrix ViewRotationMatrix;
	FMatrix ProjectionMatrix;

	FLandscapeAsyncTextureReadback* AsyncReadbackPtr = nullptr;

	void RenderLandscapeComponentToTexture_RenderThread(FRHICommandListImmediate& RHICmdList);
};

class FLandscapeGrassWeightExporter : public FLandscapeGrassWeightExporter_RenderThread
{
	TObjectPtr<ALandscapeProxy> LandscapeProxy;
	int32 ComponentSizeVerts;
	int32 SubsectionSizeQuads;
	int32 NumSubsections;
	TArray<TObjectPtr<ULandscapeGrassType>> GrassTypes;
	TObjectPtr<UTextureRenderTarget2D> RenderTargetTexture;

public:
	FLandscapeGrassWeightExporter(ALandscapeProxy* InLandscapeProxy, TArrayView<ULandscapeComponent* const> InLandscapeComponents, bool bInNeedsGrassmap = true, bool bInNeedsHeightmap = true, const TArray<int32>& InHeightMips = {}, bool bUseAsyncReadback = false);

	// If using the async readback path, check its status and update if needed. Return true when the AsyncReadbackResults are available.
	// You must call this periodically, or the async readback may not complete.
	bool CheckAndUpdateAsyncReadback()
	{
		check(AsyncReadbackPtr != nullptr);
		return AsyncReadbackPtr->CheckAndUpdate();
	}

	// return true if the async readback is complete.  (Does not update the readback state)
	bool IsAsyncReadbackComplete()
	{
		check(AsyncReadbackPtr != nullptr);
		return AsyncReadbackPtr->IsComplete();
	}

	// Fetches the results from the GPU texture and translates them into FLandscapeComponentGrassDatas.
	// If using async readback, requires AsyncReadback to be complete before calling this.
	TMap<ULandscapeComponent*, TUniquePtr<FLandscapeComponentGrassData>, TInlineSetAllocator<1>> FetchResults();

	// Fetches the results and applies them to the landscape components
	// If using async readback, requires AsyncReadback to be complete before calling this.
	void ApplyResults();

	void AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector);
};

namespace UE::Landscape::Grass
{
	void AddGrassWeightShaderTypes(FMaterialShaderTypes& InOutShaderTypes);
}

