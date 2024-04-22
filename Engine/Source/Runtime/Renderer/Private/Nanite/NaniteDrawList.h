// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NaniteShared.h"
#include "NaniteMaterials.h"
#include "PrimitiveSceneInfo.h"

struct MeshDrawCommandKeyFuncs;
class FParallelCommandListBindings;
class FRDGParallelCommandListSet;

class FNaniteMaterialListContext
{
public:
	struct FDeferredPipelines
	{
		FPrimitiveSceneInfo* PrimitiveSceneInfo;
		TArray<FNaniteRasterPipeline, TInlineAllocator<4>> RasterPipelines;
		TArray<FNaniteShadingPipeline, TInlineAllocator<4>> ShadingPipelines;
	};

public:
	void Apply(FScene& Scene);

private:
	FNaniteMaterialSlot& GetMaterialSlotForWrite(FPrimitiveSceneInfo& PrimitiveSceneInfo, ENaniteMeshPass::Type MeshPass, uint8 SectionIndex);
	void AddRasterBin(FPrimitiveSceneInfo& PrimitiveSceneInfo, const FNaniteRasterBin& PrimaryRasterBin, const FNaniteRasterBin& FallbackRasterBin, ENaniteMeshPass::Type MeshPass, uint8 SectionIndex);
	void AddShadingBin(FPrimitiveSceneInfo& PrimitiveSceneInfo, const FNaniteShadingBin& ShadingBin, ENaniteMeshPass::Type MeshPass, uint8 SectionIndex);

public:
	TArray<FDeferredPipelines> DeferredPipelines[ENaniteMeshPass::Num];
	FMaterialRelevance CombinedRelevance;
};

class FNaniteDrawListContext : public FMeshPassDrawListContext
{
public:
	struct FDeferredCommand
	{
		FPrimitiveSceneInfo* PrimitiveSceneInfo;
		FMeshDrawCommand MeshDrawCommand;
		FNaniteMaterialCommands::FCommandHash CommandHash;
		FNaniteMaterialDebugViewInfo MaterialDebugViewInfo;
		uint8 SectionIndex;
		bool bWPOEnabled;
	};

	struct FDeferredPipelines
	{
		FPrimitiveSceneInfo* PrimitiveSceneInfo;
		TArray<FNaniteRasterPipeline, TInlineAllocator<4>> RasterPipelines;
		TArray<FNaniteShadingPipeline, TInlineAllocator<4>> ShadingPipelines;
	};

public:
	struct FPrimitiveSceneInfoScope
	{
		FPrimitiveSceneInfoScope(const FPrimitiveSceneInfoScope&) = delete;
		FPrimitiveSceneInfoScope& operator=(const FPrimitiveSceneInfoScope&) = delete;

		inline FPrimitiveSceneInfoScope(FNaniteDrawListContext& InContext, FPrimitiveSceneInfo& PrimitiveSceneInfo)
			: Context(InContext)
		{
			Context.BeginPrimitiveSceneInfo(PrimitiveSceneInfo);
		}

		inline ~FPrimitiveSceneInfoScope()
		{
			Context.EndPrimitiveSceneInfo();
		}

	private:
		FNaniteDrawListContext& Context;
	};

	struct FMeshPassScope
	{
		FMeshPassScope(const FMeshPassScope&) = delete;
		FMeshPassScope& operator=(const FMeshPassScope&) = delete;

		inline FMeshPassScope(FNaniteDrawListContext& InContext, ENaniteMeshPass::Type MeshPass)
			: Context(InContext)
		{
			Context.BeginMeshPass(MeshPass);
		}

		inline ~FMeshPassScope()
		{
			Context.EndMeshPass();
		}
		
	private:
		FNaniteDrawListContext& Context;
	};

	virtual FMeshDrawCommand& AddCommand(FMeshDrawCommand& Initializer, uint32 NumElements) override final;

	virtual void FinalizeCommand(
		const FMeshBatch& MeshBatch,
		int32 BatchElementIndex,
		const FMeshDrawCommandPrimitiveIdInfo& IdInfo,
		ERasterizerFillMode MeshFillMode,
		ERasterizerCullMode MeshCullMode,
		FMeshDrawCommandSortKey SortKey,
		EFVisibleMeshDrawCommandFlags Flags,
		const FGraphicsMinimalPipelineStateInitializer& PipelineState,
		const FMeshProcessorShaders* ShadersForDebugging,
		FMeshDrawCommand& MeshDrawCommand
	) override final;

	void BeginPrimitiveSceneInfo(FPrimitiveSceneInfo& PrimitiveSceneInfo);
	void EndPrimitiveSceneInfo();

	void BeginMeshPass(ENaniteMeshPass::Type MeshPass);
	void EndMeshPass();

	void Apply(FScene& Scene);

private:
	FNaniteMaterialSlot& GetMaterialSlotForWrite(FPrimitiveSceneInfo& PrimitiveSceneInfo, ENaniteMeshPass::Type MeshPass, uint8 SectionIndex);
	void AddShadingCommand(FPrimitiveSceneInfo& PrimitiveSceneInfo, const FNaniteCommandInfo& CommandInfo, ENaniteMeshPass::Type MeshPass, uint8 SectionIndex);
	void AddShadingBin(FPrimitiveSceneInfo& PrimitiveSceneInfo, const FNaniteShadingBin& ShadingBin, ENaniteMeshPass::Type MeshPass, uint8 SectionIndex);
	void AddRasterBin(FPrimitiveSceneInfo& PrimitiveSceneInfo, const FNaniteRasterBin& PrimaryRasterBin, const FNaniteRasterBin& FallbackRasterBin, ENaniteMeshPass::Type MeshPass, uint8 SectionIndex);

private:
	FMeshDrawCommand MeshDrawCommandForStateBucketing;
	FPrimitiveSceneInfo* CurrentPrimitiveSceneInfo = nullptr;
	ENaniteMeshPass::Type CurrentMeshPass = ENaniteMeshPass::Num;

public:
	TArray<FDeferredCommand> DeferredCommands[ENaniteMeshPass::Num];
	TArray<FDeferredPipelines> DeferredPipelines[ENaniteMeshPass::Num];

	FMaterialRelevance CombinedRelevance;
};

void SubmitNaniteMultiViewMaterial(
	const FMeshDrawCommand& MeshDrawCommand,
	const float MaterialDepth,
	const TShaderMapRef<FNaniteMultiViewMaterialVS>& VertexShader,
	const FGraphicsMinimalPipelineStateSet& GraphicsMinimalPipelineStateSet,
	const uint32 InstanceFactor,
	FRHICommandList& RHICmdList,
	FMeshDrawCommandStateCache& StateCache,
	uint32 InstanceBaseOffset
);
