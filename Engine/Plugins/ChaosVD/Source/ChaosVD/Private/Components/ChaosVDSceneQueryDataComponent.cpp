// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/ChaosVDSceneQueryDataComponent.h"

#include "ChaosVDRecording.h"

UChaosVDSceneQueryDataComponent::UChaosVDSceneQueryDataComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	
	SetCanEverAffectNavigation(false);
	bNavigationRelevant = false;
}

void UChaosVDSceneQueryDataComponent::UpdateQueriesFromFrameData(const FChaosVDGameFrameData& InGameFrameData)
{
	const int32 RecordedQueriesNum = InGameFrameData.RecordedSceneQueries.Num();

	RecordedQueriesByType.Empty(RecordedQueriesNum);
	RecordedQueriesByID.Empty(RecordedQueriesNum);
	RecordedQueries.Empty(RecordedQueriesNum);

	for (const TPair<int32, TSharedPtr<FChaosVDQueryDataWrapper>>& QueryIDPair : InGameFrameData.RecordedSceneQueries)
	{
		if (TSharedPtr<FChaosVDQueryDataWrapper> QueryData = QueryIDPair.Value)
		{
			TArray<TSharedPtr<FChaosVDQueryDataWrapper>>& QueriesForType = RecordedQueriesByType.FindOrAdd(QueryData->Type);
			QueriesForType.Emplace(QueryData);

			RecordedQueriesByID.Add(QueryIDPair.Key, QueryData);
			RecordedQueries.Add(QueryData);
		}
	}
}

TConstArrayView<TSharedPtr<FChaosVDQueryDataWrapper>> UChaosVDSceneQueryDataComponent::GetQueriesByType(EChaosVDSceneQueryType Type) const
{
	if (const TArray<TSharedPtr<FChaosVDQueryDataWrapper>>* FoundQueries = RecordedQueriesByType.Find(Type))
	{
		return MakeArrayView(*FoundQueries);
	}

	return TArrayView<TSharedPtr<FChaosVDQueryDataWrapper>>();
}

TConstArrayView<TSharedPtr<FChaosVDQueryDataWrapper>> UChaosVDSceneQueryDataComponent::GetAllQueries() const
{
	return RecordedQueries;
}

TSharedPtr<FChaosVDQueryDataWrapper> UChaosVDSceneQueryDataComponent::GetQueryByID(int32 QueryID) const
{
	if (const TSharedPtr<FChaosVDQueryDataWrapper>* FoundQuery = RecordedQueriesByID.Find(QueryID))
	{
		return *FoundQuery;
	}

	return nullptr;
}

void UChaosVDSceneQueryDataComponent::ClearData()
{
	RecordedQueriesByType.Reset();
	RecordedQueriesByID.Reset();
	RecordedQueries.Reset();
}
