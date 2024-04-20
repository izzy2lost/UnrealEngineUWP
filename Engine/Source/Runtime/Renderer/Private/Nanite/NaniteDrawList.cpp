// Copyright Epic Games, Inc. All Rights Reserved.

#include "NaniteDrawList.h"
#include "BasePassRendering.h"
#include "NaniteSceneProxy.h"
#include "NaniteShading.h"
#include "NaniteVertexFactory.h"
#include "SceneUtils.h"
#include "ScenePrivate.h"
#include "MeshPassProcessor.inl"

int32 GNaniteMaterialSortMode = 4;
static FAutoConsoleVariableRef CVarNaniteMaterialSortMode(
	TEXT("r.Nanite.MaterialSortMode"),
	GNaniteMaterialSortMode,
	TEXT("Method of sorting Nanite material draws. 0=disabled, 1=shader, 2=sortkey, 3=refcount"),
	ECVF_RenderThreadSafe
);

int32 GNaniteAllowProgrammableDistances = 1;
static FAutoConsoleVariableRef CVarNaniteAllowProgrammableDistances(
	TEXT("r.Nanite.AllowProgrammableDistances"),
	GNaniteAllowProgrammableDistances,
	TEXT("Whether or not to allow disabling of Nanite programmable raster features (World Position Offset, Pixel Depth Offset, ")
	TEXT("Masked Opaque, or Displacement) at a distance from the camera."),
	ECVF_ReadOnly
);

FMeshDrawCommand& FNaniteDrawListContext::AddCommand(FMeshDrawCommand& Initializer, uint32 NumElements)
{
	checkf(CurrentPrimitiveSceneInfo != nullptr, TEXT("BeginPrimitiveSceneInfo() must be called on the context before adding commands"));
	checkf(CurrentMeshPass < ENaniteMeshPass::Num, TEXT("BeginMeshPass() must be called on the context before adding commands"));

	{
		MeshDrawCommandForStateBucketing.~FMeshDrawCommand();
		new(&MeshDrawCommandForStateBucketing) FMeshDrawCommand();
	}

	MeshDrawCommandForStateBucketing = Initializer;
	return MeshDrawCommandForStateBucketing;
}

void FNaniteDrawListContext::BeginPrimitiveSceneInfo(FPrimitiveSceneInfo& PrimitiveSceneInfo)
{
	checkf(CurrentPrimitiveSceneInfo == nullptr, TEXT("BeginPrimitiveSceneInfo() was called without a matching EndPrimitiveSceneInfo()"));
	check(PrimitiveSceneInfo.Proxy->IsNaniteMesh());

	CurrentPrimitiveSceneInfo = &PrimitiveSceneInfo;
}

void FNaniteDrawListContext::EndPrimitiveSceneInfo()
{
	checkf(CurrentPrimitiveSceneInfo != nullptr, TEXT("EndPrimitiveSceneInfo() was called without matching BeginPrimitiveSceneInfo()"));
	CurrentPrimitiveSceneInfo = nullptr;
}

void FNaniteDrawListContext::BeginMeshPass(ENaniteMeshPass::Type MeshPass)
{
	checkf(CurrentMeshPass == ENaniteMeshPass::Num, TEXT("BeginMeshPass() was called without a matching EndMeshPass()"));
	check(MeshPass < ENaniteMeshPass::Num);
	CurrentMeshPass = MeshPass;
}

void FNaniteDrawListContext::EndMeshPass()
{
	checkf(CurrentMeshPass < ENaniteMeshPass::Num, TEXT("EndMeshPass() was called without matching BeginMeshPass()"));
	CurrentMeshPass = ENaniteMeshPass::Num;
}

FNaniteMaterialSlot& FNaniteDrawListContext::GetMaterialSlotForWrite(FPrimitiveSceneInfo& PrimitiveSceneInfo, ENaniteMeshPass::Type MeshPass, uint8 SectionIndex)
{	
	TArray<FNaniteMaterialSlot>& MaterialSlots = PrimitiveSceneInfo.NaniteMaterialSlots[MeshPass];

	// Initialize material slots if they haven't been already
	// NOTE: Lazily initializing them like this prevents adding material slots for primitives that have no bins in the pass
	if (MaterialSlots.Num() == 0)
	{
		check(PrimitiveSceneInfo.Proxy->IsNaniteMesh());
		check(PrimitiveSceneInfo.NaniteLumenCommands.Num() == 0);
		check(PrimitiveSceneInfo.NaniteRasterBins[MeshPass].Num() == 0);
		check(PrimitiveSceneInfo.NaniteShadingBins[MeshPass].Num() == 0);

		auto* NaniteSceneProxy = static_cast<const Nanite::FSceneProxyBase*>(PrimitiveSceneInfo.Proxy);
		const int32 NumMaterialSections = NaniteSceneProxy->GetMaterialSections().Num();

		MaterialSlots.SetNumUninitialized(NumMaterialSections);
		FMemory::Memset(MaterialSlots.GetData(), 0xFF, NumMaterialSections * MaterialSlots.GetTypeSize());
	}

	check(MaterialSlots.IsValidIndex(SectionIndex));
	return MaterialSlots[SectionIndex];
}

void FNaniteDrawListContext::AddShadingCommand(FPrimitiveSceneInfo& PrimitiveSceneInfo, const FNaniteCommandInfo& ShadingCommand, ENaniteMeshPass::Type MeshPass, uint8 SectionIndex)
{
	FNaniteMaterialSlot& MaterialSlot = GetMaterialSlotForWrite(PrimitiveSceneInfo, MeshPass, SectionIndex);
	check(MaterialSlot.LegacyShadingId == 0xFFFFu);
	MaterialSlot.LegacyShadingId = uint16(ShadingCommand.GetMaterialSlot());

	PrimitiveSceneInfo.NaniteLumenCommands.Add(ShadingCommand);
}

void FNaniteDrawListContext::AddRasterBin(
	FPrimitiveSceneInfo& PrimitiveSceneInfo,
	const FNaniteRasterBin& PrimaryRasterBin,
	const FNaniteRasterBin& FallbackRasterBin,
	ENaniteMeshPass::Type MeshPass,
	uint8 SectionIndex)
{
	check(PrimaryRasterBin.IsValid());
	
	FNaniteMaterialSlot& MaterialSlot = GetMaterialSlotForWrite(PrimitiveSceneInfo, MeshPass, SectionIndex);
	check(MaterialSlot.RasterBin == 0xFFFFu);
	MaterialSlot.RasterBin = PrimaryRasterBin.BinIndex;
	MaterialSlot.FallbackRasterBin = FallbackRasterBin.BinIndex;
	
	PrimitiveSceneInfo.NaniteRasterBins[MeshPass].Add(PrimaryRasterBin);
	if (FallbackRasterBin.IsValid())
	{
		PrimitiveSceneInfo.NaniteRasterBins[MeshPass].Add(FallbackRasterBin);
	}
}

void FNaniteDrawListContext::FinalizeCommand(
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
)
{
	checkf(CurrentPrimitiveSceneInfo != nullptr, TEXT("BeginPrimitiveSceneInfo() must be called on the context before finalizing commands"));
	checkf(CurrentMeshPass < ENaniteMeshPass::Num, TEXT("BeginMeshPass() must be called on the context before finalizing commands"));

	FGraphicsMinimalPipelineStateId PipelineId;
	PipelineId = FGraphicsMinimalPipelineStateId::GetPersistentId(PipelineState);
	MeshDrawCommand.SetDrawParametersAndFinalize(MeshBatch, BatchElementIndex, PipelineId, ShadersForDebugging);
#if UE_BUILD_DEBUG
	FMeshDrawCommand MeshDrawCommandDebug = FMeshDrawCommand(MeshDrawCommand);
	check(MeshDrawCommandDebug.ShaderBindings.GetDynamicInstancingHash() == MeshDrawCommand.ShaderBindings.GetDynamicInstancingHash());
	check(MeshDrawCommandDebug.GetDynamicInstancingHash() == MeshDrawCommand.GetDynamicInstancingHash());
#endif

#if MESH_DRAW_COMMAND_DEBUG_DATA
	// When using state buckets, multiple PrimitiveSceneProxies can use the same 
	// MeshDrawCommand, so The PrimitiveSceneProxy pointer can't be stored.
	MeshDrawCommand.ClearDebugPrimitiveSceneProxy();
#endif

	const ERHIFeatureLevel::Type FeatureLevel = CurrentPrimitiveSceneInfo->Scene->GetFeatureLevel();

	FNaniteMaterialDebugViewInfo MaterialDebugViewInfo {};
#if WITH_DEBUG_VIEW_MODES
	if (ShadersForDebugging != nullptr)
	{
		uint32 InstructionCountVS = ShadersForDebugging->VertexShader->GetNumInstructions();
		uint32 InstructionCountPS = ShadersForDebugging->PixelShader->GetNumInstructions();
		MaterialDebugViewInfo.InstructionCountVS = static_cast<uint16>(FMath::Clamp(InstructionCountVS, 0, TNumericLimits<uint16>::Max()));
		MaterialDebugViewInfo.InstructionCountPS = static_cast<uint16>(FMath::Clamp(InstructionCountPS, 0, TNumericLimits<uint16>::Max()));

#if WITH_EDITOR
		FMaterialShaderMap* MaterialShaderMap = MeshDrawCommand.GetDebugData().Material->GetRenderingThreadShaderMap();
		if (ensure(MaterialShaderMap))
		{
			uint32 LWCComplexityVS = 0;
			uint32 LWCComplexityPS = 0;

			MaterialShaderMap->GetEstimatedLWCFuncUsageComplexity(LWCComplexityVS, LWCComplexityPS);

			// Set minimum complexity to 1, to differentiate between 0 cost and missing data
			MaterialDebugViewInfo.LWCComplexityVS = static_cast<uint16>(FMath::Clamp(LWCComplexityVS++, 1, TNumericLimits<uint16>::Max()));
			MaterialDebugViewInfo.LWCComplexityPS = static_cast<uint16>(FMath::Clamp(LWCComplexityPS++, 1, TNumericLimits<uint16>::Max()));
		}
#endif
	}
#endif

	const bool bWPOEnabled = MeshBatch.MaterialRenderProxy && MeshBatch.MaterialRenderProxy->GetIncompleteMaterialWithFallback(FeatureLevel).MaterialUsesWorldPositionOffset_RenderThread();

	// Defer the command
	DeferredCommands[CurrentMeshPass].Add(
		FDeferredCommand {
			CurrentPrimitiveSceneInfo,
			MeshDrawCommand,
			FNaniteMaterialEntryMap::ComputeHash(MeshDrawCommand),
			MaterialDebugViewInfo,
			MeshBatch.SegmentIndex,
			bWPOEnabled
		}
	);
}

void FNaniteDrawListContext::Apply(FScene& Scene)
{
	check(IsInParallelRenderingThread());

	{
		FNaniteMaterialCommands& ShadingCommands = Scene.NaniteLumenMaterials;
		FNaniteRasterPipelines& RasterPipelines  = Scene.NaniteRasterPipelines[ENaniteMeshPass::LumenCardCapture];
		FNaniteShadingPipelines& ShadingPipelines = Scene.NaniteShadingPipelines[ENaniteMeshPass::LumenCardCapture];
		FNaniteVisibility& Visibility = Scene.NaniteVisibility[ENaniteMeshPass::LumenCardCapture];

		for (auto& Command : DeferredCommands[ENaniteMeshPass::LumenCardCapture])
		{
			FPrimitiveSceneInfo* PrimitiveSceneInfo = Command.PrimitiveSceneInfo;
			FNaniteCommandInfo CommandInfo = ShadingCommands.Register(Command.MeshDrawCommand, Command.CommandHash, Command.MaterialDebugViewInfo, Command.bWPOEnabled);
			AddShadingCommand(*PrimitiveSceneInfo, CommandInfo, ENaniteMeshPass::Type(ENaniteMeshPass::LumenCardCapture), Command.SectionIndex);
		}

		for (const FDeferredPipelines& PipelinesCommand : DeferredPipelines[ENaniteMeshPass::LumenCardCapture])
		{
			FPrimitiveSceneInfo* PrimitiveSceneInfo = PipelinesCommand.PrimitiveSceneInfo;
			FNaniteVisibility::PrimitiveRasterBinType*  RasterBins  = Visibility.GetRasterBinReferences(PrimitiveSceneInfo);
			FNaniteVisibility::PrimitiveShadingBinType* ShadingBins = Visibility.GetShadingBinReferences(PrimitiveSceneInfo);

			const int32 MaterialSectionCount = PipelinesCommand.RasterPipelines.Num();
			for (int32 MaterialSectionIndex = 0; MaterialSectionIndex < MaterialSectionCount; ++MaterialSectionIndex)
			{
				// Register raster bin
				{
					const FNaniteRasterPipeline& RasterPipeline = PipelinesCommand.RasterPipelines[MaterialSectionIndex];
					FNaniteRasterBin PrimaryRasterBin = RasterPipelines.Register(RasterPipeline);

					// Check to register a fallback bin (used to disable programmable functionality at a distance)
					FNaniteRasterBin FallbackRasterBin;
					FNaniteRasterPipeline FallbackRasterPipeline;
					if (GNaniteAllowProgrammableDistances && RasterPipeline.GetFallbackPipeline(FallbackRasterPipeline))
					{
						FallbackRasterBin = RasterPipelines.Register(FallbackRasterPipeline);
					}

					AddRasterBin(*PrimitiveSceneInfo, PrimaryRasterBin, FallbackRasterBin, ENaniteMeshPass::LumenCardCapture, uint8(MaterialSectionIndex));

					if (RasterBins)
					{
						RasterBins->Add(FNaniteVisibility::FRasterBin{ PrimaryRasterBin.BinIndex, FallbackRasterBin.BinIndex });
					}
				}
			}
		}
	}
}

void SubmitNaniteMultiViewMaterial(
	const FMeshDrawCommand& MeshDrawCommand,
	const float MaterialDepth,
	const TShaderMapRef<FNaniteMultiViewMaterialVS>& VertexShader,
	const FGraphicsMinimalPipelineStateSet& GraphicsMinimalPipelineStateSet,
	const uint32 InstanceFactor,
	FRHICommandList& RHICmdList,
	FMeshDrawCommandStateCache& StateCache,
	uint32 InstanceBaseOffset)
{
#if WANTS_DRAW_MESH_EVENTS
	FMeshDrawCommand::FMeshDrawEvent MeshEvent(MeshDrawCommand, InstanceFactor, RHICmdList);
#endif

	FMeshDrawCommandSceneArgs SceneArgs;
	bool bAllowSkipDrawCommand = true;
	if (!FMeshDrawCommand::SubmitDrawBegin(MeshDrawCommand, GraphicsMinimalPipelineStateSet, SceneArgs, InstanceFactor, RHICmdList, StateCache, bAllowSkipDrawCommand))
	{
		return;
	}

	// All Nanite mesh draw commands are using the same vertex shader, which has a material depth parameter we assign at render time.
	{
		FNaniteMultiViewMaterialVS::FParameters Parameters;
		Parameters.MaterialDepth = MaterialDepth;
		Parameters.InstanceBaseOffset = InstanceBaseOffset;
		SetShaderParameters(RHICmdList, VertexShader, VertexShader.GetVertexShader(), Parameters);
	}

	FMeshDrawCommand::SubmitDrawEnd(MeshDrawCommand, SceneArgs, InstanceFactor, RHICmdList);
}

/////

FNaniteMaterialSlot& FNaniteMaterialListContext::GetMaterialSlotForWrite(FPrimitiveSceneInfo& PrimitiveSceneInfo, ENaniteMeshPass::Type MeshPass, uint8 SectionIndex)
{	
	TArray<FNaniteMaterialSlot>& MaterialSlots = PrimitiveSceneInfo.NaniteMaterialSlots[MeshPass];

	// Initialize material slots if they haven't been already
	// NOTE: Lazily initializing them like this prevents adding material slots for primitives that have no bins in the pass
	if (MaterialSlots.Num() == 0)
	{
		check(PrimitiveSceneInfo.Proxy->IsNaniteMesh());
		check(PrimitiveSceneInfo.NaniteRasterBins[MeshPass].Num() == 0);
		check(PrimitiveSceneInfo.NaniteShadingBins[MeshPass].Num() == 0);

		auto* NaniteSceneProxy = static_cast<const Nanite::FSceneProxyBase*>(PrimitiveSceneInfo.Proxy);
		const int32 NumMaterialSections = NaniteSceneProxy->GetMaterialSections().Num();

		MaterialSlots.SetNumUninitialized(NumMaterialSections);
		FMemory::Memset(MaterialSlots.GetData(), 0xFF, NumMaterialSections * MaterialSlots.GetTypeSize());
	}

	check(MaterialSlots.IsValidIndex(SectionIndex));
	return MaterialSlots[SectionIndex];
}

void FNaniteMaterialListContext::AddShadingBin(FPrimitiveSceneInfo& PrimitiveSceneInfo, const FNaniteShadingBin& ShadingBin, ENaniteMeshPass::Type MeshPass, uint8 SectionIndex)
{
	FNaniteMaterialSlot& MaterialSlot = GetMaterialSlotForWrite(PrimitiveSceneInfo, MeshPass, SectionIndex);
	check(MaterialSlot.ShadingBin == 0xFFFFu);
	MaterialSlot.ShadingBin = ShadingBin.BinIndex;

	PrimitiveSceneInfo.NaniteShadingBins[MeshPass].Add(ShadingBin);
}

void FNaniteMaterialListContext::AddRasterBin(
	FPrimitiveSceneInfo& PrimitiveSceneInfo,
	const FNaniteRasterBin& PrimaryRasterBin,
	const FNaniteRasterBin& FallbackRasterBin,
	ENaniteMeshPass::Type MeshPass,
	uint8 SectionIndex)
{
	check(PrimaryRasterBin.IsValid());
	
	FNaniteMaterialSlot& MaterialSlot = GetMaterialSlotForWrite(PrimitiveSceneInfo, MeshPass, SectionIndex);
	check(MaterialSlot.RasterBin == 0xFFFFu);
	MaterialSlot.RasterBin = PrimaryRasterBin.BinIndex;
	MaterialSlot.FallbackRasterBin = FallbackRasterBin.BinIndex;
	
	PrimitiveSceneInfo.NaniteRasterBins[MeshPass].Add(PrimaryRasterBin);
	if (FallbackRasterBin.IsValid())
	{
		PrimitiveSceneInfo.NaniteRasterBins[MeshPass].Add(FallbackRasterBin);
	}
}

void FNaniteMaterialListContext::Apply(FScene& Scene)
{
	check(IsInParallelRenderingThread());

	{
		FNaniteRasterPipelines& RasterPipelines = Scene.NaniteRasterPipelines[ENaniteMeshPass::BasePass];
		FNaniteShadingPipelines& ShadingPipelines = Scene.NaniteShadingPipelines[ENaniteMeshPass::BasePass];
		FNaniteVisibility& Visibility = Scene.NaniteVisibility[ENaniteMeshPass::BasePass];

		for (const FDeferredPipelines& PipelinesCommand : DeferredPipelines[ENaniteMeshPass::BasePass])
		{
			FPrimitiveSceneInfo* PrimitiveSceneInfo = PipelinesCommand.PrimitiveSceneInfo;
			FNaniteVisibility::PrimitiveRasterBinType* RasterBins = Visibility.GetRasterBinReferences(PrimitiveSceneInfo);
			FNaniteVisibility::PrimitiveShadingBinType* ShadingBins = Visibility.GetShadingBinReferences(PrimitiveSceneInfo);

			check((PipelinesCommand.RasterPipelines.Num() == PipelinesCommand.ShadingPipelines.Num()));
			const int32 MaterialSectionCount = PipelinesCommand.RasterPipelines.Num();
			for (int32 MaterialSectionIndex = 0; MaterialSectionIndex < MaterialSectionCount; ++MaterialSectionIndex)
			{
				// Register raster bin
				{
					const FNaniteRasterPipeline& RasterPipeline = PipelinesCommand.RasterPipelines[MaterialSectionIndex];
					FNaniteRasterBin PrimaryRasterBin = RasterPipelines.Register(RasterPipeline);

					// Check to register a fallback bin (used to disable programmable functionality at a distance)
					FNaniteRasterBin FallbackRasterBin;
					FNaniteRasterPipeline FallbackRasterPipeline;
					if (GNaniteAllowProgrammableDistances && RasterPipeline.GetFallbackPipeline(FallbackRasterPipeline))
					{
						FallbackRasterBin = RasterPipelines.Register(FallbackRasterPipeline);
					}

					AddRasterBin(*PrimitiveSceneInfo, PrimaryRasterBin, FallbackRasterBin, ENaniteMeshPass::BasePass, uint8(MaterialSectionIndex));

					if (RasterBins)
					{
						RasterBins->Add(FNaniteVisibility::FRasterBin{ PrimaryRasterBin.BinIndex, FallbackRasterBin.BinIndex });
					}
				}

				// Register shading bin
				{
					const FNaniteShadingPipeline& ShadingPipeline = PipelinesCommand.ShadingPipelines[MaterialSectionIndex];
					const FNaniteShadingBin ShadingBin = ShadingPipelines.Register(ShadingPipeline);
					AddShadingBin(*PrimitiveSceneInfo, ShadingBin, ENaniteMeshPass::BasePass, uint8(MaterialSectionIndex));

					if (ShadingBins)
					{
						ShadingBins->Add(FNaniteVisibility::FShadingBin{ ShadingBin.BinIndex });
					}
				}
			}

			// This will register the primitive's raster bins for custom depth, if necessary
			PrimitiveSceneInfo->RefreshNaniteRasterBins();
		}
	}

}
