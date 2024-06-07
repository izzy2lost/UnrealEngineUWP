// Copyright Epic Games, Inc. All Rights Reserved.

#include "NNERuntimeRDGPad.h"
#include "NNEHlslShadersPadCS.h"
#include "NNERuntimeRDGHlslHelper.h"
#include "Helper/NNERuntimeRDGOperatorHelper.h"
#include "NNETensor.h"
#include "NNETypes.h"

namespace UE::NNERuntimeRDG::Private::Hlsl
{
	DECLARE_GPU_STAT_NAMED(FNNEOperatorPad, TEXT("NNE.Operator.Hlsl.Pad"));

	using EPadMode = UE::NNEHlslShaders::Internal::EPadMode;

	/**
	 * Pad operator implementation
	 */
	template<int Version>
	class FPad : public FOperatorHlsl
	{
	public:

		FPad() {}
		virtual ~FPad() = default;

		TArray<int32> Pads;
		float Value = 0.0f;
		EPadMode Mode;

	public:

		virtual int PrepareOutputs(TConstArrayView<NNE::Internal::FTensorRef> InputTensors, TArrayView<NNE::Internal::FTensorRef> OutputTensors) override
		{
			check(Version >= 11 ? (InputTensors.Num() >= 2 && InputTensors.Num() <= 3 ) : (InputTensors.Num() == 1));
			check(OutputTensors.Num() == 1);

			const NNE::Internal::FTensor& X = *InputTensors[0];

			if constexpr (Version >= 11)
			{
				const NNE::Internal::FTensorRef PadsTensor = InputTensors[1];
				if(!PadsTensor->HasPreparedData())
				{
					UE_LOG(LogNNE, Warning, TEXT("Pad input 'pads' (name: %s) should be constant."), *PadsTensor->GetName());
					return -1;
				}

				OperatorHelper::GetInt32ArrayFromConstTensor(Pads, PadsTensor);

				if(InputTensors.Num() == 3)
				{
					const NNE::Internal::FTensor& ValueTensor = *InputTensors[2];

					if(!ValueTensor.HasPreparedData())
					{
						UE_LOG(LogNNE, Warning, TEXT("Pad input 'constant_value' (name: %s) should be constant."), *ValueTensor.GetName());
						return -1;
					}

					if(ValueTensor.GetPreparedData<float>().Num() != 1)
					{
						UE_LOG(LogNNE, Warning, TEXT("Pad input 'constant_value' (name: %s) should be scalar, however it is not."), *ValueTensor.GetName());
						return -1;
					}

					Value = ValueTensor.GetPreparedData<float>()[0];
				}
			}

			if ((2*X.GetShape().Rank()) != Pads.Num())
			{
				UE_LOG(LogNNE, Warning, TEXT("pads attribute lenght (%d) should be twice the rank of input X (%d)."), Pads.Num(), X.GetShape().Rank());
				return false;
			}

			TArray<uint32> OutputShapeData;
			for (int32 i = 0; i < X.GetShape().Rank(); ++i)
			{
				int32 PrePad = Pads[i];
				int32 PostPad = Pads[i + X.GetShape().Rank()];
				int32 OutputDim = PrePad + X.GetShape().GetData()[i] + PostPad;
				if (OutputDim < 1)
				{
					UE_LOG(LogNNE, Warning, TEXT("Pads cannot reduce dimension below 1, but would for tensor (name:%s) at rank %d of size %d with prepad %d and postpad %d."), *X.GetName(), i, X.GetShape().GetData()[i], PrePad, PostPad);
					return -1;
				}
				OutputShapeData.Emplace(OutputDim);
			}

			NNE::FTensorShape OutputShape = NNE::FTensorShape::Make(OutputShapeData);
			OutputTensors[0]->SetShape(OutputShape);

			return 0;
		};

		virtual bool Initialize(TConstArrayView<NNE::FTensorDesc> InputTensorDescs, TConstArrayView<NNE::FTensorDesc> OutputTensorDescs, const NNE::FAttributeMap& Attributes) override
		{
			using namespace UE::NNEHlslShaders::Internal;
			
			check(Version >= 11 ? (InputTensorDescs.Num() >= 2 && InputTensorDescs.Num() <= 3 ) : (InputTensorDescs.Num() == 1));
			check(OutputTensorDescs.Num() == 1);
			
			if constexpr (Version < 11)
			{
				Pads = Attributes.GetValue<TArray<int32>>(TEXT("pads"));
				Value = Attributes.GetValueOrDefault<float>(TEXT("value"), 0.0f);
			}
			FPadCS::LexFromString(Mode, *Attributes.GetValueOrDefault<FString>(TEXT("mode"), TEXT("constant")));

			return true;
		}

		virtual void Dispatch(FRDGBuilder& GraphBuilder, TConstArrayView<FTensorRDGRef> InputTensors, TConstArrayView<FTensorRDGRef> OutputTensors) override
		{
			using namespace UE::NNEHlslShaders::Internal;
			
			check(Version >= 11 ? (InputTensors.Num() >= 2 && InputTensors.Num() <= 3 ) : (InputTensors.Num() == 1));
			check(OutputTensors.Num() == 1);
			check(InputTensors[0] != nullptr);
			check(OutputTensors[0] != nullptr);
			const FTensorRDG& Input = *InputTensors[0];
			const FTensorRDG& Output = *OutputTensors[0];

			FRDGBufferSRVRef InputSRV = GraphBuilder.CreateSRV(FRDGBufferSRVDesc(Input.GetBuffer(), PF_R32_FLOAT));
			FRDGBufferUAVRef OutputUAV = GraphBuilder.CreateUAV(FRDGBufferUAVDesc(Output.GetBuffer(), PF_R32_FLOAT));

			FIntVector ThreadGroupCount = ComputeElementWiseThreadGroups(Output.GetVolume(), FPadConstants::NUM_GROUP_THREADS);

			// Set parameters
			FPadCS::FParameters* Params = GraphBuilder.AllocParameters<FPadCS::FParameters>();
			Params->Input = InputSRV;
			Params->Output = OutputUAV;
			FillTensorStrideShaderParameters(Input, Params->TensorInfo, 0);
			FillTensorStrideShaderParameters(Output, Params->TensorInfo, 1);
			FillTensorSizeShaderParameters(Input, Params->TensorInfo, 2);
			for (int32 i = 0; i < Input.GetShape().Rank(); ++i)
			{
				Params->TensorInfo[i][3] = BitCast<uint32>(Pads[i]); // Pre-pad encoded as uint32
			}
			Params->Value = Value;
			Params->Num = Output.GetVolume();
			Params->ThreadCountX = ThreadGroupCount.X * FPadConstants::NUM_GROUP_THREADS;

			FPadCS::FPermutationDomain PermutationVector;
			PermutationVector.Set<FPadCS::FPadMode>(Mode);
			PermutationVector.Set<FPadCS::FPadNumDimensions>(Output.GetShape().Rank());

			TShaderMapRef<FPadCS> ComputeShader(GetGlobalShaderMap(GMaxRHIFeatureLevel), PermutationVector);

			RDG_EVENT_SCOPE(GraphBuilder, "NNE.Operator.Hlsl.Pad");
			RDG_GPU_STAT_SCOPE(GraphBuilder, FNNEOperatorPad);

			FComputeShaderUtils::AddPass(
				GraphBuilder,
				RDG_EVENT_NAME("NNE.Operator.Hlsl.Pad.Dispatch"),
				ERDGPassFlags::Compute | ERDGPassFlags::NeverCull,
				ComputeShader,
				Params,
				ThreadGroupCount);
		}
	};

	template<int Version>
	bool ValidatePadOperator(const NNE::FAttributeMap& AttributeMap, TConstArrayView<ENNETensorDataType> InputTypes, TConstArrayView<NNE::FSymbolicTensorShape> InputShapes)
	{
		bool bIsValid = true;

		FAttributeValidator AttributeValidator;
		AttributeValidator.AddOptional(TEXT("mode"), ENNEAttributeDataType::String);
		if constexpr (Version < 11)
		{
			AttributeValidator.AddRequired(TEXT("pads"), ENNEAttributeDataType::Int32Array);
			AttributeValidator.AddOptional(TEXT("value"), ENNEAttributeDataType::Float);
		}
		bIsValid &= AttributeValidator.Validate(AttributeMap);
		
		FInputValidator InputValidator;
		InputValidator.SetTemplateCount(2);
		InputValidator.AddSupportedType(ENNETensorDataType::Float);
		InputValidator.AddSupportedType(ENNETensorDataType::Int64, /*TemplateIdx*/ 1);
		InputValidator.AddRequired();
		if constexpr (Version >= 11)
		{
			InputValidator.AddRequired(/*TemplateIdx*/ 1);
			InputValidator.AddOptional();
		}
		
		bIsValid &= InputValidator.Validate(InputTypes);

		return bIsValid;
	}

	template<int Version>
	FOperatorHlsl* CreatePadOperator()
	{
		return new FPad<Version>();
	}

	bool RegisterPadOperator(FOperatorRegistryHlsl& Registry)
	{
		// Note: support of a particular version is partial with respect to tensor data types (only the most typical ones are usually supported).
		#define OP(Version) \
		Registry.OpAdd({{TEXT("Pad"), TEXT("Onnx")}, Version}, CreatePadOperator<Version>, ValidatePadOperator<Version>);

		OP(2)
		OP(11)
		OP(13)

		#undef OP
		return true;
	}
} // UE::NNERuntimeRDG::Private::Hlsl
