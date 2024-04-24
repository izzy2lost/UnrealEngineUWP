// Copyright Epic Games, Inc. All Rights Reserved.

#include "NaniteDrawList.h"
#include "BasePassRendering.h"
#include "NaniteSceneProxy.h"
#include "NaniteShading.h"
#include "NaniteVertexFactory.h"
#include "SceneUtils.h"
#include "ScenePrivate.h"
#include "MeshPassProcessor.inl"

int32 GNaniteAllowProgrammableDistances = 1;
static FAutoConsoleVariableRef CVarNaniteAllowProgrammableDistances(
	TEXT("r.Nanite.AllowProgrammableDistances"),
	GNaniteAllowProgrammableDistances,
	TEXT("Whether or not to allow disabling of Nanite programmable raster features (World Position Offset, Pixel Depth Offset, ")
	TEXT("Masked Opaque, or Displacement) at a distance from the camera."),
	ECVF_ReadOnly
);

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

	for (int32 MeshPassIndex = 0; MeshPassIndex < ENaniteMeshPass::Num; ++MeshPassIndex)
	{
		FNaniteRasterPipelines& RasterPipelines = Scene.NaniteRasterPipelines[MeshPassIndex];
		FNaniteShadingPipelines& ShadingPipelines = Scene.NaniteShadingPipelines[MeshPassIndex];
		FNaniteVisibility& Visibility = Scene.NaniteVisibility[MeshPassIndex];

		for (const FDeferredPipelines& PipelinesCommand : DeferredPipelines[MeshPassIndex])
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

					AddRasterBin(*PrimitiveSceneInfo, PrimaryRasterBin, FallbackRasterBin, ENaniteMeshPass::Type(MeshPassIndex), uint8(MaterialSectionIndex));

					if (RasterBins)
					{
						RasterBins->Add(FNaniteVisibility::FRasterBin{ PrimaryRasterBin.BinIndex, FallbackRasterBin.BinIndex });
					}
				}

				// Register shading bin
				{
					const FNaniteShadingPipeline& ShadingPipeline = PipelinesCommand.ShadingPipelines[MaterialSectionIndex];
					const FNaniteShadingBin ShadingBin = ShadingPipelines.Register(ShadingPipeline);
					AddShadingBin(*PrimitiveSceneInfo, ShadingBin, ENaniteMeshPass::Type(MeshPassIndex), uint8(MaterialSectionIndex));

					if (ShadingBins)
					{
						ShadingBins->Add(FNaniteVisibility::FShadingBin{ ShadingBin.BinIndex });
					}
				}
			}

			// This will register the primitive's raster bins for custom depth, if necessary
			if (MeshPassIndex == ENaniteMeshPass::BasePass)
			{
				PrimitiveSceneInfo->RefreshNaniteRasterBins();
			}
		}
	}

}
