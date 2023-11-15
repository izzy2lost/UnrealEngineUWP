// Copyright Epic Games, Inc. All Rights Reserved.

#ifdef NNE_USE_DIRECTML
#include "NNEDmlOperator.h"
#include "NNEDmlOperatorUtils.h"

namespace UE::NNERuntimeRDG::Private::Dml
{

/**
 * Element-wise unary ML operator implementation
 */
template
<
	typename DmlElementWiseOpDescType, 
	DML_OPERATOR_TYPE DmlElementWiseOpType,
	TCHAR const *OpName
>
class FOperatorDmlElementWiseUnary : public FOperatorDml
{
	static constexpr uint32 NumAllowedInputTensors = 1, NumAllowedOutputTensors = 1;
public:

	static FOperatorDml* Create()
	{
		return new FOperatorDmlElementWiseUnary();
	}

	static bool Validate(const NNE::FAttributeMap& AttributeMap, TConstArrayView<ENNETensorDataType> InputTypes, TConstArrayView<NNE::FSymbolicTensorShape> InputShapes)
	{
		if(InputShapes.Num() != NumAllowedInputTensors)
		{
			UE_LOG(LogNNE, Warning, TEXT("DML %s: Invalid number of input tensors. %d provided, it should be %d."), OpName, InputShapes.Num(), NumAllowedInputTensors);
			return false;
		}
		return CheckElementwiseTensor(OpName, InputTypes[0], InputShapes[0]);
	}

	virtual ~FOperatorDmlElementWiseUnary() = default;

private:

	float Min;
	float Max;

	FOperatorDmlElementWiseUnary() 
		: Min(TNumericLimits<float>::Lowest())
		, Max(TNumericLimits<float>::Max()) 
	{
	}
	
public:

	virtual bool Initialize(TConstArrayView<NNE::FTensorDesc> Inputs, TConstArrayView<NNE::FTensorDesc> Outputs, const NNE::FAttributeMap& Attributes) override
	{
		if constexpr (std::is_same_v<DmlElementWiseOpDescType, DML_ELEMENT_WISE_CLIP_OPERATOR_DESC>)
		{
			const FNNEAttributeValue* MinAttr = Attributes.GetAttributeValue(TEXT("min"));
			if (MinAttr)
			{
				if (MinAttr->GetType() != ENNEAttributeDataType::Float)
				{
					UE_LOG(LogNNE, Error, TEXT("Min attribute of clip must be float for DML inference"));
					return false;
				}

				Min = MinAttr->GetValue<float>();
			}
			
			const FNNEAttributeValue* MaxAttr = Attributes.GetAttributeValue(TEXT("max"));
			if (MaxAttr)
			{
				if (MaxAttr->GetType() != ENNEAttributeDataType::Float)
				{
					UE_LOG(LogNNE, Error, TEXT("Max attribute of clip must be float for DML inference"));
					return false;
				}
				Max = MaxAttr->GetValue<float>();
			}
		}

		return true;
	}

	virtual int PrepareOutputs(TConstArrayView<NNE::Internal::FTensorRef> InputTensors, TArrayView<NNE::Internal::FTensorRef> OutputTensors) const override
	{
		check(InputTensors.Num() == NumAllowedInputTensors);
		check(OutputTensors.Num() == NumAllowedOutputTensors);
		OutputTensors[0]->SetShape(InputTensors[0]->GetShape());

		return 0;
	}

	virtual bool Create(IDMLDevice* Device, TConstArrayView<NNE::Internal::FTensorRef> InputTensors, TConstArrayView<NNE::Internal::FTensorRef> OutputTensors) override
	{
		const NNE::Internal::FTensor& InputTensor = *InputTensors[0];
		const NNE::Internal::FTensor& OutputTensor = *OutputTensors[0];

		// Initialize tensor descriptor (it's same for both input and output)
		FTensorDescDml	DmlTensorDesc;

		if (!DmlTensorDesc
				.SetFromTensor(InputTensor)
				.Validate())
		{
			UE_LOG(LogNNE, Error, TEXT("Failed to initialize tensor(s) for DML inference"));
			return false;
		}

		DmlElementWiseOpDescType	DmlElemWiseOpDesc{};

		InitDmlOpDesc(DmlElemWiseOpDesc, *DmlTensorDesc.GetDmlDesc());

		DML_OPERATOR_DESC DmlOpDesc{};

		DmlOpDesc.Type = DmlElementWiseOpType;
		DmlOpDesc.Desc = &DmlElemWiseOpDesc;

		return CreateOperator(Device, DmlOpDesc);
	}

private:

	template<typename OpDesc>
	void InitDmlOpDesc(OpDesc& Desc, const DML_TENSOR_DESC& TensorDesc)
	{
		Desc.InputTensor = &TensorDesc;
		Desc.OutputTensor = &TensorDesc;
	}

	void InitDmlOpDesc(DML_ELEMENT_WISE_CLIP_OPERATOR_DESC& Desc, const DML_TENSOR_DESC& TensorDesc)
	{
		Desc.InputTensor = &TensorDesc;
		Desc.OutputTensor = &TensorDesc;
		Desc.Min = Min;
		Desc.Max = Max;
	}
};


/**
 * Element-wise binary ML operator implementation
 */
template
<
	typename DmlElementWiseOpDescType, 
	DML_OPERATOR_TYPE DmlElementWiseOpType,
	TCHAR const *OpName
>
class FOperatorDmlElementWiseBinary : public FOperatorDml
{
	static constexpr uint32 NumAllowedInputTensors = 2, NumAllowedOutputTensors = 1;
public:

	static FOperatorDml* Create()
	{
		return new FOperatorDmlElementWiseBinary();
	}

	static bool Validate(const NNE::FAttributeMap& AttributeMap, TConstArrayView<ENNETensorDataType> InputTypes, TConstArrayView<NNE::FSymbolicTensorShape> InputShapes)
	{
		if(InputShapes.Num() != NumAllowedInputTensors)
		{
			UE_LOG(LogNNE, Warning, TEXT("DML %s: Invalid number of input tensors. %d provided, it should be %d."), OpName, InputShapes.Num(), NumAllowedInputTensors);
			return false;
		}

		if(!CheckElementwiseTensor(OpName, InputTypes[0], InputShapes[0]))
		{
			return false;
		}

		if(!CheckElementwiseTensor(OpName, InputTypes[1], InputShapes[1]))
		{
			return false;
		}

		return true;
	}

private:

	FOperatorDmlElementWiseBinary() = default;

public:

	virtual bool Initialize(TConstArrayView<NNE::FTensorDesc> Inputs, TConstArrayView<NNE::FTensorDesc> Outputs, const NNE::FAttributeMap& Attributes) override
	{
		return true;
	}

	virtual int PrepareOutputs(TConstArrayView<NNE::Internal::FTensorRef> InputTensors, TArrayView<NNE::Internal::FTensorRef> OutputTensors) const override
	{
		check(InputTensors.Num() == NumAllowedInputTensors);
		check(OutputTensors.Num() == NumAllowedOutputTensors);

		TConstArrayView<uint32> ShapeA = InputTensors[0]->GetShape().GetData();
		TConstArrayView<uint32> ShapeB = InputTensors[1]->GetShape().GetData();
		const int32 OutRank = FMath::Max(ShapeA.Num(), ShapeB.Num());
		
		TArray<uint32> OutShape;

		OutShape.SetNumUninitialized(OutRank);
		for (int32 i = 0; i < OutRank; ++i)
		{
			int32 IndexA = ShapeA.Num() - 1 - i;
			int32 IndexB = ShapeB.Num() - 1 - i;
			int32 ValueA = IndexA >= 0 ? ShapeA[IndexA] : 1;
			int32 ValueB = IndexB >= 0 ? ShapeB[IndexB] : 1;
			
			if (ValueA != ValueB && ValueA != 1 && ValueB != 1)
			{
				UE_LOG(LogNNE, Warning, TEXT("Error while computing shape for element wise binary op, input shapes are not compatible"));
				return -1;
			}
			
			OutShape[OutRank - 1 - i] = FMath::Max(ValueA, ValueB);
		}

		OutputTensors[0]->SetShape(NNE::FTensorShape::Make(OutShape));

		return 0;
	}

	virtual bool Create(IDMLDevice* Device, TConstArrayView<NNE::Internal::FTensorRef> InputTensors, TConstArrayView<NNE::Internal::FTensorRef> OutputTensors) override
	{
		const NNE::Internal::FTensor& InputATensor = *InputTensors[0];
		const NNE::Internal::FTensor& InputBTensor = *InputTensors[1];
		const NNE::Internal::FTensor& OutputTensor = *OutputTensors[0];

		// Initialize tensor descriptors
		FTensorDescDml	DmlInputATensorDesc;
		FTensorDescDml	DmlInputBTensorDesc;
		FTensorDescDml	DmlOutputTensorDesc;

		if (!DmlInputATensorDesc
				.SetFromTensorBroadcast(InputATensor, OutputTensor.GetShape())
				.Validate())
		{
			UE_LOG(LogNNE, Error, TEXT("Failed to initialize tensor(s) for DML inference"));
			return false;
		}

		if (!DmlInputBTensorDesc
				.SetFromTensorBroadcast(InputBTensor, OutputTensor.GetShape())
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

		DmlElementWiseOpDescType	DmlElemWiseOpDesc{};

		InitDmlOpDesc(DmlElemWiseOpDesc, *DmlInputATensorDesc.GetDmlDesc(), *DmlInputBTensorDesc.GetDmlDesc(), *DmlOutputTensorDesc.GetDmlDesc());

		DML_OPERATOR_DESC DmlOpDesc{};

		DmlOpDesc.Type = DmlElementWiseOpType;
		DmlOpDesc.Desc = &DmlElemWiseOpDesc;

		return CreateOperator(Device, DmlOpDesc);
	}

private:

	template<typename OpDesc>
	void InitDmlOpDesc(OpDesc& Desc, const DML_TENSOR_DESC& LHSTensorDesc, const DML_TENSOR_DESC& RHSTensorDesc, const DML_TENSOR_DESC& OutputTensorDesc)
	{
		Desc.ATensor = &LHSTensorDesc;
		Desc.BTensor = &RHSTensorDesc;
		Desc.OutputTensor = &OutputTensorDesc;
	}

	void InitDmlOpDesc(DML_ELEMENT_WISE_POW_OPERATOR_DESC& Desc, const DML_TENSOR_DESC& LHSTensorDesc, const DML_TENSOR_DESC& RHSTensorDesc, const DML_TENSOR_DESC& OutputTensorDesc)
	{
		Desc.InputTensor = &LHSTensorDesc;
		Desc.ExponentTensor = &RHSTensorDesc;
		Desc.OutputTensor = &OutputTensorDesc;
	}
};

#define REGISTER_OP_ELEMENT_WISE(OpName, DmlOpName, OpClass) \
TCHAR const Op##OpName##Name[] = TEXT(#OpName); \
struct FDmlOperator##OpName##Registrator \
{ \
	FDmlOperator##OpName##Registrator() \
	{ \
		FOperatorRegistryDml::Get()->OpAdd({{TEXT(#OpName), TEXT("Onnx")}}, OpClass<DML_ELEMENT_WISE_##DmlOpName##_OPERATOR_DESC, DML_OPERATOR_ELEMENT_WISE_##DmlOpName, Op##OpName##Name>::Create, OpClass<DML_ELEMENT_WISE_##DmlOpName##_OPERATOR_DESC, DML_OPERATOR_ELEMENT_WISE_##DmlOpName, Op##OpName##Name>::Validate); \
	} \
}; \
\
static FDmlOperator##OpName##Registrator RegisterDmlOperator##OpName;


#define REGISTER_OP_ELEMENT_WISE_UNARY(OpName, DmlOpName) REGISTER_OP_ELEMENT_WISE(OpName, DmlOpName, FOperatorDmlElementWiseUnary)

#define REGISTER_OP_ELEMENT_WISE_BINARY(OpName, DmlOpName) REGISTER_OP_ELEMENT_WISE(OpName, DmlOpName, FOperatorDmlElementWiseBinary)

REGISTER_OP_ELEMENT_WISE_UNARY( Abs,				ABS )
REGISTER_OP_ELEMENT_WISE_UNARY( Acos, 				ACOS )
REGISTER_OP_ELEMENT_WISE_UNARY( Acosh, 				ACOSH )
REGISTER_OP_ELEMENT_WISE_UNARY( Asin, 				ASIN )
REGISTER_OP_ELEMENT_WISE_UNARY( Asinh, 				ASINH )
REGISTER_OP_ELEMENT_WISE_UNARY( Atan,				ATAN )
REGISTER_OP_ELEMENT_WISE_UNARY( Atanh, 				ATANH )
REGISTER_OP_ELEMENT_WISE_UNARY( Ceil,				CEIL )
REGISTER_OP_ELEMENT_WISE_UNARY( Clip, 				CLIP )
REGISTER_OP_ELEMENT_WISE_UNARY( Cos,				COS )
REGISTER_OP_ELEMENT_WISE_UNARY( Cosh,				COSH )
REGISTER_OP_ELEMENT_WISE_UNARY( Erf,				ERF )
REGISTER_OP_ELEMENT_WISE_UNARY( Exp, 				EXP )
REGISTER_OP_ELEMENT_WISE_UNARY( Floor,				FLOOR )
REGISTER_OP_ELEMENT_WISE_UNARY( IsInf, 				IS_INFINITY )
REGISTER_OP_ELEMENT_WISE_UNARY( IsNan, 				IS_NAN )
REGISTER_OP_ELEMENT_WISE_UNARY( Log,				LOG )
REGISTER_OP_ELEMENT_WISE_UNARY( Neg,				NEGATE )
REGISTER_OP_ELEMENT_WISE_UNARY( Reciprocal, 		RECIP )
REGISTER_OP_ELEMENT_WISE_UNARY( Round,				ROUND )
REGISTER_OP_ELEMENT_WISE_UNARY( Sign,				SIGN )
REGISTER_OP_ELEMENT_WISE_UNARY( Sin,				SIN )
REGISTER_OP_ELEMENT_WISE_UNARY( Sinh,				SINH )
REGISTER_OP_ELEMENT_WISE_UNARY( Sqrt,				SQRT )
REGISTER_OP_ELEMENT_WISE_UNARY( Tan,				TAN )
REGISTER_OP_ELEMENT_WISE_UNARY( Tanh,				TANH )

REGISTER_OP_ELEMENT_WISE_BINARY( Add,				ADD )
REGISTER_OP_ELEMENT_WISE_BINARY( Div,				DIVIDE )
REGISTER_OP_ELEMENT_WISE_BINARY( Mul,				MULTIPLY )
REGISTER_OP_ELEMENT_WISE_BINARY( Pow,				POW )
REGISTER_OP_ELEMENT_WISE_BINARY( Sub,				SUBTRACT )

} // namespace UE::NNERuntimeRDG::Private::Dml

#endif // NNE_USE_DIRECTML