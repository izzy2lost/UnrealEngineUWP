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

		FrameFileData.Empty();
		
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

	FrameFileData.Empty();
	RecordingMaxFrames = 0;
	MaxFrameDiskSize = 0;
	EarliestFrameToStream = 0;
	InitialFrameToStream = 0;
	TotalFramesToStream = 0;
	
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
			TRange<int32> BufferedFramesLocal = GetBufferedFrames();
			if (InTotalFrames > TotalFramesToStream
			|| BufferedFramesLocal.Contains(InRange) || AsyncStreamTask->IsDone())
			{
				break;
			}
			
			FPlatformProcess::Sleep(0.002);
		}
	}
}

TRange<int32> ULiveLinkUAssetRecording::GetBufferedFrames() const
{
	FScopeLock Lock(&BufferedFrameMutex);
	if (FrameFileData.Num() > 0)
	{
		int32 MaxStart = MIN_int32;
		int32 MinEnd = MAX_int32;

		for (const FFrameFileData& FrameData : FrameFileData)
		{
			const int32 RangeStart = FrameData.BufferedFrames.GetLowerBoundValue();
			const int32 RangeEnd = FrameData.BufferedFrames.GetUpperBoundValue();

			if (RangeStart > MaxStart)
			{
				MaxStart = RangeStart;
			}

			if (RangeEnd < MinEnd)
			{
				MinEnd = RangeEnd;
			}
		}

		if (MaxStart <= MinEnd)
		{
			return TRange<int32>(MaxStart, MinEnd);
		}
	}
	
	return TRange<int32>(0, 0);
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

void ULiveLinkUAssetRecording::SetBufferedFrames(FFrameFileData& InFrameData, const TRange<int32>& InNewRange)
{
	FScopeLock Lock(&BufferedFrameMutex);
	InFrameData.BufferedFrames = InNewRange;
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

static TAutoConsoleVariable<float> CVarLiveLinkHubDebugFrameBufferDelay(
	TEXT("LiveLinkHub.Debug.FrameBufferDelay"),
	0.0f,
	TEXT("The number of seconds to wait when buffering each frame.")
);

void ULiveLinkUAssetRecording::LoadRecordingAsync(int32 InStartFrame, int32 InCurrentFrame, int32 InNumFramesToLoad)
{
	const int32 MaxPossibleFrame = RecordingMaxFrames - 1;
	InStartFrame = FMath::Clamp(InStartFrame, 0, MaxPossibleFrame);
	InCurrentFrame = FMath::Clamp(InCurrentFrame, 0, MaxPossibleFrame);
	const int32 EndFrame = InStartFrame + InNumFramesToLoad - 1;
	
	ON_SCOPE_EXIT
	{
		// Always set to true. Some blocking operations wait for this, and in the case of a non-fatal error
		// we want to display error logs and don't want the program to freeze.
		bPerformedInitialLoad = true;
	};
	
	if (GetBufferedFrames().Contains(TRange<int32>(InStartFrame, FMath::Min(EndFrame, MaxPossibleFrame > 0 ? MaxPossibleFrame : EndFrame))))
	{
		// All frames are already buffered.
		return;
	}

	DebugSleepTime = CVarLiveLinkHubDebugFrameBufferDelay.GetValueOnAnyThread();
	
	// Read file for streaming into memory.
	if (!RecordingFileReader->AtEnd())
	{
		// Perform initial load and record entry frame file offsets.
		const bool bInitialLoad = FrameFileData.Num() == 0;
		if (bInitialLoad)
		{
			RecordingFileReader->Seek(0);
		
			int32 LoadedRecordingVersion;
			*RecordingFileReader << LoadedRecordingVersion;

			// If we modify the RecordingVersion we can perform import logic here.
			ensure(LoadedRecordingVersion == RecordingVersion);
			
			// Process static data.
		
			int32 NumStaticData = 0;
			*RecordingFileReader << NumStaticData;

			for (int32 StaticIdx = 0; StaticIdx < NumStaticData; ++StaticIdx)
			{
				FGuid KeySource = FGuid();
				FString KeyName;
			
				*RecordingFileReader << KeySource;
				*RecordingFileReader << KeyName;

				// Create framedata just to load initial static frame data. Static data doesn't require this afterward.
				FFrameFileData TemporaryFrameData;
				TemporaryFrameData.FrameDataSubjectKey = MakeShared<FLiveLinkSubjectKey>(KeySource, *KeyName);
				if (!LoadInitialFrameData(TemporaryFrameData))
				{
					return;
				}
				
				FLiveLinkRecordingStaticDataContainer& DataContainer = RecordingData.StaticData.FindChecked(*TemporaryFrameData.FrameDataSubjectKey.Get());
				LoadFrameData(TemporaryFrameData, DataContainer, 0, 0, 1);
			}

			// Process frame data.
		
			int32 NumFrameData = 0;
			*RecordingFileReader << NumFrameData;

			for (int32 FrameIdx = 0; FrameIdx < NumFrameData; ++FrameIdx)
			{
				FGuid KeySource = FGuid();
				FString KeyName;
			
				*RecordingFileReader << KeySource;
				*RecordingFileReader << KeyName;

				FFrameFileData KeyPosition;
				KeyPosition.FrameDataSubjectKey = MakeShared<FLiveLinkSubjectKey>(KeySource, *KeyName);
				if (!LoadInitialFrameData(KeyPosition))
				{
					return;
				}

				// Offset to the end of this block if there is multiple NumFrameData.
				const int64 EndBlockPosition = KeyPosition.GetFrameFilePosition(KeyPosition.MaxFrames);
				RecordingFileReader->Seek(EndBlockPosition);
				FrameFileData.Add(MoveTemp(KeyPosition));
			}
		}

		// Load the required frames, either on initial load or subsequent loads.
		for (FFrameFileData& FrameData : FrameFileData)
		{
			if (ensure(FrameData.FrameDataSubjectKey.IsValid()))
			{
				FLiveLinkRecordingBaseDataContainer& DataContainer = RecordingData.FrameData.FindChecked(*FrameData.FrameDataSubjectKey.Get());
				LoadFrameData(FrameData, DataContainer, InStartFrame, InCurrentFrame, InNumFramesToLoad);
			}
			else
			{
				UE_LOG(LogLiveLinkHub, Error, TEXT("FrameDataSubjectKey is missing for file %s."), *GetRecordingDataFilePath());
			}
		}
	}
}

bool ULiveLinkUAssetRecording::LoadInitialFrameData(FFrameFileData& OutFrameData)
{
	int32 MaxFrames = 0;
	(*RecordingFileReader) << MaxFrames;

	if (MaxFrames > RecordingMaxFrames)
	{
		RecordingMaxFrames = MaxFrames;
	}
	
	OutFrameData.MaxFrames = MaxFrames;
			
	if (MaxFrames > 0)
	{	
		FString StructTypeName;
		int32 SerializedStructureSize;
			
		*RecordingFileReader << StructTypeName;
		*RecordingFileReader << SerializedStructureSize;

		OutFrameData.SerializedStructureSize = SerializedStructureSize;
		OutFrameData.RecordingStartFrameFilePosition = RecordingFileReader->Tell();

		OutFrameData.LoadedStruct = FindObject<UScriptStruct>(nullptr, *StructTypeName, true);
		if (!OutFrameData.LoadedStruct.IsValid())
		{
			UE_LOG(LogLiveLinkHub, Error, TEXT("Script struct type '%s' not found."), *StructTypeName);
			return false;
		}

		// The size on disk for each frame-- consisting of the frame index, timestamp, and frame struct data.
		OutFrameData.FrameDiskSize = (sizeof(int32) + sizeof(double) + SerializedStructureSize);

		if (OutFrameData.FrameDiskSize > MaxFrameDiskSize)
		{
			MaxFrameDiskSize = OutFrameData.FrameDiskSize;
		}
	}

	return true;
}

void ULiveLinkUAssetRecording::LoadFrameData(FFrameFileData& InFrameData, FLiveLinkRecordingBaseDataContainer& InDataContainer,
                                             int32 RequestedStartFrame, int32 RequestedInitialFrame, int32 RequestedFramesToLoad)
{
	bStreamingFrameChange = false;

	int32 MaxFrames = InFrameData.MaxFrames;
	if (MaxFrames > 0)
	{
		if (RequestedFramesToLoad > 0)
		{
			// Don't go past requested frames or max frames.
			MaxFrames = FMath::Min(MaxFrames, RequestedStartFrame + RequestedFramesToLoad);
		}
		
		UScriptStruct* LoadedStruct = InFrameData.LoadedStruct.Get();
		if (LoadedStruct == nullptr)
		{
			UE_LOG(LogLiveLinkHub, Error, TEXT("Script struct type not found."));
			return;
		}

		// Seek to the requested start frame position in the file.
		int64 NewPosition = InFrameData.GetFrameFilePosition(RequestedStartFrame);
		RecordingFileReader->Seek(NewPosition);

		// Arrays to store the newly loaded data.
		TArray<double> NewTimestamps;
		TArray<TSharedPtr<FInstancedStruct>> NewRecordedData;

		NewTimestamps.Reserve(MaxFrames);
		NewRecordedData.Reserve(MaxFrames);

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

			int64 FramePosition = InFrameData.GetFrameFilePosition(FrameToLoad);
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
			if (TSharedPtr<FInstancedStruct> ExistingFrame = InDataContainer.TryGetFrame(ParsedFrameIdx, ExistingTimestamp))
			{
				check(RecordingFileReader->Tell() != 0);
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

			// Test slow buffer.
			if (DebugSleepTime > 0.f)
			{
				FPlatformProcess::Sleep(DebugSleepTime);
			}
			
			if (bStreamingFrameChange)
			{
				// The requested frames to stream have changed, finish the cycle and let the async task continue with updated data.
				break;
			}
		}

		// Record all the frames that have been buffered.
		SetBufferedFrames(InFrameData, TRange<int32>(LastLoadedLeftFrame, LastLoadedRightFrame));

		// Output the streamed data to the data container. This will unload unused frames.
		{
			FScopeLock Lock(&DataContainerMutex);
			InDataContainer.Timestamps = MoveTemp(NewTimestamps);
			InDataContainer.RecordedData = MoveTemp(NewRecordedData);
			InDataContainer.RecordedDataStartFrame = LastLoadedLeftFrame;

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
