// Copyright Epic Games, Inc. All Rights Reserved.


#include "RigUnit_EvaluateChooser.h"


FRigUnit_EvaluateChooser_Execute()
{
	Result = nullptr;

	if (ContextObject && Chooser)
	{
		FChooserEvaluationContext ChooserContext;
		ChooserContext.AddObjectParam(ContextObject);

		UChooserTable::EvaluateChooser(ChooserContext, Chooser, FObjectChooserBase::FObjectChooserIteratorCallback::CreateLambda([&Result](UObject* InResult)
			{
				Result = InResult;
				return FObjectChooserBase::EIteratorStatus::Stop;
			}));
	}
}
