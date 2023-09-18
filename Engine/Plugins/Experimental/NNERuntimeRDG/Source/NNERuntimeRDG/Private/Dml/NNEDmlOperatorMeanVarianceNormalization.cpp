// Copyright Epic Games, Inc. All Rights Reserved.

#ifdef NNE_USE_DIRECTML
#include "NNEDmlOperator.h"
#include "NNEDmlOperatorUtils.h"

namespace UE::NNERuntimeRDG::Private::Dml
{

//
//
//
class FOperatorDmlMeanVarianceNormalization : public FOperatorDml
{
	static constexpr float DefaultEpsilon = 1e-5f;

	Util::FSmallUIntArray	Axes;
	int32					AcrossChannels;
	int32					NormalizeVariance;

public:

	//
	//
	//
	static FOperatorDml* Create()
	{
		return new FOperatorDmlMeanVarianceNormalization();
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
		check(Inputs.Num() == 1);
		check(Outputs.Num() == 1);

		// Read attributes
		AcrossChannels = Attributes.GetValueOrDefault<int32>(TEXT("across_channels"), 0);
		NormalizeVariance = Attributes.GetValueOrDefault<int32>(TEXT("normalize_variance"), 1);

		TArray<int32>	OnnxAxes;

		const FNNEAttributeValue* AttrAxes = Attributes.GetAttributeValue("axes");

		if (AttrAxes)
		{
			OnnxAxes = AttrAxes->GetValue<TArray<int32>>();
		}
		else
		{
			constexpr int32 CrossChannelAxes[] = { 0, 1, 2, 3 };
			constexpr int32 NonChannelAxes[] = { 0, 2, 3 };

			if (AcrossChannels)
			{
				OnnxAxes.Append(CrossChannelAxes, UE_ARRAY_COUNT(CrossChannelAxes));
			}
			else
			{
				OnnxAxes.Append(NonChannelAxes, UE_ARRAY_COUNT(NonChannelAxes));
			}
		}

		SetDmlAxesFromOnnx(Axes, Inputs[0].GetShape().Rank(), OnnxAxes);

		return true;
	}

	//
	//
	//
	virtual int PrepareOutputs(TConstArrayView<NNE::Internal::FTensorRef> InputTensors, TArrayView<NNE::Internal::FTensorRef> OutputTensors) const override
	{
		OutputTensors[0]->SetShape(InputTensors[0]->GetShape());
		return 0;
	}

	//
	//
	//
	virtual bool Create(IDMLDevice* Device, TConstArrayView<NNE::Internal::FTensorRef> InputTensors, TConstArrayView<NNE::Internal::FTensorRef> OutputTensors) override
	{
		const NNE::Internal::FTensor& InputTensor = *InputTensors[0];
		const NNE::Internal::FTensor& OutputTensor = *OutputTensors[0];

		if (InputTensor.GetShape().Rank() > 8)
		{
			UE_LOG(LogNNE, Warning, TEXT("InputTensor rank should be between 1 and 8, got:%d"), InputTensor.GetShape().Rank());
			return false;
		}

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
		
		DML_MEAN_VARIANCE_NORMALIZATION1_OPERATOR_DESC	OpDesc{};

		OpDesc.InputTensor = DmlInputTensorDesc.GetDmlDesc();
		OpDesc.ScaleTensor = nullptr;
		OpDesc.BiasTensor = nullptr;
		OpDesc.OutputTensor = DmlOutputTensorDesc.GetDmlDesc();
		OpDesc.AxisCount = Axes.Num();
		OpDesc.Axes = Axes.GetData();
		OpDesc.NormalizeVariance = NormalizeVariance;
		OpDesc.Epsilon = DefaultEpsilon;
		OpDesc.FusedActivation = nullptr;

		return CreateOperator(Device, DML_OPERATOR_DESC{ DML_OPERATOR_MEAN_VARIANCE_NORMALIZATION1, &OpDesc });
	}
};

// Register operator on Module startup
NNE_DML_REGISTER_OP(MeanVarianceNormalization)

} // namespace UE::NNERuntimeRDG::Private::Dml

#endif // NNE_USE_DIRECTML
