// Copyright Epic Games, Inc. All Rights Reserved.

#include "NeuralPostProcessing.h"
#include "Interfaces/IPluginManager.h"
#include "NNE.h"
#include "NNEModelData.h"
#include "NNERuntimeRDG.h"
#include "Engine/NeuralProfile.h"
#include "NeuralPostProcessModelInstance.h"
#include "NeuralPostProcessingCS.h"
#include "PostProcess/NeuralPostProcessInterface.h"
#include "PixelShaderUtils.h"
#include "RenderGraphEvent.h"

#define LOCTEXT_NAMESPACE "FNeuralPostProcessingModule"

#if WITH_EDITOR
DEFINE_LOG_CATEGORY(LogNeuralPostProcessing);
#endif

namespace
{
	TAutoConsoleVariable<int32> CVarNeuralPostProcessApply(
		TEXT("r.neuralpostprocess.apply"),
		1,
		TEXT(" 0: disabled\n")
		TEXT(" 1: enabled (default)"),
		ECVF_RenderThreadSafe | ECVF_Scalability);

	DECLARE_GPU_STAT(NeuralPostProcessing)
}


/**
*
===============================================================================================================================
                    FNeuralPostProcessModelInstanceManager: Hold the mapping between neural profile and neural network models.
										/|\   /|\
										 | USE |
										 |     |
	|-ENGINE_API FNeuralProfileManager---|	   |
	|(INeuralProfileManager)			 |	   |
	|									 |	   |----RENDERING_API FNeuralPostProcess----|
	|									 |	   |	(INeuralPostProcessInterface)		|
	|	Manage the neural profile		 |	   |										|
	|									 |	   |										|
	|------------------------------------|	   |	Interface accessed by RDGBuilder	|
											   |	in post process material.			|
											   |----------------------------------------|
											   |    AllocateBuffer()                    |
											   |    Apply(): apply neural networks      |
===============================================================================================================================
*/

class FNeuralPostProcessModelInstanceManager
{
public:
	static FNeuralPostProcessModelInstanceManager* Get()
	{
		static FNeuralPostProcessModelInstanceManager Manager;
		return &Manager;
	}

	UNeuralPostProcessModelInstance* GetModelInstance(int32 ProfileId)
	{
		if (NeuralPostProcessModelInstances.Contains(ProfileId))
		{
			return NeuralPostProcessModelInstances[ProfileId];
		}
		else
		{
			return nullptr;
		}
	}

	void SetModelInstance(int32 ProfileId, UNeuralPostProcessModelInstance* Instance)
	{
		NeuralPostProcessModelInstances.Emplace(ProfileId, Instance);
	}

	UNeuralPostProcessModelInstance* Remove(int32 ProfileId)
	{
		return NeuralPostProcessModelInstances.FindAndRemoveChecked(ProfileId);
	}

	~FNeuralPostProcessModelInstanceManager()
	{
		NeuralPostProcessModelInstances.Reset();
	}
public:
	TMap<int32, UNeuralPostProcessModelInstance*> NeuralPostProcessModelInstances;
};

class FNeuralProfileManager : public NeuralProfile::INeuralProfileManager
{
public:
	FNeuralProfileManager() {}

	virtual void UpdateModel(int32 AllocationId, UObject* NNEModelData, FString RuntimeName) override
	{
		check(IsInRenderingThread())

		UNNEModelData* RawNNEModelData = Cast<UNNEModelData>(NNEModelData);

		if (!IsValid(RawNNEModelData))
		{
#if WITH_EDITOR
			UE_LOG(LogNeuralPostProcessing, Error, TEXT("NNEModelData is invalid at Slot %d."), AllocationId);
#endif
			return;
		}

		UNeuralPostProcessModelInstance* ModelInstance = NewObject<UNeuralPostProcessModelInstance>();
		ModelInstance->Update(RawNNEModelData, RuntimeName);

		FNeuralPostProcessModelInstanceManager::Get()->SetModelInstance(AllocationId, ModelInstance->IsValid()? ModelInstance : nullptr);
	}

	virtual void UpdateTileType(int32 AllocationId, ENeuralModelTileType ModelTileType) override
	{
		UNeuralPostProcessModelInstance* Instance = FNeuralPostProcessModelInstanceManager::Get()->GetModelInstance(AllocationId);
		Instance->UpdateModelTileType(ModelTileType);
	}

	virtual bool UpdateBatchSize(int32 AllocationId, int32 BatchSize) override
	{
		UNeuralPostProcessModelInstance* Instance = FNeuralPostProcessModelInstanceManager::Get()->GetModelInstance(AllocationId);
		return Instance->ModifyInputShape(0, BatchSize);
	}

	virtual void RemoveModel(int32 AllocationId) override
	{
		FNeuralPostProcessModelInstanceManager::Get()->Remove(AllocationId);
	}

	virtual FIntVector4 GetInputDimension(UObject* NNEModelData, FString RuntimeName) override
	{
		UE::NNE::FTensorShape TensorShape;
		FIntVector4 InputDimension = FIntVector4(-1, -1, -1, -1);

		if (UNNEModelData* ModelData = Cast<UNNEModelData>(NNEModelData))
		{
			// Need to create the ModelInstance in order to get the dimension in case the Model is not created
			// in the rendering thread.
			TSharedPtr<UE::NNE::IModelInstanceCPU> ModelInstance = CreateNNECpuModelInstance(ModelData);

			if (ModelInstance)
			{
				TConstArrayView<UE::NNE::FTensorDesc> InputTensorDescs = ModelInstance->GetInputTensorDescs();
				UE::NNE::FSymbolicTensorShape InputTensorShape = InputTensorDescs[0].GetShape();

				// Support only output dimension of rank 4
				checkf(InputTensorShape.Rank() == 4, TEXT("Neural Post Processing requires models with input shape N x channel x height x width!"));

				InputDimension = FIntVector4(
					InputTensorShape.GetData()[0],
					InputTensorShape.GetData()[1],
					InputTensorShape.GetData()[2],
					InputTensorShape.GetData()[3]);
			}
			ModelInstance.Reset();
		}

		return InputDimension;
	}

	virtual FIntVector4 GetOutputDimension(UObject* NNEModelData, FString RuntimeName) override
	{
		UE::NNE::FTensorShape TensorShape;
		FIntVector4 OutputDimension = FIntVector4(-1,-1,-1,-1);

		if (UNNEModelData* ModelData = Cast<UNNEModelData>(NNEModelData))
		{
			TSharedPtr<UE::NNE::IModelInstanceCPU> ModelInstance = CreateNNECpuModelInstance(ModelData);
			
			if (ModelInstance)
			{
				TConstArrayView<UE::NNE::FTensorDesc> OutputTensorDescs = ModelInstance->GetOutputTensorDescs();
				UE::NNE::FSymbolicTensorShape OutputTensorShape = OutputTensorDescs[0].GetShape();

				checkf(OutputTensorShape.Rank() == 4, TEXT("Neural Post Processing requires models with output shape N x channel x height x width!"));

				OutputDimension = FIntVector4(
					OutputTensorShape.GetData()[0],
					OutputTensorShape.GetData()[1],
					OutputTensorShape.GetData()[2],
					OutputTensorShape.GetData()[3]);
			}

			ModelInstance.Reset();
		}
		return OutputDimension;
	}

	~FNeuralProfileManager() {}
};


int32 GetTotalModelTileCount(ENeuralModelTileType ModelTileSize)
{
	int TotalTileCount = 1;

	//@TODO: uncomment and implement the corresponding function.
	switch (ModelTileSize)
	{
	case ENeuralModelTileType::TwoByTwo:
		TotalTileCount = 2 * 2;
		break;
	case ENeuralModelTileType::FourByFour:
		TotalTileCount = 4 * 4;
		break;
	case ENeuralModelTileType::EightByEight:
		TotalTileCount = 8 * 8;
		break;
	//case ENeuralModelTileType::Auto:
	//	TotalTileCount = -1;	// determined at runtime by the viewport size and W,H dimension of the network 
	//	break;
	case ENeuralModelTileType::OneByOne:
	default:
		break;
	}

	return TotalTileCount;
}

static void AllocateInputBuffer_RenderingThread(
	FRDGBuilder& GraphBuilder,
	const FScreenPassTextureViewport& Viewport,
	int32 ProfileId,
	FRDGBufferRef& InputNeuralBuffer,
	FVector4f& InputBufferDimension)
{
	check(IsInRenderingThread())
		check(ProfileId >= 0);

	UNeuralPostProcessModelInstance* Model = FNeuralPostProcessModelInstanceManager::Get()->GetModelInstance(ProfileId);

	if (!IsValid(Model))
	{
		InputNeuralBuffer = nullptr;
		return;
	}

	UE::NNE::FTensorShape InputShape = Model->GetResolvedInputTensorShape();
	InputBufferDimension = FVector4f(InputShape.GetData()[0], InputShape.GetData()[1], InputShape.GetData()[2], InputShape.GetData()[3]);
	
	// Calculate the number of tiles
	int NumTiles = 1;
	{
		ENeuralModelTileType ModelTileSize = Model->GetModelTileType();
		// @todo: Finish the implement of auto
		//if (ModelTileSize == ENeuralModelTileType::Auto)
		//{
		//	// allocate the number of tiles based on the size of the viewport and the size of the buffer.
		//	FScreenPassTextureViewportParameters ViewportParameters = GetScreenPassTextureViewportParameters(Viewport);
		//	FVector2f TileSizeWH = FMath::DivideAndRoundUp(ViewportParameters.ViewportSize, FVector2f(InputBufferDimension.W, InputBufferDimension.Z));
		//	float BatchDim = InputBufferDimension.X;
		//	NumTiles = FMath::DivideAndRoundUp(TileSizeWH.X * TileSizeWH.Y, BatchDim);
		//}
		//else
		{
			NumTiles = GetTotalModelTileCount(ModelTileSize);
		}

		Model->UpdateTileSize(NumTiles);
	}

	Model->CreateRDGBuffersIfNeeded(GraphBuilder, true);

	// Output the buffer and dimension for use.
	InputNeuralBuffer = Model->GetTiledInputBuffer();
	InputBufferDimension.X *= NumTiles;
}

static void	ApplyNeuralNetworks_RenderingThread(
	FRDGBuilder& GraphBuilder,
	int32 ProfileId,
	FRDGTextureRef NeuralTexture,
	FIntRect ViewRect,
	FRDGBufferRef InputSourceType,
	FRDGBufferRef& OutputNeuralBuffer,
	FVector4f& BufferDimension)
{
	check(IsInRenderingThread())
		check(ProfileId >= 0);

	UNeuralPostProcessModelInstance* Model = FNeuralPostProcessModelInstanceManager::Get()->GetModelInstance(ProfileId);

	if (!IsValid(Model))
	{
		return;
	}

	RDG_GPU_STAT_SCOPE(GraphBuilder, NeuralPostProcessing);

	FIntPoint TextureSize = NeuralTexture->Desc.Extent;
	FGlobalShaderMap* GlobalShaderMap = GetGlobalShaderMap(GMaxRHIFeatureLevel);
	float Scale = 1.0;
	
	const FScreenPassTextureViewport NeuralPostProcessViewport(NeuralTexture, ViewRect);
	const FScreenPassTextureViewportParameters InputViewportParameters = GetScreenPassTextureViewportParameters(NeuralPostProcessViewport);

	// 1. Preprocess the input data by copying from Texture to buffer if required
	{
		// Build the indirect dispatch parameters
		FRDGBufferRef IndirectDispatchBuffer = GraphBuilder.CreateBuffer(
			FRDGBufferDesc::CreateIndirectDesc<FRHIDispatchIndirectParameters>(1), TEXT("NeuralPostProcessing.IndirectDispatchBuffer"));

		UE::NNE::FTensorShape InputShape = Model->GetResolvedInputTensorShape();
		FIntPoint NeuralNetworkInputSize = { (int32)InputShape.GetData()[3], (int32)InputShape.GetData()[2] };// Width, height
		int32 TileSize = Model->GetTileSize();

		// Build the indirect dispatch parameters with InputSourceType
		{
			typedef FNeuralPostProcessingBuildIndirectDispatchArgsCS ARGSETUPSHADER;
			ARGSETUPSHADER::FParameters* PassParameters = GraphBuilder.AllocParameters<ARGSETUPSHADER::FParameters>();
			PassParameters->NetworkTextureSize = NeuralNetworkInputSize;
			PassParameters->SourceType = GraphBuilder.CreateSRV(InputSourceType, EPixelFormat::PF_R32_UINT);
			PassParameters->RWIndirectDispatchArgsBuffer = GraphBuilder.CreateUAV(IndirectDispatchBuffer, EPixelFormat::PF_R32_UINT);
			TShaderMapRef<ARGSETUPSHADER> ComputeShader(GlobalShaderMap);
			FComputeShaderUtils::AddPass(GraphBuilder, FRDGEventName(TEXT("NeuralPostProcessing::BuildIndirectArgs(Dispatch)")), ComputeShader, PassParameters, FIntVector(1,1,1));
		}

		// Use the neural network's input dimension configuration if available, otherwise use the dynamic size of the buffer
		if (TileSize == 1)
		{
			typedef FNeuralPostProcessingPrepareInputCS SHADER;
			SHADER::FParameters* PassParameters = GraphBuilder.AllocParameters<SHADER::FParameters>();
			{
				PassParameters->Input0 = GetNeuralPostProcessInput(NeuralTexture, InputViewportParameters);
				PassParameters->InputTextureSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
				PassParameters->InputBufferWidth = NeuralNetworkInputSize.X;
				PassParameters->InputBufferHeight = NeuralNetworkInputSize.Y;
				PassParameters->InputBuffer = GraphBuilder.CreateUAV(FRDGBufferUAVDesc(Model->GetInputBuffer(), PF_R32_FLOAT));
				PassParameters->ColorScale = Scale;
				PassParameters->IndirectDispatchArgsBuffer = IndirectDispatchBuffer;
			}

			TShaderMapRef<FNeuralPostProcessingPrepareInputCS> ComputeShader(GlobalShaderMap);
			FComputeShaderUtils::AddPass(
				GraphBuilder,
				RDG_EVENT_NAME("NeuralPostProcessing::PrepareInput"),
				ERDGPassFlags::Compute, //| ERDGPassFlags::NeverCull
				ComputeShader,
				PassParameters,
				PassParameters->IndirectDispatchArgsBuffer,0);
		}
		else
		{
			// @TODO: Implement texture copy when neural network buffer is accessed by texture index.
		}
	}

	// 2. Run the network
	const bool bNeuralPostProcessApply = CVarNeuralPostProcessApply.GetValueOnRenderThread() > 0;
	if (bNeuralPostProcessApply)
	{
		Model->Execute(GraphBuilder);
	}

	// 3. Pass the output from the neural network and fill the scene color texture
	// when batch and channel dimension matches between the input and output buffer.

	auto ShouldUpdateNeuralTexture = [&]()->bool {
		UE::NNE::FTensorShape OutputShape = Model->GetResolvedOutputTensorShape();
		UE::NNE::FTensorShape InputShape = Model->GetResolvedInputTensorShape();

		return OutputShape.GetData()[0] == InputShape.GetData()[0] &&
			OutputShape.GetData()[1] == InputShape.GetData()[1] &&
			Model->GetTileSize() == 1; //@TODO: add texture copy when tile size is larger than batch size. 
	};

	if (ShouldUpdateNeuralTexture())
	{
		FRDGBufferRef OutputBuffer = bNeuralPostProcessApply ? Model->GetOutputBuffer() : Model->GetInputBuffer();
		FRDGBufferUAVRef OutputBufferUAV = GraphBuilder.CreateUAV(FRDGBufferUAVDesc(OutputBuffer, PF_R32_FLOAT));

		UE::NNE::FTensorShape OutputShape = bNeuralPostProcessApply ? Model->GetResolvedOutputTensorShape() : Model->GetResolvedInputTensorShape();
		FIntPoint NeuralNetworkOutputSize = { (int32)OutputShape.GetData()[3], (int32)OutputShape.GetData()[2] };

		const FIntRect Viewport = FIntRect(0,TextureSize);

		FNeuralPostProcessingProcessOutputPS::FParameters* ProcessOutputParameters = GraphBuilder.AllocParameters<FNeuralPostProcessingProcessOutputPS::FParameters>();
		ProcessOutputParameters->Input0.Texture = nullptr;
		ProcessOutputParameters->Input0.Viewport = InputViewportParameters;
		ProcessOutputParameters->OutputBufferWidth = NeuralNetworkOutputSize.X;
		ProcessOutputParameters->OutputBufferHeight = NeuralNetworkOutputSize.Y;
		ProcessOutputParameters->OutputBuffer = OutputBufferUAV;
		ProcessOutputParameters->ColorScale = Scale;
		ProcessOutputParameters->RenderTargets[0] = FRenderTargetBinding(NeuralTexture, ERenderTargetLoadAction::ELoad);

		TShaderMapRef<FNeuralPostProcessingProcessOutputPS> WriteOutputShader(GlobalShaderMap);

		FPixelShaderUtils::AddFullscreenPass(
			GraphBuilder,
			GlobalShaderMap,
			RDG_EVENT_NAME("NeuralPostProcessing::ProcessOutput"),
			WriteOutputShader,
			ProcessOutputParameters,
			ViewRect);
	}

	// 4. Read back the buffer, so the user can directly decode in the post process material.
	OutputNeuralBuffer = bNeuralPostProcessApply ? Model->GetTiledOutputBuffer() : Model->GetTiledInputBuffer();
	UE::NNE::FTensorShape OutputShape = bNeuralPostProcessApply ? Model->GetResolvedOutputTensorShape() : Model->GetResolvedInputTensorShape();		
	BufferDimension = FVector4f(OutputShape.GetData()[0], OutputShape.GetData()[1], OutputShape.GetData()[2], OutputShape.GetData()[3]);
	BufferDimension.X *= Model->GetTileSize();
}

class FNeuralPostProcess : public INeuralPostProcessInterface
{
public:
	virtual void Apply(FRDGBuilder& GraphBuilder, int32 NeuralProfileId,
		FRDGTexture* NeuralTexture, FIntRect ViewRect, FRDGBufferRef InputSourceType,
		FRDGBufferRef& OutputNeuralBuffer, FVector4f& BufferDimension) override
	{
		ApplyNeuralNetworks_RenderingThread(
			GraphBuilder,
			NeuralProfileId,
			NeuralTexture,
			ViewRect,
			InputSourceType,
			OutputNeuralBuffer,
			BufferDimension);
	}

	virtual void AllocateBuffer(FRDGBuilder& GraphBuilder, const FScreenPassTextureViewport& Viewport,
		int32 NeuralProfileId, FRDGBufferRef& InputNeuralBuffer, FVector4f& InputBufferDimension) override
	{
		AllocateInputBuffer_RenderingThread(
			GraphBuilder, 
			Viewport, 
			NeuralProfileId, 
			InputNeuralBuffer, 
			InputBufferDimension);
	}
};

void FNeuralPostProcessingModule::StartupModule()
{
#if WITH_EDITOR
	UE_LOG(LogNeuralPostProcessing, Log, TEXT("NeuralPostProcessing starting up"));
#endif

	const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("NeuralRendering"));
	if (Plugin.IsValid())
	{
		FString ModuleDir = Plugin->GetBaseDir() + TEXT("/Source/NeuralPostProcessing");
		AddShaderSourceDirectoryMapping(TEXT("/NeuralRendering"), FPaths::Combine(ModuleDir, TEXT("Shaders")));
	}
	else
	{
#if WITH_EDITOR
		UE_LOG(LogNeuralPostProcessing, Error, TEXT("Shaders directory not added. Failed to find NeuralPostProcessing plugin"));
#endif
	}

	GNeuralProfileManager.Reset(new FNeuralProfileManager());
	GNeuralPostProcess.Reset(new FNeuralPostProcess());
}

void FNeuralPostProcessingModule::ShutdownModule()
{
#if WITH_EDITOR
	UE_LOG(LogNeuralPostProcessing, Log, TEXT("NeuralPostProcessing function shutting down"));
#endif

	GNeuralProfileManager.Reset(nullptr);
	GNeuralPostProcess.Reset(nullptr);
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FNeuralPostProcessingModule, NeuralPostProcessing)