// Copyright Epic Games, Inc. All Rights Reserved.

#include "Trace/DataProcessors/ChaosVDSceneQueryVisitDataProcessor.h"

#include "ChaosVDRecording.h"
#include "DataWrappers/ChaosVDQueryDataWrappers.h"
#include "Serialization/MemoryReader.h"
#include "Trace/ChaosVDTraceProvider.h"


FChaosVDSceneQueryVisitDataProcessor::FChaosVDSceneQueryVisitDataProcessor() : IChaosVDDataProcessor(FChaosVDQueryVisitStep::WrapperTypeName)
{
}

bool FChaosVDSceneQueryVisitDataProcessor::ProcessRawData(const TArray<uint8>& InData)
{
	const TSharedPtr<FChaosVDTraceProvider> ProviderSharedPtr = TraceProvider.Pin();
	if (!ensure(ProviderSharedPtr.IsValid()))
	{
		return false;
	}

	if (const TSharedPtr<FChaosVDGameFrameData> CurrentFrameData = ProviderSharedPtr->GetCurrentGameFrame().Pin())
	{
		FChaosVDQueryVisitStep VisitStepData;

		FMemoryReader MemReader(InData);

		VisitStepData.Serialize(MemReader);
		
		if (TSharedPtr<FChaosVDQueryDataWrapper>* QueryDataPtrPtr = CurrentFrameData->RecordedSceneQueries.Find(VisitStepData.OwningQueryID))
		{
			TSharedPtr<FChaosVDQueryDataWrapper> QueryDataPtr = *QueryDataPtrPtr;
			if (QueryDataPtrPtr->IsValid())
			{
				if (VisitStepData.HitData.HasValidData())
				{
					// Quick and dirty way of show the hits in the details panel. If copying this data around becomes a bottle neck we can write a customization layout for it
					QueryDataPtr->Hits.Add(VisitStepData);
				}
	
				QueryDataPtr->SQVisitData.Emplace(MoveTemp(VisitStepData));
			}
		}
	}

	return true;
}