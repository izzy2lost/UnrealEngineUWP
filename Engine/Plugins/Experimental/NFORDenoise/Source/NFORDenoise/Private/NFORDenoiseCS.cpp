// Copyright Epic Games, Inc. All Rights Reserved.

#include "NFORDenoiseCS.h"
#include "SystemTextures.h"
#include "PixelShaderUtils.h"

#include "NFORWeightedLSRCommon.h"
#include "NFORRegressionCPUSolver.h"

namespace NFORDenoise
{
	
	TAutoConsoleVariable<bool> CVarNFORFeatureAddConstant(
		TEXT("r.NFOR.Feature.AddConstant"),
		1,
		TEXT("Add a constant 1 feature for denoising. Especially useful when all other features are zero."),
		ECVF_RenderThreadSafe);

	TAutoConsoleVariable<float> CVarNFORFeatureMaxAlbedoGreyscale(
		TEXT("r.NFOR.Feature.MaxAlbedoGreyscale"),
		2.0,
		TEXT("Set the max albedo in greyscale used for denoising. Scale the albedo variance as well. Used for suppressing specular noise.")
		TEXT("<=0: Ignore scaling."),
		ECVF_RenderThreadSafe);

	TAutoConsoleVariable<float> CVarNFORFeatureMaxNormalLength(
		TEXT("r.NFOR.Feature.MaxNormalLength"),
		10.0,
		TEXT("Set the max normal length used for denoising. Scale the normal variance as well. Used for suppressing specular noise.")
		TEXT("<=0: Ignore scaling."),
		ECVF_RenderThreadSafe);

	TAutoConsoleVariable<int32> CVarNFORPredivideAlbedo(
		TEXT("r.NFOR.PredivideAlbedo"),
		1,
		TEXT("Enable pre-albedo divide to denoise only the demodulated singal. It preserves more high frequency details."),
		ECVF_RenderThreadSafe);

	TAutoConsoleVariable<float> CVarNFORPredivideAlbedoOffset(
		TEXT("r.NFOR.PredivideAlbedo.Offset"),
		1e-3,
		TEXT("Offset for albedo for regions other than full reflection and sky materials. Increase to get a smoother result."),
		ECVF_RenderThreadSafe);

	TAutoConsoleVariable<float> CVarNFORPredivideAlbedoOffsetSky(
		TEXT("r.NFOR.PredivideAlbedo.OffsetSky"),
		0.2,
		TEXT("Sky or reflection of sky material has very small albedo that will cause noise. Offset more."),
		ECVF_RenderThreadSafe);

	TAutoConsoleVariable<int32> CVarNFORFrameCount(
		TEXT("r.NFOR.FrameCount"),
		0,
		TEXT("n: Use the previous n frames, the current frame, and the future n frames. Suggested range is 0~2. Max=3."),
		ECVF_RenderThreadSafe);

	TAutoConsoleVariable<int32> CVarNFORFrameCountCondition(
		TEXT("r.NFOR.FrameCount.Condition"),
		1,
		TEXT("0: Denoise even if the frame count accumulated is less than the required frame count (used for debug).")
		TEXT("1: Denoise only when the number of frame count meets requirement."),
		ECVF_RenderThreadSafe);

	TAutoConsoleVariable<int32> CVarNFORDenoisingFrameIndex(
		TEXT("r.NFOR.DenoisingFrameIndex"),
		-1,
		TEXT("The index of the denoising frame.")
		TEXT("-1: Automatically determine the index.")
		TEXT("i: Use all frames other than the ith frame to denoise."),
		ECVF_RenderThreadSafe);

	TAutoConsoleVariable<int32> CVarNFORNumOfTile(
		TEXT("r.NFOR.NumOfTile"),
		10,
		TEXT("<=1: Use a single dispatch. Could run out of memory.\n")
		TEXT("n: Divide the image into n x n tiles in [1,32].\n"),
		ECVF_RenderThreadSafe);

	TAutoConsoleVariable<int32> CVarNFORTileDebug(
		TEXT("r.NFOR.TileDebug"),
		0,
		TEXT(">0: Turn on tile debug mode."),
		ECVF_RenderThreadSafe);

	TAutoConsoleVariable<int32> CVarNFORTileDebugIndex(
		TEXT("r.NFOR.TileDebug.Index"),
		-1,
		TEXT("Tile index number to debug.")
		TEXT(" -1: The middle index in range of 0 ~ (NumOfTile * NumOfTile - 1).")
		TEXT(">=0: Select a specific tile to render for debug."),
		ECVF_RenderThreadSafe);

	TAutoConsoleVariable<int32> CVarNFORRegressionDevice(
		TEXT("r.NFOR.Regression.Device"),
		1,
		TEXT(" 0: CPU (verification). Used only for feature development.\n")
		TEXT(" 1: GPU.\n"),
		ECVF_RenderThreadSafe);

	TAutoConsoleVariable<float> CVarNFORRegressionDataRatioToParameters(
		TEXT("r.NFOR.Regression.MaxDataRatioToParemters"),
		20.0f,
		TEXT("The max number of observations per parameter in the regression. <1 to use all."),
		ECVF_RenderThreadSafe);

	TAutoConsoleVariable<int32> CVarNFORLinearSolverDevice(
		TEXT("r.NFOR.LinearSolver.Device"),
		1,
		TEXT("0: Solve Ax=B on CPU. Use householder QR decomposition from Eigen library.")
		TEXT("1: Solve Ax=B on GPU."),
		ECVF_RenderThreadSafe);

	TAutoConsoleVariable<int32> CVarNFORReconstructionType(
		TEXT("r.NFOR.Reconstruction.Type"),
		0,
		TEXT("0: Scatter for the denoising frame, gather for other temporal frames (default).")
		TEXT("1: Force gathering."),
		ECVF_RenderThreadSafe);

	TAutoConsoleVariable<int32> CVarNFORReconstructionDebugFrameIndex(
		TEXT("r.NFOR.Reconstruction.Debug.FrameIndex"),
		-1,
		TEXT(">=0: Output the denoising contribution from the ith frame only.")
		TEXT("-1: do not perform debug. Output contributions from all frames."),
		ECVF_RenderThreadSafe);

	TAutoConsoleVariable<int32> CVarNFORNonLocalMeanFeaturePatchSize(
		TEXT("r.NFOR.NonLocalMean.Feature.PatchSize"),
		3,
		TEXT("The patch size of the non-local mean algorithm for feature filtering. The patch width/height = PatchSize * 2 + 1."),
		ECVF_RenderThreadSafe);

	TAutoConsoleVariable<int32> CVarNFORNonLocalMeanFeaturePatchDistance(
		TEXT("r.NFOR.NonLocalMean.Feature.PatchDistance"),
		5,
		TEXT("The search distance of the non-local mean algorithm for feature filtering. The searching patch width/height = PatchDistance * 2 + 1."),
		ECVF_RenderThreadSafe);

	TAutoConsoleVariable<int32> CVarNFORNonLocalMeanRadiancePatchSize(
		TEXT("r.NFOR.NonLocalMean.Radiance.PatchSize"),
		3,
		TEXT("The patch size of the non-local mean algorithm. The patch width/height = PatchSize * 2 + 1."),
		ECVF_RenderThreadSafe);

	TAutoConsoleVariable<int32> CVarNFORNonLocalMeanRadiancePatchDistance(
		TEXT("r.NFOR.NonLocalMean.Radiance.PatchDistance"),
		9,
		TEXT("The search distance of the non-local mean algorithm. The searching patch width/height = PatchDistance * 2 + 1.")
		TEXT("The patch distance for bandwidth selection dependents on this parameters for MSE and selection filtering."),
		ECVF_RenderThreadSafe);

	TAutoConsoleVariable<bool> CVarNFORBandwidthSelection(
		TEXT("r.NFOR.BandwidthSelection"),
		true,
		TEXT("true: Apply bandwidth selection. It helps to preserve both high and low frequency details."),
		ECVF_RenderThreadSafe);

	TAutoConsoleVariable<float> CVarNFORBandwidthSelectionBandwidth(
		TEXT("r.NFOR.BandwidthSelection.Bandwidth"),
		-1,
		TEXT("-1: Use predefined bandwidths {0.5f, 1.0f}.")
		TEXT("(0,1]: Use a specific bandwidth."),
		ECVF_RenderThreadSafe);

	// Working in progress
	TAutoConsoleVariable<int32> CVarNFORAlbedoDivideRecoverPhase(
		TEXT("r.NFOR.AlbedoDivide.RecoverPhase"),
		0,
		TEXT("0: Add back in the last step. Denoised = Albedo_{center} * \\sum_{all frames}{denoised radiance}. Require high sample count for high quality albedo.")
		TEXT("1: Add back at each scattering or gathering. Denoised = * \\sum_{i \\in frames}{Albedo_i * denoised radiance}."),
		ECVF_RenderThreadSafe
	);

	//--------------------------------------------------------------------------------------------------------------------
	// Functions based on CVars

	bool ShouldFeatureAddConstant()
	{
		return CVarNFORFeatureAddConstant.GetValueOnRenderThread();
	}

	float GetFeatureMaxAlbedoGrayscale()
	{
		return CVarNFORFeatureMaxAlbedoGreyscale.GetValueOnRenderThread();
	}

	float GetFeatureMaxNormalLength()
	{
		return CVarNFORFeatureMaxNormalLength.GetValueOnRenderThread();
	}

	bool IsPreAlbedoDivideEnabled()
	{
		const bool bPredivideAlbedo = CVarNFORPredivideAlbedo.GetValueOnRenderThread() != 0;
		return bPredivideAlbedo;
	}

	FLinearColor GetPreAlbedoDivideAlbedoOffset()
	{
		const float Offset = FMath::Max(1e-8, CVarNFORPredivideAlbedoOffset.GetValueOnRenderThread());
		const float OffsetSky = FMath::Max(1e-8, CVarNFORPredivideAlbedoOffsetSky.GetValueOnRenderThread());
		return FLinearColor(Offset, OffsetSky, 0.0f);
	}

	int32 GetFrameCount()
	{
		const int32 NumFrames = FMath::Clamp(1 + 2 * CVarNFORFrameCount.GetValueOnRenderThread(), 1, 7);
		return NumFrames;
	}

	enum class EDenoiseFrameCountCondition : uint32
	{
		Any,
		Equal,
		MAX
	};

	EDenoiseFrameCountCondition GetFrameCountCoundition()
	{
		uint32 ConditionValue = CVarNFORFrameCountCondition.GetValueOnRenderThread();
		EDenoiseFrameCountCondition Condition = EDenoiseFrameCountCondition::Any;
		if (ConditionValue != 0)
		{
			Condition = EDenoiseFrameCountCondition::Equal;
		}
		return Condition;
	}

	int32 GetDenoisingFrameIndex(int32 NumberOfFrameInBuffer)
	{
		int32 TargetFrameCount = GetFrameCount();
		int32 DenoisingFrameIndex = CVarNFORDenoisingFrameIndex.GetValueOnRenderThread();
		int32 ResolvedSourceFrameIndex = INDEX_NONE;
		if (DenoisingFrameIndex < 0)
		{
			// If no specific denoising frame index is specified, use the center one
			if (NumberOfFrameInBuffer >= 0)
			{
				ResolvedSourceFrameIndex = (NumberOfFrameInBuffer > TargetFrameCount / 2) ? (TargetFrameCount / 2) : INDEX_NONE;
			}
			else
			{
				ResolvedSourceFrameIndex = TargetFrameCount / 2;
			}
		}
		else
		{
			// If the user has set the denoising index, use the available index within the limit
			ResolvedSourceFrameIndex = FMath::Clamp(DenoisingFrameIndex, 0, TargetFrameCount - 1);
			if (NumberOfFrameInBuffer - 1 < ResolvedSourceFrameIndex)
			{
				ResolvedSourceFrameIndex = INDEX_NONE;
			}
		}
		
		return ResolvedSourceFrameIndex;
	}

	int32 GetNumOfTiles()
	{
		return FMath::Clamp(CVarNFORNumOfTile.GetValueOnRenderThread(), 1, 32);
	}

	bool IsTileDebugEnabled()
	{
		return CVarNFORTileDebug.GetValueOnRenderThread() > 0;
	}

	int32 GetTileDebugIndex()
	{
		return CVarNFORTileDebugIndex.GetValueOnRenderThread();
	}

	enum class ERegressionDevice : int32
	{
		CPU,
		GPU,
		MAX
	};

	ERegressionDevice GetRegressionDevice()
	{
		const int32 RegressionDevice = FMath::Clamp(CVarNFORRegressionDevice.GetValueOnRenderThread(),
			static_cast<int32>(ERegressionDevice::CPU),
			static_cast<int32>(ERegressionDevice::MAX) - 1);
		return static_cast<ERegressionDevice>(RegressionDevice);
	}

	int32 GetSamplingStep(int32 NumberOfParameters, int32 TotalDataRecords)
	{
		int32 DataRatioToParameters = CVarNFORRegressionDataRatioToParameters.GetValueOnRenderThread();
		if (DataRatioToParameters < 1)
		{
			return 1;
		}
		return FMath::Max(1, TotalDataRecords / (NumberOfParameters * DataRatioToParameters));
	}

	enum class ELinearSolverDevice : int32
	{
		CPU,
		GPU,
		MAX
	};

	ELinearSolverDevice GetLinearSolverDevice()
	{
		const int32 LinearSolverDevice = FMath::Clamp(CVarNFORLinearSolverDevice.GetValueOnRenderThread(),
			static_cast<int32>(ELinearSolverDevice::CPU),
			static_cast<int32>(ELinearSolverDevice::MAX) - 1);
		return static_cast<ELinearSolverDevice>(LinearSolverDevice);
	}

	RegressionKernel::FReconstructSpatialTemporalImage::EReconstructionType 
		GetReconstructionType( int32 CurrentFrameIndex, int32 DenoisingFrameIndex)
	{
		RegressionKernel::FReconstructSpatialTemporalImage::EReconstructionType ReconstructionType;
		ReconstructionType = RegressionKernel::FReconstructSpatialTemporalImage::EReconstructionType::Gather;

		if (CurrentFrameIndex == DenoisingFrameIndex)
		{
			ReconstructionType = RegressionKernel::FReconstructSpatialTemporalImage::EReconstructionType::Scatter;
		}

		if (CVarNFORReconstructionType.GetValueOnRenderThread() != 0)
		{
			ReconstructionType = RegressionKernel::FReconstructSpatialTemporalImage::EReconstructionType::Gather;
		}

		return ReconstructionType;
	}

	int32 GetReconstructionDebugFrameIndex()
	{
		return CVarNFORReconstructionDebugFrameIndex.GetValueOnRenderThread();
	}

	int32 GetNonLocalMeanFeaturePatchSize()
	{
		return FMath::Clamp(CVarNFORNonLocalMeanFeaturePatchSize.GetValueOnRenderThread(), 0, 10);
	}

	int32 GetNonLocalMeanFeaturePatchDistance()
	{
		return FMath::Clamp(CVarNFORNonLocalMeanFeaturePatchDistance.GetValueOnRenderThread(), 0, 30);
	}

	int32 GetNonLocalMeanRadiancePatchSize()
	{
		return FMath::Clamp(CVarNFORNonLocalMeanRadiancePatchSize.GetValueOnRenderThread(), 0, 10);
	}

	int32 GetNonLocalMeanRadiancePatchDistance()
	{
		return FMath::Clamp(CVarNFORNonLocalMeanRadiancePatchDistance.GetValueOnRenderThread(), 0, 30);
	}

	bool IsBandwidthSelectionEnabled()
	{
		return CVarNFORBandwidthSelection.GetValueOnRenderThread();
	}

	TArray<float> GetBandwidthsConfiguration()
	{
		TArray<float> Bandwidths = { 0.5f, 1.0f };
		{
			float BandWidthOverride = FMath::Min(CVarNFORBandwidthSelectionBandwidth.GetValueOnRenderThread(), 1.0f);
			if (BandWidthOverride > 0)
			{
				Bandwidths = { BandWidthOverride };
			}
		}

		return Bandwidths;
	}

	EAlbedoDivideRecoverPhase GetPreAlbedoDivideRecoverPhase()
	{
		EAlbedoDivideRecoverPhase AlbedoDivideRecoverPhase = EAlbedoDivideRecoverPhase::Disabled;
		if (IsPreAlbedoDivideEnabled())
		{
			if (CVarNFORAlbedoDivideRecoverPhase.GetValueOnRenderThread() == 0)
			{
				AlbedoDivideRecoverPhase = EAlbedoDivideRecoverPhase::Final;
			}
			else
			{
				AlbedoDivideRecoverPhase = EAlbedoDivideRecoverPhase::Each;
			}
		}
		return AlbedoDivideRecoverPhase;
	}

	//--------------------------------------------------------------------------------------------------------------------
	// Shader implementations
	//--------------------------------------------------------------------------------------------------------------------
	// General texture operations
	IMPLEMENT_GLOBAL_SHADER(FTextureMultiplyCS, "/NFORDenoise/NFORDenoise.usf", "TextureOperationCS", SF_Compute);
	IMPLEMENT_GLOBAL_SHADER(FTextureDivideCS, "/NFORDenoise/NFORDenoise.usf", "TextureOperationCS", SF_Compute);
	IMPLEMENT_GLOBAL_SHADER(FTextureAccumulateConstantCS, "/NFORDenoise/NFORDenoise.usf", "TextureOperationCS", SF_Compute);
	IMPLEMENT_GLOBAL_SHADER(FTextureAccumulateCS, "/NFORDenoise/NFORDenoise.usf", "TextureOperationCS", SF_Compute);
	IMPLEMENT_GLOBAL_SHADER(FCopyTexturePS, "/NFORDenoise/NFORDenoise.usf", "CopyTexturePS", SF_Pixel);

	//--------------------------------------------------------------------------------------------------------------------
	// Feature range adjustment and radiance normalization
	IMPLEMENT_GLOBAL_SHADER(FClassifyPreAlbedoDivideMaskIdCS, "/NFORDenoise/NFORDenoise.usf", "ClassifyPreAlbedoDivideMaskIdCS", SF_Compute);
	IMPLEMENT_GLOBAL_SHADER(FNormalizeRadianceVarianceByAlbedoCS, "/NFORDenoise/NFORDenoise.usf", "NormalizeRadianceVarianceByAlbedoCS", SF_Compute);
	IMPLEMENT_GLOBAL_SHADER(FAdjustFeatureRangeCS, "/NFORDenoise/NFORDenoise.usf", "AdjustFeatureRangeCS", SF_Compute);

	//--------------------------------------------------------------------------------------------------------------------
	// Non-local mean weight and filtering
	IMPLEMENT_GLOBAL_SHADER(FNonLocalMeanFilteringCS, "/NFORDenoise/NFORDenoise.usf", "NonLocalMeanFilteringCS", SF_Compute);
	IMPLEMENT_GLOBAL_SHADER(FNonLocalMeanWeightsCS, "/NFORDenoise/NFORDenoise.usf", "NonLocalMeanWeightsCS", SF_Compute);

	//--------------------------------------------------------------------------------------------------------------------
	// Collaborative filtering
	//	1. Tiling
	IMPLEMENT_GLOBAL_SHADER(FCopyTextureToBufferCS, "/NFORDenoise/NFORDenoise.usf", "CopyTextureToBufferCS", SF_Compute);
	IMPLEMENT_GLOBAL_SHADER(FNormalizeTextureCS, "/NFORDenoise/NFORDenoise.usf", "NormalizeTextureCS", SF_Compute);
	
	//	2. Weighted Least-square solver
	IMPLEMENT_GLOBAL_SHADER(RegressionKernel::FInPlaceBatchedMatrixMultiplicationCS, "/NFORDenoise/NFORDenoise.usf", "InPlaceBatchedMatrixMultiplicationCS", SF_Compute);
	IMPLEMENT_GLOBAL_SHADER(RegressionKernel::FLinearSolverCS, "/NFORDenoise/NFORDenoise.usf", "LinearSolverCS", SF_Compute);
	IMPLEMENT_GLOBAL_SHADER(RegressionKernel::FReconstructSpatialTemporalImage, "/NFORDenoise/NFORDenoise.usf", "ReconstructSpatialTemporalImageCS", SF_Compute);
	IMPLEMENT_GLOBAL_SHADER(FAccumulateBufferToTextureCS, "/NFORDenoise/NFORDenoise.usf", "AccumulateBufferToTextureCS", SF_Compute);

	//--------------------------------------------------------------------------------------------------------------------
	// Bandwidth selection
	IMPLEMENT_GLOBAL_SHADER(FMSEEstimationCS, "/NFORDenoise/NFORDenoise.usf", "MSEEstimationCS", SF_Compute);
	IMPLEMENT_GLOBAL_SHADER(FGenerateSelectionMapCS, "/NFORDenoise/NFORDenoise.usf", "GenerateSelectionMapCS", SF_Compute);
	IMPLEMENT_GLOBAL_SHADER(FCombineFilteredImageCS, "/NFORDenoise/NFORDenoise.usf", "CombineFilteredImageCS", SF_Compute);

	//--------------------------------------------------------------------------------------------------------------------
	// General texture operations
	void AddMultiplyTextureRegionPass(FRDGBuilder& GraphBuilder, const FRDGTextureRef& SourceTexture, const FRDGTextureRef& TargetTexture,
		bool bForceMultiply, FIntPoint SourcePosition, FIntPoint TargetPosition, FIntPoint Size)
	{
		Size = Size == FIntPoint::ZeroValue ? SourceTexture->Desc.Extent : Size;
		FGlobalShaderMap* GlobalShaderMap = GetGlobalShaderMap(GMaxRHIFeatureLevel);

		typedef FTextureMultiplyCS SHADER;
		SHADER::FParameters* PassParameters = GraphBuilder.AllocParameters<SHADER::FParameters>();
		{
			PassParameters->Source = GraphBuilder.CreateSRV(FRDGTextureSRVDesc::Create(SourceTexture));
			PassParameters->RWTarget = GraphBuilder.CreateUAV(TargetTexture);
			PassParameters->SourcePosition = SourcePosition;
			PassParameters->TargetPosition = TargetPosition;
			PassParameters->ForceOperation = static_cast<int32>(bForceMultiply);
			PassParameters->Size = Size;
		}

		TShaderMapRef<SHADER> ComputeShader(GlobalShaderMap);

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("NFOR::AddMultiplyTextureRegionPass (%s [%d,%d] -> %s [%d,%d], size:%dx%d)",
				SourceTexture->Name,
				SourcePosition.X,
				SourcePosition.Y,
				TargetTexture->Name,
				TargetPosition.X,
				TargetPosition.Y,
				Size.X,
				Size.Y),
			ERDGPassFlags::Compute,
			ComputeShader,
			PassParameters,
			FComputeShaderUtils::GetGroupCount(Size, NON_LOCAL_MEAN_THREAD_GROUP_SIZE));
	}

	void AddDivideTextureRegionPass(FRDGBuilder& GraphBuilder, const FRDGTextureRef& SourceTexture, const FRDGTextureRef& TargetTexture,
		bool bForceDivide, FIntPoint SourcePosition, FIntPoint TargetPosition, FIntPoint Size)
	{
		Size = Size == FIntPoint::ZeroValue ? SourceTexture->Desc.Extent : Size;
		FGlobalShaderMap* GlobalShaderMap = GetGlobalShaderMap(GMaxRHIFeatureLevel);

		typedef FTextureDivideCS SHADER;
		SHADER::FParameters* PassParameters = GraphBuilder.AllocParameters<SHADER::FParameters>();
		{
			PassParameters->Source = GraphBuilder.CreateSRV(FRDGTextureSRVDesc::Create(SourceTexture));
			PassParameters->RWTarget = GraphBuilder.CreateUAV(TargetTexture);
			PassParameters->SourcePosition = SourcePosition;
			PassParameters->TargetPosition = TargetPosition;
			PassParameters->ForceOperation = static_cast<int32>(bForceDivide);
			PassParameters->Size = Size;
		}

		TShaderMapRef<SHADER> ComputeShader(GlobalShaderMap);

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("NFOR::AddDivideTextureRegionPass (%s [%d,%d] -> %s [%d,%d], size:%dx%d)",
				SourceTexture->Name,
				SourcePosition.X,
				SourcePosition.Y,
				TargetTexture->Name,
				TargetPosition.X,
				TargetPosition.Y,
				Size.X,
				Size.Y),
			ERDGPassFlags::Compute,
			ComputeShader,
			PassParameters,
			FComputeShaderUtils::GetGroupCount(Size, NON_LOCAL_MEAN_THREAD_GROUP_SIZE));
	}

	void AddAccumulateTextureRegionPass(FRDGBuilder& GraphBuilder, const FRDGTextureRef& SourceTexture, const FRDGTextureRef& TargetTexture,
		FIntPoint SourcePosition, FIntPoint TargetPosition, FIntPoint Size)
	{
		Size = Size == FIntPoint::ZeroValue ? SourceTexture->Desc.Extent : Size;
		FGlobalShaderMap* GlobalShaderMap = GetGlobalShaderMap(GMaxRHIFeatureLevel);

		{
			typedef FTextureAccumulateCS SHADER;
			SHADER::FParameters* PassParameters = GraphBuilder.AllocParameters<SHADER::FParameters>();
			{
				PassParameters->Source = GraphBuilder.CreateSRV(FRDGTextureSRVDesc::Create(SourceTexture));
				PassParameters->RWTarget = GraphBuilder.CreateUAV(TargetTexture);
				PassParameters->SourcePosition = SourcePosition;
				PassParameters->TargetPosition = TargetPosition;
				PassParameters->Size = Size;
			}

			TShaderMapRef<SHADER> ComputeShader(GlobalShaderMap);

			FComputeShaderUtils::AddPass(
				GraphBuilder,
				RDG_EVENT_NAME("NFOR::AddAccumulateTextureRegionPass (%s [%d,%d] -> %s [%d,%d], size:%dx%d)",
					SourceTexture->Name,
					SourcePosition.X,
					SourcePosition.Y,
					TargetTexture->Name,
					TargetPosition.X,
					TargetPosition.Y,
					Size.X,
					Size.Y),
				ERDGPassFlags::Compute,
				ComputeShader,
				PassParameters,
				FComputeShaderUtils::GetGroupCount(Size, NON_LOCAL_MEAN_THREAD_GROUP_SIZE));
		}
	}

	void AddAccumulateConstantRegionPass(FRDGBuilder& GraphBuilder, const FLinearColor& ConstantValue, const FRDGTextureRef& TargetTexture, const FRDGTextureRef& Mask,
		FIntPoint SourcePosition, FIntPoint TargetPosition, FIntPoint Size)
	{
		Size = Size == FIntPoint::ZeroValue ? TargetTexture->Desc.Extent : Size;
		FGlobalShaderMap* GlobalShaderMap = GetGlobalShaderMap(GMaxRHIFeatureLevel);
		const bool bUseMask = Mask != nullptr;

		typedef FTextureAccumulateConstantCS SHADER;
		SHADER::FParameters* PassParameters = GraphBuilder.AllocParameters<SHADER::FParameters>();
		{
			PassParameters->RWTarget = GraphBuilder.CreateUAV(TargetTexture);
			PassParameters->TargetPosition = TargetPosition;
			PassParameters->ConstantValue = ConstantValue;
			PassParameters->Size = Size;
			PassParameters->Mask = bUseMask ? GraphBuilder.CreateSRV(Mask) : nullptr;
		}

		SHADER::FPermutationDomain ComputeShaderPermutationVector;
		ComputeShaderPermutationVector.Set<SHADER::FDimensionAccumulateByMask>(bUseMask);
		TShaderMapRef<SHADER> ComputeShader(GlobalShaderMap, ComputeShaderPermutationVector);

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("NFOR::AddAccumulateConstantRegionPass ([%.1f,%.1f,%.1f,%.1f] -> %s [%d,%d], size:%dx%d %s)",
				ConstantValue.R,
				ConstantValue.G,
				ConstantValue.B,
				ConstantValue.A,
				TargetTexture->Name,
				TargetPosition.X,
				TargetPosition.Y,
				Size.X,
				Size.Y,
				bUseMask ? TEXT("Masked") : TEXT("")),
			ERDGPassFlags::Compute,
			ComputeShader,
			PassParameters,
			FComputeShaderUtils::GetGroupCount(Size, NON_LOCAL_MEAN_THREAD_GROUP_SIZE));
	}

	void AddCopyMirroredTexturePass(FRDGBuilder& GraphBuilder, const FRDGTextureRef& SourceTexture, const FRDGTextureRef& TargetTexture,
		FIntPoint SourcePosition, FIntPoint TargetPosition, FIntPoint Size, bool bAlphaOnly)
	{
		const FIntPoint CopySize = Size == FIntPoint::ZeroValue ? TargetTexture->Desc.Extent : Size;

		typedef FCopyTexturePS SHADER;

		SHADER::FParameters* PassParameters = GraphBuilder.AllocParameters<SHADER::FParameters>();
		{
			PassParameters->Source = GraphBuilder.CreateSRV(FRDGTextureSRVDesc::Create(SourceTexture));
			PassParameters->SourceOffset = SourcePosition;
			PassParameters->TextureSize = SourceTexture->Desc.Extent;
			PassParameters->RenderTargets[0] = FRenderTargetBinding(TargetTexture, ERenderTargetLoadAction::ENoAction);
		}

		const FIntRect ViewRect(TargetPosition, TargetPosition + CopySize);

		FGlobalShaderMap* ShaderMap = GetGlobalShaderMap(GMaxRHIFeatureLevel);
		TShaderMapRef<FCopyTexturePS> PixelShader(ShaderMap);

		FRHIBlendState* BlendState = TStaticBlendState<CW_RGBA, BO_Add, BF_One, BF_Zero, BO_Add, BF_One, BF_Zero>::GetRHI();
		if (bAlphaOnly)
		{
			BlendState = TStaticBlendState<CW_RGBA, BO_Add, BF_Zero, BF_One, BO_Add, BF_One, BF_Zero>::GetRHI();
		}

		FPixelShaderUtils::AddFullscreenPass(
			GraphBuilder,
			ShaderMap,
			RDG_EVENT_NAME("CopyTexture (%s -> %s,Mirrored%s)",
				SourceTexture->Name,
				TargetTexture->Name,
				bAlphaOnly ? TEXT(" AlphaOnly") : TEXT("")),
			PixelShader,
			PassParameters,
			ViewRect,
			BlendState
		);
	}

	void AddNormalizeRadianceVariancePass(FRDGBuilder& GraphBuilder, const FRDGTextureRef& Albedo, const FRDGTextureRef& RadianceVariance)
	{
		FIntPoint Size = RadianceVariance->Desc.Extent;
		typedef FNormalizeRadianceVarianceByAlbedoCS SHADER;
		SHADER::FParameters* PassParameters = GraphBuilder.AllocParameters<SHADER::FParameters>();
		{
			PassParameters->Albedo = GraphBuilder.CreateSRV(FRDGTextureSRVDesc::Create(Albedo));
			PassParameters->RWRadianceVariance = GraphBuilder.CreateUAV(RadianceVariance);
			PassParameters->Size = Size;
		}

		TShaderMapRef<SHADER> ComputeShader(GetGlobalShaderMap(GMaxRHIFeatureLevel));

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("NFOR::AddNormalizeRadianceVariancePass (%s.RadianceVariance / %s, size:%dx%d)",
				RadianceVariance->Name,
				Albedo->Name,
				Size.X,
				Size.Y),
			ERDGPassFlags::Compute,
			ComputeShader,
			PassParameters,
			FComputeShaderUtils::GetGroupCount(Size, NON_LOCAL_MEAN_THREAD_GROUP_SIZE));
	}

	//--------------------------------------------------------------------------------------------------------------------
	// Radiance normalization
	FRDGTextureRef GetPreAlbedoDivideMask(FRDGBuilder& GraphBuilder, const FSceneView& View, const FRDGTextureRef& Normal, const FRDGTextureRef& NormalVariance)
	{
		FRDGTextureDesc Desc = Normal->Desc;
		Desc.Format = PF_R8_UINT;
		FRDGTextureRef MaskTexture = GraphBuilder.CreateTexture(Desc, TEXT("NFOR.MaskTexture"));
		{
			typedef FClassifyPreAlbedoDivideMaskIdCS SHADER;
			SHADER::FParameters* PassParameters = GraphBuilder.AllocParameters<SHADER::FParameters>();
			{
				PassParameters->Normal = GraphBuilder.CreateSRV(Normal);
				PassParameters->NormalVariance = GraphBuilder.CreateSRV(NormalVariance);
				PassParameters->TextureSize = Desc.Extent;
				PassParameters->RWMask = GraphBuilder.CreateUAV(MaskTexture);
			}

			TShaderMapRef<SHADER> ComputeShader(GetGlobalShaderMap(GMaxRHIFeatureLevel));

			FComputeShaderUtils::AddPass(
				GraphBuilder,
				RDG_EVENT_NAME("NFOR::ClassifyPreAlbedoDivideMaskIdCS (size:%dx%d)",
					Desc.Extent.X,
					Desc.Extent.Y),
				ERDGPassFlags::Compute,
				ComputeShader,
				PassParameters,
				FComputeShaderUtils::GetGroupCount(Desc.Extent, NON_LOCAL_MEAN_THREAD_GROUP_SIZE));
		}

		return MaskTexture;
	}

	void AddAdjustFeatureRangePass(FRDGBuilder& GraphBuilder, const FFeatureDesc& FeatureDesc, float MaxValue)
	{
		checkf(FeatureDesc.Data.NumOfChannel == 4, TEXT("Only feature with 4 channels can be adjusted"));
		checkf(FeatureDesc.VarianceType != EVarianceType::Colored, TEXT("Feature variance of type EVarianceType::Colored cannot be adjusted"));
		
		if (MaxValue <= 0)
		{
			return;
		}

		FIntPoint Size = FeatureDesc.Data.Image->Desc.Extent;
		typedef FAdjustFeatureRangeCS SHADER;
		SHADER::FParameters* PassParameters = GraphBuilder.AllocParameters<SHADER::FParameters>();
		{
			PassParameters->RWImage = GraphBuilder.CreateUAV(FeatureDesc.Data.Image);
			PassParameters->RWImageVariance = GraphBuilder.CreateUAV(FeatureDesc.Variance.Image);
			PassParameters->Size = Size;
			PassParameters->VarianceChannelOffset = FeatureDesc.Variance.ChannelOffset;
			PassParameters->MaxValue = MaxValue;
		}

		SHADER::FPermutationDomain ComputeShaderPermutationVector;
		{
			ComputeShaderPermutationVector.Set<SHADER::FDimensionVarianceType>(FeatureDesc.VarianceType);
		}

		TShaderMapRef<SHADER> ComputeShader(GetGlobalShaderMap(GMaxRHIFeatureLevel), ComputeShaderPermutationVector);

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("NFOR::AddAdjustFeatureRangePass (%s, MaxValue=%.2f, size:%dx%d)",
				FeatureDesc.Data.Image->Name,
				MaxValue,
				Size.X,
				Size.Y),
			ERDGPassFlags::Compute,
			ComputeShader,
			PassParameters,
			FComputeShaderUtils::GetGroupCount(Size, NON_LOCAL_MEAN_THREAD_GROUP_SIZE));
	}

	//--------------------------------------------------------------------------------------------------------------------
	// Non-local mean weight and filtering
	FNonLocalMeanParameters GetNonLocalMeanParameters(int32 PatchSize, int32 PatchDistance, float Bandwidth)
	{
		FNonLocalMeanParameters NonLocalMeanParameters;
		NonLocalMeanParameters.PatchSize = PatchSize;
		NonLocalMeanParameters.PatchDistance = PatchDistance;
		NonLocalMeanParameters.Bandwidth = Bandwidth;

		return NonLocalMeanParameters;
	}

	FNonLocalMeanParameters GetFeatureNonLocalMeanParameters(float Bandwidth)
	{
		const int32 PatchSize = GetNonLocalMeanFeaturePatchSize();
		const int32 PatchDistance = GetNonLocalMeanFeaturePatchDistance();

		return GetNonLocalMeanParameters(PatchSize, PatchDistance, Bandwidth);
	}

	void ApplyNonLocalMeanFilter(
		FRDGBuilder& GraphBuilder,
		const FNonLocalMeanParameters& NonLocalMeanParameters,
		const FNFORTextureDesc& Texture,
		const FNFORTextureDesc& Variance,
		EVarianceType VarianceTyle,
		const FRDGTextureRef& FilteredTexture)
	{
		FGlobalShaderMap* GlobalShaderMap = GetGlobalShaderMap(GMaxRHIFeatureLevel);
		FIntPoint TextureSize = Texture.Image->Desc.Extent;
		const FRDGTextureRef VarianceTexture = Variance.Image ? Variance.Image : GSystemTextures.GetBlackDummy(GraphBuilder);

		typedef FNonLocalMeanFilteringCS SHADER;

		SHADER::FParameters* PassParameters = GraphBuilder.AllocParameters<SHADER::FParameters>();
		{
			PassParameters->NLMParams = NonLocalMeanParameters;
			PassParameters->Image = GraphBuilder.CreateSRV(FRDGTextureSRVDesc::Create(Texture.Image));
			PassParameters->Variance = GraphBuilder.CreateSRV(FRDGTextureSRVDesc::Create(VarianceTexture));
			PassParameters->TextureSize = TextureSize;
			PassParameters->VarianceChannelOffset = Variance.ChannelOffset;
			PassParameters->DenoisingChannelCount = Texture.ChannelCount;
			PassParameters->DenoisedImage = GraphBuilder.CreateUAV(FRDGTextureUAVDesc(FilteredTexture));
		}

		SHADER::FPermutationDomain ComputeShaderPermutationVector;
		{
			ComputeShaderPermutationVector.Set<SHADER::FDimensionVarianceType>(VarianceTyle);
			ComputeShaderPermutationVector.Set<SHADER::FDimensionUseGuide>(false);
			ComputeShaderPermutationVector.Set<SHADER::FDimensionImageChannelCount>(Texture.NumOfChannel);
			ComputeShaderPermutationVector.Set<SHADER::FDimPreAlbedoDivide>(GetPreAlbedoDivideRecoverPhase());
		}

		TShaderMapRef<SHADER> ComputeShader(GlobalShaderMap, ComputeShaderPermutationVector);

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("NFOR::FeatureFiltering(%s, Dim=%d,%d)", Texture.Image->Name, TextureSize.X, TextureSize.Y),
			ERDGPassFlags::Compute,
			ComputeShader,
			PassParameters,
			FComputeShaderUtils::GetGroupCount(TextureSize, NON_LOCAL_MEAN_THREAD_GROUP_SIZE));
	}

	void GetNLMWeights(
		FRDGBuilder& GraphBuilder,
		const FSceneView& View,
		const FRadianceDesc& SourceRadiance,
		const FRadianceDesc& TargetRadiance,
		const FRDGBufferRef& NonLocalMeanWeightsBuffer,
		FIntRect Region,
		const FNonLocalMeanParameters& NonLocalMeanParameters)
	{
		const int32 SearchingPatchSize = (NonLocalMeanParameters.PatchDistance * 2 + 1);
		const int32 NumberOfWeightsPerPixel = SearchingPatchSize * SearchingPatchSize;
		const FIntPoint TextureSize = SourceRadiance.Data.Image->Desc.Extent;
		const bool bSeparateSourceTarget = (SourceRadiance.Data.Image != TargetRadiance.Data.Image);
		
		// Query the non-local mean weights for the radiance.
		{
			typedef FNonLocalMeanWeightsCS SHADER;
			SHADER::FParameters* PassParameters = GraphBuilder.AllocParameters<SHADER::FParameters>();
			{
				PassParameters->CommonParameters.NLMParams = NonLocalMeanParameters;
				PassParameters->CommonParameters.Image = GraphBuilder.CreateSRV(FRDGTextureSRVDesc::Create(SourceRadiance.Data.Image));
				PassParameters->CommonParameters.Variance = GraphBuilder.CreateSRV(FRDGTextureSRVDesc::Create(SourceRadiance.Variance.Image));
				PassParameters->CommonParameters.TextureSize = TextureSize;
				PassParameters->CommonParameters.VarianceChannelOffset = SourceRadiance.Variance.ChannelOffset;

				PassParameters->NonLocalMeanWeights = GraphBuilder.CreateUAV(NonLocalMeanWeightsBuffer, PF_R32_FLOAT);
				PassParameters->Region = Region;

				if (bSeparateSourceTarget)
				{
					PassParameters->TargetImage = GraphBuilder.CreateSRV(FRDGTextureSRVDesc::Create(TargetRadiance.Data.Image));
					PassParameters->TargetVariance = GraphBuilder.CreateSRV(FRDGTextureSRVDesc::Create(TargetRadiance.Variance.Image));
				}
			}

			SHADER::FPermutationDomain ComputeShaderPermutationVector;
			{
				ComputeShaderPermutationVector.Set<SHADER::FDimensionVarianceType>(EVarianceType::GreyScale);
				ComputeShaderPermutationVector.Set<SHADER::FDimensionUseGuide>(false);
				ComputeShaderPermutationVector.Set<SHADER::FDimensionImageChannelCount>(SourceRadiance.Data.NumOfChannel);
				ComputeShaderPermutationVector.Set<SHADER::FDimensionSeparateSourceTarget>(bSeparateSourceTarget);
				ComputeShaderPermutationVector.Set<SHADER::FDimPreAlbedoDivide>(GetPreAlbedoDivideRecoverPhase());
			}

			TShaderMapRef<SHADER> ComputeShader(GetGlobalShaderMap(GMaxRHIFeatureLevel), ComputeShaderPermutationVector);

			FComputeShaderUtils::AddPass(
				GraphBuilder,
				RDG_EVENT_NAME("NFOR::NonLocalMeanWeights (Rect=(%d,%d,%d,%d),ps=%d,pd=%d,bw=%.2f)",
					Region.Min.X,
					Region.Min.Y,
					Region.Max.X,
					Region.Max.Y,
					NonLocalMeanParameters.PatchSize,
					NonLocalMeanParameters.PatchDistance,
					NonLocalMeanParameters.Bandwidth),
				ERDGPassFlags::Compute,
				ComputeShader,
				PassParameters,
				FComputeShaderUtils::GetGroupCount(Region.Size(), NON_LOCAL_MEAN_THREAD_GROUP_SIZE));
		}
	}

	//--------------------------------------------------------------------------------------------------------------------
	// Collaborative filtering
	//	1. Tiling
	void AddCopyTextureToBufferPass(
		FRDGBuilder& GraphBuilder,
		const FRDGTextureRef Source,
		const FRDGBufferRef Dest,
		int32 CopyChannelOffset,
		int32 CopyChannelCount,
		int32 NumberOfSourceChannel,
		int32 BufferChannelOffset,
		int32 BufferChannelSize,
		FIntRect CopyRegion)
	{
		FGlobalShaderMap* GlobalShaderMap = GetGlobalShaderMap(GMaxRHIFeatureLevel);
		const int SourceChannelCount = NumberOfSourceChannel;
		FIntPoint TextureSize = Source->Desc.Extent;

		{
			typedef FCopyTextureToBufferCS SHADER;
			SHADER::FParameters* PassParameters = GraphBuilder.AllocParameters<SHADER::FParameters>();
			{
				PassParameters->Source = GraphBuilder.CreateSRV(FRDGTextureSRVDesc::Create(Source));
				PassParameters->Dest = GraphBuilder.CreateUAV(Dest, PF_R32_FLOAT);
				PassParameters->TextureSize = TextureSize;
				PassParameters->CopyChannelOffset = CopyChannelOffset;
				PassParameters->CopyChannelCount = CopyChannelCount;
				PassParameters->BufferChannelOffset = BufferChannelOffset;
				PassParameters->BufferChannelSize = BufferChannelSize;
				PassParameters->CopyRegion = CopyRegion;
			}

			SHADER::FPermutationDomain ComputeShaderPermutationVector;
			ComputeShaderPermutationVector.Set<SHADER::FDimensionSourceChannelCount>(SourceChannelCount);
			TShaderMapRef<SHADER> ComputeShader(GlobalShaderMap, ComputeShaderPermutationVector);

			FComputeShaderUtils::AddPass(
				GraphBuilder,
				RDG_EVENT_NAME("NFOR::CopyTextureToBuffer (Dim=%dx%d,s:%d:%d -> b:%d)",
					TextureSize.X,
					TextureSize.Y,
					CopyChannelOffset,
					CopyChannelOffset + CopyChannelCount - 1,
					BufferChannelOffset),
				ERDGPassFlags::Compute,
				ComputeShader,
				PassParameters,
				FComputeShaderUtils::GetGroupCount(CopyRegion.Size(), NON_LOCAL_MEAN_THREAD_GROUP_SIZE));
		}
	}

	void AddCopyTextureToBufferPass(
		FRDGBuilder& GraphBuilder,
		const FRDGTextureRef Source,
		const FRDGBufferRef Dest,
		int32 CopyChannelOffset,
		int32 CopyChannelCount,
		int32 NumberOfSourceChannel,
		int32 BufferChannelOffset,
		int32 BufferChannelSize)
	{
		AddCopyTextureToBufferPass(
			GraphBuilder,
			Source,
			Dest,
			CopyChannelOffset,
			CopyChannelCount,
			NumberOfSourceChannel,
			BufferChannelOffset,
			BufferChannelSize,
			FIntRect(FIntPoint(0, 0), Source->Desc.Extent));
	}

	void AddNormalizeTexturePass( FRDGBuilder& GraphBuilder, const FRDGTextureRef& InputTexture)
	{
		FIntPoint TextureSize = InputTexture->Desc.Extent;
		FGlobalShaderMap* GlobalShaderMap = GetGlobalShaderMap(GMaxRHIFeatureLevel);

		{
			typedef FNormalizeTextureCS SHADER;
			SHADER::FParameters* PassParameters = GraphBuilder.AllocParameters<SHADER::FParameters>();
			{
				PassParameters->RWSource = GraphBuilder.CreateUAV(InputTexture);
				PassParameters->TextureSize = TextureSize;
			}

			TShaderMapRef<SHADER> ComputeShader(GlobalShaderMap);

			FComputeShaderUtils::AddPass(
				GraphBuilder,
				RDG_EVENT_NAME("NFOR::AddNormalizeTexturePass (%dx%d)",
					TextureSize.X,
					TextureSize.Y),
				ERDGPassFlags::Compute,
				ComputeShader,
				PassParameters,
				FComputeShaderUtils::GetGroupCount(TextureSize, NON_LOCAL_MEAN_THREAD_GROUP_SIZE));
		}
	}


	//	2. Weighted Least-square solver
	FRDGBufferRef RegressionKernel::AllocateMatrixfBuffer(FRDGBuilder& GraphBuilder, int32 NumOfMatrices, int32 Dim0, int32 Dim1, const TCHAR* Name)
	{
		const int32 BytesPerElement = sizeof(float);

		FRDGBufferDesc BufferDesc =
			FRDGBufferDesc::CreateBufferDesc(BytesPerElement, NumOfMatrices * Dim0 * Dim1);

		return GraphBuilder.CreateBuffer(BufferDesc, Name);
	}

	FRDGBufferRef RegressionKernel::FInPlaceBatchedMatrixMultiplicationCS::AllocateResultBuffer( FRDGBuilder& GraphBuilder, FIntPoint Size, int32 F, int32 A)
	{
		return AllocateMatrixfBuffer(GraphBuilder, Size.X * Size.Y, F, A, TEXT("NFOR.Matrix.Result"));
	}

	FRDGBufferRef ApplyBatchedInPlaceMatrixMultiplication(
		FRDGBuilder& GraphBuilder,
		FRDGBufferRef X,
		FIntPoint XDim,
		FRDGBufferRef W,
		int32 WDim,
		FIntPoint TextureSize,
		int PatchDistance,
		RegressionKernel::EWeightedMultiplicationType MultiplicationType = RegressionKernel::EWeightedMultiplicationType::Quadratic,
		FRDGBufferRef Y = nullptr,
		FIntPoint YDim = 0)
	{
		FRDGBufferRef ResultMatrix = nullptr;
		{
			typedef RegressionKernel::FInPlaceBatchedMatrixMultiplicationCS SHADER;
			const bool GeneralizedMultiplication = MultiplicationType == RegressionKernel::EWeightedMultiplicationType::Generalized;
			const bool bFeatureAddConstant = ShouldFeatureAddConstant();
			const int BufferXDimWithConstant = XDim.Y + (bFeatureAddConstant ? 1 : 0);
			FIntPoint ResultMatrixDimension = FIntPoint(BufferXDimWithConstant, GeneralizedMultiplication ? YDim.Y : BufferXDimWithConstant);
			int32 SamplingStep = GetSamplingStep(WDim, ResultMatrixDimension.X * ResultMatrixDimension.Y);

			ResultMatrix = SHADER::AllocateResultBuffer(GraphBuilder, TextureSize, ResultMatrixDimension.X, ResultMatrixDimension.Y);

			SHADER::FParameters* PassParameters = GraphBuilder.AllocParameters<SHADER::FParameters>();
			{
				PassParameters->X = GraphBuilder.CreateSRV(FRDGBufferSRVDesc(X, PF_R32_FLOAT));
				PassParameters->XDim = XDim;

				PassParameters->W = GraphBuilder.CreateSRV(FRDGBufferSRVDesc(W, PF_R32_FLOAT));
				PassParameters->WDim = WDim;


				if (GeneralizedMultiplication)
				{
					PassParameters->Y = GraphBuilder.CreateSRV(FRDGBufferSRVDesc(Y, PF_R32_FLOAT));
					PassParameters->YDim = YDim;
				}
				else
				{
					PassParameters->Y = GraphBuilder.CreateSRV(FRDGBufferSRVDesc(X, PF_R32_FLOAT));
					PassParameters->YDim = XDim;
				}

				PassParameters->TextureSize = TextureSize;
				PassParameters->PatchDistance = PatchDistance;
				PassParameters->NumOfWeigthsPerPixelPerFrame = (PatchDistance * 2 + 1) * (PatchDistance * 2 + 1);
				PassParameters->NumOfTemporalFrames = WDim / PassParameters->NumOfWeigthsPerPixelPerFrame;
				PassParameters->SourceFrameIndex = GetDenoisingFrameIndex(PassParameters->NumOfTemporalFrames);
				PassParameters->SamplingStep = SamplingStep;
				PassParameters->Result = GraphBuilder.CreateUAV(FRDGBufferUAVDesc(ResultMatrix, PF_R32_FLOAT));
			}

			//TODO: clean up mutation.
			SHADER::FPermutationDomain ComputeShaderPermutationVector;
			{
				ComputeShaderPermutationVector.Set<SHADER::FDimWeightedMultiplicationType>(MultiplicationType);
				ComputeShaderPermutationVector.Set<SHADER::FDimAddConstantFeatureDim>(bFeatureAddConstant);
				ComputeShaderPermutationVector.Set<SHADER::FDimOptimizeTargetMatrixMultiplication>(true);
				ComputeShaderPermutationVector.Set<SHADER::FDimNumFeature>(BufferXDimWithConstant);
				ComputeShaderPermutationVector.Set<SHADER::FDimUseSamplingStep>(SamplingStep > 1);
			}

			TShaderMapRef<SHADER> ComputeShader(GetGlobalShaderMap(GMaxRHIFeatureLevel), ComputeShaderPermutationVector);

			FComputeShaderUtils::AddPass(
				GraphBuilder,
				RDG_EVENT_NAME("NFOR::Matrix Multiplication (%s%s)",
					RegressionKernel::GetEventName(MultiplicationType),
					bFeatureAddConstant? TEXT(" +Const. Feature":TEXT(""))),
				ERDGPassFlags::Compute,
				ComputeShader,
				PassParameters,
				FComputeShaderUtils::GetGroupCount(TextureSize, SHADER::GetThreadGroupSize()));
		}

		return ResultMatrix;
	}

	void ReconstructByFrame(
		FRDGBuilder& GraphBuilder,
		FRDGBufferRef Feature,
		FRDGBufferRef ReconstructionWeights,
		FRDGBufferRef NonLocalMeanWeightsBuffer,
		FRDGTextureRef FilteredRadiance,
		FRDGTextureRef SourceAlbedo,
		FWeightedLSRDesc WeightedLSRDesc,
		int FrameIndex,
		RegressionKernel::FReconstructSpatialTemporalImage::EReconstructionType ReconstructionType = RegressionKernel::FReconstructSpatialTemporalImage::EReconstructionType::Scatter
	)
	{

		checkf(FrameIndex < WeightedLSRDesc.NumOfFrames,
			TEXT("FrameIndex should be less than total number of frames: %d < %d failed:"), FrameIndex, WeightedLSRDesc.NumOfFrames);
		
		FRDGTextureRef ReconstructionBuffer64 = nullptr;
		FRDGBufferRef ReconstructionBuffer = nullptr;

		FIntPoint TextureSize = FilteredRadiance->Desc.Extent;

		{
			typedef RegressionKernel::FReconstructSpatialTemporalImage SHADER;
			if (ReconstructionType == SHADER::EReconstructionType::Scatter)
			{
				FRDGBufferDesc BufferDesc = FRDGBufferDesc::CreateStructuredDesc(sizeof(FVector4f),TextureSize.X * TextureSize.Y);
				ReconstructionBuffer = GraphBuilder.CreateBuffer(BufferDesc, TEXT("NFOR.WeightedLSR.ReconstructionBuffer"));

				const EPixelFormat PixelFormat64 = GPixelFormats[PF_R64_UINT].Supported ? PF_R64_UINT : PF_R32G32_UINT;

				const FRDGTextureDesc ReconstructionBufferDesc = FRDGTextureDesc::Create2D(
					FIntPoint(TextureSize.X*4, TextureSize.Y),
					PixelFormat64,
					FClearValueBinding::None,
					TexCreate_RenderTargetable|TexCreate_ShaderResource | TexCreate_UAV | ETextureCreateFlags::Atomic64Compatible);

				ReconstructionBuffer64 = GraphBuilder.CreateTexture(ReconstructionBufferDesc, TEXT("NFOR.WeightedLSR.ReconstructionBuffer64"));

				AddClearUAVPass(GraphBuilder, GraphBuilder.CreateUAV(ReconstructionBuffer), 0);
				AddClearRenderTargetPass(GraphBuilder, ReconstructionBuffer64, FLinearColor::Transparent);
			}

			const bool bFeatureAddConstant = ShouldFeatureAddConstant();
			const int32 NumOfAdditionalFeatures = (bFeatureAddConstant ? 1 : 0);

			FIntPoint XDimension = FIntPoint(WeightedLSRDesc.NumOfWeightsPerPixel, WeightedLSRDesc.NumOfFeatureChannelsPerFrame);
			const int PatchDistance = (FMath::Sqrt(float(WeightedLSRDesc.NumOfWeightsPerPixel / WeightedLSRDesc.NumOfFrames)) - 1) / 2;
			const int TotalNumOfFeaturesPerFrame = WeightedLSRDesc.NumOfFeatureChannelsPerFrame + NumOfAdditionalFeatures;
			FIntPoint BDim = FIntPoint(TotalNumOfFeaturesPerFrame, WeightedLSRDesc.NumOfRadianceChannelsPerFrame);

			SHADER::FParameters* PassParameters = GraphBuilder.AllocParameters<SHADER::FParameters>();
			{
				PassParameters->X = GraphBuilder.CreateSRV(FRDGBufferSRVDesc(Feature, PF_R32_FLOAT));
				PassParameters->W = GraphBuilder.CreateSRV(FRDGBufferSRVDesc(NonLocalMeanWeightsBuffer, PF_R32_FLOAT));
				PassParameters->B = GraphBuilder.CreateSRV(FRDGBufferSRVDesc(ReconstructionWeights, PF_R32_FLOAT));
				PassParameters->RWReconstruction = GraphBuilder.CreateUAV(FilteredRadiance);
				PassParameters->RWReconstructBuffer = ReconstructionBuffer ? GraphBuilder.CreateUAV(ReconstructionBuffer) : nullptr;
				PassParameters->RWReconstructBuffer64 = ReconstructionBuffer ? GraphBuilder.CreateUAV(ReconstructionBuffer64) : nullptr;

				PassParameters->XDim = XDimension;
				PassParameters->WDim = WeightedLSRDesc.NumOfWeightsPerPixel;
				PassParameters->BDim = BDim;

				PassParameters->TextureSize = FIntPoint(WeightedLSRDesc.Width, WeightedLSRDesc.Height);
				PassParameters->PatchDistance = PatchDistance;
				PassParameters->FrameIndex = FrameIndex;

				PassParameters->NumOfTemporalFrames = WeightedLSRDesc.NumOfFrames;
				PassParameters->NumOfWeigthsPerPixelPerFrame = WeightedLSRDesc.NumOfWeightsPerPixel / WeightedLSRDesc.NumOfFrames;
			}
			
			SHADER::FPermutationDomain ComputeShaderPermutationVector;
			{
				ComputeShaderPermutationVector.Set<SHADER::FDimReconstructionType>(ReconstructionType);
				ComputeShaderPermutationVector.Set<SHADER::FDimPreAlbedoDivide>(GetPreAlbedoDivideRecoverPhase());
				ComputeShaderPermutationVector.Set<SHADER::FDimNumFeature>(BDim.X);
			}

			TShaderMapRef<SHADER> ComputeShader(GetGlobalShaderMap(GMaxRHIFeatureLevel), ComputeShaderPermutationVector);

			FComputeShaderUtils::AddPass(
				GraphBuilder,
				RDG_EVENT_NAME("NFOR::Reconstruction(T=%d,%s)",
					FrameIndex,
					SHADER::GetEventName(ReconstructionType)),
				ERDGPassFlags::Compute,
				ComputeShader,
				PassParameters,
				FComputeShaderUtils::GetGroupCount(FIntPoint(WeightedLSRDesc.Width, WeightedLSRDesc.Height), NON_LOCAL_MEAN_THREAD_GROUP_SIZE));
		}

		if (ReconstructionBuffer)
		{
			typedef FAccumulateBufferToTextureCS SHADER;
			SHADER::FParameters* PassParameters = GraphBuilder.AllocParameters<SHADER::FParameters>();
			{
				PassParameters->StructuredBufferSource = GraphBuilder.CreateSRV(FRDGBufferSRVDesc(ReconstructionBuffer));
				PassParameters->ReconstructBuffer64 = GraphBuilder.CreateSRV(ReconstructionBuffer64);
				PassParameters->RWTarget = GraphBuilder.CreateUAV(FilteredRadiance);
				PassParameters->TextureSize = TextureSize;
			}

			SHADER::FPermutationDomain ComputeShaderPermutationVector;
			{
				ComputeShaderPermutationVector.Set<SHADER::FDimPreAlbedoDivide>(GetPreAlbedoDivideRecoverPhase());
			}

			TShaderMapRef<SHADER> ComputeShader(GetGlobalShaderMap(GMaxRHIFeatureLevel), ComputeShaderPermutationVector);

			FComputeShaderUtils::AddPass(
				GraphBuilder,
				RDG_EVENT_NAME("NFOR::AccumulateBufferToTexture(%dx%d)", TextureSize.X, TextureSize.Y),
				ERDGPassFlags::Compute,
				ComputeShader,
				PassParameters,
				FComputeShaderUtils::GetGroupCount(TextureSize, NON_LOCAL_MEAN_THREAD_GROUP_SIZE));
		}
	}

	void SolveWeightedLSR(
		FRDGBuilder& GraphBuilder,
		const FSceneView& View,
		const FRDGBufferRef& Feature,
		const FRDGTextureRef& Radiance,
		const FRDGBufferRef& NonLocalMeanWeightsBuffer,
		const FRDGTextureRef& FilteredRadiance,
		FWeightedLSRDesc WeightedLSRDesc,
		const FRDGBufferRef Radiances,
		const FRDGTextureRef& SourceAlbedo
	)
	{
		FIntPoint TextureSize = Radiance->Desc.Extent;
		

		if (GetRegressionDevice() == ERegressionDevice::CPU)
		{
			SolveWeightedLSRCPU(
				GraphBuilder,
				View,
				Feature,
				Radiance,
				NonLocalMeanWeightsBuffer,
				FilteredRadiance,
				WeightedLSRDesc,
				Radiances,
				SourceAlbedo);

			return;
		}

		checkf(WeightedLSRDesc.SolverType == EWeightedLSRSolverType::Tiled,TEXT("The weighted LSR solver should select tiled"));

		FIntPoint XDimension = FIntPoint(WeightedLSRDesc.NumOfWeightsPerPixel, WeightedLSRDesc.NumOfFeatureChannelsPerFrame);
		int PatchDistance = (FMath::Sqrt(float(WeightedLSRDesc.NumOfWeightsPerPixel / WeightedLSRDesc.NumOfFrames)) - 1) / 2;

		// 1. Process the data into A, B for Ax=B.
		FRDGBufferRef AMatrix = nullptr;
		FRDGBufferRef BMatrix = nullptr;
		{
			AMatrix = ApplyBatchedInPlaceMatrixMultiplication(
				GraphBuilder,
				Feature,
				XDimension,
				NonLocalMeanWeightsBuffer,
				WeightedLSRDesc.NumOfWeightsPerPixel,
				FIntPoint(WeightedLSRDesc.Width, WeightedLSRDesc.Height),
				PatchDistance,
				RegressionKernel::EWeightedMultiplicationType::Quadratic);

			BMatrix = ApplyBatchedInPlaceMatrixMultiplication(
				GraphBuilder,
				Feature,
				XDimension,
				NonLocalMeanWeightsBuffer,
				WeightedLSRDesc.NumOfWeightsPerPixel,
				FIntPoint(WeightedLSRDesc.Width, WeightedLSRDesc.Height),
				PatchDistance,
				RegressionKernel::EWeightedMultiplicationType::Generalized,
				Radiances,
				FIntPoint(WeightedLSRDesc.NumOfWeightsPerPixel, WeightedLSRDesc.NumOfRadianceChannelsPerFrame));
		}

		// 2. Solve the linear equation Ax=B.
		const bool bFeatureAddConstant = ShouldFeatureAddConstant();
		const int32 NumOfAdditionalFeatures = bFeatureAddConstant ? 1 : 0;
		const int32 NumOfElementsPerRow = WeightedLSRDesc.Width;
		const int32 NumOfElements = WeightedLSRDesc.Width * WeightedLSRDesc.Height;
		const int32 TotalNumOfFeaturesPerFrame = WeightedLSRDesc.NumOfFeatureChannelsPerFrame + NumOfAdditionalFeatures;

		FIntPoint BDim = FIntPoint(TotalNumOfFeaturesPerFrame, WeightedLSRDesc.NumOfRadianceChannelsPerFrame);
		FRDGBufferRef ReconstructionWeights = RegressionKernel::AllocateMatrixfBuffer(
			GraphBuilder, 
			NumOfElements,
			BDim.X,
			BDim.Y, TEXT("NFOR.WeightedLSR.ReconstructWeights"));

		if (GetLinearSolverDevice() == ELinearSolverDevice::CPU)
		{
			SolveLinearEquationCPU(
				GraphBuilder,
				AMatrix,
				BMatrix,
				NumOfElements,
				BDim,
				ReconstructionWeights);
		}
		else
		{
			typedef RegressionKernel::FLinearSolverCS SHADER;
			SHADER::FParameters* PassParameters = GraphBuilder.AllocParameters<SHADER::FParameters>();
			PassParameters->A = GraphBuilder.CreateSRV(FRDGBufferSRVDesc(AMatrix, PF_R32_FLOAT));
			PassParameters->ADim = FIntPoint(TotalNumOfFeaturesPerFrame, TotalNumOfFeaturesPerFrame);
			PassParameters->B = GraphBuilder.CreateSRV(FRDGBufferSRVDesc(BMatrix, PF_R32_FLOAT));
			PassParameters->BDim = BDim;
			PassParameters->Result = GraphBuilder.CreateUAV(FRDGBufferUAVDesc(ReconstructionWeights, PF_R32_FLOAT));

			PassParameters->NumOfElements = NumOfElements;
			PassParameters->NumOfElementsPerRow = NumOfElementsPerRow;

			SHADER::FPermutationDomain ComputeShaderPermutationVector;

			checkf(BDim.X >= 6 && BDim.X <= 8, TEXT("Number of features should be between 6 and 8"));
			ComputeShaderPermutationVector.Set<SHADER::FDimNumFeature>(BDim.X);

			TShaderMapRef<SHADER> ComputeShader(GetGlobalShaderMap(GMaxRHIFeatureLevel), ComputeShaderPermutationVector);

			FComputeShaderUtils::AddPass(
				GraphBuilder,
				RDG_EVENT_NAME("NFOR::BatchedLinearSolver(F=%d, C=%d)", BDim.X, BDim.Y),
				ERDGPassFlags::Compute,
				ComputeShader,
				PassParameters,
				FComputeShaderUtils::GetGroupCount(FIntPoint(WeightedLSRDesc.Width, WeightedLSRDesc.Height), NON_LOCAL_MEAN_THREAD_GROUP_SIZE));
		}

		// 3. Reconstruct 
		{
			AddClearUAVPass(GraphBuilder,GraphBuilder.CreateUAV(FilteredRadiance), 0.0f, ERDGPassFlags::Compute);

			const int32 ReconstructDebugFrameIndex = GetReconstructionDebugFrameIndex();
			const bool IsReconstructDebugEnabled = ReconstructDebugFrameIndex >= 0;

			for (int32 FrameIndex = 0; FrameIndex < WeightedLSRDesc.NumOfFrames; ++FrameIndex)
			{
				if (IsReconstructDebugEnabled)
				{
					FrameIndex = FMath::Min(ReconstructDebugFrameIndex, WeightedLSRDesc.NumOfFrames - 1);
				}

				RegressionKernel::FReconstructSpatialTemporalImage::EReconstructionType
					ReconstructionType = GetReconstructionType(FrameIndex, GetDenoisingFrameIndex(WeightedLSRDesc.NumOfFrames));
				
				ReconstructByFrame(
					GraphBuilder,
					Feature,
					ReconstructionWeights,
					NonLocalMeanWeightsBuffer,
					FilteredRadiance,
					SourceAlbedo,
					WeightedLSRDesc,
					FrameIndex,
					ReconstructionType);

				if (IsReconstructDebugEnabled)
				{
					break;
				}
			}
		}

		// Multiply albedo if the albedo recover phase is final.
		if (GetPreAlbedoDivideRecoverPhase()== EAlbedoDivideRecoverPhase::Final)
		{
			FIntPoint SourcePosition = WeightedLSRDesc.TileStartPosition - WeightedLSRDesc.Offset;
			AddMultiplyTextureRegionPass(GraphBuilder, SourceAlbedo, FilteredRadiance, true, SourcePosition, FIntPoint::ZeroValue, WeightedLSRDesc.TextureSize);
		}
	}

	int32 GetNumOfCombinedFeatureChannels(const TArray<FFeatureDesc>& FeatureDescs) {
		int NumOfChannels = 0;
		for (FFeatureDesc FeatureDesc : FeatureDescs)
		{
			if (FeatureDesc.Feature.Image)
			{
				NumOfChannels += FeatureDesc.Feature.ChannelCount;
			}
		}
		return NumOfChannels;
	};

	FRDGTextureRef CollaborativeRegression(
		FRDGBuilder& GraphBuilder,
		const FSceneView& View,
		const TArray<FRadianceDesc>& Radiances,
		const TArray<FFeatureDesc>& Features,
		const FNonLocalMeanParameters& RadianceNonLocalMeanParameters)
	{
		const int32 NumOfFeatures = Features.Num();
		const int32 NumOfRadiances = Radiances.Num();
		const int32 SourceIndex = GetDenoisingFrameIndex(NumOfRadiances); // The current denoising frame data index

		const int SearchingPatchSize = (RadianceNonLocalMeanParameters.PatchDistance * 2 + 1);
		const int NumberOfWeightsPerPixel = SearchingPatchSize * SearchingPatchSize;

		// TODO: adaptive tile size for best performance.
		const int32 NumOfTilesOneSide = GetNumOfTiles();
		FIntPoint NumOfTiles = FIntPoint(NumOfTilesOneSide, NumOfTilesOneSide);
		const int32 TotalTileCount = NumOfTilesOneSide * NumOfTilesOneSide;

		FIntPoint TextureSize = Radiances[0].Data.Image->Desc.Extent;
		FIntPoint TileSize = FMath::DivideAndRoundUp(TextureSize, NumOfTiles);
		FIntPoint PaddingTileOffset = RadianceNonLocalMeanParameters.PatchDistance;
		FIntPoint PaddedTileSize = TileSize + PaddingTileOffset * 2;
		FIntRect  PaddedTileRect = FIntRect(FIntPoint(0,0), PaddedTileSize);
		const int NumOfCombinedFeatureChannels = GetNumOfCombinedFeatureChannels(Features);

		const int32 NonLocalMeanSingleFrameWeightSize = TileSize.X * TileSize.Y * NumberOfWeightsPerPixel;
		const int32 BytesPerElement = sizeof(float);
		FRDGBufferDesc NonLocalMeanSingleFrameWeightsBufferDesc = 
			FRDGBufferDesc::CreateBufferDesc(BytesPerElement, NonLocalMeanSingleFrameWeightSize);
		FRDGBufferRef NonLocalMeanSingleFrameWeightsBuffer = GraphBuilder.CreateBuffer(NonLocalMeanSingleFrameWeightsBufferDesc, TEXT("NFOR.NLMSingleFrameWeightsBuffer"));
		
		FRDGBufferRef NonLocalMeanWeightsBuffer = nullptr;
		if (NumOfRadiances > 1)
		{
			FRDGBufferDesc NonLocalFrameWeightsBufferDesc =
				FRDGBufferDesc::CreateBufferDesc(BytesPerElement, NonLocalMeanSingleFrameWeightSize * NumOfRadiances);
			NonLocalMeanWeightsBuffer = GraphBuilder.CreateBuffer(NonLocalFrameWeightsBufferDesc, TEXT("NFOR.NLMWeightsBuffer"));
		}
		else
		{
			NonLocalMeanWeightsBuffer = NonLocalMeanSingleFrameWeightsBuffer;
		}

		FRDGBufferDesc CombinedFeatureDesc = FRDGBufferDesc::CreateBufferDesc(BytesPerElement, PaddedTileSize.X * PaddedTileSize.Y * NumOfCombinedFeatureChannels);
		FRDGBufferRef CombinedFeatures = GraphBuilder.CreateBuffer(CombinedFeatureDesc, TEXT("NFOR.CombinedFeatures"));

		const int32 NumOfCombinedRadianceChannels = GetNumOfCombinedFeatureChannels(Radiances);
		FRDGBufferDesc CombinedRadianceDesc = FRDGBufferDesc::CreateBufferDesc(BytesPerElement * 4, PaddedTileSize.X * PaddedTileSize.Y * NumOfRadiances);
		FRDGBufferRef CombinedRadiances = GraphBuilder.CreateBuffer(CombinedRadianceDesc, TEXT("NFOR.CombinedRadiances"));

		FRDGTextureDesc FilteredRadianceDesc = Radiances[SourceIndex].Data.Image->Desc;
		FilteredRadianceDesc.Flags |= TexCreate_RenderTargetable;
		FRDGTextureRef FilteredRadiance = GraphBuilder.CreateTexture(FilteredRadianceDesc, TEXT("NFOR.FilteredRadiance"));

		FRDGTextureDesc RadianceTileDesc = FilteredRadianceDesc;
		RadianceTileDesc.Extent = PaddedTileSize;
		FRDGTextureRef RadianceTileTexture = GraphBuilder.CreateTexture(RadianceTileDesc, TEXT("NFOR.RadianceTile"));
		FRDGTextureRef DenoisedTileTexture = GraphBuilder.CreateTexture(RadianceTileDesc, TEXT("NFOR.DenoisedRadianceTile"));
		
		AddClearUAVPass(GraphBuilder, GraphBuilder.CreateUAV(FilteredRadiance), FLinearColor::Transparent, ERDGPassFlags::Compute);
		
		auto GetTileIndex = [TotalTileCount, NumOfTilesOneSide](int32 Index)
		{
			// TODO: generate tiles first and then iterate on tiles?
			if (IsTileDebugEnabled())
			{
				if (GetTileDebugIndex() < 0)
				{
					Index = TotalTileCount / 2 - NumOfTilesOneSide / 2;
				}
				else
				{
					Index = GetTileDebugIndex();
				}
			}
			return Index;
		};

		//TODO: Each tile can be parallelized.
		for (int32 i = 0; i < TotalTileCount; ++i)
		{
			int32 TileIndex = GetTileIndex(i);

			FIntPoint TileStartPoint = FIntPoint(TileIndex % NumOfTiles.X, TileIndex / NumOfTiles.X) * TileSize;
			FIntRect TileRegion = FIntRect(FIntPoint(0), TileSize) + TileStartPoint;
			FIntRect  PaddedTileRegion = PaddedTileRect + TileStartPoint - PaddingTileOffset;

			// Get the weights W
			for (int32 RadianceId = 0; RadianceId < NumOfRadiances; ++RadianceId)
			{
				GetNLMWeights(
					GraphBuilder,
					View,
					Radiances[SourceIndex],
					Radiances[RadianceId],
					NonLocalMeanSingleFrameWeightsBuffer,
					TileRegion,
					RadianceNonLocalMeanParameters);

				if (NumOfRadiances > 1)
				{
					AddCopyBufferPass(GraphBuilder, NonLocalMeanWeightsBuffer, NonLocalMeanSingleFrameWeightSize * BytesPerElement * RadianceId, 
						NonLocalMeanSingleFrameWeightsBuffer, 0, NonLocalMeanSingleFrameWeightSize * BytesPerElement);
				}
			}

			// Get raw color Y
			{
				int32 BufferChannelOffset = 0;
				
				for (int32 RadianceId = 0; RadianceId < NumOfRadiances; ++RadianceId)
				{
					AddCopyMirroredTexturePass(GraphBuilder, Radiances[RadianceId].Data.Image, RadianceTileTexture, PaddedTileRegion.Min, FIntPoint::ZeroValue, PaddedTileSize);

					FNFORTextureDesc Texture = Radiances[RadianceId].Data;

					AddCopyTextureToBufferPass(GraphBuilder, Texture.Image, CombinedRadiances,
						Texture.ChannelOffset,
						Texture.ChannelCount,
						Texture.NumOfChannel,
						BufferChannelOffset,
						NumOfCombinedRadianceChannels,
						PaddedTileRegion);

					BufferChannelOffset += Texture.ChannelCount;
				}

				checkf(BufferChannelOffset == NumOfCombinedRadianceChannels, TEXT("Number of channels used by radiances does not match the channel count in the buffer."));
			}

			// Get the feature vector X
			{
				int32 BufferChannelOffset = 0;

				for (int32 FeatureId = 0; FeatureId < NumOfFeatures; ++FeatureId)
				{
					FNFORTextureDesc Texture = Features[FeatureId].Data;

					AddCopyTextureToBufferPass(GraphBuilder, Texture.Image, CombinedFeatures,
						Texture.ChannelOffset,
						Texture.ChannelCount,
						Texture.NumOfChannel,
						BufferChannelOffset,
						NumOfCombinedFeatureChannels,
						PaddedTileRegion);

					BufferChannelOffset += Texture.ChannelCount;
				}

				checkf(BufferChannelOffset == NumOfCombinedFeatureChannels, TEXT("Number of channels used by feature does not match the channel count in the buffer."));
			}

			// Solve the weighted LSR.
			{
				FWeightedLSRDesc WeightedLSRDesc;
				{
					WeightedLSRDesc.NumOfFeatureChannels = NumOfCombinedFeatureChannels;
					WeightedLSRDesc.NumOfFeatureChannelsPerFrame = NumOfCombinedFeatureChannels / NumOfRadiances;
					WeightedLSRDesc.NumOfWeightsPerPixel = NumberOfWeightsPerPixel * NumOfRadiances;
					WeightedLSRDesc.NumOfWeightsPerPixelPerFrame = NumberOfWeightsPerPixel;
					WeightedLSRDesc.NumOfRadianceChannels = NumOfCombinedRadianceChannels;
					WeightedLSRDesc.NumOfRadianceChannelsPerFrame = NumOfCombinedRadianceChannels / NumOfRadiances;

					WeightedLSRDesc.Width = TileSize.X;
					WeightedLSRDesc.Height = TileSize.Y;
					WeightedLSRDesc.Offset = PaddingTileOffset;
					WeightedLSRDesc.TileStartPosition = TileStartPoint;
					WeightedLSRDesc.NumOfFrames = NumOfRadiances;
					WeightedLSRDesc.TextureSize = RadianceTileTexture->Desc.Extent;
					WeightedLSRDesc.SolverType = EWeightedLSRSolverType::Tiled;
				}

				const int SourceAlbedoFeatureIndex = (NumOfFeatures / NumOfRadiances) * SourceIndex;

				SolveWeightedLSR(
					GraphBuilder,
					View,
					CombinedFeatures,
					RadianceTileTexture,
					NonLocalMeanWeightsBuffer,
					DenoisedTileTexture,
					WeightedLSRDesc,
					CombinedRadiances,
					Features[SourceAlbedoFeatureIndex].Data.Image);
			}

			// Copy back and accumulate.
			AddAccumulateTextureRegionPass(GraphBuilder, DenoisedTileTexture, FilteredRadiance, FIntPoint::ZeroValue, PaddedTileRegion.Min, PaddedTileSize);

			if (IsTileDebugEnabled())
			{
				break;
			}
		}

		// Normalize the image by weights stored in alpha channel.
		AddNormalizeTexturePass(GraphBuilder, FilteredRadiance);

		// TODO: denoise the alpha channel of the original radiance.
		// Pass through alpha channel from the source index.
		AddCopyMirroredTexturePass(GraphBuilder, Radiances[SourceIndex].Data.Image, FilteredRadiance, FIntPoint::ZeroValue, FIntPoint::ZeroValue, FIntPoint::ZeroValue,true/*bAlphaOnly*/);

		return FilteredRadiance;
	}

	//--------------------------------------------------------------------------------------------------------------------
	// Bandwidth selection

	FNFORTextureDesc MSEEstimation(
		FRDGBuilder& GraphBuilder,
		const FSceneView& View,
		const FRadianceDesc& Radiance,
		const FRDGTextureRef FilteredImage)
	{
		FIntPoint TextureSize = Radiance.Data.Image->Desc.Extent;
		FRDGTextureDesc Desc = Radiance.Variance.Image->Desc;
		Desc.Format = PF_R32_FLOAT;
		FRDGTextureRef MSE = GraphBuilder.CreateTexture(Desc, TEXT("NFOR.MSE"));
		NFORDenoise::FNFORTextureDesc NFORMSETexture(MSE, 0, 1, 1);

		typedef FMSEEstimationCS SHADER;
		SHADER::FParameters* PassParameters = GraphBuilder.AllocParameters<SHADER::FParameters>();
		{
			PassParameters->Variance = GraphBuilder.CreateSRV(FRDGTextureSRVDesc::Create(Radiance.Variance.Image));
			PassParameters->Image = GraphBuilder.CreateSRV(FRDGTextureSRVDesc::Create(Radiance.Data.Image));
			PassParameters->FilteredImage = GraphBuilder.CreateSRV(FRDGTextureSRVDesc::Create(FilteredImage));
			PassParameters->TextureSize = TextureSize;
			PassParameters->VarianceChannelOffset = Radiance.Variance.ChannelOffset;
			PassParameters->MSE = GraphBuilder.CreateUAV(NFORMSETexture.Image);
		}

		SHADER::FPermutationDomain ComputeShaderPermutationVector;
		ComputeShaderPermutationVector.Set<SHADER::FDimensionVarianceType>(Radiance.VarianceType);
		FGlobalShaderMap* GlobalShaderMap = GetGlobalShaderMap(GMaxRHIFeatureLevel);
		TShaderMapRef<SHADER> ComputeShader(GlobalShaderMap, ComputeShaderPermutationVector);

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("NFOR::MSEEstimation (Dim=%d,%d)", TextureSize.X, TextureSize.Y),
			ERDGPassFlags::Compute,
			ComputeShader,
			PassParameters,
			FComputeShaderUtils::GetGroupCount(TextureSize, NON_LOCAL_MEAN_THREAD_GROUP_SIZE));

		return NFORMSETexture;
	}

	//--------------------------------------------------------------------------------------------------------------------
	// NFOR filtering and denoising

	void FilterFeatures(
	FRDGBuilder& GraphBuilder,
	const FSceneView& View,
	const TArray<FFeatureDesc>& FeatureDescs)
	{
		
		FNonLocalMeanParameters FeatureNonLocalMeanParameters = GetFeatureNonLocalMeanParameters(0.5f);

		const int NumOfFeatures = FeatureDescs.Num();
		typedef FNonLocalMeanFilteringCS SHADER;

		int32 BufferChannelOffset = 0;
		for (int i = 0; i < NumOfFeatures; ++i)
		{
			if (FeatureDescs[i].Feature.Image == nullptr 
				|| FeatureDescs[i].bCleanFeature)
			{
				continue;
			}

			const FFeatureDesc& FeatureDesc = FeatureDescs[i];
			FRDGTextureRef Feature = FeatureDesc.Feature.Image;
			FRDGTextureRef FilterdFeature = GraphBuilder.CreateTexture(Feature->Desc, TEXT("NFOR.FilteredFeature"));

			ApplyNonLocalMeanFilter(
				GraphBuilder,
				FeatureNonLocalMeanParameters,
				FeatureDesc.Feature,
				FeatureDesc.Variance,
				FeatureDesc.VarianceType,
				FilterdFeature);

			AddCopyTexturePass(GraphBuilder, FilterdFeature, Feature);
		}
	}

	bool FilterMain(
		FRDGBuilder& GraphBuilder,
		const FSceneView& View,
		const TArray<FRadianceDesc>& Radiances,
		const TArray<FFeatureDesc>& FeatureDescs,
		const FRDGTextureRef& DenoisedRadiance
	)
	{
		// Frame 0, 1,...,n-1
		//         0: new frames with feature frame denoised with NLM.
		// 1,...,n-1: old frames with feature frame denoised with NLM.

		// Denoise for frame n/2. E.g., 
		//		when n=3, n_m=1,2nd frame is the current frame to denoise. 0, |1|, 2
		//		when n=5, n_m=2,3rd frame is the current frame to denoise. 0, 1, |2|, 3, 4
		// Special case when n_a < n
		//		n_a <= n/2: n_m=n_a
		//      n_a >  n/2: n_m=n/2
		// Since the frame size can be very large, we denoise tile by tile and resolve at last
		// 
		// Pseudo code:
		// 
		//  Preprocessing
		// 
		//	For each bandwidth:
		//		For each tile in tiles:
		//			collaborative regression(tile)
		//		denoised = recombine(tiles)
		// 
		//  Bandwidth Selection
  
		const int32 NumOfTemporalFrames = Radiances.Num();
		const int32 NumOfFeatures = FeatureDescs.Num();
		const int32 NumOfFeaturesPerFrame = NumOfFeatures / NumOfTemporalFrames;
		const int32 SourceRadianceIndex = GetDenoisingFrameIndex(NumOfTemporalFrames);
		
		// Preprocessing
		// Feature range adjustment, radiance normalization and filtering frames
		{
			// Latest frame only 
			for (int i = 0; i < 1; ++i)
			{
				const FFeatureDesc& Albedo = FeatureDescs[i * NumOfFeaturesPerFrame + 0]; //TODO: unify the index
				const FFeatureDesc& Normal = FeatureDescs[i * NumOfFeaturesPerFrame + 1]; //TODO: unify the index

				// Adjust feature range if required
				AddAdjustFeatureRangePass(GraphBuilder, Albedo, GetFeatureMaxAlbedoGrayscale());
				AddAdjustFeatureRangePass(GraphBuilder, Normal, GetFeatureMaxNormalLength());

				if (IsPreAlbedoDivideEnabled())
				{
					FRDGTextureRef AlbedoTex = Albedo.Feature.Image; 
					FRDGTextureRef NormalTex = Normal.Feature.Image; 
					FRDGTextureRef NormalVarianceTex = Normal.Variance.Image;
					FRDGTextureRef MaskTexture = NFORDenoise::GetPreAlbedoDivideMask(GraphBuilder, View, NormalTex, NormalVarianceTex);

					FLinearColor RGBOffset = NFORDenoise::GetPreAlbedoDivideAlbedoOffset();

					NFORDenoise::AddAccumulateConstantRegionPass(GraphBuilder, RGBOffset, AlbedoTex, MaskTexture);

					FRDGTextureRef RadianceTexture = Radiances[i].Data.Image;
					FRDGTextureRef RadianceVarianceTexture = Radiances[i].Variance.Image;

					// Normalization should apply to both texture and variance.
					NFORDenoise::AddDivideTextureRegionPass(GraphBuilder, AlbedoTex, RadianceTexture);
					NFORDenoise::AddNormalizeRadianceVariancePass(GraphBuilder, AlbedoTex, RadianceVarianceTexture);
				}
			}

			TArray<FFeatureDesc> LatestFrameFeature = TArray<FFeatureDesc>(FeatureDescs.GetData(), NumOfFeaturesPerFrame);
			NFORDenoise::FilterFeatures(GraphBuilder, View, LatestFrameFeature);
		}

		{	// Early out if radiance denoising is not required.
			EDenoiseFrameCountCondition Condition = GetFrameCountCoundition();

			if (Condition == EDenoiseFrameCountCondition::Equal && 
				SourceRadianceIndex == INDEX_NONE)
			{
				return false;
			}
		}

		TArray<FRDGTextureRef> FilteredImages = {};
		TArray<FNFORTextureDesc> FilteredMSEs = {};
		TArray<float> Bandwidths = GetBandwidthsConfiguration();

		const bool bPerformBandwidthSelection = IsBandwidthSelectionEnabled() && Bandwidths.Num() == 2;
		const FRadianceDesc& SourceRadiance = Radiances[SourceRadianceIndex];
		const int32 RadiancePatchSize = GetNonLocalMeanRadiancePatchSize();
		const int32 RadiancePatchDistance = GetNonLocalMeanRadiancePatchDistance();

		for (int i = 0; i < Bandwidths.Num(); ++i)
		{
			// Collaborative regression.
			FNonLocalMeanParameters RadianceNonLocalMeanParameters = GetNonLocalMeanParameters(RadiancePatchSize, RadiancePatchDistance, Bandwidths[i]);
			FRDGTextureRef FilteredImage = CollaborativeRegression(GraphBuilder, View, Radiances, FeatureDescs, RadianceNonLocalMeanParameters);

			FilteredImages.Add(FilteredImage);

			// MSE estimation and filtering.
			if (bPerformBandwidthSelection)
			{		
				FNFORTextureDesc MSE = MSEEstimation(GraphBuilder, View, SourceRadiance, FilteredImage);

				// NLM filtering of MSE texture.
				FRDGTextureRef FilteredMSETexure = GraphBuilder.CreateTexture(MSE.Image->Desc, TEXT("NFOR.FilteredMSE"));
				FNonLocalMeanParameters MSENonLocalMeanParameters = GetNonLocalMeanParameters(1, RadiancePatchDistance, 1.0f);

				ApplyNonLocalMeanFilter(
					GraphBuilder,
					MSENonLocalMeanParameters,
					MSE,
					SourceRadiance.Variance,
					SourceRadiance.VarianceType,
					FilteredMSETexure);

				FNFORTextureDesc FilterdMSE = MSE;
				FilterdMSE.Image = FilteredMSETexure;

				FilteredMSEs.Add(FilterdMSE);
			}
		}
		
		if (bPerformBandwidthSelection)
		{
			FGlobalShaderMap* GlobalShaderMap = GetGlobalShaderMap(GMaxRHIFeatureLevel);
			FRDGTextureDesc Desc = FilteredMSEs[0].Image->Desc;
			FNFORTextureDesc NFORSelectionMap = FilteredMSEs[0];
			NFORSelectionMap.Image = GraphBuilder.CreateTexture(Desc, TEXT("NFOR.SelectionMap"));
			FIntPoint TextureSize = Desc.Extent;
			{
				typedef FGenerateSelectionMapCS SHADER;
				SHADER::FParameters* PassParameters = GraphBuilder.AllocParameters<SHADER::FParameters>();
				{
					PassParameters->FilteredMSEs[0] = GraphBuilder.CreateSRV(FRDGTextureSRVDesc::Create(FilteredMSEs[0].Image));
					PassParameters->FilteredMSEs[1] = GraphBuilder.CreateSRV(FRDGTextureSRVDesc::Create(FilteredMSEs[1].Image));
					PassParameters->TextureSize = TextureSize;
					PassParameters->RWSelectionMap = GraphBuilder.CreateUAV(NFORSelectionMap.Image);
				}

				TShaderMapRef<SHADER> ComputeShader(GlobalShaderMap);

				FComputeShaderUtils::AddPass(
					GraphBuilder,
					RDG_EVENT_NAME("NFOR::GenerateSelectionMap (Dim=%d,%d)", TextureSize.X, TextureSize.Y),
					ERDGPassFlags::Compute,
					ComputeShader,
					PassParameters,
					FComputeShaderUtils::GetGroupCount(TextureSize, NON_LOCAL_MEAN_THREAD_GROUP_SIZE));
			}

			// Filter the selection map with image variance
			FRDGTextureRef FilteredSelectionMap = GraphBuilder.CreateTexture(Desc, TEXT("NFOR.FilteredSelectionMap"));
			{
				FNonLocalMeanParameters SelectionMapNonLocalMeanParameters = GetNonLocalMeanParameters(1, RadiancePatchDistance, 1.0f);

				ApplyNonLocalMeanFilter(
					GraphBuilder,
					SelectionMapNonLocalMeanParameters,
					NFORSelectionMap,
					SourceRadiance.Variance,
					SourceRadiance.VarianceType,
					FilteredSelectionMap);

				// Combine the filtered images.
				{
					typedef FCombineFilteredImageCS SHADER;
					SHADER::FParameters* PassParameters = GraphBuilder.AllocParameters<SHADER::FParameters>();
					{
						PassParameters->FilteredImages[0] = GraphBuilder.CreateSRV(FRDGTextureSRVDesc::Create(FilteredImages[0]));
						PassParameters->FilteredImages[1] = GraphBuilder.CreateSRV(FRDGTextureSRVDesc::Create(FilteredImages[1]));
						PassParameters->SelectionMap = GraphBuilder.CreateSRV(FRDGTextureSRVDesc::Create(FilteredSelectionMap));
						PassParameters->TextureSize = TextureSize;
						PassParameters->RWFilteredImage = GraphBuilder.CreateUAV(DenoisedRadiance);
					}

					TShaderMapRef<SHADER> ComputeShader(GlobalShaderMap);

					FComputeShaderUtils::AddPass(
						GraphBuilder,
						RDG_EVENT_NAME("NFOR::ApplySelectionMap (Dim=%d,%d)", TextureSize.X, TextureSize.Y),
						ERDGPassFlags::Compute,
						ComputeShader,
						PassParameters,
						FComputeShaderUtils::GetGroupCount(TextureSize, NON_LOCAL_MEAN_THREAD_GROUP_SIZE));
				}
			}

			// Second regression pass is ignored as there is only one buffer.
		}
		else
		{
			AddCopyTexturePass(GraphBuilder, FilteredImages[0], DenoisedRadiance);
		}

		return true;
	}
}
