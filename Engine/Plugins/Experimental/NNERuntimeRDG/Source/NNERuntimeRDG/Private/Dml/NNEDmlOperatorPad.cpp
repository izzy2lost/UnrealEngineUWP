// Copyright Epic Games, Inc. All Rights Reserved.

#ifdef NNE_USE_DIRECTML
#include "NNEDmlOperator.h"

namespace UE::NNERuntimeRDG::Private::Dml
{
//
//
//
class FOperatorDmlPad : public FOperatorDml
{
	float					Value;
	DML_PADDING_MODE		Mode;
	mutable TArray<int32>	Pads;

	//
	//
	//
	static DML_PADDING_MODE ModeFromString(FStringView StringVal)
	{
		if (FCString::Stricmp(StringVal.GetData(), TEXT("CONSTANT")) == 0)
		{
			return DML_PADDING_MODE_CONSTANT;
		}
		else if (FCString::Stricmp(StringVal.GetData(), TEXT("REFLECT")) == 0)
		{
			return DML_PADDING_MODE_REFLECTION;
		}
		else if (FCString::Stricmp(StringVal.GetData(), TEXT("EDGE")) == 0)
		{
			return DML_PADDING_MODE_EDGE;
		}
		else
		{
			return DML_PADDING_MODE_CONSTANT;
		}
	}

public:

	//
	//
	//
	static FOperatorDml* Create()
	{
		return new FOperatorDmlPad();
	}

	//
	//
	//
	static bool Validate(const NNE::FAttributeMap& AttributeMap, TConstArrayView<ENNETensorDataType> InputTypes, TConstArrayView<NNE::FSymbolicTensorShape> InputShapes)
	{
		//TODO
		return true;
	}

	//
	//
	//
	virtual bool Initialize(TConstArrayView<NNE::FTensorDesc> Inputs, TConstArrayView<NNE::FTensorDesc> Outputs, const NNE::FAttributeMap& Attributes) override
	{
		check(Inputs.Num() >= 1);
		check(Outputs.Num() == 1);

		// Read attributes
		Value = Attributes.GetValueOrDefault<float>(TEXT("value"), 0.0f);
		Mode = ModeFromString(Attributes.GetValue<FString>(TEXT("mode")));

		if (Inputs.Num() >= 2)
		{
			ConstantCPUInputs.Add(1);
		}
		else
		{
			Pads = Attributes.GetValue<TArray<int32>>(TEXT("pads"));
		}

		return true;
	}

	//
	//
	//
	virtual int PrepareOutputs(TConstArrayView<NNE::Internal::FTensorRef> InputTensors, TArrayView<NNE::Internal::FTensorRef> OutputTensors) const override
	{
		if (InputTensors.Num() >= 2)
		{
			if (!InputTensors[1]->HasPreparedData())
			{
				UE_LOG(LogNNE, Error, TEXT("pads is only supported as an attribute or a constant tensor, it is here a variable tensor of name %s."), *InputTensors[1]->GetName());
				return -1;
			}

			Pads.Append(InputTensors[1]->GetPreparedData<int64>());
		}

		if (Pads.IsEmpty())
		{
			UE_LOG(LogNNE, Error, TEXT("pads are empty"));
			return -1;
		}

		TConstArrayView<uint32> InputShape = InputTensors[0]->GetShape().GetData();

		if (InputShape.Num() * 2 != Pads.Num())
		{
			UE_LOG(LogNNE, Error, TEXT("pads attribute length (%d) should be twice the rank of input X (%d)."), Pads.Num(), InputShape.Num());
			return false;
		}

		TArray<uint32> OutputShape;
		for (int32 i = 0; i < InputShape.Num(); ++i)
		{
			int32 PrePad = Pads[i];
			int32 PostPad = Pads[i + InputShape.Num()];
			int32 OutputDim = PrePad + InputShape[i] + PostPad;

			if (OutputDim < 1)
			{
				UE_LOG(
					LogNNE, Warning, TEXT("Pads cannot reduce dimension below 1, but would for tensor (name:%s) at rank %d of size %d with prepad %d and postpad %d."), 
					*InputTensors[0]->GetName(), i, InputShape[i], PrePad, PostPad);

				return -1;
			}

			OutputShape.Emplace(OutputDim);
		}

		OutputTensors[0]->SetShape(NNE::FTensorShape::Make(OutputShape));

		return 0;
	}

	//
	//
	//
	virtual bool Create(IDMLDevice* Device, TConstArrayView<NNE::Internal::FTensorRef> InputTensors, TConstArrayView<NNE::Internal::FTensorRef> OutputTensors) override
	{
		const NNE::Internal::FTensor& InputTensor = *InputTensors[0];
		const NNE::Internal::FTensor& OutputTensor = *OutputTensors[0];

		// Initialize tensor descriptors
		FTensorDescDml	DmlInputTensorDesc;
		FTensorDescDml	DmlOutputTensorDesc;

		if (!DmlInputTensorDesc
				.SetFromTensor(InputTensor)
				.Validate())
		{
			UE_LOG(LogNNE, Error, TEXT("Failed to initialize tensor(s) for DML inference"));
			return false;
		}

		if (!DmlOutputTensorDesc
				.SetFromTensor(OutputTensor)
				.Validate())
		{
			UE_LOG(LogNNE, Error, TEXT("Failed to initialize tensor(s) for DML inference"));
			return false;
		}

		DML_PADDING_OPERATOR_DESC	PadOpDesc{};

		PadOpDesc.InputTensor = DmlInputTensorDesc.GetDmlDesc();
		PadOpDesc.OutputTensor = DmlOutputTensorDesc.GetDmlDesc();
		PadOpDesc.PaddingMode = Mode;
		PadOpDesc.PaddingValue = Value;
		PadOpDesc.DimensionCount = Pads.Num() / 2;
		PadOpDesc.StartPadding = (uint32*) Pads.GetData();
		PadOpDesc.EndPadding = (uint32*) (Pads.GetData() + PadOpDesc.DimensionCount);

		return CreateOperator(Device, DML_OPERATOR_DESC { DML_OPERATOR_PADDING, &PadOpDesc });
	}
};

// Register Pad operator on Module startup
NNE_DML_REGISTER_OP(Pad)

} // namespace UE::NNERuntimeRDG::Private::Dml

#endif // NNE_USE_DIRECTML
