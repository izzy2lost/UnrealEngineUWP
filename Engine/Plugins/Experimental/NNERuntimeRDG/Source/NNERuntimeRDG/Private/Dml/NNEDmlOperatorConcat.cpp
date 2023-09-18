// Copyright Epic Games, Inc. All Rights Reserved.

#ifdef NNE_USE_DIRECTML

#include "NNEDmlOperator.h"
#include "Misc/EnumerateRange.h"
#include "Algo/Find.h"
#include "NNEDmlOperatorUtils.h"

namespace UE::NNERuntimeRDG::Private::Dml
{

/**
 * Concat
 */
class FOperatorDmlConcat : public FOperatorDml
{
	int32 Axis;

public:

	//
	//
	//
	static FOperatorDml* Create()
	{
		return new FOperatorDmlConcat();
	}

	//
	//
	//
	static bool Validate(const NNE::FAttributeMap& AttributeMap, TConstArrayView<ENNETensorDataType> InputTypes, TConstArrayView<NNE::FSymbolicTensorShape> InputShapes)
	{
		if (InputShapes.Num() == 0)
		{
			UE_LOG(LogNNE, Warning, TEXT("DML no input tensors"));
			return false;
		}

		const int32 InputRank = InputShapes[0].Rank();
		
		int32 Axis = HandleNegativeAxis(AttributeMap.GetValue<int32>(TEXT("axis")), InputRank);
		if (Axis >= InputRank)
		{
			UE_LOG(LogNNE, Warning, TEXT("DML invalid axis: %d"), Axis);
			return false;
		}

		if (!CheckGenericTensor(InputTypes[0], InputShapes[0]))
		{
			return false;
		}
		
		for (int32 Idx = 1; Idx < InputShapes.Num(); ++Idx)
		{
			if (!CheckGenericTensor(InputTypes[Idx], InputShapes[Idx]))
			{
				return false;
			}

			if (InputShapes[Idx].Rank() != InputRank)
			{
				UE_LOG(LogNNE, Warning, TEXT("DML concat rank mismatch for tensor %d"), Idx);
				return false;
			}

			for (int32 Dim = 0; Dim < InputShapes[Idx].Rank(); ++Dim)
			{
				if (Dim != Axis)
				{
					if (InputShapes[Idx].GetData()[Dim] != InputShapes[0].GetData()[Dim])
					{
						UE_LOG(LogNNE, Warning, TEXT("DML concat dimension mismatch for tensor %d"), Idx);
						return false;
					}
				}
			}
		}

		return true;
	}

	//
	//
	//
	virtual bool Initialize(TConstArrayView<NNE::FTensorDesc> Inputs, TConstArrayView<NNE::FTensorDesc> Outputs, const NNE::FAttributeMap& Attributes) override
	{
		check(Inputs.Num() >= 1);
		check(Outputs.Num() == 1);

		const int32 InputRank = Inputs[0].GetShape().Rank();

		Axis = Attributes.GetValue<int32>(TEXT("axis"));

		if (Axis < -InputRank || Axis >(InputRank - 1))
		{
			UE_LOG(LogNNE, Warning, TEXT("Axis should be in range [-r,r-1] however it is %d while inputs have rank %d."), Axis, InputRank);
			return false;
		}

		Axis = HandleNegativeAxis(Axis, InputRank);
		check(Axis < InputRank);

		return true;
	}

	//
	//
	//
	virtual int PrepareOutputs(TConstArrayView<NNE::Internal::FTensorRef> InputTensors, TArrayView<NNE::Internal::FTensorRef> OutputTensors) const override
	{
		TArray<uint32> OutputShape(InputTensors[0]->GetShape().GetData());
		
		const int32 AxisIndex = Axis >= 0 ? Axis : InputTensors[0]->GetShape().Rank() - Axis;

		for (int32 i = 1; i < InputTensors.Num(); ++i)
		{
			OutputShape[AxisIndex] += InputTensors[i]->GetShape().GetData()[AxisIndex];

			for (int32 r = 0; r < OutputShape.Num(); ++r)
			{
				if (r != AxisIndex && (OutputShape[r] != InputTensors[i]->GetShape().GetData()[r]))
				{
					UE_LOG(LogNNE, Warning, TEXT("Concat: all input tensors should have the same shape except on the concatenation axis"));
					return false;
				}
			}
		}

		OutputTensors[0]->SetShape(NNE::FTensorShape::Make(OutputShape));

		return 0;
	}

	//
	//
	//
	virtual bool Create(IDMLDevice* Device, TConstArrayView<NNE::Internal::FTensorRef> InputTensors, TConstArrayView<NNE::Internal::FTensorRef> OutputTensors) override
	{
		TArray<FTensorDescDml> InputDescs;
		InputDescs.SetNum(InputTensors.Num());
		
		TArray<DML_TENSOR_DESC> DmlInputDescs;
		DmlInputDescs.SetNumUninitialized(InputTensors.Num());
		
		for (TConstEnumerateRef<NNE::Internal::FTensorRef> It : EnumerateRange(InputTensors))
		{
			const NNE::Internal::FTensorRef& Tensor = *It;
			int Idx = It.GetIndex();
			
			if (Algo::Find(Tensor->GetShape().GetData(), 0) == nullptr)
			{
				if (!InputDescs[Idx]
						.SetFromTensor(*Tensor)
						.Validate())
				{
					UE_LOG(LogNNE, Error, TEXT("Failed to initialize Concat input for DML inference"));
					return false;
				}

				DmlInputDescs[Idx] = *InputDescs[Idx].GetDmlDesc();
			}
		}
		check(InputDescs.Num() > 0);

		FTensorDescDml OutputTensorDesc;

		if (!OutputTensorDesc
				.SetFromTensor(*OutputTensors[0])
				.Validate())
		{
			UE_LOG(LogNNE, Error, TEXT("Failed to initialize Concat output for DML inference"));
			return false;
		}

		DML_JOIN_OPERATOR_DESC DmlJoinOpDesc = {};
		DmlJoinOpDesc.InputCount = (uint32) DmlInputDescs.Num();
		DmlJoinOpDesc.InputTensors = DmlInputDescs.GetData();
		DmlJoinOpDesc.OutputTensor = OutputTensorDesc.GetDmlDesc();
		DmlJoinOpDesc.Axis = Axis;

		return CreateOperator(Device, DML_OPERATOR_DESC{ DML_OPERATOR_JOIN, &DmlJoinOpDesc} );
	}
};

// Register Concat operator on Module startup
NNE_DML_REGISTER_OP(Concat)

} // namespace UE::NNERuntimeRDG::Private::Dml

#endif // NNE_USE_DIRECTML
