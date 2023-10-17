// Copyright Epic Games, Inc. All Rights Reserved.

#include "LandscapeGrassWeightExporter.h"
#include "SceneRendererInterface.h"
#include "DataDrivenShaderPlatformInfo.h"
#include "Materials/Material.h"
#include "LandscapeGrassType.h"
#include "Engine/TextureRenderTarget2D.h"
#include "EngineModule.h"
#include "LandscapeRender.h"
#include "MeshPassProcessor.h"
#include "MeshPassProcessor.inl"
#include "SimpleMeshDrawCommandPass.h"
#include "TextureResource.h"
#include "RenderCaptureInterface.h"

#if WITH_EDITOR
class FLandscapeGrassWeightVS;
class FLandscapeGrassWeightPS;

int32 RenderCaptureNextGrassmapDraws = 0;
static FAutoConsoleVariableRef CVarRenderCaptureNextGrassmapDraws(
	TEXT("landscape.RenderCaptureNextGrassmapDraws"),
	RenderCaptureNextGrassmapDraws,
	TEXT("Trigger render captures during the next N grassmap draw calls."));

BEGIN_SHADER_PARAMETER_STRUCT(FLandscapeGrassPassParameters, )
	SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
	SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FSceneUniformParameters, Scene)
	SHADER_PARAMETER_STRUCT_INCLUDE(FInstanceCullingDrawParams, InstanceCullingDrawParams)
	RENDER_TARGET_BINDING_SLOTS()
	END_SHADER_PARAMETER_STRUCT()

class FLandscapeGrassWeightShaderElementData : public FMeshMaterialShaderElementData
{
public:
	int32 OutputPass;
	FVector2f RenderOffset;
};

class FLandscapeGrassWeightMeshProcessor : public FMeshPassProcessor
{
public:
	FLandscapeGrassWeightMeshProcessor(const FScene* Scene, ERHIFeatureLevel::Type InFeatureLevel, const FSceneView* InViewIfDynamicMeshCommand, FMeshPassDrawListContext* InDrawListContext);

	void AddMeshBatch(const FMeshBatch& RESTRICT MeshBatch,
		uint64 BatchElementMask,
		int32 NumPasses,
		FVector2D ViewOffset,
		float PassOffsetX,
		int32 FirstHeightMipsPassIndex,
		const TArray<int32>& HeightMips,
		const FPrimitiveSceneProxy* RESTRICT PrimitiveSceneProxy);

	virtual void AddMeshBatch(const FMeshBatch& RESTRICT MeshBatch, uint64 BatchElementMask, const FPrimitiveSceneProxy* RESTRICT PrimitiveSceneProxy, int32 StaticMeshId = -1) override final
	{
		checkf(false, TEXT("Default AddMeshBatch can't be used as rendering requires extra parameters per pass."));
	}

private:
	bool TryAddMeshBatch(
		const FMeshBatch& RESTRICT MeshBatch,
		uint64 BatchElementMask,
		const FPrimitiveSceneProxy* RESTRICT PrimitiveSceneProxy,
		int32 StaticMeshId,
		const FMaterialRenderProxy& MaterialRenderProxy,
		const FMaterial& MaterialResource,
		int32 NumPasses,
		FVector2D ViewOffset,
		float PassOffsetX,
		int32 FirstHeightMipsPassIndex,
		const TArray<int32>& HeightMips);

	bool Process(
		const FMeshBatch& MeshBatch,
		uint64 BatchElementMask,
		const FPrimitiveSceneProxy* RESTRICT PrimitiveSceneProxy,
		const FMaterialRenderProxy& RESTRICT MaterialRenderProxy,
		const FMaterial& RESTRICT MaterialResource,
		int32 NumPasses,
		FVector2D ViewOffset,
		float PassOffsetX,
		int32 FirstHeightMipsPassIndex,
		const TArray<int32>& HeightMips);

	FMeshPassProcessorRenderState PassDrawRenderState;
};

FLandscapeGrassWeightMeshProcessor::FLandscapeGrassWeightMeshProcessor(const FScene* Scene, ERHIFeatureLevel::Type InFeatureLevel, const FSceneView* InViewIfDynamicMeshCommand, FMeshPassDrawListContext* InDrawListContext)
	: FMeshPassProcessor(EMeshPass::Num, Scene, InFeatureLevel, InViewIfDynamicMeshCommand, InDrawListContext)
{
	PassDrawRenderState.SetBlendState(TStaticBlendState<>::GetRHI());
	PassDrawRenderState.SetDepthStencilState(TStaticDepthStencilState<false, CF_Always>::GetRHI());
}

void FLandscapeGrassWeightMeshProcessor::AddMeshBatch(const FMeshBatch& RESTRICT MeshBatch,
	uint64 BatchElementMask,
	int32 NumPasses,
	FVector2D ViewOffset,
	float PassOffsetX,
	int32 FirstHeightMipsPassIndex,
	const TArray<int32>& HeightMips,
	const FPrimitiveSceneProxy* RESTRICT PrimitiveSceneProxy)
{
	const FMaterialRenderProxy* MaterialRenderProxy = MeshBatch.MaterialRenderProxy;
	while (MaterialRenderProxy)
	{
		const FMaterial* Material = MaterialRenderProxy->GetMaterialNoFallback(FeatureLevel);
		if (Material)
		{
			if (TryAddMeshBatch(MeshBatch, BatchElementMask, PrimitiveSceneProxy, -1, *MaterialRenderProxy, *Material, NumPasses, ViewOffset, PassOffsetX, FirstHeightMipsPassIndex, HeightMips))
			{
				break;
			}
		}

		MaterialRenderProxy = MaterialRenderProxy->GetFallback(FeatureLevel);
	}
}

bool FLandscapeGrassWeightMeshProcessor::TryAddMeshBatch(
	const FMeshBatch& RESTRICT MeshBatch,
	uint64 BatchElementMask,
	const FPrimitiveSceneProxy* RESTRICT PrimitiveSceneProxy,
	int32 StaticMeshId,
	const FMaterialRenderProxy& MaterialRenderProxy,
	const FMaterial& MaterialResource,
	int32 NumPasses,
	FVector2D ViewOffset,
	float PassOffsetX,
	int32 FirstHeightMipsPassIndex,
	const TArray<int32>& HeightMips)
{
	check(MeshBatch.VertexFactory != nullptr);
	return Process(MeshBatch, BatchElementMask, PrimitiveSceneProxy, MaterialRenderProxy, MaterialResource, NumPasses, ViewOffset, PassOffsetX, FirstHeightMipsPassIndex, HeightMips);
}

bool FLandscapeGrassWeightMeshProcessor::Process(
	const FMeshBatch& MeshBatch,
	uint64 BatchElementMask,
	const FPrimitiveSceneProxy* RESTRICT PrimitiveSceneProxy,
	const FMaterialRenderProxy& RESTRICT MaterialRenderProxy,
	const FMaterial& RESTRICT MaterialResource,
	int32 NumPasses,
	FVector2D ViewOffset,
	float PassOffsetX,
	int32 FirstHeightMipsPassIndex,
	const TArray<int32>& HeightMips)
{
	const FVertexFactory* VertexFactory = MeshBatch.VertexFactory;

	TMeshProcessorShaders<
		FLandscapeGrassWeightVS,
		FLandscapeGrassWeightPS> PassShaders;

	FMaterialShaderTypes ShaderTypes;
	ShaderTypes.AddShaderType<FLandscapeGrassWeightVS>();
	ShaderTypes.AddShaderType<FLandscapeGrassWeightPS>();

	FMaterialShaders Shaders;
	if (!MaterialResource.TryGetShaders(ShaderTypes, VertexFactory->GetType(), Shaders))
	{
		return false;
	}

	Shaders.TryGetVertexShader(PassShaders.VertexShader);
	Shaders.TryGetPixelShader(PassShaders.PixelShader);

	const FMeshDrawingPolicyOverrideSettings OverrideSettings = ComputeMeshOverrideSettings(MeshBatch);
	const ERasterizerFillMode MeshFillMode = ComputeMeshFillMode(MaterialResource, OverrideSettings);
	const ERasterizerCullMode MeshCullMode = CM_None;

	FLandscapeGrassWeightShaderElementData ShaderElementData;
	ShaderElementData.InitializeMeshMaterialData(ViewIfDynamicMeshCommand, PrimitiveSceneProxy, MeshBatch, -1, true);

	const FMeshDrawCommandSortKey SortKey = CalculateMeshStaticSortKey(PassShaders.VertexShader, PassShaders.PixelShader);

	for (int32 PassIndex = 0; PassIndex < NumPasses; ++PassIndex)
	{
		ShaderElementData.OutputPass = (PassIndex >= FirstHeightMipsPassIndex) ? 0 : PassIndex;
		ShaderElementData.RenderOffset = FVector2f(ViewOffset) + FVector2f(PassOffsetX * PassIndex, 0);	// LWC_TODO: Precision loss

		uint64 Mask = (PassIndex >= FirstHeightMipsPassIndex) ? HeightMips[PassIndex - FirstHeightMipsPassIndex] : BatchElementMask;

		BuildMeshDrawCommands(
			MeshBatch,
			Mask,
			PrimitiveSceneProxy,
			MaterialRenderProxy,
			MaterialResource,
			PassDrawRenderState,
			PassShaders,
			MeshFillMode,
			MeshCullMode,
			SortKey,
			EMeshPassFeatures::Default,
			ShaderElementData);
	}

	return true;
}


void FLandscapeGrassWeightExporter_RenderThread::RenderLandscapeComponentToTexture_RenderThread(FRHICommandListImmediate& RHICmdList)
{
	FRDGBuilder GraphBuilder(RHICmdList);

	FSceneViewFamilyContext ViewFamily(FSceneViewFamily::ConstructionValues(nullptr, SceneInterface, FEngineShowFlags(ESFIM_Game))
		.SetTime(FGameTime::GetTimeSinceAppStart()));

	ViewFamily.LandscapeLODOverride = 0; // Force LOD render

	// Ensure scene primitive rendering is valid (added primitives comitted, GPU-Scene updated, push/pop dynamic culling context).
	FScenePrimitiveRenderingContextScopeHelper ScenePrimitiveRenderingContextScopeHelper(GetRendererModule().BeginScenePrimitiveRendering(GraphBuilder, &ViewFamily));

	FSceneViewInitOptions ViewInitOptions;
	ViewInitOptions.SetViewRectangle(FIntRect(0, 0, TargetSize.X, TargetSize.Y));
	ViewInitOptions.ViewOrigin = ViewOrigin;
	ViewInitOptions.ViewRotationMatrix = ViewRotationMatrix;
	ViewInitOptions.ProjectionMatrix = ProjectionMatrix;
	ViewInitOptions.ViewFamily = &ViewFamily;

	GetRendererModule().CreateAndInitSingleView(RHICmdList, &ViewFamily, &ViewInitOptions);

	const FSceneView* View = ViewFamily.Views[0];

	FRDGTextureRef OutputTexture = GraphBuilder.RegisterExternalTexture(CreateRenderTarget(RenderTargetResource->GetTextureRHI(), TEXT("LandscapeGrass")));

	auto* PassParameters = GraphBuilder.AllocParameters<FLandscapeGrassPassParameters>();
	PassParameters->View = View->ViewUniformBuffer;
	PassParameters->Scene = GetSceneUniformBufferRef(GraphBuilder, *View);
	PassParameters->RenderTargets[0] = FRenderTargetBinding(OutputTexture, ERenderTargetLoadAction::EClear);

	AddSimpleMeshPass(GraphBuilder, PassParameters, SceneInterface->GetRenderScene(), *View, nullptr, RDG_EVENT_NAME("LandscapeGrass"), View->UnscaledViewRect,
		[&View, this](FDynamicPassMeshDrawListContext* DynamicMeshPassContext)
		{
			FLandscapeGrassWeightMeshProcessor PassMeshProcessor(
				nullptr,
				View->GetFeatureLevel(),
				View,
				DynamicMeshPassContext);

			const uint64 DefaultBatchElementMask = 1 << 0; // LOD 0 only

			for (auto& ComponentInfo : ComponentInfos)
			{
				if (ensure(ComponentInfo.SceneProxy))
				{
					const FMeshBatch& Mesh = ComponentInfo.SceneProxy->GetGrassMeshBatch();
					Mesh.MaterialRenderProxy->UpdateUniformExpressionCacheIfNeeded(View->GetFeatureLevel());

					PassMeshProcessor.AddMeshBatch(Mesh, DefaultBatchElementMask, ComponentInfo.NumPasses, ComponentInfo.ViewOffset, PassOffsetX, ComponentInfo.FirstHeightMipsPassIndex, HeightMips, ComponentInfo.SceneProxy);
				}
			}
		});

	if (bUseAsyncReadback)
	{
		AsyncReadback.StartReadback_RenderThread(GraphBuilder, OutputTexture);
	}

	GraphBuilder.Execute();
}


FLandscapeGrassWeightExporter::FLandscapeGrassWeightExporter(ALandscapeProxy* InLandscapeProxy, TArrayView<ULandscapeComponent* const> InLandscapeComponents, bool bInNeedsGrassmap, bool bInNeedsHeightmap, const TArray<int32>& InHeightMips, bool bInUseAsyncReadback)
	: FLandscapeGrassWeightExporter_RenderThread(InHeightMips, bInUseAsyncReadback)
	, LandscapeProxy(InLandscapeProxy)
	, ComponentSizeVerts(InLandscapeProxy->ComponentSizeQuads + 1)
	, SubsectionSizeQuads(InLandscapeProxy->SubsectionSizeQuads)
	, NumSubsections(InLandscapeProxy->NumSubsections)
	, RenderTargetTexture(nullptr)
{
	check(InLandscapeComponents.Num() > 0);
	SceneInterface = InLandscapeComponents[0]->GetScene();

	// todo: use a 2d target?
	const int32 SingleTileWidth = ComponentSizeVerts;
	TargetSize = FIntPoint(0, ComponentSizeVerts);

	// First compute the total render target size and prepare ComponentInfos (each component has its own number of needed passes because some might have different materials, thus different grass types) :
	ComponentInfos.Reserve(InLandscapeComponents.Num());
	int32 CurrentPixelOffsetX = 0;
	for (ULandscapeComponent* Component : InLandscapeComponents)
	{
		ensure(Component->SceneProxy);

		FComponentInfo& ComponentInfo = ComponentInfos.Add_GetRef(FComponentInfo(Component, bInNeedsGrassmap, bInNeedsHeightmap, InHeightMips));
		ComponentInfo.PixelOffsetX = CurrentPixelOffsetX;

		CurrentPixelOffsetX += ComponentInfo.NumPasses * SingleTileWidth;
	}
	TargetSize.X = CurrentPixelOffsetX;

	FIntPoint TargetSizeMinusOne(TargetSize - FIntPoint(1, 1));
	PassOffsetX = 2.0f * (float)SingleTileWidth / (float)TargetSize.X;

	// Then compute FComponentInfo's ViewOffset with the knowledge of the total render target size : 
	for (FComponentInfo& ComponentInfo : ComponentInfos)
	{
		FIntPoint ComponentOffset = (ComponentInfo.Component->GetSectionBase() - LandscapeProxy->LandscapeSectionOffset);
		FVector2D ViewOffset(-ComponentOffset.X, ComponentOffset.Y);
		ViewOffset.X += ComponentInfo.PixelOffsetX;
		ViewOffset /= (FVector2D(TargetSize) * 0.5f);
		ComponentInfo.ViewOffset = ViewOffset;
	}

	// center of target area in world
	FVector TargetCenter = LandscapeProxy->GetTransform().TransformPosition(FVector(TargetSizeMinusOne, 0.f) * 0.5f);

	// extent of target in world space
	FVector TargetExtent = FVector(TargetSize, 0.0f) * LandscapeProxy->GetActorScale() * 0.5f;

	ViewOrigin = TargetCenter;
	ViewRotationMatrix = FInverseRotationMatrix(LandscapeProxy->GetActorRotation());
	ViewRotationMatrix *= FMatrix(FPlane(1.0f, 0.0f, 0.0f, 0.0f),
		FPlane(0.0f, -1.0f, 0.0f, 0.0f),
		FPlane(0.0f, 0.0f, -1.0f, 0.0f),
		FPlane(0.0f, 0.0f, 0.0f, 1.0f));

	const float ZOffset = UE_OLD_WORLD_MAX;
	ProjectionMatrix = FReversedZOrthoMatrix(
		TargetExtent.X,
		TargetExtent.Y,
		0.5f / ZOffset,
		ZOffset);

	RenderTargetTexture = NewObject<UTextureRenderTarget2D>();
	check(RenderTargetTexture);
	RenderTargetTexture->ClearColor = FLinearColor::White;
	RenderTargetTexture->TargetGamma = 1.0f;
	const bool bForceLinearGamma = true;
	RenderTargetTexture->InitCustomFormat(TargetSize.X, TargetSize.Y, PF_B8G8R8A8, bForceLinearGamma);
	RenderTargetResource = RenderTargetTexture->GameThread_GetRenderTargetResource()->GetTextureRenderTarget2DResource();

	UE::RenderCommandPipe::FSyncScope SyncScope;

	RenderCaptureInterface::FScopedCapture RenderCapture((RenderCaptureNextGrassmapDraws != 0), TEXT("LandscapeGrassmapCapture"));
	RenderCaptureNextGrassmapDraws = FMath::Max(0, RenderCaptureNextGrassmapDraws - 1);

	// render
	FLandscapeGrassWeightExporter_RenderThread* Exporter = this;
	ENQUEUE_RENDER_COMMAND(FDrawSceneCommand)(
		[Exporter](FRHICommandListImmediate& RHICmdList)
		{
			Exporter->RenderLandscapeComponentToTexture_RenderThread(RHICmdList);
			FlushPendingDeleteRHIResources_RenderThread();
		});
}

TMap<ULandscapeComponent*, TUniquePtr<FLandscapeComponentGrassData>, TInlineSetAllocator<1>> FLandscapeGrassWeightExporter::FetchResults()
{
	TMap<ULandscapeComponent*, TUniquePtr<FLandscapeComponentGrassData>, TInlineSetAllocator<1>> Results;
	TArray<FColor> Samples;

	if (bUseAsyncReadback)
	{
		FIntPoint Size;
		Samples = AsyncReadback.TakeResults(&Size);
		check(Size == TargetSize);
	}
	else
	{
		Samples.SetNumUninitialized(TargetSize.X * TargetSize.Y);

		// Copy the contents of the remote texture to system memory (SamplesDataBuffer)
		// This is a synchronous operation that may stall the cpu, waiting for the GPU data to be generated and copied
		FReadSurfaceDataFlags ReadSurfaceDataFlags;
		ReadSurfaceDataFlags.SetLinearToGamma(false);
		RenderTargetResource->ReadPixels(Samples, ReadSurfaceDataFlags, FIntRect(0, 0, TargetSize.X, TargetSize.Y));
	}

	Results.Reserve(ComponentInfos.Num());

	// Local data will be moved in contiguous array at the end of export (to minimize slack waste)
	TArray<uint16> HeightData;
	TMap<ULandscapeGrassType*, TArray<uint8>> WeightData;

	for (auto& ComponentInfo : ComponentInfos)
	{
		ULandscapeComponent* Component = ComponentInfo.Component;

		TUniquePtr<FLandscapeComponentGrassData> NewGrassData = MakeUnique<FLandscapeComponentGrassData>(Component);

		if (ComponentInfo.FirstHeightMipsPassIndex != MAX_int32)
		{
			HeightData.Empty(FMath::Square(ComponentSizeVerts));
		}
		else
		{
			HeightData.Empty(0);
		}
#if WITH_EDITORONLY_DATA
		NewGrassData->HeightMipData.Empty(HeightMips.Num());
#endif // WITH_EDITORONLY_DATA

		WeightData.Empty();
		TArray<TArray<uint8>*> GrassWeightArrays;
		GrassWeightArrays.Empty(ComponentInfo.RequestedGrassTypes.Num());
		for (auto GrassType : ComponentInfo.RequestedGrassTypes)
		{
			WeightData.Add(GrassType);
		}

		// need a second loop because the WeightData map will reallocate its arrays as grass types are added
		for (auto GrassType : ComponentInfo.RequestedGrassTypes)
		{
			TArray<uint8>* DataArray = WeightData.Find(GrassType);
			check(DataArray);
			DataArray->Empty(FMath::Square(ComponentSizeVerts));
			GrassWeightArrays.Add(DataArray);
		}

		// output debug bitmap
#if UE_BUILD_DEBUG
		static bool bOutputGrassBitmap = false;
		if (bOutputGrassBitmap)
		{
			FString TempPath = FPaths::ScreenShotDir();
			TempPath += TEXT("/GrassDebug");
			IFileManager::Get().MakeDirectory(*TempPath, true);
			FFileHelper::CreateBitmap(*(TempPath / "Grass"), TargetSize.X, TargetSize.Y, Samples.GetData(), nullptr, &IFileManager::Get(), nullptr, ComponentInfo.RequestedGrassTypes.Num() >= 2);
		}
#endif

		// FColor memory layout matches our BGRA GPU layout only on little endian CPUs!
		#if PLATFORM_LITTLE_ENDIAN
			#define BGRA_AS_FCOLOR_BLUE B
			#define BGRA_AS_FCOLOR_GREEN G
			#define BGRA_AS_FCOLOR_RED R
			#define BGRA_AS_FCOLOR_ALPHA A
		#else
			#define BGRA_AS_FCOLOR_BLUE A
			#define BGRA_AS_FCOLOR_GREEN R
			#define BGRA_AS_FCOLOR_RED G
			#define BGRA_AS_FCOLOR_ALPHA B
		#endif // PLATFORM_LITTLE_ENDIAN

		for (int32 PassIdx = 0; PassIdx < ComponentInfo.NumPasses; PassIdx++)
		{
			FColor* SampleData = &Samples[ComponentInfo.PixelOffsetX + PassIdx * ComponentSizeVerts];
			if (PassIdx < ComponentInfo.FirstHeightMipsPassIndex)
			{
				if (PassIdx == 0)
				{
					for (int32 y = 0; y < ComponentSizeVerts; y++)
					{
						for (int32 x = 0; x < ComponentSizeVerts; x++)
						{
							FColor& Sample = SampleData[x + y * TargetSize.X];
							uint16 Height = (((uint16)Sample.BGRA_AS_FCOLOR_RED) << 8) + (uint16)(Sample.BGRA_AS_FCOLOR_GREEN);
							HeightData.Add(Height);
							if (ComponentInfo.RequestedGrassTypes.Num() > 0)
							{
								GrassWeightArrays[0]->Add(Sample.BGRA_AS_FCOLOR_BLUE);
								if (ComponentInfo.RequestedGrassTypes.Num() > 1)
								{
									GrassWeightArrays[1]->Add(Sample.BGRA_AS_FCOLOR_ALPHA);
								}
							}
						}
					}
				}
				else
				{
					for (int32 y = 0; y < ComponentSizeVerts; y++)
					{
						for (int32 x = 0; x < ComponentSizeVerts; x++)
						{
							FColor& Sample = SampleData[x + y * TargetSize.X];

							int32 TypeIdx = PassIdx * 4 - 2;
							GrassWeightArrays[TypeIdx++]->Add(Sample.BGRA_AS_FCOLOR_RED);
							if (TypeIdx < ComponentInfo.RequestedGrassTypes.Num())
							{
								GrassWeightArrays[TypeIdx++]->Add(Sample.BGRA_AS_FCOLOR_GREEN);
								if (TypeIdx < ComponentInfo.RequestedGrassTypes.Num())
								{
									GrassWeightArrays[TypeIdx++]->Add(Sample.BGRA_AS_FCOLOR_BLUE);
									if (TypeIdx < ComponentInfo.RequestedGrassTypes.Num())
									{
										GrassWeightArrays[TypeIdx++]->Add(Sample.BGRA_AS_FCOLOR_ALPHA);
									}
								}
							}
						}
					}
				}
			}
			else // PassIdx >= FirstHeightMipsPassIndex
			{
#if WITH_EDITORONLY_DATA
				const int32 Mip = HeightMips[PassIdx - ComponentInfo.FirstHeightMipsPassIndex];
				int32 MipSizeVerts = NumSubsections * (SubsectionSizeQuads >> Mip);
				TArray<uint16>& MipHeightData = NewGrassData->HeightMipData.Add(Mip);
				for (int32 y = 0; y < MipSizeVerts; y++)
				{
					for (int32 x = 0; x < MipSizeVerts; x++)
					{
						FColor& Sample = SampleData[x + y * TargetSize.X];
						uint16 Height = (((uint16)Sample.BGRA_AS_FCOLOR_RED) << 8) + (uint16)(Sample.BGRA_AS_FCOLOR_GREEN);
						MipHeightData.Add(Height);
					}
				}
#endif // WITH_EDITORONLY_DATA
			}
		}

		#undef BGRA_AS_FCOLOR_BLUE
		#undef BGRA_AS_FCOLOR_GREEN
		#undef BGRA_AS_FCOLOR_RED
		#undef BGRA_AS_FCOLOR_ALPHA

		// remove null grass type if we had one (can occur if the node has null entries)
		WeightData.Remove(nullptr);

		// Remove any grass data that is entirely weight 0
		for (auto Iter(WeightData.CreateIterator()); Iter; ++Iter)
		{
			if (Iter->Value.IndexOfByPredicate([&](const int8& Weight) { return Weight != 0; }) == INDEX_NONE)
			{
				Iter.RemoveCurrent();
			}
		}

		NewGrassData->InitializeFrom(HeightData, WeightData);
		Results.Add(Component, MoveTemp(NewGrassData));
	}

	return Results;
}

void FLandscapeGrassWeightExporter::ApplyResults()
{
	TMap<ULandscapeComponent*, TUniquePtr<FLandscapeComponentGrassData>, TInlineSetAllocator<1>> NewGrassData = FetchResults();

	for (auto&& GrassDataPair : NewGrassData)
	{
		ULandscapeComponent* Component = GrassDataPair.Key;
		FLandscapeComponentGrassData* ComponentGrassData = GrassDataPair.Value.Release();
		ALandscapeProxy* Proxy = Component->GetLandscapeProxy();

		// Assign the new data (thread-safe)
		Component->GrassData = MakeShareable(ComponentGrassData);

#if WITH_EDITORONLY_DATA
		Component->GrassData->bIsDirty = true;
#endif // WITH_EDITORONLY_DATA

#if WITH_EDITOR
		if (Proxy->bBakeMaterialPositionOffsetIntoCollision)
		{
			Component->DestroyCollisionData();
			Component->UpdateCollisionData();
		}
#endif // WITH_EDITOR
	}
}

void FLandscapeGrassWeightExporter::AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector)
{
	if (RenderTargetTexture)
	{
		Collector.AddReferencedObject(RenderTargetTexture);
	}

	if (LandscapeProxy)
	{
		Collector.AddReferencedObject(LandscapeProxy);
	}

	for (auto& Info : ComponentInfos)
	{
		if (Info.Component)
		{
			Collector.AddReferencedObject(Info.Component);
		}

		Collector.AddReferencedObjects(Info.RequestedGrassTypes);
	}
}

static bool ShouldCacheLandscapeGrassShaders(const FMeshMaterialShaderPermutationParameters& Parameters)
{
	// We only need grass weight shaders for Landscape vertex factories on desktop platforms
	return (Parameters.MaterialParameters.bIsUsedWithLandscape || Parameters.MaterialParameters.bIsSpecialEngineMaterial) &&
		IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5) &&
		Parameters.VertexFactoryType == FindVertexFactoryType(FName(TEXT("FLandscapeFixedGridVertexFactory"), FNAME_Find)) &&
		EnumHasAllFlags(Parameters.Flags, EShaderPermutationFlags::HasEditorOnlyData);
}

class FLandscapeGrassWeightVS : public FMeshMaterialShader
{
	DECLARE_SHADER_TYPE(FLandscapeGrassWeightVS, MeshMaterial);

	LAYOUT_FIELD(FShaderParameter, RenderOffsetParameter);

protected:

	FLandscapeGrassWeightVS()
	{}

	FLandscapeGrassWeightVS(const FMeshMaterialShaderType::CompiledShaderInitializerType& Initializer)
		: FMeshMaterialShader(Initializer)
	{
		RenderOffsetParameter.Bind(Initializer.ParameterMap, TEXT("RenderOffset"));
		PassUniformBuffer.Bind(Initializer.ParameterMap, FSceneTextureUniformParameters::FTypeInfo::GetStructMetadata()->GetShaderVariableName());
	}

public:

	static bool ShouldCompilePermutation(const FMeshMaterialShaderPermutationParameters& Parameters)
	{
		return ShouldCacheLandscapeGrassShaders(Parameters);
	}

	void GetShaderBindings(
		const FScene* Scene,
		ERHIFeatureLevel::Type FeatureLevel,
		const FPrimitiveSceneProxy* PrimitiveSceneProxy,
		const FMaterialRenderProxy& MaterialRenderProxy,
		const FMaterial& Material,
		const FMeshPassProcessorRenderState& DrawRenderState,
		const FLandscapeGrassWeightShaderElementData& ShaderElementData,
		FMeshDrawSingleShaderBindings& ShaderBindings) const
	{
		FMeshMaterialShader::GetShaderBindings(Scene, FeatureLevel, PrimitiveSceneProxy, MaterialRenderProxy, Material, DrawRenderState, ShaderElementData, ShaderBindings);

		ShaderBindings.Add(RenderOffsetParameter, ShaderElementData.RenderOffset);
	}
};

IMPLEMENT_MATERIAL_SHADER_TYPE(, FLandscapeGrassWeightVS, TEXT("/Engine/Private/LandscapeGrassWeight.usf"), TEXT("VSMain"), SF_Vertex);

class FLandscapeGrassWeightPS : public FMeshMaterialShader
{
	DECLARE_SHADER_TYPE(FLandscapeGrassWeightPS, MeshMaterial);
	LAYOUT_FIELD(FShaderParameter, OutputPassParameter);
public:

	static bool ShouldCompilePermutation(const FMeshMaterialShaderPermutationParameters& Parameters)
	{
		return ShouldCacheLandscapeGrassShaders(Parameters);
	}

	FLandscapeGrassWeightPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FMeshMaterialShader(Initializer)
	{
		OutputPassParameter.Bind(Initializer.ParameterMap, TEXT("OutputPass"));
		PassUniformBuffer.Bind(Initializer.ParameterMap, FSceneTextureUniformParameters::FTypeInfo::GetStructMetadata()->GetShaderVariableName());
	}

	FLandscapeGrassWeightPS()
	{}

	void GetShaderBindings(
		const FScene* Scene,
		ERHIFeatureLevel::Type FeatureLevel,
		const FPrimitiveSceneProxy* PrimitiveSceneProxy,
		const FMaterialRenderProxy& MaterialRenderProxy,
		const FMaterial& Material,
		const FMeshPassProcessorRenderState& DrawRenderState,
		const FLandscapeGrassWeightShaderElementData& ShaderElementData,
		FMeshDrawSingleShaderBindings& ShaderBindings) const
	{
		FMeshMaterialShader::GetShaderBindings(Scene, FeatureLevel, PrimitiveSceneProxy, MaterialRenderProxy, Material, DrawRenderState, ShaderElementData, ShaderBindings);

		ShaderBindings.Add(OutputPassParameter, ShaderElementData.OutputPass);
	}
};

IMPLEMENT_MATERIAL_SHADER_TYPE(, FLandscapeGrassWeightPS, TEXT("/Engine/Private/LandscapeGrassWeight.usf"), TEXT("PSMain"), SF_Pixel);

void UE::Landscape::Grass::AddGrassWeightShaderTypes(FMaterialShaderTypes& InOutShaderTypes)
{
	InOutShaderTypes.AddShaderType<FLandscapeGrassWeightVS>();
	InOutShaderTypes.AddShaderType<FLandscapeGrassWeightPS>();
}

#endif // WITH_EDITOR

