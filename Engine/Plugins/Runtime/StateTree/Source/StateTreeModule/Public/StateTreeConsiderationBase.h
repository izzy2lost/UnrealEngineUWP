// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AlphaBlend.h"
#include "StateTreeNodeBase.h"
#include "StateTreeTypes.h"
#include "StateTreeConsiderationBase.generated.h"

struct FStateTreeExecutionContext;

USTRUCT()
struct STATETREEMODULE_API FStateTreeConsiderationResponseCurve
{
	GENERATED_BODY()

	/* Optional Curve used to output the final normalized score.If it is set empty, the final value will be raw score normalized by bounds */
	UPROPERTY(EditAnywhere, Category = Default)
	EAlphaBlendOption BlendOption;

	/* Lower Bound used to normalize the raw score */
	UPROPERTY(EditAnywhere, Category = Default)
	float RawScoreLowerBound = .0f;

	/* Upper Bound used to normalize the raw score */
	UPROPERTY(EditAnywhere, Category = Default)
	float RawScoreUpperBound = 1.f;
};

/**
 * This feature is experimental and the API is expected to change. 
 * Base struct for all utility considerations.
 */
USTRUCT(meta = (Hidden))
struct STATETREEMODULE_API FStateTreeConsiderationBase : public FStateTreeNodeBase
{
	GENERATED_BODY()

	float ComputeNormalizedScore(FStateTreeExecutionContext& Context) const;

protected:
	virtual float ComputeRawScore(FStateTreeExecutionContext& Context) const { return .0f; }

public:
	UPROPERTY()
	EStateTreeExpressionOperand Operand = EStateTreeExpressionOperand::And;

	UPROPERTY()
	int8 DeltaIndent = 0;

	//Response Curve used to output the final normalized score.
	UPROPERTY(EditAnywhere, Category = Default)
	FStateTreeConsiderationResponseCurve ResponseCurve;
};

/**
 * Base class (namespace) for all common Utility Considerations that are generally applicable.
 * This allows schemas to safely include all considerations child of this struct.
 */
USTRUCT(meta = (Hidden))
struct STATETREEMODULE_API FStateTreeConsiderationCommonBase : public FStateTreeConsiderationBase
{
	GENERATED_BODY()
};
