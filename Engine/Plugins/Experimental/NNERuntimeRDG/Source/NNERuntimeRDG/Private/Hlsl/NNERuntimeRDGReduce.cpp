// Copyright Epic Games, Inc. All Rights Reserved.

#include "NNERuntimeRDGReduce.h"
#include "NNEHlslShadersReduceCS.h"
#include "NNERuntimeRDGHlslHelper.h"
#include "NNETensor.h"
#include "NNETypes.h"
#include "RenderGraphUtils.h"

namespace UE::NNERuntimeRDG::Private::Hlsl
{
	DECLARE_GPU_STAT_NAMED(FNNEOperatorReduceSum, TEXT("NNE.Operator.Hlsl.ReduceSum"));

	/**
	 * ReduceSum operator implementation
	 */
	class FReduceSum : public FOperatorHlsl
	{

	public:

		FReduceSum() = default;
		virtual ~FReduceSum() = default;

	private:

		TArray<int32> Axes;
		int32 KeepDims = 1;

	public:

		virtual int PrepareOutputs(TConstArrayView<NNE::Internal::FTensorRef> InputTensors, TArrayView<NNE::Internal::FTensorRef> OutputTensors) const override
		{
			check(InputTensors.Num() == 1);
			check(OutputTensors.Num() == 1);

			const NNE::FTensorShape& InputShape = InputTensors[0]->GetShape();
			const int32 InputRank = InputShape.Rank();
			TArray<uint32> OutputShape;

			OutputShape.Reserve(InputRank);
			for (int i = 0; i < InputRank; ++i)
			{
				if (!Axes.Contains(i))
				{
					OutputShape.Add(InputShape.GetData()[i]);
				}
				else if (KeepDims)
				{
					OutputShape.Add(1);
				}
			}

			OutputTensors[0]->SetShape(NNE::FTensorShape::Make(OutputShape));

			return 0;
		};

		virtual bool Initialize(TConstArrayView<NNE::FTensorDesc> InputTensorDescs, TConstArrayView<NNE::FTensorDesc> OutputTensorDescs, const NNE::FAttributeMap& Attributes) override
		{
			check(InputTensorDescs.Num() == 1);
			check(OutputTensorDescs.Num() == 1);

			const int32 InputRank = InputTensorDescs[0].GetShape().Rank();
			TArray<int32> AxesTemp;

			for (int i = 0; i < InputRank; ++i)
			{
				AxesTemp.Add(i);
			}
			AxesTemp = Attributes.GetValueOrDefault<TArray<int32>>(TEXT("axes"), AxesTemp);
			AxesTemp.Sort();

			Axes.Empty();

			for (int32 axis : AxesTemp)
			{
				if (axis > InputRank || axis  < -InputRank)
				{
					UE_LOG(LogNNE, Warning, TEXT("ReduceSum 'Axes' attribute should contain value be in the range [-r,r] with r being the rank of the input (name: %s) however got %d while rank is %d."), *InputTensorDescs[0].GetName(), axis, InputRank);
					return false;
				}

				if (axis  < 0)
				{
					axis = InputRank + axis;
				}
				Axes.AddUnique(axis);
			}
			check(Axes.Num() > 0);

			KeepDims = Attributes.GetValueOrDefault<int32>(TEXT("keepdims"), 1);

			return true;
		}

		virtual void Dispatch(FRDGBuilder& GraphBuilder, TConstArrayView<FTensorRDGRef> InputTensors, TConstArrayView<FTensorRDGRef> OutputTensors) override
		{
			using namespace UE::NNEHlslShaders::Internal;

			check(InputTensors.Num() == 1);
			check(OutputTensors.Num() == 1);
			check(InputTensors[0] != FTensorRDGRef{});
			check(OutputTensors[0] != FTensorRDGRef{});
			check(Axes.Num() > 0);

			const FTensorRDG& Input = *InputTensors[0];
			const FTensorRDG& Output = *OutputTensors[0];
			const int32 InputRank = Input.GetShape().Rank();

			RDG_EVENT_SCOPE(GraphBuilder, "NNE.Operator.Hlsl.ReduceSum");
			RDG_GPU_STAT_SCOPE(GraphBuilder, FNNEOperatorReduceSum);

			FRDGBufferRef CurrInput = Input.GetBuffer();
			TArray<uint32, TInlineAllocator<NNE::FTensorShape::MaxRank>> CurrInputShape(Input.GetShape().GetData());
			for (int32 i = Axes.Num()-1; i >= 0; --i)
			{
				const int32 Axis = Axes[i];

				TReduceCS::FParameters* Parameters = GraphBuilder.AllocParameters<TReduceCS::FParameters>();
				TReduceCS::FillInParameters(CurrInputShape, Axis, Parameters);

				FRDGBufferRef CurrOutput = nullptr;
				if (i != 0)
				{
					const FRDGBufferDesc TempBufferDesc = FRDGBufferDesc::CreateBufferDesc(Output.GetElementByteSize(), Parameters->NumElemBeforeAxis * Parameters->NumElemAfterAxis);
					CurrOutput = GraphBuilder.CreateBuffer(TempBufferDesc, TEXT("NNE.Operator.Hlsl.ReduceSum.TempBuffer"), ERDGBufferFlags::None);
				}
				else
				{
					CurrOutput = Output.GetBuffer();
				}
				check(CurrOutput);

				TReduceCS::EnqueueRDG(GraphBuilder, Parameters, CurrInput, CurrOutput);
				CurrInput = CurrOutput;
				CurrInputShape[i] = 1;
			}
		}
	};

	bool ValidateReduceSumOperator(const NNE::FAttributeMap& AttributeMap, TConstArrayView<ENNETensorDataType> InputTypes, TConstArrayView<NNE::FSymbolicTensorShape> InputShapes)
	{
		bool bIsValid = true;

		// This match version 1 and 11 of the ReduceSum operator, next version is 13
		// https://github.com/onnx/onnx/blob/main/docs/Changelog.md#Reduce-1
		FAttributeValidator AttributeValidator;
		AttributeValidator.AddOptional(TEXT("axes"), ENNEAttributeDataType::Int32Array);
		AttributeValidator.AddOptional(TEXT("keepdims"), ENNEAttributeDataType::Int32);
		bIsValid &= AttributeValidator.Validate(AttributeMap);

		FInputValidator InputValidator;
		InputValidator.AddSupportedType(ENNETensorDataType::Float);
		InputValidator.AddRequired();
		bIsValid &= InputValidator.Validate(InputTypes);

		return bIsValid;
	}

	FOperatorHlsl* CreateReduceSumOperator()
	{
		return new FReduceSum();
	}

	bool RegisterReduceOperators(FOperatorRegistryHlsl& Registry)
	{
		Registry.OpAdd({{TEXT("ReduceSum"), TEXT("Onnx")}}, CreateReduceSumOperator, ValidateReduceSumOperator);
		return true;
	}
} // UE::NNERuntimeRDG::Private::Hlsl
