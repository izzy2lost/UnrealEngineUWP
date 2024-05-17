// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "StateTreeNodeBase.h"
#include "StateTreeConsiderationBase.generated.h"

struct FStateTreeExecutionContext;
enum class EAlphaBlendOption : uint8;
enum class EStateTreeExpressionOperand : uint8;

USTRUCT()
struct STATETREEMODULE_API FStateTreeConsiderationResponseCurve
{
	GENERATED_BODY()

	FStateTreeConsiderationResponseCurve();

	/* Optional Curve used to output the final normalized score.If it is set empty, the final value will be raw score normalized by bounds */
	UPROPERTY(EditAnywhere, Category = Default)
	EAlphaBlendOption BlendOption;

	/* Lower Bound used to normalize the raw score */
	UPROPERTY(EditAnywhere, Category = Default)
	float RawScoreLowerBound;

	/* Upper Bound used to normalize the raw score */
	UPROPERTY(EditAnywhere, Category = Default)
	float RawScoreUpperBound;
};

/**
 * This feature is experimental and the API is expected to change. 
 * Base struct for all utility considerations.
 */
USTRUCT(meta = (Hidden))
struct STATETREEMODULE_API FStateTreeConsiderationBase : public FStateTreeNodeBase
{
	GENERATED_BODY()

	FStateTreeConsiderationBase();

	float ComputeNormalizedScore(FStateTreeExecutionContext& Context) const;

protected:
	virtual float ComputeRawScore(FStateTreeExecutionContext& Context) const { return .0f; }

public:
	UPROPERTY()
	EStateTreeExpressionOperand Operand;

	UPROPERTY()
	int8 DeltaIndent;

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
