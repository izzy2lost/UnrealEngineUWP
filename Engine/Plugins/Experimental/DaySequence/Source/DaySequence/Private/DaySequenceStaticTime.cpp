// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "DaySequenceStaticTime.h"

namespace UE::DaySequence
{
	void FStaticTimeManager::AddStaticTimeContributor(const FStaticTimeContributor& NewContributor)
	{
		if (!NewContributor.GetStaticTime || !NewContributor.WantsStaticTime || !NewContributor.UserObject.Get())
		{
			// Should be an ensure probably, we can't accept contributors that don't fulfill this requirement
			return;
		}

		// If contributor is already registered, just early out.
		if (Contributors.ContainsByPredicate([NewContributor](FStaticTimeContributor& Contributor)
			{ return NewContributor.UserObject == Contributor.UserObject; }))
		{
			return;
		}

		// Add the new contributor.
		Contributors.Add(NewContributor);
		
		// Sort the array in descending order by priority for efficient group processing.
		Contributors.Sort([](const FStaticTimeContributor& LHS, const FStaticTimeContributor& RHS)
			{ return LHS.Priority > RHS.Priority; });

		// Increment (or set to 1) the priority group size.
		PriorityGroupSizes.FindOrAdd(NewContributor.Priority, 0)++;
	}

	void FStaticTimeManager::RemoveStaticTimeContributor(const UObject* UserObject)
	{
		// Get the index of the contributor to remove.
		int32 RemoveIdx = Contributors.IndexOfByPredicate([UserObject](const FStaticTimeContributor& Contributor)
		{
			return Contributor.UserObject.Get() == UserObject;
		});

		// If contributor actually exists, decrement its group count and remove it.
		if (RemoveIdx != INDEX_NONE)
		{
			PriorityGroupSizes[Contributors[RemoveIdx].Priority]--;

			Contributors.RemoveAt(RemoveIdx, EAllowShrinking::No);
		}
	}
	
	bool FStaticTimeManager::HasStaticTime() const
	{
		for (const FStaticTimeContributor& Contributor : Contributors)
		{
			if (Contributor.UserObject.Get() && Contributor.WantsStaticTime())
			{
				return true;
			}
		}

		return false;
	}
	
	float FStaticTimeManager::GetStaticTime(float InitialTime) const
	{
		float AccumulatedWeight = 0.f;
		float AccumulatedTime = 0.f;
			
		// Process batches of contributors based on priority
		for (int32 PriorityGroupStartIdx = 0; PriorityGroupStartIdx < Contributors.Num();)
		{
			// Compute the index of the final element in this priority group
			const int32 CurrentPriority = Contributors[PriorityGroupStartIdx].Priority;
			const int32 GroupSize = PriorityGroupSizes.FindChecked(CurrentPriority);	// checked because if this isn't valid something is very wrong
			const int32 PriorityGroupEndIdx = PriorityGroupStartIdx + GroupSize - 1;
			
			// Process this group 
			const FStaticTimeInfo GroupInfo = ProcessPriorityGroup(PriorityGroupStartIdx, PriorityGroupEndIdx);

			const float EffectiveGroupWeight = (1.f - AccumulatedWeight) * GroupInfo.BlendWeight;
			const float EffectiveGroupTime = EffectiveGroupWeight * GroupInfo.StaticTime;

			AccumulatedWeight += EffectiveGroupWeight;
			AccumulatedTime += EffectiveGroupTime;

			// Advance to next priority group
			PriorityGroupStartIdx = PriorityGroupEndIdx + 1;
		}

		// Blend against initial value if necessary
		if (AccumulatedWeight < 1.f)
		{
			const float FillWeight = (1.f - AccumulatedWeight);
			const float FillTime = InitialTime * FillWeight;

			AccumulatedWeight += FillWeight;
			AccumulatedTime += FillTime;
		}

		// we could probably ensure(AccumulatedWeight is nearly equal to 1.f), leaving out for now

		return AccumulatedTime;
	}

	FStaticTimeInfo FStaticTimeManager::ProcessPriorityGroup(int32 StartIdx, int32 EndIdx) const
	{
		FStaticTimeInfo GroupInfo = {0.f, 0.f};
		int32 GroupContributors = 0;
		
		for (int32 CurrentIdx = StartIdx; CurrentIdx <= EndIdx; ++CurrentIdx)
		{
			const FStaticTimeContributor& CurrentContributor = Contributors[CurrentIdx];

			if (!CurrentContributor.UserObject.Get() || !CurrentContributor.WantsStaticTime())
			{
				continue;
			}

			// Only increment for active contributors
			// Note: Because removing a contributor results in a discrete change in an integer value, we get pops when contributor in a group of >1 contributors has a non-1 weight.
			GroupContributors++;
			
			FStaticTimeInfo ContributorInfo;
			CurrentContributor.GetStaticTime(ContributorInfo);

			// Accumulate contributor info (we will later divide these sums once we know how many contributors there are)
			GroupInfo.BlendWeight += ContributorInfo.BlendWeight;
			GroupInfo.StaticTime += ContributorInfo.StaticTime;
		}

		// Compute the average (before this point we have just summed all weights and times) for this group
		// If GroupContributors is 0 we treat it as 1 because that implies the values in GroupInfo are just 0.
		GroupInfo.BlendWeight /= FMath::Max(GroupContributors, 1);
		GroupInfo.StaticTime /= FMath::Max(GroupContributors, 1);
		
		return GroupInfo;
	}
}