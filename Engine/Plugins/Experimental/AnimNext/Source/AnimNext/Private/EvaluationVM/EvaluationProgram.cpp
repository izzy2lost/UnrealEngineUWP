// Copyright Epic Games, Inc. All Rights Reserved.

#include "EvaluationVM/EvaluationProgram.h"

#include "EvaluationVM/EvaluationTask.h"
#include "EvaluationVM/EvaluationVM.h"

namespace UE::AnimNext
{
	bool FEvaluationProgram::IsEmpty() const
	{
		return Tasks.IsEmpty();
	}

	void FEvaluationProgram::Execute(FEvaluationVM& VM) const
	{
		if (ensure(VM.IsValid()))
		{
			for (const TUniquePtr<FAnimNextEvaluationTask>& Task : Tasks)
			{
				Task->Execute(VM);
			}
		}
	}
}
