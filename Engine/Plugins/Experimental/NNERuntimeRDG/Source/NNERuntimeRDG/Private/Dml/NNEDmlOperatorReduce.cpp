// Copyright Epic Games, Inc. All Rights Reserved.

#ifdef NNE_USE_DIRECTML

#include "NNEDmlOperator.h"
#include "NNEDmlOperatorUtils.h"

namespace UE::NNERuntimeRDG::Private::Dml
{

//
// Reduce operator covers:
// ReduceL1
// ReduceL2
// ReduceLogSum
// ReduceLogSumExp
// ReduceMin
// ReduceMax
// ReduceMean
// ReduceProd
// ReduceSum
// ReduceSumSquare
template<DML_REDUCE_FUNCTION ReduceFunc>
class FOperatorDmlReduce : public FOperatorDml
{
	//
	//
	//
	inline static void HandleEmptyAxes(TArray<int32>& Axes, int32 Rank)
	{
		if (Axes.IsEmpty())
		{
			Axes.SetNumUninitialized(Rank);

			for (int32 Idx = 0; Idx < Rank; ++Idx)
			{
				Axes[Idx] = Idx;
			}
		}
	}

	Util::FSmallUIntArray			Axes;
	mutable Util::FSmallUIntArray	ReducedDims;
	mutable Util::FSmallArray<bool>	IsReducedDims;
	int32							KeepDims;
	DML_AXIS_DIRECTION				AxisDirection{ DML_AXIS_DIRECTION_INCREASING };

public:

	//
	//
	//
	static FOperatorDml* Create()
	{
		return new FOperatorDmlReduce<ReduceFunc>();
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
		checkf(Inputs.Num() == 1, TEXT("Dml Reduce op supports only 1 input"));
		check(Outputs.Num() == 1);

		KeepDims = Attributes.GetValueOrDefault<int32>(TEXT("keepdims"), 1);

		if constexpr (ReduceFunc == DML_REDUCE_FUNCTION_ARGMAX || ReduceFunc == DML_REDUCE_FUNCTION_ARGMIN)
		{
			AxisDirection = (DML_AXIS_DIRECTION) Attributes.GetValueOrDefault<int32>(TEXT("select_last_index"), 0);
		}

		TArray<int32>	OnnxAxes;

		if constexpr (ReduceFunc == DML_REDUCE_FUNCTION_ARGMAX || ReduceFunc == DML_REDUCE_FUNCTION_ARGMIN)
		{
			OnnxAxes.Add(Attributes.GetValueOrDefault<int32>(TEXT("axis"), 0));
		}
		else
		{
			const FNNEAttributeValue* AxesAttr = Attributes.GetAttributeValue(TEXT("axes"));
			if (AxesAttr)
			{
				OnnxAxes = AxesAttr->GetValue<TArray<int32>>();
			}
		}

		HandleNegativeAxes(OnnxAxes, Inputs[0].GetShape().Rank());
		HandleEmptyAxes(OnnxAxes, Inputs[0].GetShape().Rank());

		const int32 InputRank = Inputs[0].GetShape().Rank();

		for (int32& Dim : OnnxAxes)
		{
			checkf(Dim < InputRank, TEXT("Index out of bounds for Reduce axis"));
			Axes.Add(Dim);
		}

		return true;
	}

	//
	//
	//
	virtual int PrepareOutputs(TConstArrayView<NNE::Internal::FTensorRef> InputTensors, TArrayView<NNE::Internal::FTensorRef> OutputTensors) const override
	{
		TConstArrayView<uint32> InputShape = InputTensors[0]->GetShape().GetData();
		Util::FSmallUIntArray	OutputShape;

		ReducedDims.Reset();
		ReducedDims.Append(InputShape);

		for (const uint32& Dim : Axes)
		{
			checkf(Dim < (uint32) ReducedDims.Num(), TEXT("Index out of bounds for Reduce axis"));
			ReducedDims[Dim] = 1;
		}

		if (!KeepDims)
		{
			IsReducedDims.SetNumZeroed(ReducedDims.Num());
			for (const uint32& Dim : Axes)
			{
				IsReducedDims[Dim] = true;
			}

			for (int32 Idx = 0; Idx < ReducedDims.Num(); ++Idx)
			{
				if (!IsReducedDims[Idx])
				{
					OutputShape.Add(ReducedDims[Idx]);
				}
			}			
		}
		else
		{
			OutputShape.Append(ReducedDims);
		}
		
		OutputTensors[0]->SetShape(NNE::FTensorShape::Make(OutputShape));

		return 0;
	}

	//
	//
	//
	virtual bool Create(IDMLDevice* Device, TConstArrayView<NNE::Internal::FTensorRef> InputTensors, TConstArrayView<NNE::Internal::FTensorRef> OutputTensors) override
	{
		FTensorDescDml DmlInputTensorDesc;
		
		if (!DmlInputTensorDesc
				.SetFromTensor(*InputTensors[0])
				.Validate())
		{
			UE_LOG(LogNNE, Warning, TEXT("Failed to initialize Reduce input tensor for DML inference"));
			return false;
		}

		Util::FSmallUIntArray	OutputShape;

		if (KeepDims)
		{
			OutputShape.Append(ReducedDims);
		}
		else
		{
			// Example:
			//     input dims: {3, 2, 2}
			//     axes: 1
			//     keepDims: 0
			// 
			// Shape: {3, 2}, but DML: {3, 1, 2}	
			TConstArrayView<uint32> InputShape = InputTensors[0]->GetShape().GetData();

			OutputShape.SetNum(ReducedDims.Num());
			for (int32 Idx = 0; Idx < OutputShape.Num(); ++Idx)
			{
				OutputShape[Idx] = IsReducedDims[Idx] ? 1 : InputShape[Idx];
			}
		}
		
		FTensorDescDml DmlOutputTensorDesc;

		if (!DmlOutputTensorDesc
				.SetFromTensor(*OutputTensors[0])
				.SetShape(OutputShape)
				.Validate())
		{
			UE_LOG(LogNNE, Warning, TEXT("Failed to initialize Reduce output tensor for DML inference"));
			return false;
		}

		if constexpr (ReduceFunc == DML_REDUCE_FUNCTION_ARGMAX)
        {
            DML_ARGMAX_OPERATOR_DESC OpDesc;
            OpDesc.AxisDirection = AxisDirection;
            OpDesc.InputTensor = DmlInputTensorDesc.GetDmlDesc();
			OpDesc.OutputTensor = DmlOutputTensorDesc.GetDmlDesc();
            OpDesc.Axes = Axes.GetData();
            OpDesc.AxisCount = (uint32) Axes.Num();

			return CreateOperator(Device, DML_OPERATOR_DESC { DML_OPERATOR_ARGMAX, &OpDesc });
        }
        else if constexpr (ReduceFunc == DML_REDUCE_FUNCTION_ARGMIN)
        {
            DML_ARGMIN_OPERATOR_DESC OpDesc;
            OpDesc.AxisDirection = AxisDirection;
            OpDesc.InputTensor = DmlInputTensorDesc.GetDmlDesc();
			OpDesc.OutputTensor = DmlOutputTensorDesc.GetDmlDesc();
            OpDesc.Axes = Axes.GetData();
            OpDesc.AxisCount = (uint32) Axes.Num();

            return CreateOperator(Device, DML_OPERATOR_DESC { DML_OPERATOR_ARGMIN, &OpDesc });
        }
		else
		{
			DML_REDUCE_OPERATOR_DESC OpDesc{};

			OpDesc.InputTensor = DmlInputTensorDesc.GetDmlDesc();
			OpDesc.OutputTensor = DmlOutputTensorDesc.GetDmlDesc();
			OpDesc.Function = ReduceFunc;
			OpDesc.Axes = Axes.GetData();
			OpDesc.AxisCount = (uint32) Axes.Num();

			return CreateOperator(Device, DML_OPERATOR_DESC{ DML_OPERATOR_REDUCE, &OpDesc} );
		}
	}
};

// Register Reshape operator on Module startup
#define OP(OpName, ReduceFunc) FOperatorRegistryDml::Get()->OpAdd(TEXT(#OpName), FOperatorDmlReduce<ReduceFunc>::Create)

struct FOperatorDmlReduceRegistrator
{
	FOperatorDmlReduceRegistrator()
	{
		OP(ReduceL1,		DML_REDUCE_FUNCTION_L1);
		OP(ReduceL2,		DML_REDUCE_FUNCTION_L2);
		OP(ReduceLogSum,	DML_REDUCE_FUNCTION_LOG_SUM);
		OP(ReduceLogSumExp,	DML_REDUCE_FUNCTION_LOG_SUM_EXP);
		OP(ReduceMin,		DML_REDUCE_FUNCTION_MIN);
		OP(ReduceMax,		DML_REDUCE_FUNCTION_MAX);
		OP(ReduceMean,		DML_REDUCE_FUNCTION_AVERAGE);
		OP(ReduceProd,		DML_REDUCE_FUNCTION_MULTIPLY);
		OP(ReduceSum,		DML_REDUCE_FUNCTION_SUM);
		OP(ReduceSumSquare,	DML_REDUCE_FUNCTION_SUM_SQUARE);
		OP(ArgMax,			DML_REDUCE_FUNCTION_ARGMAX);
		OP(ArgMin,			DML_REDUCE_FUNCTION_ARGMIN);
	}
};

#undef OP

static FOperatorDmlReduceRegistrator RegisterReduceOperators;


} // namespace UE::NNERuntimeRDG::Private::Dml

#endif // NNE_USE_DIRECTML
