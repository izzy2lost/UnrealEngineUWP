// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ChaosLog.h"
#include "CoreMinimal.h"
#include "Dataflow/DataflowConnection.h"
#include "Dataflow/DataflowNodeParameters.h"
#include "Dataflow/DataflowNode.h"

#include "DataflowMathNodes.generated.h"

#define DATAFLOW_MATH_NODES_CATEGORY "Math|Scalar"

/** One input operators base class */
USTRUCT()
struct FDataflowMathOneInputOperatorNode : public FDataflowNode
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, Category = Operands, meta = (DataflowInput));
	FDataflowNumericTypes A;

	UPROPERTY(meta = (DataflowOutput))
	FDataflowNumericTypes Result;

public:
	FDataflowMathOneInputOperatorNode() {};
	FDataflowMathOneInputOperatorNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid());
	virtual void Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const override;

protected:
	void RegisterInputsAndOutputs();
	virtual double ComputeResult(Dataflow::FContext& Context, double InA) const { ensure(false); return 0.0; };
};

/** Two inputs operators base class */
USTRUCT()
struct FDataflowMathTwoInputsOperatorNode : public FDataflowNode
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, Category = Operands, meta = (DataflowInput));
	FDataflowNumericTypes A;

	UPROPERTY(EditAnywhere, Category = Operands, meta = (DataflowInput));
	FDataflowNumericTypes B;

	UPROPERTY(meta = (DataflowOutput))
	FDataflowNumericTypes Result;

public:
	FDataflowMathTwoInputsOperatorNode() {};
	FDataflowMathTwoInputsOperatorNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid());
	virtual void Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const override;

protected:
	void RegisterInputsAndOutputs();
	virtual double ComputeResult(Dataflow::FContext& Context, double InA, double InB) const { ensure(false); return 0.0; };
};

/** Addition (A + B) */
USTRUCT()
struct FDataflowMathAddNode : public FDataflowMathTwoInputsOperatorNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FDataflowMathAddNode, "Add", DATAFLOW_MATH_NODES_CATEGORY, "")

public:
	FDataflowMathAddNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid());
	virtual double ComputeResult(Dataflow::FContext& Context, double InA, double InB) const override;
};

/** Subtraction (A - B) */
USTRUCT()
struct FDataflowMathSubtractNode : public FDataflowMathTwoInputsOperatorNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FDataflowMathSubtractNode, "Subtract", DATAFLOW_MATH_NODES_CATEGORY, "")

public:
	FDataflowMathSubtractNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid());
	virtual double ComputeResult(Dataflow::FContext& Context, double InA, double InB) const override;
};

/** Multiplication (A * B) */
USTRUCT()
struct FDataflowMathMultiplyNode : public FDataflowMathTwoInputsOperatorNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FDataflowMathMultiplyNode, "Multiply", DATAFLOW_MATH_NODES_CATEGORY, "")

public:
	FDataflowMathMultiplyNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid());
	virtual double ComputeResult(Dataflow::FContext& Context, double InA, double InB) const override;
};

/**
* Division (A / B)
* if B is equal to 0, 0 is returned Fallback value
*/
USTRUCT()
struct FDataflowMathDivideNode : public FDataflowMathTwoInputsOperatorNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FDataflowMathDivideNode, "Divide", DATAFLOW_MATH_NODES_CATEGORY, "")

	UPROPERTY(EditAnywhere, Category = SafeDivide, meta = (DataflowInput));
	FDataflowNumericTypes Fallback;

public:
	FDataflowMathDivideNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid());
	virtual double ComputeResult(Dataflow::FContext& Context, double InA, double InB) const override;
};

/** Minimum ( Min(A, B) ) */
USTRUCT()
struct FDataflowMathMinimumNode : public FDataflowMathTwoInputsOperatorNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FDataflowMathMinimumNode, "Minimum", DATAFLOW_MATH_NODES_CATEGORY, "")

public:
	FDataflowMathMinimumNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid());
	virtual double ComputeResult(Dataflow::FContext& Context, double InA, double InB) const override;
};

/** Maximum ( Max(A, B) ) */
USTRUCT()
struct FDataflowMathMaximumNode : public FDataflowMathTwoInputsOperatorNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FDataflowMathMaximumNode, "Maximum", DATAFLOW_MATH_NODES_CATEGORY, "")

public:
	FDataflowMathMaximumNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid());
	virtual double ComputeResult(Dataflow::FContext& Context, double InA, double InB) const override;
};

/** 
* Reciprocal( 1 / A )
* if A is equal to 0, returns Fallback
*/
USTRUCT()
struct FDataflowMathReciprocalNode : public FDataflowMathOneInputOperatorNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FDataflowMathReciprocalNode, "Reciprocal", DATAFLOW_MATH_NODES_CATEGORY, "")

	UPROPERTY(EditAnywhere, Category = SafeDivide, meta = (DataflowInput));
	FDataflowNumericTypes Fallback;

public:
	FDataflowMathReciprocalNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid());
	virtual double ComputeResult(Dataflow::FContext& Context, double InA) const override;
};

/** Square ( A * A ) */
USTRUCT()
struct FDataflowMathSquareNode : public FDataflowMathOneInputOperatorNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FDataflowMathSquareNode, "Square", DATAFLOW_MATH_NODES_CATEGORY, "")

public:
	FDataflowMathSquareNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid());
	virtual double ComputeResult(Dataflow::FContext& Context, double InA) const override;
};

/** Cube ( A * A * A ) */
USTRUCT()
struct FDataflowMathCubeNode : public FDataflowMathOneInputOperatorNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FDataflowMathCubeNode, "Cube", DATAFLOW_MATH_NODES_CATEGORY, "")

public:
	FDataflowMathCubeNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid());
	virtual double ComputeResult(Dataflow::FContext& Context, double InA) const override;
};

/** Square Root ( sqrt(A) ) */
USTRUCT()
struct FDataflowMathSquareRootNode : public FDataflowMathOneInputOperatorNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FDataflowMathSquareRootNode, "SquareRoot", DATAFLOW_MATH_NODES_CATEGORY, "")

public:
	FDataflowMathSquareRootNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid());
	virtual double ComputeResult(Dataflow::FContext& Context, double InA) const override;
};

/** 
* Inverse Square Root ( 1 / sqrt(A) ) 
* if A is equal to 0, returns Fallback
*/
USTRUCT()
struct FDataflowMathInverseSquareRootNode : public FDataflowMathOneInputOperatorNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FDataflowMathInverseSquareRootNode, "InverseSquareRoot", DATAFLOW_MATH_NODES_CATEGORY, "")

	UPROPERTY(EditAnywhere, Category = SafeDivide, meta = (DataflowInput));
	FDataflowNumericTypes Fallback;

public:
	FDataflowMathInverseSquareRootNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid());
	virtual double ComputeResult(Dataflow::FContext& Context, double InA) const override;
};

/** Negate ( -A ) */
USTRUCT()
struct FDataflowMathNegateNode : public FDataflowMathOneInputOperatorNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FDataflowMathNegateNode, "Negate", DATAFLOW_MATH_NODES_CATEGORY, "")

	UPROPERTY(EditAnywhere, Category = SafeDivide, meta = (DataflowInput));
	FDataflowNumericTypes Fallback;

public:
	FDataflowMathNegateNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid());
	virtual double ComputeResult(Dataflow::FContext& Context, double InA) const override;
};

/** Absolute value ( |A| ) */
USTRUCT()
struct FDataflowMathAbsNode : public FDataflowMathOneInputOperatorNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FDataflowMathAbsNode, "Abs", DATAFLOW_MATH_NODES_CATEGORY, "")

	UPROPERTY(EditAnywhere, Category = SafeDivide, meta = (DataflowInput));
	FDataflowNumericTypes Fallback;

public:
	FDataflowMathAbsNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid());
	virtual double ComputeResult(Dataflow::FContext& Context, double InA) const override;
};

/** Floor ( 1.4 => 1.0 | 1.9 => 1.0 | -5.3 => -6.0 ) */
USTRUCT()
struct FDataflowMathFloorNode : public FDataflowMathOneInputOperatorNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FDataflowMathFloorNode, "Floor", DATAFLOW_MATH_NODES_CATEGORY, "")

public:
	FDataflowMathFloorNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid());
	virtual double ComputeResult(Dataflow::FContext& Context, double InA) const override;
};

/** Ceil ( 1.4 => 2.0 | 1.9 => 2.0 | -5.3 => -5.0) */
USTRUCT()
struct FDataflowMathCeilNode : public FDataflowMathOneInputOperatorNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FDataflowMathCeilNode, "Ceil", DATAFLOW_MATH_NODES_CATEGORY, "")

public:
	FDataflowMathCeilNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid());
	virtual double ComputeResult(Dataflow::FContext& Context, double InA) const override;
};

/** Round ( 1.4 => 1.0 | 1.9 => 2.0 | -5.3 => -5.0) */
USTRUCT()
struct FDataflowMathRoundNode : public FDataflowMathOneInputOperatorNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FDataflowMathRoundNode, "Round", DATAFLOW_MATH_NODES_CATEGORY, "")

public:
	FDataflowMathRoundNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid());
	virtual double ComputeResult(Dataflow::FContext& Context, double InA) const override;
};

/** Trunc ( 1.4 => 1.0 | 1.9 => 1.0 | -5.3 => -5.0) */
USTRUCT()
struct FDataflowMathTruncNode : public FDataflowMathOneInputOperatorNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FDataflowMathTruncNode, "Trunc", DATAFLOW_MATH_NODES_CATEGORY, "")

public:
	FDataflowMathTruncNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid());
	virtual double ComputeResult(Dataflow::FContext& Context, double InA) const override;
};

/** Frac ( 1.4 => 0.4 | 1.9 => 0.9 | -5.3 => 0.3 ) */
USTRUCT()
struct FDataflowMathFracNode : public FDataflowMathOneInputOperatorNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FDataflowMathFracNode, "Frac", DATAFLOW_MATH_NODES_CATEGORY, "")

public:
	FDataflowMathFracNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid());
	virtual double ComputeResult(Dataflow::FContext& Context, double InA) const override;
};

/** power ( A ^ B) */
USTRUCT()
struct FDataflowMathPowNode : public FDataflowMathTwoInputsOperatorNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FDataflowMathPowNode, "Pow", DATAFLOW_MATH_NODES_CATEGORY, "")

public:
	FDataflowMathPowNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid());
	virtual double ComputeResult(Dataflow::FContext& Context, double InA, double InB) const override;
};

/**
* Log for a specific base ( Log[Base](A) ) 
* If base is negative or zero returns 0
*/
USTRUCT()
struct FDataflowMathLogXNode : public FDataflowMathOneInputOperatorNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FDataflowMathLogXNode, "LogX", DATAFLOW_MATH_NODES_CATEGORY, "")

	UPROPERTY(EditAnywhere, Category = SafeDivide, meta = (DataflowInput));
	FDataflowNumericTypes Base;

public:
	FDataflowMathLogXNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid());
	virtual double ComputeResult(Dataflow::FContext& Context, double InA) const override;
};

/** Natural log ( Log(A) ) */
USTRUCT()
struct FDataflowMathLogNode : public FDataflowMathOneInputOperatorNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FDataflowMathLogNode, "Log", DATAFLOW_MATH_NODES_CATEGORY, "")

public:
	FDataflowMathLogNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid());
	virtual double ComputeResult(Dataflow::FContext& Context, double InA) const override;
};

/** Exponential ( Exp(A) ) */
USTRUCT()
struct FDataflowMathExpNode : public FDataflowMathOneInputOperatorNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FDataflowMathExpNode, "Exp", DATAFLOW_MATH_NODES_CATEGORY, "")

public:
	FDataflowMathExpNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid());
	virtual double ComputeResult(Dataflow::FContext& Context, double InA) const override;
};

/** return -1, 0, +1 whether the input is respectively negative, zero or positive ( Sign(A) ) */
USTRUCT()
struct FDataflowMathSignNode : public FDataflowMathOneInputOperatorNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FDataflowMathSignNode, "Sign", DATAFLOW_MATH_NODES_CATEGORY, "")

public:
	FDataflowMathSignNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid());
	virtual double ComputeResult(Dataflow::FContext& Context, double InA) const override;
};

namespace Dataflow
{
	void RegisterDataflowMathNodes();
}

/*
Nodes Left to be converted :
	FMathConstantsDataflowNode);
	FOneMinusDataflowNode);
	
	FFloatMathExpressionDataflowNode);
	FMathExpressionDataflowNode);
	
	FClampDataflowNode);
	FFitDataflowNode);
	FEFitDataflowNode);
	
	FLerpDataflowNode);
	FWrapDataflowNode);

	// trig
	FSinDataflowNode);
	FArcSinDataflowNode);
	FCosDataflowNode);
	FArcCosDataflowNode);
	FTanDataflowNode);
	FArcTanDataflowNode);
	FArcTan2DataflowNode);
	FRadiansToDegreesDataflowNode);
	FDegreesToRadiansDataflowNode);

	// vectors - requires Vector any type ? 
	FNormalizeToRangeDataflowNode);
	FScaleVectorDataflowNode);
	FDotProductDataflowNode);
	FCrossProductDataflowNode);
	FNormalizeDataflowNode);
	FLengthDataflowNode);
	FDistanceDataflowNode);
	FIsNearlyZeroDataflowNode);

	// random 
	FRandomFloatDataflowNode);
	FRandomFloatInRangeDataflowNode);
	FRandomUnitVectorDataflowNode);
	FRandomUnitVectorInConeDataflowNode);

*/