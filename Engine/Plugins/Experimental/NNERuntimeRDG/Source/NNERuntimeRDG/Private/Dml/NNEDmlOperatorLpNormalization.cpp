// Copyright Epic Games, Inc. All Rights Reserved.

#ifdef NNE_USE_DIRECTML
#include "NNEDmlOperator.h"
#include "NNEDmlOperatorUtils.h"

namespace UE::NNERuntimeRDG::Private::Dml
{

class FOperatorDmlLpNormalization : public FOperatorDml
{
	static constexpr float DefaultEpsilon = 1e-5f;

	int32	P;
	uint32	Axis;


public:

	static FOperatorDml* Create()
	{
		return new FOperatorDmlLpNormalization();
	}

	static bool Validate(const NNE::FAttributeMap& AttributeMap, TConstArrayView<ENNETensorDataType> InputTypes, TConstArrayView<NNE::FSymbolicTensorShape> InputShapes)
	{
		return true;
	}

	virtual bool Initialize(TConstArrayView<NNE::FTensorDesc> Inputs, TConstArrayView<NNE::FTensorDesc> Outputs, const NNE::FAttributeMap& Attributes) override
	{
		check(Inputs.Num() == 1);
		check(Outputs.Num() == 1);

		// Read attributes
		P = Attributes.GetValueOrDefault<int32>(TEXT("p"), 1);
		check(P >= 1 && P <= 2);

		Axis = Attributes.GetValueOrDefault<int32>(TEXT("axis"), 0);

		return true;
	}

	virtual int PrepareOutputs(TConstArrayView<NNE::Internal::FTensorRef> InputTensors, TArrayView<NNE::Internal::FTensorRef> OutputTensors) const override
	{
		OutputTensors[0]->SetShape(InputTensors[0]->GetShape());
		return 0;
	}

	virtual bool Create(IDMLDevice* Device, TConstArrayView<NNE::Internal::FTensorRef> InputTensors, TConstArrayView<NNE::Internal::FTensorRef> OutputTensors) override
	{
		const NNE::Internal::FTensor& InputTensor = *InputTensors[0];
		const NNE::Internal::FTensor& OutputTensor = *OutputTensors[0];

		FTensorDescDml	DmlInputTensorDesc;
		
		if (!DmlInputTensorDesc
				.SetFromTensor(InputTensor)
				.Validate())
		{
			UE_LOG(LogNNE, Error, TEXT("Failed to initialize tensor(s) for DML inference"));
			return false;
		}
		
		FTensorDescDml	DmlOutputTensorDesc;
		
		if (!DmlOutputTensorDesc
				.SetFromTensor(OutputTensor)
				.Validate())
		{
			UE_LOG(LogNNE, Error, TEXT("Failed to initialize tensor(s) for DML inference"));
			return false;
		}

		DML_LP_NORMALIZATION_OPERATOR_DESC	OpDesc{};

		OpDesc.InputTensor = DmlInputTensorDesc.GetDmlDesc();
		OpDesc.OutputTensor = DmlOutputTensorDesc.GetDmlDesc();
		OpDesc.Axis = GetDmlAxis(Axis, InputTensor.GetShape().Rank(), DmlInputTensorDesc.GetRank());;
		OpDesc.Epsilon = DefaultEpsilon;
		OpDesc.P = P;

		return CreateOperator(Device, DML_OPERATOR_DESC{ DML_OPERATOR_LP_NORMALIZATION, &OpDesc });
	}
};

// Register operator on Module startup
NNE_DML_REGISTER_OP(LpNormalization)

} // namespace UE::NNERuntimeRDG::Private::Dml

#endif // NNE_USE_DIRECTML
