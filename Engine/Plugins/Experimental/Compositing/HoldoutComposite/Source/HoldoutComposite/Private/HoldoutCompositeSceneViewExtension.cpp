// Copyright Epic Games, Inc. All Rights Reserved.

#include "HoldoutCompositeSceneViewExtension.h"

#include "HoldoutCompositeSettings.h"
#include "HoldoutCompositeModule.h"

#include "Components/PrimitiveComponent.h"
#include "Containers/Map.h"
#include "Containers/Set.h"
#include "EngineUtils.h"
#include "FXRenderingUtils.h"
#include "HDRHelper.h"
#include "PixelShaderUtils.h"
#include "PostProcess/LensDistortion.h"
#include "PostProcess/PostProcessMaterialInputs.h"
#include "Rendering/CustomRenderPass.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"
#include "SceneView.h"
#include "ScreenPass.h"
#include "SystemTextures.h"

// Private renderer includes currently needed...
#include "SceneRendering.h"

DECLARE_GPU_STAT_NAMED(FHoldoutCompositeDilate, TEXT("HoldoutComposite.Dilate"));
DECLARE_GPU_STAT_NAMED(FHoldoutCompositeFinal, TEXT("HoldoutComposite.Final"));

namespace HoldoutComposite
{
	enum class ESceneColorSourceEncoding : uint32
	{
		Linear = 0,
		Gamma = 1,
		sRGB = 2,
	};
}

class FDilateHoldoutCompositeCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FDilateHoldoutCompositeCS);
	SHADER_USE_PARAMETER_STRUCT(FDilateHoldoutCompositeCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, InputTexture)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float4>, RWOutputTexture)
		SHADER_PARAMETER(FIntPoint, Dimensions)
	END_SHADER_PARAMETER_STRUCT()

	static inline void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		OutEnvironment.SetDefine(TEXT("TILE_SIZE"), ThreadGroupSize);

		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
	}

	static const uint32 ThreadGroupSize = 16;
};
IMPLEMENT_GLOBAL_SHADER(FDilateHoldoutCompositeCS, "/Plugin/HoldoutComposite/Private/HoldoutCompositeDilate.usf", "MainCS", SF_Compute);

class FCompositeHoldoutCompositePS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FCompositeHoldoutCompositePS);
	SHADER_USE_PARAMETER_STRUCT(FCompositeHoldoutCompositePS, FGlobalShader);

	class FApplyGlobalExposure : SHADER_PERMUTATION_BOOL("PERMUTATION_GLOBAL_EXPOSURE");
	using FPermutationDomain = TShaderPermutationDomain<FApplyGlobalExposure>;

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
		SHADER_PARAMETER_STRUCT(FScreenPassTextureViewportParameters, Input)
		SHADER_PARAMETER_STRUCT(FScreenPassTextureViewportParameters, Custom)
		SHADER_PARAMETER_STRUCT(FScreenPassTextureViewportParameters, Output)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, InputTexture)
		SHADER_PARAMETER_SAMPLER(SamplerState, InputSampler)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, CustomTexture)
		SHADER_PARAMETER_SAMPLER(SamplerState, CustomSampler)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, UndistortingDisplacementTexture)
		SHADER_PARAMETER_SAMPLER(SamplerState, UndistortingDisplacementSampler)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<float4>, EyeAdaptationBuffer)
		SHADER_PARAMETER(FUint32Vector2, Encodings)
		SHADER_PARAMETER(FVector2f, DisplayGamma)

		RENDER_TARGET_BINDING_SLOTS()
	END_SHADER_PARAMETER_STRUCT()
};
IMPLEMENT_GLOBAL_SHADER(FCompositeHoldoutCompositePS, "/Plugin/HoldoutComposite/Private/HoldoutCompositeFinal.usf", "MainPS", SF_Pixel);

class FHoldoutCompositeCustomRenderPass : public FCustomRenderPassBase
{
public:
	IMPLEMENT_CUSTOM_RENDER_PASS(FHoldoutCompositeCustomRenderPass);

	FHoldoutCompositeCustomRenderPass(const FIntPoint& InRenderTargetSize, FHoldoutCompositeSceneViewExtension* InParentExtension, FSceneView& InView)
		: FCustomRenderPassBase(TEXT("HoldoutCompositeCustomRenderPass"), ERenderMode::DepthAndBasePass, ERenderOutput::SceneColorAndAlpha, InRenderTargetSize)
		, ParentExtension(InParentExtension)
		, ViewId(InView.GetViewKey())
		, ViewFeatureLevel(InView.GetFeatureLevel())
	{
	}

	virtual void OnPreRender(FRDGBuilder& GraphBuilder) override
	{
		/**
		 * Note: We abuse the reflection capture view property in a custom render pass to disable
		 * primitive alpha holdout during its base pass render. Because the holdout is part of the
		 * primitive uniform buffer, it cannot easily have true/false states in the same frame.
		 * We would otherwise need to duplicate the entire primitive so that the main render has
		 * holdout enabled, while the custom render pass primitive has it disabled.
		 * 
		 * Maintaining these duplicates becomes problematic, especially with different types of meshes.
		 * As a result, we consider this reflection capture property override preferable.
		*/
		for (FViewInfo* ViewInfo : Views)
		{
			// Holdout is ignored during reflections captures to preserve indirect light (see PRIMITIVE_SCENE_DATA_FLAG_HOLDOUT in BasePassPixelShader.usf)
			ViewInfo->CachedViewUniformShaderParameters->RenderingReflectionCaptureMask = 1.0f;
			ViewInfo->ViewUniformBuffer.UpdateUniformBufferImmediate(GraphBuilder.RHICmdList, *ViewInfo->CachedViewUniformShaderParameters);
		}

		const FRDGTextureDesc TextureDesc = FRDGTextureDesc::Create2D(RenderTargetSize, PF_FloatRGBA, FClearValueBinding::Black, TexCreate_RenderTargetable | TexCreate_ShaderResource);
		RenderTargetTexture = GraphBuilder.CreateTexture(TextureDesc, TEXT("HoldoutCompositeCustomTexture"));
		AddClearRenderTargetPass(GraphBuilder, RenderTargetTexture, FLinearColor::Black, FIntRect(FInt32Point(), RenderTargetSize));
	}

	virtual void OnPostRender(FRDGBuilder& GraphBuilder) override
	{
		FRDGTextureRef DilatedCRP = CreateDilatedTexture(GraphBuilder);

#if HOLDOUT_COMPOSITE_WORKAROUND_UE_209928
		ParentExtension->CollectCustomRenderTarget(ViewId, GraphBuilder.ConvertToExternalTexture(DilatedCRP));
#else
		ParentExtension->CollectCustomRenderTarget(ViewId, DilatedCRP);
#endif
	}

private:
	
	FRDGTextureRef CreateDilatedTexture(FRDGBuilder& GraphBuilder)
	{
		RDG_EVENT_SCOPE(GraphBuilder, "HoldoutComposite.Dilate");
		RDG_GPU_STAT_SCOPE(GraphBuilder, FHoldoutCompositeDilate);

		FGlobalShaderMap* GlobalShaderMap = GetGlobalShaderMap(ViewFeatureLevel);

		const FIntPoint TextureSize = FIntPoint(RenderTargetTexture->Desc.GetSize().X, RenderTargetTexture->Desc.GetSize().Y);
		const FRDGTextureDesc TextureDesc = FRDGTextureDesc::Create2D(TextureSize, PF_FloatRGBA, FClearValueBinding::Black, TexCreate_UAV | TexCreate_ShaderResource);
		FRDGTextureRef DilatedTexture = GraphBuilder.CreateTexture(TextureDesc, TEXT("HoldoutCompositeDilatedTexture"));

		// Async compute dilation pass
		{
			FDilateHoldoutCompositeCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FDilateHoldoutCompositeCS::FParameters>();
			PassParameters->InputTexture = RenderTargetTexture;
			PassParameters->RWOutputTexture = GraphBuilder.CreateUAV(DilatedTexture);
			PassParameters->Dimensions = TextureSize;

			TShaderMapRef<FDilateHoldoutCompositeCS> ComputeShader(GlobalShaderMap);
			FComputeShaderUtils::AddPass(
				GraphBuilder,
				RDG_EVENT_NAME("HoldoutComposite.Dilate (%dx%d)", TextureSize.X, TextureSize.Y),
				GSupportsEfficientAsyncCompute ? ERDGPassFlags::AsyncCompute : ERDGPassFlags::Compute,
				ComputeShader,
				PassParameters,
				FComputeShaderUtils::GetGroupCount(TextureSize, FDilateHoldoutCompositeCS::ThreadGroupSize)
			);
		}

		return DilatedTexture;
	}

	FHoldoutCompositeSceneViewExtension* ParentExtension;
	uint32 ViewId;
	ERHIFeatureLevel::Type ViewFeatureLevel;
};

FHoldoutCompositeSceneViewExtension::FHoldoutCompositeSceneViewExtension(const FAutoRegister& AutoReg, UWorld* InWorld)
	: FWorldSceneViewExtension(AutoReg, InWorld)
{
}

FHoldoutCompositeSceneViewExtension::~FHoldoutCompositeSceneViewExtension()
{
}

void FHoldoutCompositeSceneViewExtension::RegisterPrimitives(TArrayView<TSoftObjectPtr<UPrimitiveComponent>> InPrimitiveComponents, bool bInHoldoutState)
{
	check(IsInGameThread());

	for (const TSoftObjectPtr<UPrimitiveComponent>& InPrimitiveComponent : InPrimitiveComponents)
	{
		if (!InPrimitiveComponent.IsValid())
		{
			continue;
		}

		if (!CompositePrimitives.Contains(InPrimitiveComponent))
		{
			CompositePrimitives.Add(InPrimitiveComponent);
			InPrimitiveComponent->SetHoldout(bInHoldoutState);
		}
	}
}

void FHoldoutCompositeSceneViewExtension::UnregisterPrimitives(TArrayView<TSoftObjectPtr<UPrimitiveComponent>> InPrimitiveComponents, bool bInHoldoutState)
{
	check(IsInGameThread());

	for (const TSoftObjectPtr<UPrimitiveComponent>& InPrimitiveComponent : InPrimitiveComponents)
	{
		if (!InPrimitiveComponent.IsValid())
		{
			continue;
		}

		if (CompositePrimitives.Contains(InPrimitiveComponent))
		{
			CompositePrimitives.Remove(InPrimitiveComponent);
			InPrimitiveComponent->SetHoldout(bInHoldoutState);
		}
	}

}

bool FHoldoutCompositeSceneViewExtension::IsActiveThisFrame_Internal(const FSceneViewExtensionContext& Context) const
{
	bool bIsActive = FWorldSceneViewExtension::IsActiveThisFrame_Internal(Context);
	bIsActive &= !CompositePrimitives.IsEmpty();
	bIsActive &= !IsHDREnabled();

	return bIsActive;
}


//~ Begin ISceneViewExtension Interface

int32 FHoldoutCompositeSceneViewExtension::GetPriority() const
{
	const UHoldoutCompositeSettings* Settings = GetDefault<UHoldoutCompositeSettings>();
	if (Settings != nullptr)
	{
		return Settings->SceneViewExtensionPriority;
	}

	return 0;
}

void FHoldoutCompositeSceneViewExtension::SetupViewFamily(FSceneViewFamily& InViewFamily)
{
	const UHoldoutCompositeSettings* Settings = GetDefault<UHoldoutCompositeSettings>();
	if (Settings != nullptr)
	{
		bCompositeFollowsSceneExposure = Settings->bCompositeFollowsSceneExposure;
	}
}

void FHoldoutCompositeSceneViewExtension::SetupView(FSceneViewFamily& InViewFamily, FSceneView& InView)
{
	const TWeakObjectPtr<UWorld> WorldPtr = GetWorld();
	check(WorldPtr.IsValid());

	TSet<FPrimitiveComponentId> HoldoutCompositePrimitiveIds;
	for (const TSoftObjectPtr<UPrimitiveComponent>& PrimitivePtr : CompositePrimitives)
	{
		if (PrimitivePtr.IsValid())
		{
			const FPrimitiveComponentId PrimId = PrimitivePtr->GetPrimitiveSceneId();

			if (InView.ShowOnlyPrimitives.IsSet())
			{
				if (InView.ShowOnlyPrimitives.GetValue().Contains(PrimId))
				{
					HoldoutCompositePrimitiveIds.Add(PrimitivePtr->GetPrimitiveSceneId());
				}
			}
			else if (!InView.HiddenPrimitives.Contains(PrimId))
			{
				HoldoutCompositePrimitiveIds.Add(PrimitivePtr->GetPrimitiveSceneId());
			}
		}
	}

	if (HoldoutCompositePrimitiveIds.IsEmpty())
	{
		return;
	}

	// Extract the custom render target size
	FIntPoint RenderTargetViewSize;
	if (InView.PrimaryScreenPercentageMethod == EPrimaryScreenPercentageMethod::TemporalUpscale)
	{
		// Note: This is equivalent to FViewInfo::GetSecondaryViewRectSize
		FIntRect OutputRect = FIntRect(0, 0,
			FMath::CeilToInt(InView.UnscaledViewRect.Width() * InViewFamily.SecondaryViewFraction),
			FMath::CeilToInt(InView.UnscaledViewRect.Height() * InViewFamily.SecondaryViewFraction)
		);
		QuantizeSceneBufferSize(OutputRect.Max, RenderTargetViewSize);
	}
	else
	{
		RenderTargetViewSize = InView.UnscaledViewRect.Size();
	}

	// Create a new custom render pass to render the composite primitive(s)
	FHoldoutCompositeCustomRenderPass* CustomRenderPass = new FHoldoutCompositeCustomRenderPass(
		RenderTargetViewSize,
		this,
		InView
	);

	FSceneInterface::FCustomRenderPassRendererInput PassInput;
	PassInput.ViewLocation = InView.ViewLocation;
	PassInput.ViewRotationMatrix = InView.ViewMatrices.GetViewMatrix().RemoveTranslation();
	PassInput.ViewRotationMatrix.RemoveScaling();

	// Note: Projection matrix here is without jitter, GetProjectionNoAAMatrix() is invalid (not yet available).
	PassInput.ProjectionMatrix = InView.ViewMatrices.GetProjectionMatrix();
	PassInput.ViewActor = InView.ViewActor;
	PassInput.ShowOnlyPrimitives = HoldoutCompositePrimitiveIds;
	PassInput.CustomRenderPass = CustomRenderPass;
	PassInput.bIsSceneCapture = true;

	// TODO: Once CRPs are associated with view(family), disable & remove HOLDOUT_COMPOSITE_WORKAROUND_UE_209928.
	WorldPtr.Get()->Scene->AddCustomRenderPass(&InViewFamily, PassInput);
}

void FHoldoutCompositeSceneViewExtension::SubscribeToPostProcessingPass(EPostProcessingPass PassId, const FSceneView& InView, FAfterPassCallbackDelegateArray& InOutPassCallbacks, bool bIsPassEnabled)
{
	if (!CustomRenderTargetPerView_RenderThread.Contains(InView.GetViewKey()))
	{
		// Early-out to avoid needless work in the post processing callback(s).
		return;
	}

	if (PassId == EPostProcessingPass::Tonemap)
	{
		InOutPassCallbacks.Add(FAfterPassCallbackDelegate::CreateRaw(this, &FHoldoutCompositeSceneViewExtension::PostProcessPassAfterTonemap_RenderThread));
	}
}

FScreenPassTexture FHoldoutCompositeSceneViewExtension::PostProcessPassAfterTonemap_RenderThread(FRDGBuilder& GraphBuilder, const FSceneView& InView, const FPostProcessMaterialInputs& Inputs)
{
	RDG_EVENT_SCOPE(GraphBuilder, "HoldoutComposite.Final");
	RDG_GPU_STAT_SCOPE(GraphBuilder, FHoldoutCompositeFinal);
	using namespace HoldoutComposite;

	FScreenPassTexture SceneColor = FScreenPassTexture::CopyFromSlice(GraphBuilder, Inputs.GetInput(EPostProcessMaterialInput::SceneColor));
	check(SceneColor.IsValid());

	FRDGTextureRef CustomRenderPassTexture = GSystemTextures.GetBlackAlphaOneDummy(GraphBuilder);
#if HOLDOUT_COMPOSITE_WORKAROUND_UE_209928
	const TRefCountPtr<IPooledRenderTarget>* CustomRenderPassRenderTargetPtr = CustomRenderTargetPerView_RenderThread.Find(InView.GetViewKey());
	if (CustomRenderPassRenderTargetPtr != nullptr)
	{
		CustomRenderPassTexture = GraphBuilder.RegisterExternalTexture(*CustomRenderPassRenderTargetPtr);
}
#else
	const FRDGTextureRef* CustomRenderPassTexturePtr = CustomRenderTargetPerView_RenderThread.Find(InView.GetViewKey());
	if (CustomRenderPassTexturePtr && HasBeenProduced(*CustomRenderPassTexturePtr))
	{
		CustomRenderPassTexture = *CustomRenderPassTexturePtr;
	}
#endif

	FGlobalShaderMap* GlobalShaderMap = GetGlobalShaderMap(InView.GetFeatureLevel());
	const FSceneViewFamily* Family = InView.Family;

	FUint32Vector2 Encodings = FUint32Vector2::ZeroValue; // ESceneColorSourceEncoding::Linear
	if ((Family->EngineShowFlags.Tonemapper == 0) || (Family->EngineShowFlags.PostProcessing == 0))
	{
		Encodings.X = static_cast<uint32>(ESceneColorSourceEncoding::Gamma);
		Encodings.Y = static_cast<uint32>(ESceneColorSourceEncoding::Gamma);
	}
	else if (Family->SceneCaptureSource == SCS_FinalColorLDR)
	{
		Encodings.X = static_cast<uint32>(ESceneColorSourceEncoding::sRGB);
		Encodings.Y = static_cast<uint32>(ESceneColorSourceEncoding::sRGB);
	}

	FScreenPassRenderTarget Output = Inputs.OverrideOutput;
	if (!Output.IsValid())
	{
		Output = FScreenPassRenderTarget::CreateFromInput(GraphBuilder, SceneColor, InView.GetOverwriteLoadAction(), TEXT("HoldoutCompositePassOutput"));
	}

	// Compositing pass
	{
		FCompositeHoldoutCompositePS::FPermutationDomain PermutationVector;
		PermutationVector.Set<FCompositeHoldoutCompositePS::FApplyGlobalExposure>(bCompositeFollowsSceneExposure.load());

		FRDGBufferRef EyeAdaptationBuffer = GraphBuilder.RegisterExternalBuffer(InView.GetEyeAdaptationBuffer(), ERDGBufferFlags::MultiFrame);
		const FLensDistortionLUT& LensDistortionLUT = LensDistortion::GetLUTUnsafe(InView);
		const bool bLensDistortionInTSR = (LensDistortion::GetPassLocationUnsafe(InView) == LensDistortion::EPassLocation::TSR);

		FCompositeHoldoutCompositePS::FParameters* PassParameters = GraphBuilder.AllocParameters<FCompositeHoldoutCompositePS::FParameters>();
		PassParameters->View = InView.ViewUniformBuffer;
		PassParameters->Input = GetScreenPassTextureViewportParameters(FScreenPassTextureViewport(SceneColor));
		PassParameters->Custom = GetScreenPassTextureViewportParameters(FScreenPassTextureViewport(CustomRenderPassTexture));
		PassParameters->Output = GetScreenPassTextureViewportParameters(FScreenPassTextureViewport(Output));
		PassParameters->InputTexture = SceneColor.Texture;
		PassParameters->InputSampler = TStaticSamplerState<SF_Point>::GetRHI();
		PassParameters->CustomTexture = CustomRenderPassTexture;
		PassParameters->CustomSampler = TStaticSamplerState<SF_Bilinear>::GetRHI();
		PassParameters->UndistortingDisplacementTexture = GSystemTextures.GetBlackDummy(GraphBuilder);
		PassParameters->UndistortingDisplacementSampler = TStaticSamplerState<SF_Bilinear>::GetRHI();
		if (LensDistortionLUT.IsEnabled() && bLensDistortionInTSR)
		{
			PassParameters->UndistortingDisplacementTexture = LensDistortionLUT.UndistortingDisplacementTexture;
		}
		PassParameters->EyeAdaptationBuffer = GraphBuilder.CreateSRV(EyeAdaptationBuffer);
		PassParameters->Encodings = Encodings;
		PassParameters->DisplayGamma = FVector2f(Family->RenderTarget->GetDisplayGamma(), 1.0f / Family->RenderTarget->GetDisplayGamma());
		PassParameters->RenderTargets[0] = Output.GetRenderTargetBinding();

		TShaderMapRef<FCompositeHoldoutCompositePS> PixelShader(GlobalShaderMap, PermutationVector);
		FPixelShaderUtils::AddFullscreenPass(
			GraphBuilder,
			GlobalShaderMap,
			RDG_EVENT_NAME("HoldoutComposite.Final (%dx%d) PS",
				Output.ViewRect.Width(), Output.ViewRect.Height()),
			PixelShader,
			PassParameters,
			Output.ViewRect
		);
	}

	return MoveTemp(Output);
}

void FHoldoutCompositeSceneViewExtension::PostRenderViewFamily_RenderThread(FRDGBuilder& GraphBuilder, FSceneViewFamily& InViewFamily)
{
	// Cleanup invalid primitives.
	for (auto Iter = CompositePrimitives.CreateIterator(); Iter; ++Iter)
	{
		if (!Iter->IsValid())
		{
			Iter.RemoveCurrent();
		}
	}
}

void FHoldoutCompositeSceneViewExtension::PostRenderView_RenderThread(FRDGBuilder& GraphBuilder, FSceneView& InView)
{
	CustomRenderTargetPerView_RenderThread.Remove(InView.GetViewKey());
}

