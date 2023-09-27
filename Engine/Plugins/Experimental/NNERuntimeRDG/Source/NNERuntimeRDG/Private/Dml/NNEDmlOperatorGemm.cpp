// Copyright Epic Games, Inc. All Rights Reserved.

#ifdef NNE_USE_DIRECTML
#include "NNEDmlOperator.h"

namespace UE::NNERuntimeRDG::Private::Dml
{

class FOperatorDmlGemm : public FOperatorDml
{
	float Alpha = 1.0f;
	float Beta = 1.0f;
	int32 TransA = 0;
	int32 TransB = 0;

public:

	static FOperatorDml* Create()
	{
		return new FOperatorDmlGemm();
	}

	static bool Validate(const NNE::FAttributeMap& AttributeMap, TConstArrayView<ENNETensorDataType> InputTypes, TConstArrayView<NNE::FSymbolicTensorShape> InputShapes)
	{
		return true;
	}

	virtual bool Initialize(TConstArrayView<NNE::FTensorDesc> Inputs, TConstArrayView<NNE::FTensorDesc> Outputs, const NNE::FAttributeMap& Attributes) override
	{
		Alpha = Attributes.GetValueOrDefault(TEXT("alpha"), Alpha);
		Beta = Attributes.GetValueOrDefault(TEXT("beta"), Beta);
		TransA = Attributes.GetValueOrDefault(TEXT("transA"), TransA);
		TransB = Attributes.GetValueOrDefault(TEXT("transB"), TransB);
		
		const NNE::FTensorDesc& InputA = Inputs[0];
		const NNE::FTensorDesc& InputB = Inputs[1];

		if (InputA.GetShape().Rank() < 2 || InputA.GetShape().Rank() > 4)
		{
			UE_LOG(LogNNE, Error, TEXT("Gemm InputA tensor rank needs to be [2,4]"));
			return false;
		}

		if (InputB.GetShape().Rank() < 2 || InputB.GetShape().Rank() > 4)
		{
			UE_LOG(LogNNE, Error, TEXT("Gemm InputB tensor rank needs to be [2,4]"));
			return false;
		}

		if (Inputs.Num() == 3)
		{
			const NNE::FTensorDesc& InputC = Inputs[2];

			if (InputC.GetShape().Rank() > 4)
			{
				UE_LOG(LogNNE, Error, TEXT("Gemm InputC tensor rank needs to be max rank 4"));
				return false;
			}
		}

		return true;
	}

	virtual int PrepareOutputs(TConstArrayView<NNE::Internal::FTensorRef> InputTensors, TArrayView<NNE::Internal::FTensorRef> OutputTensors) const override
	{
		check(InputTensors.Num() >= 2 && InputTensors.Num() <= 3);
		check(OutputTensors.Num() == 1);

		const NNE::FTensorShape& InputA = InputTensors[0]->GetShape();
		const NNE::FTensorShape& InputB = InputTensors[1]->GetShape();
		
		checkf(InputA.Rank() >= 2, TEXT("Gemm InputA needs to have tensor rank at least size of 2"));
		checkf(InputB.Rank() >= 2, TEXT("Gemm InputB needs to have tensor rank at least size of 2"));

		const uint32 M = TransA != 0 ? InputA.GetData()[1] : InputA.GetData()[0];
		const uint32 N = TransB != 0 ? InputB.GetData()[0] : InputB.GetData()[1];
		
		TArray<uint32> OutputShape;
		OutputShape.Emplace(M);
		OutputShape.Emplace(N);

		OutputTensors[0]->SetShape(NNE::FTensorShape::Make(OutputShape));
		return 0;
	}

	virtual bool Create(IDMLDevice* Device, TConstArrayView<NNE::Internal::FTensorRef> InputTensors, TConstArrayView<NNE::Internal::FTensorRef> OutputTensors) override
	{
		const NNE::Internal::FTensor& InputATensor = *InputTensors[0];
		const NNE::Internal::FTensor& InputBTensor = *InputTensors[1];
		const NNE::Internal::FTensor& OutputTensor = *OutputTensors[0];

		FTensorDescDml	DmlInputATensorDesc;
		FTensorDescDml	DmlInputBTensorDesc;
		FTensorDescDml	DmlInputCTensorDesc;
		FTensorDescDml	DmlOutputTensorDesc;

		if (!DmlInputATensorDesc
				.SetTensorRank(2, 4)
				.SetFromTensor(InputATensor)
				.Validate())
		{
			UE_LOG(LogNNE, Error, TEXT("Failed to initialize tensor(s) for DML inference"));
			return false;
		}

		if (!DmlInputBTensorDesc
				.SetTensorRank(2, 4)
				.SetFromTensor(InputBTensor)
				.Validate())
		{
			UE_LOG(LogNNE, Error, TEXT("Failed to initialize tensor(s) for DML inference"));
			return false;
		}

		if (InputTensors.Num() > 2)
		{
			const NNE::Internal::FTensor& InputCTensor = *InputTensors[2];

			if (!DmlInputCTensorDesc
					.SetTensorRank(2, 4)
					.SetFromTensorBroadcast(InputCTensor, OutputTensor.GetShape())
					.Validate())
			{
				UE_LOG(LogNNE, Error, TEXT("Failed to initialize tensor(s) for DML inference"));
				return false;
			}
		}

		if (!DmlOutputTensorDesc
				.SetTensorRank(2, 4)
				.SetFromTensor(OutputTensor)
				.Validate())
		{
			UE_LOG(LogNNE, Error, TEXT("Failed to initialize tensor(s) for DML inference"));
			return false;
		}

		DML_GEMM_OPERATOR_DESC	DmlGemmOpDesc{};

		DmlGemmOpDesc.ATensor = DmlInputATensorDesc.GetDmlDesc();
		DmlGemmOpDesc.BTensor = DmlInputBTensorDesc.GetDmlDesc();
		DmlGemmOpDesc.CTensor = InputTensors.Num() > 2 ? DmlInputCTensorDesc.GetDmlDesc() : nullptr;
		DmlGemmOpDesc.OutputTensor = DmlOutputTensorDesc.GetDmlDesc();
		DmlGemmOpDesc.Alpha = Alpha;
		DmlGemmOpDesc.Beta = Beta;
		DmlGemmOpDesc.TransA = TransA ? DML_MATRIX_TRANSFORM_TRANSPOSE : DML_MATRIX_TRANSFORM_NONE;
		DmlGemmOpDesc.TransB = TransB ? DML_MATRIX_TRANSFORM_TRANSPOSE : DML_MATRIX_TRANSFORM_NONE;

		return CreateOperator(Device, DML_OPERATOR_DESC { DML_OPERATOR_GEMM, &DmlGemmOpDesc });
	}
};

NNE_DML_REGISTER_OP(Gemm)

} // namespace UE::NNERuntimeRDG::Private::Dml

#endif // NNE_USE_DIRECTML
