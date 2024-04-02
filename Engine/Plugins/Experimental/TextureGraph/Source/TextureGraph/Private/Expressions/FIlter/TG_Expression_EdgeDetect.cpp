// Copyright Epic Games, Inc. All Rights Reserved.

#include "Expressions/Filter/TG_Expression_EdgeDetect.h"

#include "Transform/Expressions/T_EdgeDetect.h"

void UTG_Expression_EdgeDetect::Evaluate(FTG_EvaluationContext* InContext)
{
	Super::Evaluate(InContext);

	if (!Input)
	{
		Output = FTG_Texture::GetBlack();
		return;
	}

	Output = T_EdgeDetect::Create(InContext->Cycle, Output.GetBufferDescriptor(), Input, Thickness, InContext->TargetId);
}
