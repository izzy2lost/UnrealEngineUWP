// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "LandscapeComponent.h"
#include "MeshMaterialShader.h"

class ALandscapeProxy;
class FLandscapeComponentSceneProxy;
class FTextureRenderTarget2DResource;
class UTextureRenderTarget2D;

// data also accessible by render thread
class FLandscapeGrassWeightExporter_RenderThread
{
	FLandscapeGrassWeightExporter_RenderThread(const TArray<int32>& InHeightMips)
		: HeightMips(InHeightMips)
	{
	}

	friend class FLandscapeGrassWeightExporter;

public:
	virtual ~FLandscapeGrassWeightExporter_RenderThread()
	{}

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

	void RenderLandscapeComponentToTexture_RenderThread(FRHICommandListImmediate& RHICmdList);
};

class FLandscapeGrassWeightShaderElementData : public FMeshMaterialShaderElementData
{
public:

	int32 OutputPass;
	FVector2f RenderOffset;
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
	FLandscapeGrassWeightExporter(ALandscapeProxy* InLandscapeProxy, TArrayView<ULandscapeComponent* const> InLandscapeComponents, bool bInNeedsGrassmap = true, bool bInNeedsHeightmap = true, const TArray<int32>& InHeightMips = {});
	TMap<ULandscapeComponent*, TUniquePtr<FLandscapeComponentGrassData>, TInlineSetAllocator<1>> FetchResults();
	void ApplyResults();
	void AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector);
};

namespace UE::Landscape::Grass
{
	void AddGrassWeightShaderTypes(FMaterialShaderTypes& InOutShaderTypes);
}

