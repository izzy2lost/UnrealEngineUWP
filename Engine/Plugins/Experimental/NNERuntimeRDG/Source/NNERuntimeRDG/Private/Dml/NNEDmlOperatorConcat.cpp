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
	

public:

	static FOperatorDml* Create()
	{
		return new FOperatorDmlConcat();
	}

	static bool Validate(const NNE::FAttributeMap& AttributeMap, TConstArrayView<ENNETensorDataType> InputTypes, TConstArrayView<NNE::FSymbolicTensorShape> InputShapes)
	{
		if(InputShapes.Num() == 0)
		{
			UE_LOG(LogNNE, Warning, TEXT("DML no input tensors"));
        	return false;
		}

		const int32 InputDims = InputShapes[0].Rank();
		auto NormalizeAxis = [InputDims] (int32 Axis) -> uint32
		{
			if (Axis < 0)
			{
				Axis += InputDims;
			}
			return (uint32) Axis;
		};

		uint32 Axis = NormalizeAxis(AttributeMap.GetValue<int32>(TEXT("axis")));
		if(Axis >= (uint32) InputDims)
		{
			UE_LOG(LogNNE, Warning, TEXT("DML invalid axis: %d"), Axis);
        	return false;
		}

		if(!CheckGenericTensor(InputTypes[0], InputShapes[0]))
		{
			return false;
		}
		
		for (int32 Idx = 1; Idx < InputShapes.Num(); ++Idx)
		{
			if(!CheckGenericTensor(InputTypes[Idx], InputShapes[Idx]))
			{
				return false;
			}

			if(InputShapes[Idx].Rank() != InputDims)
			{
				UE_LOG(LogNNE, Warning, TEXT("DML concat rank mismatch for tensor %d"), Idx);
				return false;
			}

			for(int32 Dim = 0; Dim < InputShapes[Idx].Rank(); ++Dim)
			{
				if(Dim != Axis)
				{
					if(InputShapes[Idx].GetData()[Dim] != InputShapes[0].GetData()[Dim])
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
	virtual bool Initialize(IDMLDevice* Device, TArrayView<const NNE::Internal::FTensor> InputTensors, TArrayView<const NNE::Internal::FTensor> OutputTensors, const NNE::FAttributeMap& Attributes) override
	{
		check(InputTensors.Num() > 0);
		check(OutputTensors.Num() == 1);

		int32 InputDims = InputTensors[0].GetShape().Rank();
		auto NormalizeAxis = [InputDims] (int32 Axis) -> uint32
		{
			if (Axis < 0)
			{
				Axis += InputDims;
			}
			return (uint32) Axis;
		};

		uint32 Axis = NormalizeAxis(Attributes.GetValue<int32>(TEXT("axis")));
		check(Axis < (uint32) InputDims);

		TArray<FTensorDescDml> InputDescs;
		InputDescs.SetNum(InputTensors.Num());
		
		TArray<DML_TENSOR_DESC> DmlInputDescs;
		DmlInputDescs.SetNumUninitialized(InputTensors.Num());
		
		for (TConstEnumerateRef<NNE::Internal::FTensor> It : EnumerateRange(InputTensors))
		{
			const NNE::Internal::FTensor& Tensor = *It;
			int Idx = It.GetIndex();
			
			if (Algo::Find(Tensor.GetShape().GetData(), 0) == nullptr)
			{
				if (!InputDescs[Idx]
						.SetFromTensor(Tensor)
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
				.SetFromTensor(OutputTensors[0])
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
