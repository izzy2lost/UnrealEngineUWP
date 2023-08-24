// Copyright Epic Games, Inc. All Rights Reserved.

#include "Tasks/SimpleTargetingSelectionTask.h"
#include "GameFramework/Actor.h"

void USimpleTargetingSelectionTask::Execute(const FTargetingRequestHandle& TargetingHandle) const
{
	Super::Execute(TargetingHandle);

	SetTaskAsyncState(TargetingHandle, ETargetingTaskAsyncState::Executing);

	if (FTargetingSourceContext* SourceContext = FTargetingSourceContext::Find(TargetingHandle))
	{
		// Call the blueprint function
		SelectTargets(TargetingHandle, *SourceContext);
	}
	SetTaskAsyncState(TargetingHandle, ETargetingTaskAsyncState::Completed);
}

bool USimpleTargetingSelectionTask::AddTargetActor(const FTargetingRequestHandle& TargetingHandle, AActor* Actor) const
{
	if (Actor)
	{
		FTargetingDefaultResultsSet& ResultsSet = FTargetingDefaultResultsSet::FindOrAdd(TargetingHandle);
		if (!ResultsSet.TargetResults.FindByPredicate([Actor](const FTargetingDefaultResultData& ResultData){ return ResultData.HitResult.GetActor() == Actor; }))
		{
			FTargetingDefaultResultData& ResultData = ResultsSet.TargetResults.AddDefaulted_GetRef();
			ResultData.HitResult.HitObjectHandle = FActorInstanceHandle(Actor);
			ResultData.HitResult.Location = Actor->GetActorLocation();
			return true;
		}
	}
	return false;
}

bool USimpleTargetingSelectionTask::AddHitResult(const FTargetingRequestHandle& TargetingHandle, const FHitResult& HitResult) const
{
	FTargetingDefaultResultsSet& ResultsSet = FTargetingDefaultResultsSet::FindOrAdd(TargetingHandle);
	if (!ResultsSet.TargetResults.FindByPredicate([HitResult](const FTargetingDefaultResultData& ResultData){ return ResultData.HitResult.GetActor() == HitResult.GetActor(); }))
	{
		FTargetingDefaultResultData& ResultData = ResultsSet.TargetResults.AddDefaulted_GetRef();
		ResultData.HitResult = HitResult;
		return true;
	}
	return false;
}
