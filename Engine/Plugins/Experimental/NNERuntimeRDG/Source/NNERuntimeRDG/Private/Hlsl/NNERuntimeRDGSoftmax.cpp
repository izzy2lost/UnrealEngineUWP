// Copyright Epic Games, Inc. All Rights Reserved.

#include "NNERuntimeRDGSoftmax.h"
#include "NNEHlslShadersSoftmaxCS.h"
#include "NNERuntimeRDGHlslHelper.h"
#include "NNETensor.h"
#include "NNETypes.h"
#include "RenderGraphUtils.h"

namespace UE::NNERuntimeRDG::Private::Hlsl
{
	DECLARE_GPU_STAT_NAMED(FNNEOperatorSoftmax, TEXT("NNE.Operator.Hlsl.Softmax"));

	/**
	 * Softmax operator implementation
	 */
	class FSoftmax : public FOperatorHlsl
	{

	public:

		FSoftmax() = default;
		virtual ~FSoftmax() = default;

	private:

		int32 Axis = 1;

	public:

		virtual int PrepareOutputs(TConstArrayView<NNE::Internal::FTensorRef> InputTensors, TArrayView<NNE::Internal::FTensorRef> OutputTensors) const override
		{
			check(InputTensors.Num() == 1);
			check(OutputTensors.Num() == 1);

			const NNE::FTensorShape& InputShape = InputTensors[0]->GetShape();

			OutputTensors[0]->SetShape(InputShape);

			return 0;
		};

		virtual bool Initialize(TConstArrayView<NNE::FTensorDesc> InputTensorDescs, TConstArrayView<NNE::FTensorDesc> OutputTensorDescs, const NNE::FAttributeMap& Attributes) override
		{
			check(InputTensorDescs.Num() == 1);
			check(OutputTensorDescs.Num() == 1);

			const int32 InputDimensions = InputTensorDescs[0].GetShape().Rank();

			if (InputDimensions < 2)
			{
				UE_LOG(LogNNE, Warning, TEXT("Input tensor should be at least 2-D (but got rank %d)"), InputDimensions);
				return false;
			}

			if (InputTensorDescs[0].GetShape().Rank() != OutputTensorDescs[0].GetShape().Rank())
			{
				UE_LOG(LogNNE, Warning, TEXT("Softwax requires the output to have the same rank as the input."));
				return false;
			}

			const int32 AxisLow = 0;
			const int32 AxisHigh = InputDimensions - 1;

			Axis = Attributes.GetValueOrDefault<int32>(TEXT("axis"), 1);
			if (Axis < AxisLow || AxisHigh < Axis)
			{
				UE_LOG(LogNNE, Warning, TEXT("Invalid axis (should in the interval [%d, %d], but got %d)"), AxisLow, AxisHigh, Axis);
				return false;
			}

			return true;
		}

		virtual void Dispatch(FRDGBuilder& GraphBuilder, TConstArrayView<FTensorRDGRef> InputTensors, TConstArrayView<FTensorRDGRef> OutputTensors) override
		{
			using namespace UE::NNEHlslShaders::Internal;

			check(InputTensors.Num() == 1);
			check(OutputTensors.Num() == 1);
			check(InputTensors[0] != FTensorRDGRef{});
			check(OutputTensors[0] != FTensorRDGRef{});

			const FTensorRDG& Input = *InputTensors[0];
			const FTensorRDG& Output = *OutputTensors[0];

			check(Input.GetVolume() == Output.GetVolume());

			const NNE::FTensorShape& InputShape = Input.GetShape();
			TConstArrayView<uint32> InputShapeData = InputShape.GetData();
			const int32 InputDimensions = InputShape.Rank();

			int32 N = 1;
			for (int32 i = 0; i < Axis; i++)
			{
				N *= InputShapeData[i];
			}

			int32 D = 1;
			for (int32 i = Axis; i < InputDimensions; i++)
			{
				D *= InputShapeData[i];
			}

			// Set parameters
			TSoftmaxCS::FParameters* Parameters = GraphBuilder.AllocParameters<TSoftmaxCS::FParameters>();
			Parameters->N = N;
			Parameters->D = D;
			Parameters->Input = GraphBuilder.CreateSRV(FRDGBufferSRVDesc(Input.GetBuffer(), PF_R32_FLOAT));
			Parameters->Output = GraphBuilder.CreateUAV(FRDGBufferUAVDesc(Output.GetBuffer(), PF_R32_FLOAT));

			TShaderMapRef<TSoftmaxCS> ComputeShader(GetGlobalShaderMap(GMaxRHIFeatureLevel));
			FIntVector ThreadGroupCount = TSoftmaxCS::GetGroupCount(*Parameters);

			RDG_EVENT_SCOPE(GraphBuilder, "NNE.Operator.Hlsl.Softmax");
			RDG_GPU_STAT_SCOPE(GraphBuilder, FNNEOperatorSoftmax);

			FComputeShaderUtils::AddPass(
				GraphBuilder,
				RDG_EVENT_NAME("NNE.Operator.Hlsl.Softmax.Dispatch"),
				ERDGPassFlags::Compute | ERDGPassFlags::NeverCull,
				ComputeShader,
				Parameters,
				ThreadGroupCount);
		}
	};

	bool ValidateSoftmaxOperator(const NNE::FAttributeMap& AttributeMap, TConstArrayView<ENNETensorDataType> InputTypes, TConstArrayView<NNE::FSymbolicTensorShape> InputShapes)
	{
		bool bIsValid = true;

		// This match version 1 of the Softmax operator, next versions are 11 and 13
		// https://github.com/onnx/onnx/blob/main/docs/Changelog.md#Softmax-1
		FAttributeValidator AttributeValidator;
		AttributeValidator.AddOptional(TEXT("axis"), ENNEAttributeDataType::Int32);
		bIsValid &= AttributeValidator.Validate(AttributeMap);

		FInputValidator InputValidator;
		InputValidator.AddSupportedType(ENNETensorDataType::Float);
		InputValidator.AddRequired();
		bIsValid &= InputValidator.Validate(InputTypes);

		return bIsValid;
	}

	FOperatorHlsl* CreateSoftmaxOperator()
	{
		return new FSoftmax();
	}

	bool RegisterSoftmaxOperator(FOperatorRegistryHlsl& Registry)
	{
		Registry.OpAdd({{TEXT("Softmax"), TEXT("Onnx")}}, CreateSoftmaxOperator, ValidateSoftmaxOperator);
		return true;
	}
} // UE::NNERuntimeRDG::Private::Hlsl
