// Copyright Epic Games, Inc. All Rights Reserved.

#include "Expressions/Filter/TG_Expression_Blur.h"

#include "Transform/Expressions/T_Blur.h"

void UTG_Expression_Blur::Evaluate(FTG_EvaluationContext* InContext)
{
	Super::Evaluate(InContext);

	if(Input)
	{
		if(BlurType == EBlurType::Gaussian)
		{
			Output = T_Blur::CreateGaussian(InContext->Cycle, Output.GetBufferDescriptor(), Input, Radius, InContext->TargetId);
		}
		else if(BlurType == EBlurType::Radial)
		{
			Output = T_Blur::CreateRadial(InContext->Cycle, Output.GetBufferDescriptor(), Input, Radius, Strength, InContext->TargetId);
		}
		else if(BlurType == EBlurType::Directional)
		{
			Output = T_Blur::CreateDirectional(InContext->Cycle, Output.GetBufferDescriptor(), Input, Angle, Strength, InContext->TargetId);
		}
	}
	else
	{
		Output = TextureHelper::GetBlack();
	}

}
