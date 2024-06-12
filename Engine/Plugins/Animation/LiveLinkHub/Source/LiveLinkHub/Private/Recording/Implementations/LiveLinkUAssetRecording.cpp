// Copyright Epic Games, Inc. All Rights Reserved.

#include "LiveLinkUAssetRecording.h"

#include "HAL/IConsoleManager.h"
#include "LiveLinkHubLog.h"
#include "LiveLinkUAssetRecordingPlayer.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/BufferArchive.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/ObjectAndNameAsStringProxyArchive.h"

ULiveLinkUAssetRecording::~ULiveLinkUAssetRecording()
{
	UnloadRecording();
}

void ULiveLinkUAssetRecording::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);
	
	FString FilePath = GetRecordingDataFilePath();
	
	if (Ar.IsSaving())
	{
		// todo: async stream save
		SaveRecording();
	}
}

void ULiveLinkUAssetRecording::SaveRecording()
{
	// Saving frame data to a custom file, so it can be streamed in later.
	const FString FilePath = GetRecordingDataFilePath();
	FArchive* FileWriter = IFileManager::Get().CreateFileWriter(*FilePath);

	int32 RecordingVersionToSave = RecordingVersion;
	*FileWriter << RecordingVersionToSave;

	// How much static data to expect.
	int32 NumStaticData = RecordingData.StaticData.Num();
	*FileWriter << NumStaticData;
		
	for (TTuple<FLiveLinkSubjectKey, FLiveLinkRecordingStaticDataContainer>& StaticData : RecordingData.StaticData)
	{
		SaveFrameData(FileWriter, StaticData.Key, StaticData.Value);
	}
	
	// How much frame data to expect.
	int32 NumFrameData = RecordingData.FrameData.Num();
	*FileWriter << NumFrameData;
		
	for (TTuple<FLiveLinkSubjectKey, FLiveLinkRecordingBaseDataContainer>& FrameData : RecordingData.FrameData)
	{
		SaveFrameData(FileWriter, FrameData.Key, FrameData.Value);
	}
		
	FileWriter->FlushCache();
	FileWriter->Close();
	delete FileWriter;
}

void ULiveLinkUAssetRecording::LoadRecording(int32 InInitialFrame, int32 InNumFramesToLoad)
{
	bCancelStream = false;
	
	int32 StartFrame = InInitialFrame - InNumFramesToLoad;
	if (StartFrame < 0)
	{
		StartFrame = 0;
	}
	
	// Additional buffer to each side, plus the initial frame.
	InNumFramesToLoad = (InNumFramesToLoad * 2) + 1;

	// Perform initial setup of the file reader.
	if (!AsyncStreamTask.IsValid())
	{
		check(RecordingFileReader == nullptr);
		
		RecordingStartFrameFilePosition = 0;
		
		const FString FilePath = GetRecordingDataFilePath();
		RecordingFileReader = IFileManager::Get().CreateFileReader(*FilePath);

		if (RecordingFileReader == nullptr)
		{
			UE_LOG(LogLiveLinkHub, Error, TEXT("Failed to open file %s for reading."), *FilePath);
			return;
		}
	}

	EarliestFrameToStream = StartFrame;

	if (InitialFrameToStream != InInitialFrame)
	{
		bStreamingFrameChange = true;
	}
	
	InitialFrameToStream = InInitialFrame;
	TotalFramesToStream = InNumFramesToLoad;

	if (!AsyncStreamTask.IsValid())
	{
		AsyncStreamTask = MakeUnique<FAsyncTask<FLiveLinkStreamAsyncTask>>(this);
		AsyncStreamTask->StartBackgroundTask();
	}
}

void ULiveLinkUAssetRecording::UnloadRecording()
{
	bCancelStream = true;

	if (AsyncStreamTask.IsValid())
	{
		if (!AsyncStreamTask->Cancel())
		{
			AsyncStreamTask->EnsureCompletion();
		}
		AsyncStreamTask.Reset();
	}

	bPerformedInitialLoad = false;

	if (RecordingFileReader)
	{
		RecordingFileReader->Close();
		delete RecordingFileReader;
		RecordingFileReader = nullptr;
	}

	FrameDataSubjectKey.Reset();
	RecordingStartFrameFilePosition = 0;
	RecordingMaxFrames = 0;
	FrameDiskSize = 0;
	EarliestFrameToStream = 0;
	InitialFrameToStream = 0;
	TotalFramesToStream = 0;
	
	BufferedFrames = TRange<int32>(0, 0);
	
	for (TTuple<FLiveLinkSubjectKey, FLiveLinkRecordingStaticDataContainer>& StaticData : RecordingData.StaticData)
	{
		StaticData.Value.Timestamps.Empty();
		StaticData.Value.RecordedData.Empty();
	}

	for (TTuple<FLiveLinkSubjectKey, FLiveLinkRecordingBaseDataContainer>& FrameData : RecordingData.FrameData)
	{
		FrameData.Value.Timestamps.Empty();
		FrameData.Value.RecordedData.Empty();
	}
}

void ULiveLinkUAssetRecording::WaitForBufferedFrames(int32 InMinFrame, int32 InMaxFrame)
{
	if (AsyncStreamTask.IsValid())
	{
		// Max frames isn't set until after the initial load.
		while (!bPerformedInitialLoad)
		{
			FPlatformProcess::Sleep(0.002);
		}

		// Clamp the frame range to the max possible range. If the selection range extends the actual frame range
		// then there would be nothing to load.
		InMinFrame = FMath::Clamp(InMinFrame, 0, RecordingMaxFrames - 1);
		InMaxFrame = FMath::Clamp(InMaxFrame, 0, RecordingMaxFrames - 1);

		const int32 InTotalFrames = InMaxFrame - InMinFrame + 1;
		const TRange<int32> InRange(InMinFrame, InMaxFrame);
		
		while (true)
		{
			TRange<int32> BufferedFramesLocal = BufferedFrames.load();
			if (InTotalFrames > TotalFramesToStream
			|| BufferedFramesLocal.Contains(InRange) || AsyncStreamTask->IsDone())
			{
				break;
			}
			
			FPlatformProcess::Sleep(0.002);
		}
	}
}

void ULiveLinkUAssetRecording::CopyRecordingData(FLiveLinkPlaybackTracks& InOutLiveLinkPlaybackTracks) const
{
	FScopeLock Lock(&DataContainerMutex);

	for (const TPair<FLiveLinkSubjectKey, FLiveLinkRecordingStaticDataContainer>& Pair : RecordingData.StaticData)
	{
		// Modify subject name so a duplicate FLiveLinkSubjectKey below doesn't produce the same hash. This allows us to efficiently
		// reuse tracks, as well as preserve the absolute frame index, which is needed since frame data is streamed in.
		FLiveLinkSubjectKey StaticSubjectKey = Pair.Key;
		StaticSubjectKey.SubjectName.Name = *(StaticSubjectKey.SubjectName.ToString() + "_STATIC");
		FLiveLinkPlaybackTrack& PlaybackTrack = InOutLiveLinkPlaybackTracks.Tracks.FindOrAdd(StaticSubjectKey);

		PlaybackTrack.FrameData = TConstArrayView<TSharedPtr<FInstancedStruct>>(Pair.Value.RecordedData);
		PlaybackTrack.Timestamps = TConstArrayView<double>(Pair.Value.Timestamps);
		PlaybackTrack.LiveLinkRole = Pair.Value.Role;
		PlaybackTrack.SubjectKey = Pair.Key;
		PlaybackTrack.StartIndexOffset = Pair.Value.RecordedDataStartFrame;
	}

	for (const TPair<FLiveLinkSubjectKey, FLiveLinkRecordingBaseDataContainer>& Pair : RecordingData.FrameData)
	{
		FLiveLinkPlaybackTrack& PlaybackTrack = InOutLiveLinkPlaybackTracks.Tracks.FindOrAdd(Pair.Key);
		
		PlaybackTrack.FrameData = TConstArrayView<TSharedPtr<FInstancedStruct>>(Pair.Value.RecordedData);
		PlaybackTrack.Timestamps = TConstArrayView<double>(Pair.Value.Timestamps);
		PlaybackTrack.SubjectKey = Pair.Key;
		PlaybackTrack.StartIndexOffset = Pair.Value.RecordedDataStartFrame;
	}
}

void ULiveLinkUAssetRecording::SaveFrameData(FArchive* InFileWriter, const FLiveLinkSubjectKey& InSubjectKey, FLiveLinkRecordingBaseDataContainer& InBaseDataContainer)
{
		// This will crash if it fails -- we don't want to save invalid data.
		InBaseDataContainer.ValidateData();
		
		// Start block with map key.
		FGuid Source = InSubjectKey.Source;
		FString SubjectName = InSubjectKey.SubjectName.ToString();
		int32 NumFrames = InBaseDataContainer.RecordedData.Num();
		*InFileWriter << Source;
		*InFileWriter << SubjectName;
		*InFileWriter << NumFrames;

		uint64 SerializedFrameSizePosition = 0;
		int32 SerializedFrameSize = 0;
		if (NumFrames > 0)
		{
			const UScriptStruct* ScriptStruct = InBaseDataContainer.RecordedData[0]->GetScriptStruct();
			FString StructTypeName = ScriptStruct->GetPathName();

			// Write the struct name and size so it can be loaded later.
			*InFileWriter << StructTypeName;

			// Remember the position to write the frame size.
			SerializedFrameSizePosition = InFileWriter->Tell();
			*InFileWriter << SerializedFrameSize;
		}
			
		for (int32 FrameIdx = 0; FrameIdx < NumFrames; ++FrameIdx)
		{
			TSharedPtr<FInstancedStruct>& Frame = InBaseDataContainer.RecordedData[FrameIdx];
			check(Frame.IsValid() && Frame->IsValid());
			
			// Write the frame index for streaming frames when loading.
			*InFileWriter << FrameIdx;

			// Write the frame's timestamp.
			double Timestamp = InBaseDataContainer.Timestamps[FrameIdx];
			*InFileWriter << Timestamp;
			
			// Write the entire frame data.
			uint64 SerializeDataStart = InFileWriter->Tell();
			FObjectAndNameAsStringProxyArchive StructAr(*InFileWriter, false);
			Frame->Serialize(StructAr);

			// Store the serialized frame size, so we can write it once later.
			{
				int32 CurrentSerializedFrameSize = InFileWriter->Tell() - SerializeDataStart;
				// Sanity check that the serialized frame size is consistent.
				ensure(CurrentSerializedFrameSize == SerializedFrameSize || SerializedFrameSize == 0);
				SerializedFrameSize = CurrentSerializedFrameSize;
			}
		}

		if (SerializedFrameSize > 0)
		{
			// Write the frame data offset at the beginning of the block.
			const uint64 FinalOffset = InFileWriter->Tell();
			InFileWriter->Seek(SerializedFrameSizePosition);
			*InFileWriter << SerializedFrameSize;
			InFileWriter->Seek(FinalOffset);
		}
}

void ULiveLinkUAssetRecording::LoadRecordingAsync(int32 InStartFrame, int32 InCurrentFrame, int32 InNumFramesToLoad)
{
	const int32 MaxPossibleFrame = RecordingMaxFrames - 1;
	InStartFrame = FMath::Clamp(InStartFrame, 0, MaxPossibleFrame);
	InCurrentFrame = FMath::Clamp(InCurrentFrame, 0, MaxPossibleFrame);
	const int32 EndFrame = InStartFrame + InNumFramesToLoad - 1;

	if (BufferedFrames.load().Contains(TRange<int32>(InStartFrame, FMath::Min(EndFrame, MaxPossibleFrame > 0 ? MaxPossibleFrame : EndFrame))))
	{
		// All frames are already buffered.
		return;
	}
	
	RecordingFileReader->Seek(RecordingStartFrameFilePosition);

	// Read file for streaming into memory.
	if (!RecordingFileReader->AtEnd())
	{
		// Perform initial load and record entry frame file offsets.
		const bool bInitialLoad = RecordingStartFrameFilePosition == 0;
		if (bInitialLoad)
		{
			int32 LoadedRecordingVersion;
			*RecordingFileReader << LoadedRecordingVersion;

			// If we modify the RecordingVersion we can perform import logic here.
			ensure(LoadedRecordingVersion == RecordingVersion);
			
			// Process static data.
		
			int32 NumStaticData = 0;
			*RecordingFileReader << NumStaticData;

			if (NumStaticData > 0)
			{
				FGuid KeySource = FGuid();
				FString KeyName;
			
				*RecordingFileReader << KeySource;
				*RecordingFileReader << KeyName;
			
				FLiveLinkSubjectKey SubjectKey(KeySource, *KeyName);

				FLiveLinkRecordingStaticDataContainer& DataContainer = RecordingData.StaticData.FindChecked(SubjectKey);
				LoadFrameData(&DataContainer, 0, 0, 1);
			}

			// Process frame data.
		
			int32 NumFrameData = 0;
			*RecordingFileReader << NumFrameData;

			if (NumFrameData > 0)
			{
				FGuid KeySource = FGuid();
				FString KeyName;
			
				*RecordingFileReader << KeySource;
				*RecordingFileReader << KeyName;
			
				FrameDataSubjectKey = MakeShared<FLiveLinkSubjectKey>(KeySource, *KeyName);
			}

			RecordingStartFrameFilePosition = RecordingFileReader->Tell();
		}

		// Load the required frames, either on initial load or subsequent loads.
		if (ensure(FrameDataSubjectKey.IsValid()))
		{
			FLiveLinkRecordingBaseDataContainer& DataContainer = RecordingData.FrameData.FindChecked(*FrameDataSubjectKey.Get());
			LoadFrameData(&DataContainer, InStartFrame, InCurrentFrame, InNumFramesToLoad);
		}
		else
		{
			UE_LOG(LogLiveLinkHub, Error, TEXT("FrameDataSubjectKey is missing for file %s."), *GetRecordingDataFilePath());
		}
		bPerformedInitialLoad = true;
	}
}

void ULiveLinkUAssetRecording::LoadFrameData(FLiveLinkRecordingBaseDataContainer* DataContainer, int32 RequestedStartFrame, int32 RequestedInitialFrame, int32 RequestedFramesToLoad)
{
	bStreamingFrameChange = false;

	int32 MaxFrames = 0;
	(*RecordingFileReader) << MaxFrames;

	if (MaxFrames > RecordingMaxFrames)
	{
		RecordingMaxFrames = MaxFrames;
	}
			
	if (MaxFrames > 0)
	{
		if (RequestedFramesToLoad > 0)
		{
			// Don't go past requested frames or max frames.
			MaxFrames = FMath::Min(MaxFrames, RequestedStartFrame + RequestedFramesToLoad);
		}
				
		FString StructTypeName;
		int32 SerializedStructureSize;
			
		*RecordingFileReader << StructTypeName;
		*RecordingFileReader << SerializedStructureSize;

		int64 StartPosition = RecordingFileReader->Tell();
				
		UScriptStruct* LoadedStruct = FindObject<UScriptStruct>(nullptr, *StructTypeName, true);
		if (LoadedStruct == nullptr)
		{
			UE_LOG(LogLiveLinkHub, Error, TEXT("Script struct type '%s' not found."), *StructTypeName);
			return;
		}

		// The size on disk for each frame-- consisting of the frame index, timestamp, and frame struct data.
		FrameDiskSize = (sizeof(int32) + sizeof(double) + SerializedStructureSize);
		
		auto GetFrameFilePosition = [StartPosition, this](int32 FrameIdx)
		{
			return StartPosition + (FrameDiskSize * FrameIdx);
		};

		// Seek to the requested start frame position in the file.
		int64 NewPosition = GetFrameFilePosition(RequestedStartFrame);
		RecordingFileReader->Seek(NewPosition);

		// Arrays to store the newly loaded data.
		TArray<double> NewTimestamps;
		TArray<TSharedPtr<FInstancedStruct>> NewRecordedData;

		NewTimestamps.Reserve(RequestedFramesToLoad);
		NewRecordedData.Reserve(RequestedFramesToLoad);

		// Load each frame from the initial frame, alternating right to left each frame. This creates a buffer to support
		// scrubbing each direction and makes sure the immediate frames are loaded first.
		
		int32 RightFrameIdx = RequestedInitialFrame;
		int32 LeftFrameIdx = RequestedInitialFrame - 1; // - 1 So we don't try to load the same initial frame when alternating to the left.
		int32 LastLoadedRightFrame = RequestedInitialFrame;
		int32 LastLoadedLeftFrame = RequestedInitialFrame;
		bool bLoadRight = true; // Start right -> left
		
		// We could potentially optimize this further -- such as adjusting the ratio of ahead/behind frames to buffer based on whether
		// the recording is playing forward or reverse vs being scrubbed.

		auto AlternateLoadDirection = [&]()
		{
			bLoadRight = !bLoadRight;
		};
		
		while (RightFrameIdx < MaxFrames || LeftFrameIdx >= RequestedStartFrame)
		{
			int32 FrameToLoad;
    
			if (bLoadRight)
			{
				if (RightFrameIdx >= MaxFrames)
				{
					bLoadRight = false;
					continue;
				}
				FrameToLoad = RightFrameIdx++;
				LastLoadedRightFrame = FrameToLoad;
			}
			else
			{
				if (LeftFrameIdx < RequestedStartFrame)
				{
					bLoadRight = true;
					continue;
				}
				FrameToLoad = LeftFrameIdx--;
				LastLoadedLeftFrame = FrameToLoad;
			}

			int64 FramePosition = GetFrameFilePosition(FrameToLoad);
			RecordingFileReader->Seek(FramePosition);

			int32 ParsedFrameIdx = 0;
			*RecordingFileReader << ParsedFrameIdx;

			// Ensure the parsed frame index matches the expected frame
			if (!ensure(ParsedFrameIdx == FrameToLoad))
			{
				UE_LOG(LogLiveLinkHub, Error, TEXT("Frame index mismatch: expected %d, got %d"), FrameToLoad, ParsedFrameIdx);
				break;
			}

			auto InsertFrame = [&](TSharedPtr<FInstancedStruct>& InFrame, double InTimestamp, bool bCopy)
			{
#if UE_BUILD_DEBUG
				ensure(!NewTimestamps.Contains(InTimestamp));
#endif
				if (bLoadRight)
				{
					NewTimestamps.Add(InTimestamp);
					NewRecordedData.Add(bCopy ? InFrame : MoveTemp(InFrame));
				}
				else
				{
					NewTimestamps.Insert(InTimestamp, 0);
					NewRecordedData.Insert(bCopy ? InFrame : MoveTemp(InFrame), 0);
				}
				
#if UE_BUILD_DEBUG
				// Additional validation to ensure timestamps / frames are loaded in the correct order.
				for (int32 Idx = 1; Idx < NewTimestamps.Num(); ++Idx)
				{
					double LastTimestamp = NewTimestamps[Idx - 1];
					double CurrentTimestamp = NewTimestamps[Idx];
					ensure(LastTimestamp < CurrentTimestamp);
				}
#endif
			};
			
			// Don't load a frame already in memory.
			double ExistingTimestamp = 0.f;
			if (TSharedPtr<FInstancedStruct> ExistingFrame = DataContainer->TryGetFrame(ParsedFrameIdx, ExistingTimestamp))
			{
				check(RecordingStartFrameFilePosition != 0);
				constexpr bool bCopy = true; // Copy, as the data container could still be having its frame data pushed to animation.
				InsertFrame(ExistingFrame, ExistingTimestamp, bCopy);
				AlternateLoadDirection();
				continue;
			}

			double Timestamp = 0;
			*RecordingFileReader << Timestamp;

			// Instantiate the animation frame.
			FObjectAndNameAsStringProxyArchive StructAr(*RecordingFileReader, false);
			TSharedPtr<FInstancedStruct> DataInstancedStruct = MakeShared<FInstancedStruct>(LoadedStruct);
			DataInstancedStruct->Serialize(StructAr);

			InsertFrame(DataInstancedStruct, Timestamp, false);
		
			ensure(NewTimestamps.Num() == NewRecordedData.Num());

			AlternateLoadDirection();
			
			if (bStreamingFrameChange)
			{
				// The requested frames to stream have changed, finish the cycle and let the async task continue with updated data.
				break;
			}
		}

		// Record all the frames that have been buffered.
		BufferedFrames = TRange<int32>(LastLoadedLeftFrame, LastLoadedRightFrame);

		// Output the streamed data to the data container. This will unload unused frames.
		{
			FScopeLock Lock(&DataContainerMutex);
			DataContainer->Timestamps = MoveTemp(NewTimestamps);
			DataContainer->RecordedData = MoveTemp(NewRecordedData);
			DataContainer->RecordedDataStartFrame = LastLoadedLeftFrame;

			// This could potentially be optimized, such as outputting directly to the data container during the iteration, which could
			// allow smoother streaming when scrubbing to a position that isn't buffered at all. However, we would need to be careful of the cost
			// of locking the container each iteration.
		}
	}
}

FString ULiveLinkUAssetRecording::GetRecordingDataFilePath() const
{
	// todo: This path needs to be calculated from the settings file.
	FString AssetPath = GetPathName();
	FString BasePath = FPaths::ProjectSavedDir();
	FString FilePath = FString::Printf(TEXT("%sRecordingData/%s.rec"), *BasePath, *FPaths::GetBaseFilename(AssetPath));

	return FilePath;
}

void ULiveLinkUAssetRecording::FLiveLinkStreamAsyncTask::DoWork()
{
	int32 LastStartFrame = -1;
	int32 LastTotalFrames = -1;
	int32 LastInitialFrame = -1;
	while (LiveLinkRecording && !LiveLinkRecording->bCancelStream)
	{
		if (LastStartFrame != LiveLinkRecording->EarliestFrameToStream
			|| LastTotalFrames != LiveLinkRecording->TotalFramesToStream
			|| LastInitialFrame != LiveLinkRecording->InitialFrameToStream)
		{
			LastStartFrame = LiveLinkRecording->EarliestFrameToStream;
			LastTotalFrames = LiveLinkRecording->TotalFramesToStream;
			LastInitialFrame = LiveLinkRecording->InitialFrameToStream;
			LiveLinkRecording->LoadRecordingAsync(LiveLinkRecording->EarliestFrameToStream,
				LiveLinkRecording->InitialFrameToStream, LiveLinkRecording->TotalFramesToStream);
		}
	}
}
