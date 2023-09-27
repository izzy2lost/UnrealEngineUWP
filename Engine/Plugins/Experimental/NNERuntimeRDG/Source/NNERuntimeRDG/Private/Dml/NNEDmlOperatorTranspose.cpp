// Copyright Epic Games, Inc. All Rights Reserved.

#ifdef NNE_USE_DIRECTML

#include "NNEDmlOperator.h"
#include "Math/Range.h"
#include "Algo/Transform.h"
#include "Algo/Reverse.h"

namespace UE::NNERuntimeRDG::Private::Dml
{

class FOperatorDmlTranspose : public FOperatorDml
{
	// Apply permutations to input array view
	Util::FSmallUIntArray Permute(TConstArrayView<uint32> InputView) const
	{
		Util::FSmallUIntArray Permuted;

		for (int32 PermVal : Perm)
		{
			Permuted.Add(InputView[PermVal]);
		}

		return Permuted;
	};

	TArray<int32> Perm;

public:

	static FOperatorDml* Create()
	{
		return new FOperatorDmlTranspose();
	}

	static bool Validate(const NNE::FAttributeMap& AttributeMap, TConstArrayView<ENNETensorDataType> InputTypes, TConstArrayView<NNE::FSymbolicTensorShape> InputShapes)
	{
		return true;
	}

	virtual bool Initialize(TConstArrayView<NNE::FTensorDesc> Inputs, TConstArrayView<NNE::FTensorDesc> Outputs, const NNE::FAttributeMap& Attributes) override
	{
		check(Inputs.Num() == 1);
		check(Outputs.Num() == 1);

		const int32 NumDims = Inputs[0].GetShape().Rank();
		check(NumDims > 0);
		check(NumDims == Outputs[0].GetShape().Rank());

		// Default permutation is reverse
		Util::FSmallIntArray ReversePerm;

		for (int Idx = NumDims - 1; Idx >= 0; Idx--)
		{
			ReversePerm.Add(Idx);
		}

		Perm = Attributes.GetValueOrDefault<TArray<int32>>(TEXT("perm"), (TArray<int32>) ReversePerm);
		check(Perm.Num() == NumDims);
		return true;

	}

	virtual int PrepareOutputs(TConstArrayView<NNE::Internal::FTensorRef> InputTensors, TArrayView<NNE::Internal::FTensorRef> OutputTensors) const override
	{
		Util::FSmallUIntArray OutputShape = Permute(InputTensors[0]->GetShape().GetData());

		OutputTensors[0]->SetShape(NNE::FTensorShape::Make(OutputShape));

		return 0;
	}

	virtual bool Create(IDMLDevice* Device, TConstArrayView<NNE::Internal::FTensorRef> InputTensors, TConstArrayView<NNE::Internal::FTensorRef> OutputTensors) override
	{
		TConstArrayView<uint32>	InputShape = InputTensors[0]->GetShape().GetData();

		FTensorDescDml DmlInputTensorDesc;

		DmlInputTensorDesc
			.SetFromTensor(*InputTensors[0])
			.SetShape(Permute(InputShape))
			.SetStridesFromShape(InputShape)
		;

		DmlInputTensorDesc.SetStrides(Permute(DmlInputTensorDesc.GetStrides()));

		if (!DmlInputTensorDesc.Validate())
		{
			UE_LOG(LogNNE, Error, TEXT("Failed to initialize Transpose input for DML inference"));
			return false;
		}

		FTensorDescDml DmlOutputTensorDesc;

		if (!DmlOutputTensorDesc
				.SetFromTensor(*OutputTensors[0])
				.Validate())
		{
			UE_LOG(LogNNE, Error, TEXT("Failed to initialize Transpose output for DML inference"));
			return false;
		}

		check(Util::IsSameShape(DmlOutputTensorDesc, DmlInputTensorDesc));

		DML_ELEMENT_WISE_IDENTITY_OPERATOR_DESC DmlIdentityOpDesc{};

		DmlIdentityOpDesc.InputTensor = DmlInputTensorDesc.GetDmlDesc();
		DmlIdentityOpDesc.OutputTensor = DmlOutputTensorDesc.GetDmlDesc();

		return CreateOperator(Device, DML_OPERATOR_DESC{ DML_OPERATOR_ELEMENT_WISE_IDENTITY, &DmlIdentityOpDesc });
	}
};

// Register Transpose operator on Module startup
NNE_DML_REGISTER_OP(Transpose)

} // namespace UE::NNERuntimeRDG::Private::Dml

#endif // NNE_USE_DIRECTML
