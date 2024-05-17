// Copyright Epic Games, Inc. All Rights Reserved.

#include "StateTreeConsiderationBase.h"
#include "AlphaBlend.h"
#include "StateTreeTypes.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(StateTreeConsiderationBase)

FStateTreeConsiderationResponseCurve::FStateTreeConsiderationResponseCurve()
	: BlendOption(EAlphaBlendOption::Linear)
	, RawScoreLowerBound(.0f)
	, RawScoreUpperBound(.1f)
{
}

FStateTreeConsiderationBase::FStateTreeConsiderationBase()
	: Operand(EStateTreeExpressionOperand::And)
	, DeltaIndent(0)
{
}

float FStateTreeConsiderationBase::ComputeNormalizedScore(FStateTreeExecutionContext& Context) const
{
	const float RawScore = ComputeRawScore(Context);
	const float NormalizedScore = FMath::Clamp<float>(
		FMath::GetRangePct(ResponseCurve.RawScoreLowerBound, 
			ResponseCurve.RawScoreUpperBound, 
			RawScore), 
		0.f, 1.f);
	
	constexpr class UCurveFloat* OptionalCurve = nullptr;
	return FAlphaBlend::AlphaToBlendOption(NormalizedScore, ResponseCurve.BlendOption, OptionalCurve);
}
