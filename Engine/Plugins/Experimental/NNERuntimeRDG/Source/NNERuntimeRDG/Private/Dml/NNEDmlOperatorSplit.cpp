// Copyright Epic Games, Inc. All Rights Reserved.

#ifdef NNE_USE_DIRECTML
#include "NNEDmlOperator.h"
#include "NNEDmlOperatorUtils.h"

namespace UE::NNERuntimeRDG::Private::Dml
{

class FOperatorDmlSplit : public FOperatorDml
{
	TArray<int32>	Split;
	int				Axis;

public:

	static FOperatorDml* Create()
	{
		return new FOperatorDmlSplit();
	}

	static bool Validate(const NNE::FAttributeMap& AttributeMap, TConstArrayView<ENNETensorDataType> InputTypes, TConstArrayView<NNE::FSymbolicTensorShape> InputShapes)
	{
		return true;
	}

	virtual bool Initialize(TConstArrayView<NNE::FTensorDesc> Inputs, TConstArrayView<NNE::FTensorDesc> Outputs, const NNE::FAttributeMap& Attributes) override
	{
		check(Inputs.Num() == 1);
		
		Axis = Attributes.GetValueOrDefault<int>(TEXT("axis"), 0);
		Axis = HandleNegativeAxis(Axis, Inputs[0].GetShape().Rank());

		const FNNEAttributeValue* AttrSplit = Attributes.GetAttributeValue(TEXT("split"));

		if (AttrSplit)
		{
			Split = AttrSplit->GetValue<TArray<int32>>();
			
			if (Split.Num() != Outputs.Num())
			{
				UE_LOG(LogNNE, Error, TEXT("Attribute split needs to have same count as number of outputs"));
				return false;
			}

			if ((Split.Num() % Outputs.Num()) != 0)
			{
				UE_LOG(LogNNE, Error, TEXT("Attribute split count needs to be divisible by the number of outputs"));
				return false;
			}
		}

		// Check split size is correct
		for (int Idx = 0; Idx < Outputs.Num(); ++Idx)
		{
			if (Outputs[Idx].GetShape().Rank() != Inputs[0].GetShape().Rank())
			{
				UE_LOG(LogNNE, Error, TEXT("Rank of output tensor and input tensor should be the same"));
				return false;
			}			
		}

		return true;
	}

	virtual int PrepareOutputs(TConstArrayView<NNE::Internal::FTensorRef> InputTensors, TArrayView<NNE::Internal::FTensorRef> OutputTensors) const override
	{
		const NNE::Internal::FTensor&	InputTensor = *InputTensors[0];
		TConstArrayView<uint32>			InputShape = InputTensor.GetShape().GetData();

		if (!Split.IsEmpty())
		{
			int32 TotalElemCount = 0;

			for (int32 ElemCount : Split)
			{
				TotalElemCount += ElemCount;
			}

			if (TotalElemCount != InputShape[Axis])
			{
				UE_LOG(LogNNE, Error, TEXT("Incorrect elem count for split axis"));
				return -1;
			}
		
			for (int32 Idx = 0; Idx < OutputTensors.Num(); ++Idx)
			{
				Util::FSmallUIntArray OutputShape;

				OutputShape.Append(InputShape.GetData(), InputShape.Num());
				OutputShape[Axis] = Split[Idx];

				OutputTensors[Idx]->SetShape(NNE::FTensorShape::Make(OutputShape));
			}
		}
		else
		{
			uint32 EqualSplit = InputShape[Axis] / OutputTensors.Num();

			for (int Idx = 0; Idx < OutputTensors.Num(); ++Idx)
			{
				Util::FSmallUIntArray OutputShape;

				OutputShape.Append(InputShape.GetData(), InputShape.Num());
				OutputShape[Axis] = EqualSplit;

				OutputTensors[Idx]->SetShape(NNE::FTensorShape::Make(OutputShape));
			}
		}

		return 0;
	}

	virtual bool Create(IDMLDevice* Device, TConstArrayView<NNE::Internal::FTensorRef> InputTensors, TConstArrayView<NNE::Internal::FTensorRef> OutputTensors) override
	{
		FTensorDescDml InputTensorDesc;
		
		if (!InputTensorDesc
				.SetFromTensor(*InputTensors[0])
				.Validate())
		{
			UE_LOG(LogNNE, Error, TEXT("Failed to initialize pooling operator's input tensor for DML inference"));
			return false;
		}

		Util::FSmallArray<FTensorDescDml> OutputTensorDescs;
		Util::FSmallArray<DML_TENSOR_DESC> DmlOutputTensorDescs;
		
		OutputTensorDescs.SetNum(OutputTensors.Num());
		DmlOutputTensorDescs.SetNumUninitialized(OutputTensors.Num());

		for(int Idx = 0; Idx < OutputTensors.Num(); ++Idx)
		{
			if (!OutputTensorDescs[Idx]
					.SetFromTensor(*OutputTensors[Idx])
					.Validate())
			{
				UE_LOG(LogNNE, Error, TEXT("Failed to initialize pooling operator's output tensor for DML inference"));
				return false;
			}

			DmlOutputTensorDescs[Idx] = *OutputTensorDescs[Idx].GetDmlDesc();
		}
		
		DML_SPLIT_OPERATOR_DESC	SplitOpDesc{};

		SplitOpDesc.InputTensor = InputTensorDesc.GetDmlDesc();
		SplitOpDesc.OutputCount = DmlOutputTensorDescs.Num();
		SplitOpDesc.OutputTensors = DmlOutputTensorDescs.GetData();
		SplitOpDesc.Axis = (UINT) Axis;

		return CreateOperator(Device, DML_OPERATOR_DESC { DML_OPERATOR_SPLIT, &SplitOpDesc });
	}
};

// Register Split operator on Module startup
NNE_DML_REGISTER_OP(Split)

} // namespace UE::NNERuntimeRDG::Private::Dml

#endif // NNE_USE_DIRECTML
