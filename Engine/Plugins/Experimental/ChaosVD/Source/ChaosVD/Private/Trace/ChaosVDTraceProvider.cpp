// Copyright Epic Games, Inc. All Rights Reserved.

#include "Trace/ChaosVDTraceProvider.h"

#include "ChaosVDModule.h"
#include "ChaosVDRecording.h"

#include "Chaos/ChaosArchive.h"

#include "Compression/OodleDataCompressionUtil.h"
#include "Serialization/MemoryReader.h"
#include "Trace/DataProcessors/ChaosVDConstraintDataProcessor.h"
#include "Trace/DataProcessors/ChaosVDMidPhaseDataProcessor.h"
#include "Trace/DataProcessors/ChaosVDTraceImplicitObjectProcessor.h"
#include "Trace/DataProcessors/ChaosVDTraceParticleDataProcessor.h"

FName FChaosVDTraceProvider::ProviderName("ChaosVDProvider");

FChaosVDTraceProvider::FChaosVDTraceProvider(TraceServices::IAnalysisSession& InSession): Session(InSession)
{
}

void FChaosVDTraceProvider::CreateRecordingInstanceForSession(const FString& InSessionName)
{
	DeleteRecordingInstanceForSession();

	InternalRecording = MakeShared<FChaosVDRecording>();
	InternalRecording->SessionName = InSessionName;
}

void FChaosVDTraceProvider::DeleteRecordingInstanceForSession()
{
	InternalRecording.Reset();
}

void FChaosVDTraceProvider::AddSolverFrame(const int32 InSolverID, FChaosVDSolverFrameData&& FrameData)
{
	if (InternalRecording.IsValid())
	{
		InternalRecording->AddFrameForSolver(InSolverID, MoveTemp(FrameData));
	}
}

void FChaosVDTraceProvider::AddGameFrame(FChaosVDGameFrameData&& FrameData)
{
	if (InternalRecording.IsValid())
	{
		// In PIE, we can have a lot of empty frames at the beginning, so we discard them here
		{
			// This is a kind of made up number, this includes RBAN solvers so we have more than 1 for sure.
			constexpr int32 MinSolverIDsExpected = 10;
			
			static TArray<int32> SolverIDs;
			SolverIDs.Reserve(MinSolverIDsExpected);
			
			FWriteScopeLock WriteLock(GetDataLock());
			const int32 AvailableGameFrames = InternalRecording->GetAvailableGameFramesNumber_AssumesLocked();
			InternalRecording->GetAvailableSolverIDsAtGameFrameNumber_AssumesLocked(AvailableGameFrames -1, SolverIDs);
			if (SolverIDs.IsEmpty())
			{
				if (FChaosVDGameFrameData* GameFrame = InternalRecording->GetLastGameFrameData_AssumesLocked())
				{
					*GameFrame = FrameData;
					return;
				}
			}

			SolverIDs.Reset();
		}

		InternalRecording->AddGameFrameData(MoveTemp(FrameData));
	}
}

FChaosVDSolverFrameData* FChaosVDTraceProvider::GetSolverFrame_AssumesLocked(const int32 InSolverID, const int32 FrameNumber) const
{
	return InternalRecording.IsValid() ? InternalRecording->GetSolverFrameData_AssumesLocked(InSolverID, FrameNumber) : nullptr;
}

FChaosVDSolverFrameData* FChaosVDTraceProvider::GetLastSolverFrame_AssumesLocked(const int32 InSolverID) const
{
	if (InternalRecording.IsValid())
	{
		const int32 AvailableFramesNumber = InternalRecording->GetAvailableSolverFramesNumber_AssumesLocked(InSolverID);

		if (AvailableFramesNumber > 0)
		{
			return GetSolverFrame_AssumesLocked(InSolverID, AvailableFramesNumber - 1);
		}
	}

	return nullptr;
}

FChaosVDGameFrameData* FChaosVDTraceProvider::GetLastGameFrame_AssumesLocked() const
{
	return InternalRecording.IsValid() ? InternalRecording->GetLastGameFrameData_AssumesLocked() : nullptr;
}

FChaosVDBinaryDataContainer& FChaosVDTraceProvider::FindOrAddUnprocessedData(const int32 DataID)
{
	if (const TSharedPtr<FChaosVDBinaryDataContainer>* UnprocessedData = UnprocessedDataByID.Find(DataID))
	{
		check(UnprocessedData->IsValid());
		return *UnprocessedData->Get();
	}
	else
	{
		const TSharedPtr<FChaosVDBinaryDataContainer> DataContainer = MakeShared<FChaosVDBinaryDataContainer>(DataID);
		UnprocessedDataByID.Add(DataID, DataContainer);
		return *DataContainer.Get();
	}
}

bool FChaosVDTraceProvider::ProcessBinaryData(const int32 DataID)
{
	RegisterDefaultDataProcessorsIfNeeded();

	if (const TSharedPtr<FChaosVDBinaryDataContainer>* UnprocessedDataPtr = UnprocessedDataByID.Find(DataID))
	{
		const TSharedPtr<FChaosVDBinaryDataContainer> UnprocessedData = *UnprocessedDataPtr;
		if (UnprocessedData.IsValid())
		{
			UnprocessedData->bIsReady = true;

			const TArray<uint8>* RawData = nullptr;
			TArray<uint8> UncompressedData;
			if (UnprocessedData->bIsCompressed)
			{
				UncompressedData.Reserve(UnprocessedData->UncompressedSize);
				FOodleCompressedArray::DecompressToTArray(UncompressedData, UnprocessedData->RawData);
				RawData = &UncompressedData;
			}
			else
			{
				RawData = &UnprocessedData->RawData;
			}

			if (TSharedPtr<IChaosVDDataProcessor>* DataProcessorPtrPtr = RegisteredDataProcessors.Find(UnprocessedData->TypeName))
			{
				if (TSharedPtr<IChaosVDDataProcessor> DataProcessorPtr = *DataProcessorPtrPtr)
				{
					if (ensure(DataProcessorPtr->ProcessRawData(*RawData)))
					{
						return true;
					}
				}
			}
			else
			{
				UE_LOG(LogChaosVDEditor, Warning, TEXT("[%s] Data processor for type [%s] not found"), ANSI_TO_TCHAR(__FUNCTION__), *UnprocessedData->TypeName);
			}
		}
	}

	return false;
}

TSharedPtr<FChaosVDRecording> FChaosVDTraceProvider::GetRecordingForSession() const
{
	return InternalRecording;
}

void FChaosVDTraceProvider::RegisterDataProcessor(TSharedPtr<IChaosVDDataProcessor> InDataProcessor)
{
	RegisteredDataProcessors.Add(InDataProcessor->GetCompatibleTypeName(), InDataProcessor);
}

FRWLock& FChaosVDTraceProvider::GetDataLock()
{
	check(InternalRecording.IsValid());
	return InternalRecording->GetRecordingDataLock();
}

void FChaosVDTraceProvider::RegisterDefaultDataProcessorsIfNeeded()
{
	if (bDefaultDataProcessorsRegistered)
	{
		return;
	}
	
	TSharedPtr<FChaosVDTraceImplicitObjectProcessor> ImplicitObjectProcessor = MakeShared<FChaosVDTraceImplicitObjectProcessor>();
	ImplicitObjectProcessor->SetTraceProvider(AsShared());
	RegisterDataProcessor(ImplicitObjectProcessor);

	TSharedPtr<FChaosVDTraceParticleDataProcessor> ParticleDataProcessor = MakeShared<FChaosVDTraceParticleDataProcessor>();
	ParticleDataProcessor->SetTraceProvider(AsShared());
	RegisterDataProcessor(ParticleDataProcessor);

	TSharedPtr<FChaosVDMidPhaseDataProcessor> MidPhaseDataProcessor = MakeShared<FChaosVDMidPhaseDataProcessor>();
	MidPhaseDataProcessor->SetTraceProvider(AsShared());
	RegisterDataProcessor(MidPhaseDataProcessor);

	TSharedPtr<FChaosVDConstraintDataProcessor> ConstraintDataProcessor = MakeShared<FChaosVDConstraintDataProcessor>();
	ConstraintDataProcessor->SetTraceProvider(AsShared());
	RegisterDataProcessor(ConstraintDataProcessor);

	bDefaultDataProcessorsRegistered = true;
}
