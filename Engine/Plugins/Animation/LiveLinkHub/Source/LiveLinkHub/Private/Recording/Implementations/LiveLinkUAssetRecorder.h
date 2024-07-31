// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Async/AsyncWork.h"
#include "LiveLinkTypes.h"
#include "LiveLinkUAssetRecording.h"
#include "Misc/CoreMiscDefines.h"
#include "Recording/LiveLinkRecorder.h"
#include "Templates/PimplPtr.h"

struct FLiveLinkRecordingBaseDataContainer;
struct FLiveLinkUAssetRecordingData;
struct FInstancedStruct;

/** UAsset implementation for serializing recorded livelink data. */
class FLiveLinkUAssetRecorder : public ILiveLinkRecorder
{
public:

	//~ Begin ILiveLinkRecorder
	virtual void StartRecording() override;
	virtual void StopRecording() override;
	virtual void RecordStaticData(const FLiveLinkSubjectKey& SubjectKey, TSubclassOf<ULiveLinkRole> Role, const FLiveLinkStaticDataStruct& StaticData) override;
	virtual void RecordFrameData(const FLiveLinkSubjectKey& SubjectKey, const FLiveLinkFrameDataStruct& FrameData) override;
	virtual bool IsRecording() const override;
	virtual bool IsSavingRecording(ULiveLinkRecording* InRecording) const override;
	//~ End ILiveLinkRecorder

private:
	/** Prompt the user for a destination path for the recording. */
	bool OpenSaveDialog(const FString& InDefaultPath, const FString& InNewNameSuggestion, FString& OutPackageName);
	/** Creates a unique asset name and prompts the user for the recording name. */
	bool GetSavePresetPackageName(FString& OutName);
	/** Create a recording package and save it. */
	void SaveRecording();
	/** Record data to a ULiveLinkRecording object. */
	void RecordBaseData(FLiveLinkRecordingBaseDataContainer& StaticDataContainer, TSharedPtr<FInstancedStruct>&& DataToRecord);
	/** Record initial data for all livelink subjects. (Useful when static data was sent before the recording started). */
	void RecordInitialStaticData();
	/** Called on the game thread after a recording has been saved. */
	void OnRecordingSaved_GameThread(TWeakObjectPtr<ULiveLinkUAssetRecording> InRecording);

private:
	class FLiveLinkSaveRecordingAsyncTask : public FNonAbandonableTask
	{
	public:
		FLiveLinkSaveRecordingAsyncTask(ULiveLinkUAssetRecording* InLiveLinkRecording, FLiveLinkUAssetRecorder* InRecorder)
		{
			LiveLinkRecording = TStrongObjectPtr(InLiveLinkRecording);
			Recorder = InRecorder;
		}

		TStatId GetStatId() const
		{
			RETURN_QUICK_DECLARE_CYCLE_STAT(LiveLinkSaveRecordingAsyncTask, STATGROUP_ThreadPoolAsyncTasks);
		}

		void DoWork();

	private:
		/** The recording being saved. */
		TStrongObjectPtr<ULiveLinkUAssetRecording> LiveLinkRecording;
		/** The recorder owner. */
		FLiveLinkUAssetRecorder* Recorder = nullptr;
	};

	/** Current async save tasks. */
	TMap<TWeakObjectPtr<ULiveLinkRecording>, TUniquePtr<FAsyncTask<FLiveLinkSaveRecordingAsyncTask>>> AsyncSaveTasks;
	/** Holds metadata and recording data. */
	TPimplPtr<FLiveLinkUAssetRecordingData> CurrentRecording;
	/** Whether we're currently recording livelink data. */
	bool bIsRecording = false;
	/** Timestamp in seconds of when the recording was started. */
	double TimeRecordingStarted = 0.0;
	/** Timestamp in seconds of when the recording ended. */
	double TimeRecordingEnded = 0.0;
};
