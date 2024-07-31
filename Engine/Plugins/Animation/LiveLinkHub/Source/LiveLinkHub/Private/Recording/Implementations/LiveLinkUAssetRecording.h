// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Async/AsyncWork.h"
#include "Data/LiveLinkHubBulkData.h"
#include "StructUtils/InstancedStruct.h"
#include "LiveLinkPreset.h"
#include "LiveLinkRole.h"
#include "LiveLinkTypes.h"
#include "Misc/DateTime.h"
#include "Recording/LiveLinkRecording.h"
#include "Templates/SubclassOf.h"
#include "UObject/Object.h"

#include "LiveLinkUAssetRecording.generated.h"

class FMemoryReader;
struct FLiveLinkPlaybackTracks;

/** Base data container for a recording track. */
USTRUCT()
struct FLiveLinkRecordingBaseDataContainer
{
	GENERATED_BODY()

	/** SERIALIZED DATA - Timestamps for the recorded data. Each entry matches an entry in the RecordedData array. */
	TArray<double> Timestamps;
	
	/**
	 * SERIALIZED DATA - Array of either static or frame recorded for a given timestamp.
	 * TSharedPtr used as streaming the data in may require shared access.
	 * FSharedStruct is not used because it doesn't implement Serialize().
	 */
	TArray<TSharedPtr<FInstancedStruct>> RecordedData;

	/** The current start frame for RecordedData. */
	int32 RecordedDataStartFrame = 0;
	
	/**
	 * Retrieve a loaded frame.
	 *
	 * @param InFrame The absolute frame index to load.
	 *
	 * @return The frame if it exists.
	 */
	TSharedPtr<FInstancedStruct> TryGetFrame(const int32 InFrame) const
	{
		if (IsFrameLoaded(InFrame))
		{
			const int32 RelativeFrameIdx = InFrame - RecordedDataStartFrame;
			return RecordedData[RelativeFrameIdx];
		}
		return nullptr;
	}

	/**
	 * Retrieve a loaded frame.
	 *
	 * @param InFrame The absolute frame index to load.
	 * @param OutTimestamp Return the timestamp, if it exists.
	 *
	 * @return The frame if it exists.
	 */
	TSharedPtr<FInstancedStruct> TryGetFrame(const int32 InFrame, double& OutTimestamp) const
	{
		if (TSharedPtr<FInstancedStruct> OutFrame = TryGetFrame(InFrame))
		{
			const int32 RelativeFrameIdx = InFrame - RecordedDataStartFrame;
			OutTimestamp = Timestamps[RelativeFrameIdx];
			return OutFrame;
		}
		return nullptr;
	}
	
	/**
	 * Checks if a frame is currently loaded.
	 * 
	 * @param InFrame The absolute frame index to check.
	 *
	 * @return True if the frame index exists within the frame array.
	 */
	bool IsFrameLoaded(const int32 InFrame) const
	{
		return InFrame >= RecordedDataStartFrame && InFrame < RecordedDataStartFrame + RecordedData.Num();
	}

	/** Check data memory is valid and expected. */
	void ValidateData() const
	{
		check(Timestamps.Num() == RecordedData.Num());
		for (const TSharedPtr<FInstancedStruct>& InstancedStruct : RecordedData)
		{
			check(InstancedStruct.IsValid() && InstancedStruct->IsValid());
		}
	}
};

/** Container for static data. */
USTRUCT()
struct FLiveLinkRecordingStaticDataContainer : public FLiveLinkRecordingBaseDataContainer
{
	GENERATED_BODY()

	/** The role of the static data being recorded. */
	UPROPERTY()
	TSubclassOf<ULiveLinkRole> Role = nullptr;
};

USTRUCT()
struct FLiveLinkUAssetRecordingData
{
	GENERATED_BODY()

	/** Length of the recording in seconds. */
	UPROPERTY()
	double LengthInSeconds = 0;

	/** Static data encountered while recording. */
	UPROPERTY()
	TMap<FLiveLinkSubjectKey, FLiveLinkRecordingStaticDataContainer> StaticData;
	
	/** Frame data encountered while recording. */
	UPROPERTY()
	TMap<FLiveLinkSubjectKey, FLiveLinkRecordingBaseDataContainer> FrameData;
};

UCLASS()
class ULiveLinkUAssetRecording : public ULiveLinkRecording
{
public:
	GENERATED_BODY()

	virtual ~ULiveLinkUAssetRecording() override;
	
	virtual void Serialize(FArchive& Ar) override;

	virtual void PostDuplicate(EDuplicateMode::Type DuplicateMode) override;
	virtual void PostRename(UObject* OldOuter, const FName OldName) override;

	virtual bool IsFullyLoaded() const override { return bIsFullyLoaded; }
	virtual bool IsSavingRecordingData() const override { return bIsSavingRecordingData; }
	
	/** Save recording data to disk. */
	void SaveRecordingData();

	/** Load recording data from disk. */
	void LoadRecordingData(int32 InInitialFrame, int32 InNumFramesToLoad);

	/** Free memory and close file reader. */
	void UnloadRecordingData();

	/** Block until frames are loaded. */
	void WaitForBufferedFrames(int32 InMinFrame, int32 InMaxFrame);
	
	/** Return the maximum frames for this recording. */
	int32 GetMaxFrames() const { return RecordingMaxFrames; }

	/** The size in bytes of each animation frame. */
	int32 GetFrameDiskSize() const { return MaxFrameDiskSize; }
	
	/** Return the minimum buffered frame range. */
	TRange<int32> GetBufferedFrames() const;

	/** Copy the asset's loaded recording data to a format suitable for playback in live link. */
	void CopyRecordingData(FLiveLinkPlaybackTracks& InOutLiveLinkPlaybackTracks) const;

	/** Initial setup of new recording data. */
	void InitializeNewRecordingData(FLiveLinkUAssetRecordingData&& InRecordingData, double InRecordingLengthSeconds);
	
private:
	/** Frame data file information when loading from a recording file. */
	struct FFrameFileData
	{
		/** The subject key used for the frame data. */
		TSharedPtr<FLiveLinkSubjectKey> FrameDataSubjectKey;
		/** The struct for this frame data. */
		TWeakObjectPtr<UScriptStruct> LoadedStruct;
		/** The position in the file recording where frame data begins. */
		int64 RecordingStartFrameFilePosition = 0;
		/** Maximum number of frames. */
		int32 MaxFrames = 0;
		/** Total size of the structure. */
		int32 SerializedStructureSize = 0;
		/** The size in bytes of each animation frame. */
		int32 FrameDiskSize;
		/** Buffered frames for this framedata. */
		TRange<int32> BufferedFrames = TRange<int32>(0, 0);

		/** Find the correct file offset based on the frame index. */
		int64 GetFrameFilePosition(const int32 InFrameIdx) const
		{
			return RecordingStartFrameFilePosition + (FrameDiskSize * InFrameIdx);
		};
	};
	
	/** Update the buffered frame range. */
	void SetBufferedFrames(FFrameFileData& InFrameData, const TRange<int32>& InNewRange);
	
	/** Serialize the number of frames (array size) of the BaseDataContainer to the archive. */
	void SaveFrameData(FArchive* InFileWriter, const FLiveLinkSubjectKey& InSubjectKey, FLiveLinkRecordingBaseDataContainer& InBaseDataContainer);
	
	/** Initialize or update an async load. */
	void LoadRecordingAsync(int32 InStartFrame, int32 InCurrentFrame, int32 InNumFramesToLoad);

	/** Initial processing on a frame, finding the correct struct and offsets. The RecordingFileReader is assumed to be at the correct position.  */
	bool LoadInitialFrameData(FFrameFileData& OutFrameData);
	
	/** Load frame data to a data container. */
	void LoadFrameData(FFrameFileData& InFrameData, FLiveLinkRecordingBaseDataContainer& InDataContainer,
		int32 RequestedStartFrame, int32 RequestedInitialFrame, int32 RequestedFramesToLoad);
	
	/** Retrieve the recording data file path for this asset. */
	FString GetRecordingDataFilePath() const;

	/** Eject this recording and make sure it is unloaded. */
	void EjectAndUnload();

	/** Make the thread wait if we are paused. */
	void WaitIfPaused_AsyncThread();

	/** Signal and wait for the stream to be paused. */
	void PauseStream();

	/** Signal the stream can be resumed. */
	void UnpauseStream();
	
	/** Called before garbage collection. */
	void OnPreGarbageCollect();

	/** Called after garbage collection. */
	void OnPostGarbageCollect();
	
public:
	/** Recorded static and frame data. */
	UPROPERTY()
	FLiveLinkUAssetRecordingData RecordingData;
	
private:
	class FLiveLinkStreamAsyncTask : public FNonAbandonableTask
	{
	public:
		FLiveLinkStreamAsyncTask(ULiveLinkUAssetRecording* InLiveLinkRecording)
		{
			LiveLinkRecording = InLiveLinkRecording;
		}

		~FLiveLinkStreamAsyncTask()
		{
			if (LiveLinkRecording)
			{
				// Make sure we aren't waiting for a pause.
				LiveLinkRecording->OnStreamPausedEvent->Trigger();
			}
		}

		TStatId GetStatId() const
		{
			RETURN_QUICK_DECLARE_CYCLE_STAT(LiveLinkStreamAsyncTask, STATGROUP_ThreadPoolAsyncTasks);
		}

		void DoWork();

	private:
		TObjectPtr<ULiveLinkUAssetRecording> LiveLinkRecording;
	};

	/** The animation data -- bulk data stored within this uasset. */
	FLiveLinkHubBulkData AnimationData;
	
	/** The loaded frame data keys and position. */
	TArray<FFrameFileData> FrameFileData;

	/** The maximum frames for this recording. */
	int32 RecordingMaxFrames = 0;
	
	/** The first (left most) frame to stream. */
	int32 EarliestFrameToStream = 0;

	/** The initial frame to start streaming (the current playhead position).  */
	int32 InitialFrameToStream = 0;

	/** Total frames which should be streamed. */
	int32 TotalFramesToStream = 0;

	/** When the streaming frame has changed, signalling the current stream task should restart. */
	std::atomic<bool> bStreamingFrameChange = false;

	/** Signal that the stream should be canceled. */
	std::atomic<bool> bCancelStream = false;

	/** Signal that the stream should be paused. */
	std::atomic<bool> bPauseStream = false;

	/** True once a full initial load has been performed -- static + frame data. */
	std::atomic<bool> bPerformedInitialLoad = false;

	/** If we are currently saving recording frame data to disk. */
	std::atomic<bool> bIsSavingRecordingData = false;

	/** The maximum frame disk size across frame data. */
	std::atomic<int32> MaxFrameDiskSize = 0;

	/** Mutex for accessing the buffered frames. */
	mutable FCriticalSection BufferedFrameMutex;
	
	/** Mutex for accessing the data container from multiple threads. */
	mutable FCriticalSection DataContainerMutex;

	/** The thread streaming data from disk. */
	TUniquePtr<FAsyncTask<FLiveLinkStreamAsyncTask>> AsyncStreamTask;

	/** Handle for when gc is about to run. */
	FDelegateHandle OnPreGarbageCollectHandle;

	/** Handle for when gc has finished. */
	FDelegateHandle OnPostGarbageCollectHandle;

	/** Signalled when the stream is successfully paused. */
	FEventRef OnStreamPausedEvent = FEventRef(EEventMode::ManualReset);
	
	/** Signalled when the stream has been unpaused. */
	FEventRef OnStreamUnpausedEvent = FEventRef(EEventMode::ManualReset);
	
	/** Test slow frame buffering. */
	float DebugSleepTime = 0.f;

	/** Write the frame buffer size every n iterations. */
	int32 ReportFrameBufferOnIteration = 5;

	/** If the recording is fully loaded into memory. */
	bool bIsFullyLoaded = false;
	
	/** The current version of the recording. */
	const int32 RecordingVersion = 1;
};
