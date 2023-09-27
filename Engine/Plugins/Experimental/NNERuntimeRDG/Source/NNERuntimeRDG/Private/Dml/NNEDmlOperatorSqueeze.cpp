// Copyright Epic Games, Inc. All Rights Reserved.

#ifdef NNE_USE_DIRECTML

#include "NNEDmlOperator.h"
#include "NNEDmlOperatorUtils.h"

namespace UE::NNERuntimeRDG::Private::Dml
{

class FOperatorDmlSqueeze : public FOperatorDml
{
	mutable TArray<int32>	Axes;

public:

	static FOperatorDml* Create()
	{
		return new FOperatorDmlSqueeze();
	}

	static bool Validate(const NNE::FAttributeMap& AttributeMap, TConstArrayView<ENNETensorDataType> InputTypes, TConstArrayView<NNE::FSymbolicTensorShape> InputShapes)
	{
		return true;
	}

	virtual bool Initialize(TConstArrayView<NNE::FTensorDesc> Inputs, TConstArrayView<NNE::FTensorDesc> Outputs, const NNE::FAttributeMap& Attributes) override
	{
		check(Inputs.Num() == 1);
		check(Outputs.Num() == 1);

		const int32					InputShapeRank = Inputs[0].GetShape().Rank();
		const FNNEAttributeValue*	AxesAttr = Attributes.GetAttributeValue(TEXT("axes"));

		if (AxesAttr)
		{
			Axes = AxesAttr->GetValue<TArray<int32>>();
			HandleNegativeAxes(Axes, InputShapeRank);
			Algo::Sort(Axes);
		}

		return true;
	}

	virtual int PrepareOutputs(TConstArrayView<NNE::Internal::FTensorRef> InputTensors, TArrayView<NNE::Internal::FTensorRef> OutputTensors) const override
	{
		TConstArrayView<uint32>	InputShape = InputTensors[0]->GetShape().GetData();
		
		if (Axes.IsEmpty())
		{
			for (int32 Idx = 0; Idx < InputShape.Num(); ++Idx)
			{
				if (InputShape[Idx] == 1)
				{
					Axes.Add(Idx);
				}
			}
		}
		
		Util::FSmallUIntArray	OutputShape;

		OutputShape.Reserve(InputShape.Num());
		OutputShape.Append(InputShape);

		for (int32 Idx = Axes.Num() - 1; Idx >= 0; --Idx)
		{
			const int32 Axe = Axes[Idx];

			if (OutputShape[Axe] != 1)
			{
				UE_LOG(
					LogNNE, 
					Warning, 
					TEXT("Squeeze at axe %d for 'Data' (name: %s) should be targeting a dimension of size 1 but it is %d."), 
					Axe, 
					*InputTensors[0]->GetName(), 
					OutputShape[Axe]);
				
				return false;
			}

			OutputShape.RemoveAt(Axe);
		}

		OutputTensors[0]->SetShape(NNE::FTensorShape::Make(OutputShape));

		return 0;
	}

	virtual bool Create(IDMLDevice* Device, TConstArrayView<NNE::Internal::FTensorRef> InputTensors, TConstArrayView<NNE::Internal::FTensorRef> OutputTensors) override
	{
		FTensorDescDml DmlTensorDesc;
		
		if (!DmlTensorDesc
				.SetFromTensor(*OutputTensors[0])
				.Validate())
		{
			UE_LOG(LogNNE, Warning, TEXT("Failed to initialize Unsqueeze's output tensor for DML inference"));
			return false;
		}

		DML_ELEMENT_WISE_IDENTITY_OPERATOR_DESC DmlIdentityOpDesc{};

		DmlIdentityOpDesc.InputTensor = DmlTensorDesc.GetDmlDesc();
		DmlIdentityOpDesc.OutputTensor = DmlTensorDesc.GetDmlDesc();

		return CreateOperator(Device, DML_OPERATOR_DESC{ DML_OPERATOR_ELEMENT_WISE_IDENTITY, &DmlIdentityOpDesc });
	}
};

// Register Reshape operator on Module startup
NNE_DML_REGISTER_OP(Squeeze)

} // namespace UE::NNERuntimeRDG::Private::Dml

#endif // NNE_USE_DIRECTML
