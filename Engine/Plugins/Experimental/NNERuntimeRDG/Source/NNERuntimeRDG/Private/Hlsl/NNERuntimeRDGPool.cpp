// Copyright Epic Games, Inc. All Rights Reserved.

#include "NNERuntimeRDGPool.h"
#include "NNEHlslShadersConvCS.h"
#include "NNEHlslShadersPoolCS.h"
#include "NNERuntimeRDGHlslHelper.h"
#include "NNETensor.h"
#include "NNETypes.h"
#include "RenderGraphUtils.h"


namespace UE::NNERuntimeRDG::Private::Hlsl
{
	DECLARE_GPU_STAT_NAMED(FNNEOperatorMaxPool, TEXT("NNE.Operator.Hlsl.MaxPool"));

	/**
	 * MaxPool operator implementation
	 */
	class FMaxPool : public FOperatorHlsl
	{
	public:

		FMaxPool() {}
		virtual ~FMaxPool() = default;

		int32 NumSpatialDimensions = 0;
		NNEHlslShaders::Internal::EConvAutoPad AutoPad = NNEHlslShaders::Internal::EConvAutoPad::NOTSET;
		TArray<int32> Pads;
		TArray<int32> Strides;
		TArray<int32> KernelShape;

	public:

		virtual int PrepareOutputs(TConstArrayView<NNE::Internal::FTensorRef> InputTensors, TArrayView<NNE::Internal::FTensorRef> OutputTensors) const override
		{
			check(InputTensors.Num() == 1);
			check(OutputTensors.Num() == 1);

			const NNE::Internal::FTensor& X = *InputTensors[0];
			TConstArrayView<uint32> InputShape = X.GetShape().GetData();
			TArray<uint32, TInlineAllocator<NNE::FTensorShape::MaxRank>> OutputShape;

			check(Pads.Num() == 2*NumSpatialDimensions);
			check(Strides.Num() == NumSpatialDimensions);
			check(KernelShape.Num() == NumSpatialDimensions);

			OutputShape.SetNumUninitialized(InputShape.Num());
			OutputShape[0] = InputShape[0];
			OutputShape[1] = InputShape[1];
			for (int32 i = 0; i < NumSpatialDimensions; ++i)
			{
				//See https://github.com/onnx/onnx/blob/main/docs/Changelog.md#MaxPool-8
				switch (AutoPad)
				{
					case NNEHlslShaders::Internal::EConvAutoPad::NOTSET:
					{
						uint32 PadShape = Pads[i] + Pads[i+NumSpatialDimensions];
						OutputShape[i + 2] = (uint32)FMath::FloorToFloat((float)(InputShape[i + 2] + PadShape - KernelShape[i]) / (float)Strides[i] + 1.0f);
						break;
					}
					case NNEHlslShaders::Internal::EConvAutoPad::VALID:
					{
						OutputShape[i + 2] = (uint32)FMath::CeilToFloat((float)(InputShape[i + 2] - KernelShape[i] + 1) / (float)Strides[i]);
						break;
					}
					case NNEHlslShaders::Internal::EConvAutoPad::SAME_UPPER:
					case NNEHlslShaders::Internal::EConvAutoPad::SAME_LOWER:
					{
						OutputShape[i + 2] = (uint32)FMath::CeilToFloat((float)InputShape[i + 2] / (float)Strides[i]);
						break;
					}
				}
			}
			OutputTensors[0]->SetShape(NNE::FTensorShape::Make(OutputShape));

			return 0;
		};

		virtual bool Initialize(TConstArrayView<NNE::FTensorDesc> InputTensorDescs, TConstArrayView<NNE::FTensorDesc> OutputTensorDescs, const NNE::FAttributeMap& Attributes) override
		{
			check(InputTensorDescs.Num() == 1);
			check(OutputTensorDescs.Num() >= 1);

			if (OutputTensorDescs.Num() > 1)
			{
				UE_LOG(LogNNE, Warning, TEXT("MaxPool 2nd optional output 'Indices' is not supported."));
				return false;
			}

			const NNE::FTensorDesc& Input = InputTensorDescs[0];
			const NNE::FTensorDesc& Output = OutputTensorDescs[0];

			if (Input.GetShape().Rank() < 3)
			{
				UE_LOG(LogNNE, Warning, TEXT("MaxPool input should be at least of rank 3, to have 1+ spatial dimension(s) but is of rank %d"), Input.GetShape().Rank());
				return false;
			}
			NumSpatialDimensions = Input.GetShape().Rank() - 2;

			TArray<int32> StridesDefault;
			TArray<int32> PadsDefault;
			StridesDefault.Init(1, NumSpatialDimensions);
			PadsDefault.Init(0, 2 * NumSpatialDimensions);

			NNEHlslShaders::Internal::FConvCS::LexFromString(AutoPad, *Attributes.GetValueOrDefault<FString>(TEXT("auto_pad"), TEXT("NOTSET")));
			Pads = Attributes.GetValueOrDefault<TArray<int32>>(TEXT("pads"), PadsDefault);
			Strides = Attributes.GetValueOrDefault<TArray<int32>>(TEXT("strides"), StridesDefault);
			KernelShape = Attributes.GetValue<TArray<int32>>(TEXT("kernel_shape"));

			if (KernelShape.Num() != NumSpatialDimensions)
			{
				UE_LOG(LogNNE, Warning, TEXT("MaxPool KernelShape should have as many elements as the spatial dimensions of the input, got %d while input have %d."), KernelShape.Num(), NumSpatialDimensions);
				return false;
			}
			if (Strides.Num() != NumSpatialDimensions)
			{
				UE_LOG(LogNNE, Warning, TEXT("MaxPool Strides should have as many elements as the spatial dimensions of the input, got %d while input have %d."), Strides.Num(), NumSpatialDimensions);
				return false;
			}
			if (Pads.Num() != 2*NumSpatialDimensions)
			{
				UE_LOG(LogNNE, Warning, TEXT("MaxPool Pads should have twice as many elements as the spatial dimensions of the input, got %d while input have %d."), Pads.Num(), NumSpatialDimensions);
				return false;
			}

			return true;
		}

		virtual void Dispatch(FRDGBuilder& GraphBuilder, TConstArrayView<FTensorRDGRef> InputTensors, TConstArrayView<FTensorRDGRef> OutputTensors) override
		{
			using namespace UE::NNEHlslShaders::Internal;

			check(InputTensors.Num() == 1);
			check(OutputTensors.Num() == 1);
			check(InputTensors[0] != nullptr);
			check(OutputTensors[0] != nullptr);

			const FTensorRDG& Input = *InputTensors[0];
			const FTensorRDG& Output = *OutputTensors[0];
			const FRDGBufferSRVRef InputSRV = GraphBuilder.CreateSRV(FRDGBufferSRVDesc(Input.GetBuffer(), PF_R32_FLOAT));
			const FRDGBufferUAVRef OutputUAV = GraphBuilder.CreateUAV(FRDGBufferUAVDesc(Output.GetBuffer(), PF_R32_FLOAT));
			const FIntVector ThreadGroupCount = ComputeElementWiseThreadGroups(Output.GetVolume(), FPoolConstants::NUM_GROUP_THREADS);

			// Set parameters
			FPoolCS::FParameters* Params = GraphBuilder.AllocParameters<FPoolCS::FParameters>();
			Params->Input = InputSRV;
			Params->Output = OutputUAV;
			Params->Num = Output.GetVolume();
			Params->ThreadCountX = ThreadGroupCount.X * FPoolConstants::NUM_GROUP_THREADS;
			FillTensorStrideShaderParameters(Output, Params->TensorInfo, 0);
			FillTensorStrideShaderParameters(Input, Params->TensorInfo, 1);
			FillTensorSizeShaderParameters(Input, Params->TensorInfo, 2);
			for (int32 i = 0; i < NumSpatialDimensions; ++i)
			{
				Params->SpatialInfo[i][0] = Strides[i];
				Params->SpatialInfo[i][1] = KernelShape[i];
				if (AutoPad == NNEHlslShaders::Internal::EConvAutoPad::SAME_LOWER)
				{
					// see https://github.com/onnx/onnx/blob/main/docs/Changelog.md#MaxPool-8
					// only needed for SAME_LOWER as SAME_UPPER is handled by index clamping in the HLSL kernel
					Params->SpatialInfo[i][2] = (Output.GetShape().GetData()[i + 2] - 1) * Strides[i] + KernelShape[i] - Input.GetShape().GetData()[i + 2];
				}
				else
				{
					Params->SpatialInfo[i][2] = Pads[i];
				}
			}

			FPoolCS::FPermutationDomain PermutationVector;
			PermutationVector.Set<FPoolCS::FPoolNumSpatialDimensions>(NumSpatialDimensions);

			TShaderMapRef<FPoolCS> ComputeShader(GetGlobalShaderMap(GMaxRHIFeatureLevel), PermutationVector);

			RDG_EVENT_SCOPE(GraphBuilder, "NNE.Operator.Hlsl.MaxPool");
			RDG_GPU_STAT_SCOPE(GraphBuilder, FNNEOperatorMaxPool);

			FComputeShaderUtils::AddPass(
				GraphBuilder,
				RDG_EVENT_NAME("NNE.Operator.Hlsl.MaxPool.Dispatch"),
				ERDGPassFlags::Compute | ERDGPassFlags::NeverCull,
				ComputeShader,
				Params,
				ThreadGroupCount);
		}
	};

	bool ValidateMaxPoolOperator(const NNE::FAttributeMap& AttributeMap, TConstArrayView<ENNETensorDataType> InputTypes, TConstArrayView<NNE::FSymbolicTensorShape> InputPools)
	{
		//This match version 8 of the MaxPool operator, next version is 10
		//https://github.com/onnx/onnx/blob/main/docs/Operators.md#MaxPool
		bool bIsValid = true;

		FAttributeValidator AttributeValidator;
		AttributeValidator.AddOptional(TEXT("auto_pad"), ENNEAttributeDataType::String);
		AttributeValidator.AddRequired(TEXT("kernel_shape"), ENNEAttributeDataType::Int32Array);
		AttributeValidator.AddOptional(TEXT("pads"), ENNEAttributeDataType::Int32Array);
		AttributeValidator.AddOptional(TEXT("storage_order"), ENNEAttributeDataType::Int32);//Unused, only needed for 2nd output itself not supported, see https://github.com/onnx/onnx/issues/1370
		AttributeValidator.AddOptional(TEXT("strides"), ENNEAttributeDataType::Int32Array);
		bIsValid &= AttributeValidator.Validate(AttributeMap);

		FInputValidator InputValidator;
		InputValidator.AddSupportedType(ENNETensorDataType::Float);
		InputValidator.AddRequired();
		bIsValid &= InputValidator.Validate(InputTypes);

		return bIsValid;
	}

	FOperatorHlsl* CreateMaxPoolOperator()
	{
		return new FMaxPool();
	}

	bool RegisterPoolOperators(FOperatorRegistryHlsl& Registry)
	{
		Registry.OpAdd({{TEXT("MaxPool"), TEXT("Onnx")}}, CreateMaxPoolOperator, ValidateMaxPoolOperator);
		return true;
	}
} // UE::NNERuntimeRDG::Private::Hlsl
