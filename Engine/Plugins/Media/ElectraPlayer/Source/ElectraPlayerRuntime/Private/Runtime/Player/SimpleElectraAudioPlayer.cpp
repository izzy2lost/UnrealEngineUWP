// Copyright Epic Games, Inc. All Rights Reserved.

#include "SimpleElectraAudioPlayer.h"
#include "AdaptiveStreamingPlayer.h"
#include "Renderer/RendererAudio.h"
#include "CoreMinimal.h"
#include "Misc/Guid.h"
#include "Misc/Optional.h"
#include "Tickable.h"
#include "CoreGlobals.h"
#include "Stats/Stats.h"
#include "Stats/Stats2.h"
#include "MediaAudioDecoderOutput.h"
#include "Utilities/URLParser.h"
#include "HAL/IConsoleManager.h"
#include "IAnalyticsProviderET.h"

#if !UE_BUILD_SHIPPING && WITH_ENGINE
#define ENABLE_DEBUG_STATS 1
#else
#define ENABLE_DEBUG_STATS 0
#endif

#if ENABLE_DEBUG_STATS
#include "HAL/PlatformTime.h"
#include "Engine/Engine.h"
#include "Engine/Canvas.h"
#include "Debug/DebugDrawService.h"
#endif

DEFINE_LOG_CATEGORY_STATIC(LogSimpleElectraPlayer, Log, All);


namespace
{
// How many _different_ stream analytics to keep in memory.
static int32 ElectraCSAudio_MaxStreamAnalytics = 100;

// Whether or not analytics reporting is enabled.
static int32 ElectraCSAudio_SendAnalytics = 1;


// Remaining play time of a stream to be considered for reactivation.
static float ElectraCSAudio_MinRemainPlaytimeForReactivation = 6.0f;
// Maximum time a stream is allowed to have been in the stopped state
// to be allowed to be reactivated. If stopped for longer than this
// it won't be considered any more.
static float ElectraCSAudio_MaxStoppedDurationForReactivation = 20.0f;

// Maximum number of concurrent instances. 0=unlimited
static int32 ElectraCSAudio_MaxInstances = 0;

// Whether or not to resume instaces that were stopped due to max instance limit being reached.
static int32 ElectraCSAudio_ResumeStopped = 1;

static int32 ElectraCSAudio_BlobCacheSize = 1024 * 512;
static float ElectraCSAudio_BlobCacheAge = 3600.0f;
}

FAutoConsoleVariableRef CVarElectraCSAudio_MaxInstances(
	TEXT("ElectraCSAudio.MaxInstances"),
	ElectraCSAudio_MaxInstances,
	TEXT("Maximum concurrent cloud stream instances (0=no limit)"),
	ECVF_Default);

FAutoConsoleVariableRef CVarElectraCSAudio_ResumeStopped(
	TEXT("ElectraCSAudio.ResumeStopped"),
	ElectraCSAudio_ResumeStopped,
	TEXT("Whether or not to resume previously stopped streams when an instance becomes available again (0=do not resume, 1=resume)"),
	ECVF_Default);

FAutoConsoleVariableRef CVarElectraCSAudio_MinRemainResumeTime(
	TEXT("ElectraCSAudio.MinRemainResumeTime"),
	ElectraCSAudio_MinRemainPlaytimeForReactivation,
	TEXT("How many seconds of remaining playtime a stopped stream needs to have to be resumed."),
	ECVF_Default);

FAutoConsoleVariableRef CVarElectraCSAudio_MaxStoppedResumeDuration(
	TEXT("ElectraCSAudio.MaxStoppedResumeDuration"),
	ElectraCSAudio_MaxStoppedDurationForReactivation,
	TEXT("How many seconds a stream may be stopped at most in order to be resumed."),
	ECVF_Default);

FAutoConsoleVariableRef CVarElectraCSAudio_MaxBlobCacheSize(
	TEXT("ElectraCSAudio.MaxBlobCacheSize"),
	ElectraCSAudio_BlobCacheSize,
	TEXT("Size of the blob response cache in bytes."),
	ECVF_Default);

FAutoConsoleVariableRef CVarElectraCSAudio_MaxBlobCacheDuration(
	TEXT("ElectraCSAudio.MaxBlobCacheDuration"),
	ElectraCSAudio_BlobCacheAge,
	TEXT("How many seconds a blob response remains valid in the cache."),
	ECVF_Default);

FAutoConsoleVariableRef CVarElectraCSAudio_MaxStreamAnalytics(
	TEXT("ElectraCSAudio.MaxStreamAnalytics"),
	ElectraCSAudio_MaxStreamAnalytics,
	TEXT("Maximum number of stream URLs for which to track analytics at the same time."),
	ECVF_Default);

FAutoConsoleVariableRef CVarElectraCSAudio_SendAnalytics(
	TEXT("ElectraCSAudio.SendAnalytics"),
	ElectraCSAudio_SendAnalytics,
	TEXT("Whether or not to send analytic metrics. (0=off (default), 1=on)"),
	ECVF_Default);

using namespace Electra;

namespace SimpleElectraAudioPlayerUtil
{
#if UE_BUILD_SHIPPING
#define HIDE_URLS_FROM_LOG	1
#else
#define HIDE_URLS_FROM_LOG	0
#endif

static FString RedactMessage(FString InMessage)
{
#if !HIDE_URLS_FROM_LOG
	return MoveTemp(InMessage);
#else
	int32 searchPos = 0;
	while(1)
	{
		static FString SchemeStr(TEXT("://"));
		static FString DotDotDotStr(TEXT("..."));
		static FString TermChars(TEXT("'\",; "));
		int32 schemePos = InMessage.Find(SchemeStr, ESearchCase::IgnoreCase, ESearchDir::FromStart, searchPos);
		if (schemePos != INDEX_NONE)
		{
			schemePos += SchemeStr.Len();
			// There may be a generic user message following a potential URL that we do not want to clobber.
			// We search for any next character that tends to end a URL in a user message, like one of ['",; ]
			int32 EndPos = InMessage.Len();
			int32 Start = schemePos;
			while(Start < EndPos)
			{
				int32 pos;
				if (TermChars.FindChar(InMessage[Start], pos))
				{
					break;
				}
				++Start;
			}
			InMessage.RemoveAt(schemePos, Start-schemePos);
			InMessage.InsertAt(schemePos, DotDotDotStr);
			searchPos = schemePos + SchemeStr.Len();
		}
		else
		{
			break;
		}
	}
	return InMessage;
#endif
}
}

class FSimpleElectraAudioPlayerRenderer;

class FSimpleElectraAudioPlayer : public ISimpleElectraAudioPlayer, public IAdaptiveStreamingPlayerMetrics, public FTickableGameObject, public TSharedFromThis<FSimpleElectraAudioPlayer, ESPMode::ThreadSafe>
{
public:
	static TSharedPtr<FSimpleElectraAudioPlayer, ESPMode::ThreadSafe> Create(const FCreateParams& InCreateParams)
	{
		return MakeShareable(new FSimpleElectraAudioPlayer(InCreateParams), TDeleter());
	}
	static void SendAnalyticMetrics(const TSharedPtr<IAnalyticsProviderET>& InAnalyticsProvider);

	static TSharedPtr<ICacheElementBase, ESPMode::ThreadSafe> GetCustomCacheElement(const FString& InForURL);
	static void SetCustomCacheElement(const FString& InForURL, const TSharedPtr<ICacheElementBase, ESPMode::ThreadSafe>& InElement);

	FSimpleElectraAudioPlayer(const FCreateParams& InCreateParams);
	~FSimpleElectraAudioPlayer();
	bool Open(const TMap<FString, FVariant>& InOptions, const FString& InManifestURL, const FTimespan& InStartPosition, const FTimespan& InEncodedDuration, bool bInAutoPlay, bool bInSetLooping, TSharedPtr<IElectraPlayerDataCache, ESPMode::ThreadSafe> InPlayerDataCache) override;
	void PrepareToLoopToBeginning() override;
	void SeekTo(const FTimespan& NewPosition) override
	{
		FlushAudio(true);
		StartPosition = NewPosition;
		IAdaptiveStreamingPlayer::FSeekParam sp;
		sp.Time.SetFromTimespan(NewPosition);
		if (TSharedPtr<IAdaptiveStreamingPlayer, ESPMode::ThreadSafe> LockedPlayer = Player)
		{
			LockedPlayer->SeekTo(sp);
		}
		else
		{
			bMaybeReopenAfterSeek = true;
		}
	}
	void Pause() override
	{
		if (TSharedPtr<IAdaptiveStreamingPlayer, ESPMode::ThreadSafe> LockedPlayer = Player)
		{
			LockedPlayer->Pause();
		}
	}
	void Resume() override
	{ 
		if (TSharedPtr<IAdaptiveStreamingPlayer, ESPMode::ThreadSafe> LockedPlayer = Player)
		{
			LockedPlayer->Resume();
		}
	}
	void Stop() override
	{ 
		if (TSharedPtr<IAdaptiveStreamingPlayer, ESPMode::ThreadSafe> LockedPlayer = Player)
		{
			LockedPlayer->Stop(); 
		}
	}
	bool HasErrored() const override
	{ return bHasErrored; }
	FString GetError() const override
	{ return ErrorMessage; }
	bool HaveMetadata() const override
	{ 
		if (TSharedPtr<IAdaptiveStreamingPlayer, ESPMode::ThreadSafe> LockedPlayer = Player)
		{
			return LockedPlayer->HaveMetadata();
		}
		return false;
	}
	int32 GetBinaryMetadata(TSharedPtr<TArray<uint8>, ESPMode::ThreadSafe>& OutMetadata) const override;
	FTimespan GetDuration() const override
	{ 
		if (TSharedPtr<IAdaptiveStreamingPlayer, ESPMode::ThreadSafe> LockedPlayer = Player)
		{
			return LockedPlayer->GetDuration().GetAsTimespan();
		}
		return FTimespan::Zero();
	}
	FTimespan GetPlayPosition() const override
	{ return NextPTS; }
	bool HasEnded() const override
	{ 
		if (TSharedPtr<IAdaptiveStreamingPlayer, ESPMode::ThreadSafe> LockedPlayer = Player)
		{
			return LockedPlayer->HasEnded();
		}
		return false;
	}
	bool IsBuffering() const override
	{ 
		if (TSharedPtr<IAdaptiveStreamingPlayer, ESPMode::ThreadSafe> LockedPlayer = Player)
		{
			return LockedPlayer->IsBuffering(); 
		}
		return false;
	}
	bool IsSeeking() const override
	{ 
		if (TSharedPtr<IAdaptiveStreamingPlayer, ESPMode::ThreadSafe> LockedPlayer = Player)
		{
			return LockedPlayer->IsSeeking(); 
		}
		return false;
	}
	bool IsPlaying() const override
	{ 
		if (TSharedPtr<IAdaptiveStreamingPlayer, ESPMode::ThreadSafe> LockedPlayer = Player)
		{
			return LockedPlayer->IsPlaying(); 
		}
		return false;
	}
	bool IsPaused() const override
	{ 
		if (TSharedPtr<IAdaptiveStreamingPlayer, ESPMode::ThreadSafe> LockedPlayer = Player)
		{
			return LockedPlayer->IsPaused(); 
		}
		return false;
	}
	void SetBitrateCeiling(int32 HighestSelectableBitrate) override
	{ 
		if (TSharedPtr<IAdaptiveStreamingPlayer, ESPMode::ThreadSafe> LockedPlayer = Player)
		{
			LockedPlayer->SetBitrateCeiling(HighestSelectableBitrate); 
		}
	}

	bool GetStreamFormat(FStreamFormat& OutFormat) const override;
	int64 GetNextSamples(FTimespan& OutPTS, bool& bOutIsFirstBlock, int16* OutBuffer, int32 InBufferSizeInFrames, int32 InNumSamplesToGet, const FDefaultSampleInfo& InDefaultSampleInfo) override;

	void FlushAudio(bool bForceFlush);
	bool CanAcceptAudioFrames(int32 InNumFrames);
	bool GetEnqueuedFrameInfo(int32& OutNumberOfEnqueuedFrames, FTimespan& OutDurationOfEnqueuedFrames) const;
	bool EnqueueAudioFrames(const void* InBufferAddress, int32 InNumSamples, int32 InNumChannels, int32 InSampleRate, const FTimespan& InPTS, int64 InSequenceIndex, const FTimespan& InDuration);
	void SetReceivedLastBuffer();

	void ReportOpenSource(const FString& InURL) override
	{
		if (!bIsResuming)
		{
			FString Msg = FString::Printf(TEXT("%s: Opening"), *AssetName);
			UE_LOG(LogSimpleElectraPlayer, Verbose, TEXT("%s"), *Msg);
		}
	}
	void ReportReceivedMasterPlaylist(const FString& InEffectiveURL) override
	{ }
	void ReportReceivedPlaylists() override
	{
		if (TSharedPtr<IAdaptiveStreamingPlayer, ESPMode::ThreadSafe> LockedPlayer = Player)
		{
			Duration = LockedPlayer->GetDuration().GetAsTimespan();
			LockedPlayer->DeselectTrack(EStreamType::Video);
			IAdaptiveStreamingPlayer::FSeekParam sp;
			if (StartPosition > FTimespan::Zero())
			{
				sp.Time.SetFromTimespan(StartPosition);
			}
			LockedPlayer->SeekTo(sp);
		}
	}
	void ReportTracksChanged() override
	{ }
	void ReportPlaylistDownload(const Metrics::FPlaylistDownloadStats& InPlaylistDownloadStats) override
	{ }
	void ReportCleanStart() override
	{ }
	void ReportBufferingStart(Metrics::EBufferingReason InBufferingReason) override
	{ }
	void ReportBufferingEnd(Metrics::EBufferingReason InBufferingReason) override
	{ }
	void ReportBandwidth(int64 InEffectiveBps, int64 InThroughputBps, double InLatencyInSeconds) override
	{ }
	void ReportBufferUtilization(const Metrics::FBufferStats& InBufferStats) override
	{ }
	void ReportSegmentDownload(const Metrics::FSegmentDownloadStats& InSegmentDownloadStats) override
	{
		if (!InSegmentDownloadStats.bWasSuccessful || InSegmentDownloadStats.RetryNumber)
		{
			FString Msg = FString::Printf(TEXT("%s: Segment download issue \"%s\", retry:%d, aborted:%d, silence:%d"), *AssetName,
											*InSegmentDownloadStats.FailureReason, InSegmentDownloadStats.RetryNumber, InSegmentDownloadStats.bWasAborted, InSegmentDownloadStats.bInsertedFillerData);
			UE_LOG(LogSimpleElectraPlayer, Verbose, TEXT("%s"), *Msg);
		}

		// Update analytics
		Analytics.BestQualityLevel = InSegmentDownloadStats.HighestQualityIndex;
		if (InSegmentDownloadStats.bWasSuccessful)
		{
			++Analytics.NumSegmentsLoadedAtQuality.FindOrAdd(InSegmentDownloadStats.QualityIndex);
			if (InSegmentDownloadStats.bIsCachedResponse)
			{
				++Analytics.CacheHitsAtQuality.FindOrAdd(InSegmentDownloadStats.QualityIndex);
			}
		}
		else if (!InSegmentDownloadStats.bWasAborted && !InSegmentDownloadStats.bIsMissingSegment && !InSegmentDownloadStats.bWaitingForRemoteRetryElement)
		{
			// Use error `1` for timeout and `10` for parse errors.
			int32 ErrorCode = InSegmentDownloadStats.bDidTimeout ? 1 : InSegmentDownloadStats.bParseFailure ? 10 : InSegmentDownloadStats.HTTPStatusCode;
			++Analytics.ErrorCodeAndCount.FindOrAdd(ErrorCode);
		}
	}
	void ReportLicenseKey(const Metrics::FLicenseKeyStats& InLicenseKeyStats) override
	{ }
	void ReportVideoQualityChange(int32 InNewBitrate, int32 InPreviousBitrate, bool bInIsDrasticDownswitch) override
	{ }
	void ReportDataAvailabilityChange(const Metrics::FDataAvailabilityChange& InDataAvailability) override
	{ }
	void ReportDecodingFormatChange(const FStreamCodecInformation& InNewDecodingFormat) override
	{ }
	void ReportPrerollStart() override
	{ }
	void ReportPrerollEnd() override
	{
		if (bAutoPlay)
		{
			if (TSharedPtr<IAdaptiveStreamingPlayer, ESPMode::ThreadSafe> LockedPlayer = Player)
			{
				LockedPlayer->Resume();
			}
		}
	}
	void ReportPlaybackStart() override
	{ }
	void ReportPlaybackPaused() override
	{ }
	void ReportPlaybackResumed() override
	{ }
	void ReportPlaybackEnded() override
	{ }
	void ReportJumpInPlayPosition(const FTimeValue& InToNewTime, const FTimeValue& InFromTime, Metrics::ETimeJumpReason InTimejumpReason) override
	{ }
	void ReportPlaybackStopped() override
	{ }
	void ReportSeekCompleted() override
	{ }
	void ReportMediaMetadataChanged(TSharedPtrTS<UtilsMP4::FMetadataParser> InMetadata) override
	{ }
	void ReportError(const FString& InErrorReason) override
	{
		bHasErrored = true;
		ErrorMessage = InErrorReason;
		FString Msg = FString::Printf(TEXT("%s: %s"), *AssetName, *SimpleElectraAudioPlayerUtil::RedactMessage(InErrorReason));
		UE_LOG(LogSimpleElectraPlayer, Error, TEXT("%s"), *Msg);
	}
	void ReportLogMessage(IInfoLog::ELevel InLogLevel, const FString& InLogMessage, int64 InPlayerWallclockMilliseconds) override
	{
		// Ignore common authoring mistakes.
		if (InLogMessage.Contains(TEXT("differs from timescale")))
		{
			return;
		}

		FString Msg = FString::Printf(TEXT("%s: %s"), *AssetName, *SimpleElectraAudioPlayerUtil::RedactMessage(InLogMessage));
		switch(InLogLevel)
		{
			case IInfoLog::ELevel::Verbose:
			{
				UE_LOG(LogSimpleElectraPlayer, Verbose, TEXT("%s"), *Msg);
				break;
			}
			case IInfoLog::ELevel::Info:
			{
				UE_LOG(LogSimpleElectraPlayer, Log, TEXT("%s"), *Msg)
				break;
			}
			case IInfoLog::ELevel::Warning:
			{
				UE_LOG(LogSimpleElectraPlayer, Warning, TEXT("%s"), *Msg)
				break;
			}
			case IInfoLog::ELevel::Error:
			{
				UE_LOG(LogSimpleElectraPlayer, Error, TEXT("%s"), *Msg)
				break;
			}
		}
	}
	void ReportDroppedVideoFrame() override
	{ }
	void ReportDroppedAudioFrame() override
	{ }

private:
	void Tick(float DeltaTime) override;
	ETickableTickType GetTickableTickType() const override
	{ return ETickableTickType::Conditional; }
	bool IsTickable() const override
	{ return bIsReadyToTick; }
	bool IsAllowedToTick() const override
	{ return bIsReadyToTick; }
	TStatId GetStatId() const override
	{ RETURN_QUICK_DECLARE_CYCLE_STAT(FSimpleElectraAudioPlayer, STATGROUP_Tickables); }
	bool IsTickableWhenPaused() const override
	{ return true; }
	bool IsTickableInEditor() const override
	{ return true; }


	class TDeleter
	{
	public:
		void operator()(FSimpleElectraAudioPlayer* InInstanceToDelete)
		{
			InInstanceToDelete->bDestructionRequested = true;
		}
	};

	class FResourceProvider : public IAdaptiveStreamingPlayerResourceProvider
	{
	public:
		void SetPlaylistURL(const FString& InPlaylistURL)
		{ PlaylistURL = InPlaylistURL; }
		void SetOptions(const TMap<FString, FVariant>& InOptions)
		{ Options = InOptions; }
		void ProvideStaticPlaybackDataForURL(TSharedPtr<IAdaptiveStreamingPlayerResourceRequest, ESPMode::ThreadSafe> InOutRequest) override;
	private:
		FString PlaylistURL;
		TMap<FString, FVariant> Options;
	};

	class FBlobRequest
	{
	public:
		FBlobRequest() : Request(MakeShared<Electra::FHTTPResourceRequest, ESPMode::ThreadSafe>())
		{ }
		void OnBlobRequestComplete(TSharedPtrTS<Electra::FHTTPResourceRequest> InRequest)
		{
			ErrorCode = 0;
			if (!InRequest->GetWasCanceled())
			{
				ErrorCode = InRequest->GetError();
				BlobData = MakeShared<TArray<uint8>, ESPMode::ThreadSafe>();
				TSharedPtrTS<IElectraHttpManager::FReceiveBuffer> ResponseBuffer = InRequest->GetResponseBuffer();
				if (ResponseBuffer.IsValid())
				{
					BlobData->Append((const uint8*)ResponseBuffer->Buffer.GetLinearReadData(), ResponseBuffer->Buffer.Num());
				}
			}
			bIsComplete = true;
			Request.Reset();
		}
		void SetBlobData(TSharedPtr<TArray<uint8>, ESPMode::ThreadSafe> InBlobData)
		{
			ErrorCode = 0;
			bIsComplete = true;
			BlobData = MoveTemp(InBlobData);
			Request.Reset();
		}
		TSharedPtr<Electra::FHTTPResourceRequest, ESPMode::ThreadSafe> Request;
		TSharedPtr<TArray<uint8>, ESPMode::ThreadSafe> BlobData;
		FString URL;
		volatile int32 ErrorCode = -1;
		volatile bool bIsComplete = false;
		bool bReportedError = false;
		bool bWasCached = false;
	};

	struct FBlobCacheEntry
	{
		TSharedPtr<TArray<uint8>, ESPMode::ThreadSafe> Data;
		TSharedPtr<ISimpleElectraAudioPlayer::ICacheElementBase, ESPMode::ThreadSafe> CustomData;
		FString URL;
		double TimeAdded = 0.0;
	};

	class FBlobCache
	{
	public:
		TSharedPtr<TArray<uint8>, ESPMode::ThreadSafe> GetData(const FString& InForURL);
		void AddData(const FString& InForURL, const TSharedPtr<TArray<uint8>, ESPMode::ThreadSafe>& InData);

		TSharedPtr<ISimpleElectraAudioPlayer::ICacheElementBase, ESPMode::ThreadSafe> GetCustomData(const FString& InForURL);
		void AddCustomData(const FString& InForURL, const TSharedPtr<ISimpleElectraAudioPlayer::ICacheElementBase, ESPMode::ThreadSafe>& InData);
	private:
		void CheckLimits(int32 InBytesNeeded);
		FCriticalSection Lock;
		TMap<FString, TSharedPtr<FBlobCacheEntry, ESPMode::ThreadSafe>> EntryMap;
		TArray<TSharedPtr<FBlobCacheEntry, ESPMode::ThreadSafe>> EntryLRU;
		int32 SizeInUse = 0;
	};

	struct FSampleBlock
	{
		FTimespan PTS;
		void* Buffer = nullptr;
		int32 NumSamples = 0;
		int32 Offset = 0;
		void Free()
		{
			if (Buffer)
			{
				FMemory::Free(Buffer);
				Buffer = nullptr;
			}
		}
	};

	enum class EState
	{
		Uninitialized,
		OpenBlob,
		OpeningStream,
		TryReopeningStream,
		CreatingPlayer,
		Active,
		Stopped,
		Errored
	};

	const int32 kNumDummyBlockSamples = 1024;

	FCreateParams CreateParams;

	volatile EState CurrentState = EState::Uninitialized;
	volatile bool bIsReadyToTick = false;
	volatile bool bAskedToBeReleased = false;
	volatile bool bCloseInProgress = false;
	volatile bool bDestructionRequested = false;
	volatile bool bDestructionIssued = false;
	volatile bool bDestructionComplete = false;

	const double SuggestedBufferDuration = 0.5;
	const int32 MinNumSamplesInBuffer = 16384;	// Minimum number of samples, regardless of sample rate.

	TOptional<FString> BlobParameters;
	TSharedPtr<FBlobRequest, ESPMode::ThreadSafe> PendingBlobRequest;

	TMap<FString, FVariant> Options;
	TSharedPtr<IAdaptiveStreamingPlayer, ESPMode::ThreadSafe> Player;
	TSharedPtr<FSimpleElectraAudioPlayerRenderer, ESPMode::ThreadSafe> Renderer;
	TSharedPtr<FResourceProvider, ESPMode::ThreadSafe> ResourceProvider;
	TWeakPtr<IElectraPlayerDataCache, ESPMode::ThreadSafe> PlayerDataCache;

	FString URL;
	FString BaseURL;
	FString AssetName;
	FTimespan StartPosition;
	bool bAutoPlay = true;
	bool bSetLooping = false;

	FTimespan EncodedDuration;
	FTimespan Duration;
	FString ErrorMessage;
	bool bHasErrored = false;

	mutable FCriticalSection Lock;
	FStreamFormat StreamFormat;

	struct FBlockSequence
	{
		~FBlockSequence()
		{
			for(int32 i=0; i<SampleBlocks.Num(); ++i)
			{
				SampleBlocks[i].Free();
			}
			SampleBlocks.Empty();
		}

		TArray<FSampleBlock> SampleBlocks;
		int64 SequenceIndex = 0;
		int64 NumFramesAvailable = 0;
		bool bReachedEOS = false;
		bool bReadEnded = false;
	};
	TArray<TSharedPtr<FBlockSequence, ESPMode::ThreadSafe>> NextPendingSampleBlocks;
	TSharedPtr<FBlockSequence, ESPMode::ThreadSafe> CurrentWriteSampleBlock;
	TSharedPtr<FBlockSequence, ESPMode::ThreadSafe> CurrentReadSampleBlock;

	void ActivateBlockReadSequence()
	{
		if ((!CurrentReadSampleBlock.IsValid() || CurrentReadSampleBlock->SequenceIndex == -1) && NextPendingSampleBlocks.Num())
		{
			CurrentReadSampleBlock = NextPendingSampleBlocks[0];
			NextPendingSampleBlocks.RemoveAt(0);
		}
	}

	FTimespan NextPTS;
	int64 NumActiveFramesTotal = 0;
	int32 NumEnqueuedBlocks = 0;
	bool bIsFirstSampleBlock = false;

	double TimeCreated = -1.0;
	double TimeUntilReady = -1.0;
	double TimeStopped = -1.0;
	bool bMaybeReopenAfterSeek = false;
	bool bIsResuming = false;

	void CreatePlayerInstance();
	void ClosePlayerInstance(bool bDeleteThis);

	struct FClosePlayerInstanceHelper
	{
		TSharedPtr<IAdaptiveStreamingPlayer, ESPMode::ThreadSafe> PlayerInstance;
		FSimpleElectraAudioPlayer* This = nullptr;
		bool bDeleteThis = false;
	};
	static void DoClosePlayerAsync(TSharedPtr<FClosePlayerInstanceHelper, ESPMode::ThreadSafe>&& InPlayerHelper);

	bool HasBeenSuspended() const;
	void UpdateAssetName();
	void InternalApplyOptions();
	void HandleOpenStream(bool bOnlyIfAvailable);
	static void MaybeRestartAStoppedStream(FSimpleElectraAudioPlayer* This);
	void CreatePlayerAsync();
	void CreateIdleBufferIfNecessary();

	static FBlobCache& GetBlobCache()
	{
		static FBlobCache Cache;
		return Cache;
	}

	static FCriticalSection InstanceLock;
	static TArray<FSimpleElectraAudioPlayer*> AllInstances;
	static TArray<FSimpleElectraAudioPlayer*> ActiveInstances;
	static TArray<FSimpleElectraAudioPlayer*> StoppedInstances;


	struct FAnalyticsEntry
	{
		int32 NumTimesStarted = 0;
		int32 NumTimesSuspended = 0;
		int32 NumTimesResumed = 0;
		int32 BestQualityLevel = 0;
		double StartLatency = 0.0;
		TMap<int32, int32> ErrorCodeAndCount;
		TMap<int32, int32> NumSegmentsLoadedAtQuality;
		TMap<int32, int32> CacheHitsAtQuality;
		FString Analytics_ErrorCodeAndCount;
		FString Analytics_NumSegmentsLoadedAtQuality;
		FString Analytics_CacheHitsAtQuality;

		void PrepareAnalytics()
		{
			Analytics_ErrorCodeAndCount = MakeAnalyticsStringFromMap(ErrorCodeAndCount);
			Analytics_NumSegmentsLoadedAtQuality = MakeAnalyticsStringFromMap(NumSegmentsLoadedAtQuality);
			Analytics_CacheHitsAtQuality = MakeAnalyticsStringFromMap(CacheHitsAtQuality);
		}
	
		FString MakeAnalyticsStringFromMap(TMap<int32, int32>& InMap) const
		{
			InMap.KeySort([](int32 A, int32 B){return A<B;});
			FString s;
			int32 i=0;
			for(auto It=InMap.CreateConstIterator(); It; ++It, ++i)
			{
				if (i)
				{
					s.AppendChar(TCHAR(','));
				}
				s += FString::Printf(TEXT("%d:%d"), It->Key, It->Value);
			}
			return s;
		}
	};
	mutable FAnalyticsEntry Analytics;
	static FCriticalSection AnalyticsLock;
	static TMap<FString, FAnalyticsEntry> AnalyticEntries;
	static void MergeAnalytics(const FString& AssetId, const FAnalyticsEntry& InAnalytics);


#if ENABLE_DEBUG_STATS
public:
	void DebugDrawInst(UCanvas* InCanvas);

	/** Needed for debug draw management. */
	static void EnableDebugDraw(bool bEnable);
	static void DebugDraw(UCanvas* InCanvas, APlayerController* InPlayerController);
	static void DebugDrawPrintLine(UCanvas* InCanvas, const FString& InString, const FLinearColor& InColor);
	static FDelegateHandle DebugDrawDelegateHandle;
	static UFont* DebugDrawFont;
	static int32 DebugDrawFontRowHeight;
	static FVector2D DebugDrawTextPos;
	static const FLinearColor DebugColor_Orange;
	static const FLinearColor DebugColor_Green;
	static const FLinearColor DebugColor_Red;
#endif
};
FCriticalSection FSimpleElectraAudioPlayer::InstanceLock;
TArray<FSimpleElectraAudioPlayer*> FSimpleElectraAudioPlayer::AllInstances;
TArray<FSimpleElectraAudioPlayer*> FSimpleElectraAudioPlayer::ActiveInstances;
TArray<FSimpleElectraAudioPlayer*> FSimpleElectraAudioPlayer::StoppedInstances;

FCriticalSection FSimpleElectraAudioPlayer::AnalyticsLock;
TMap<FString, FSimpleElectraAudioPlayer::FAnalyticsEntry> FSimpleElectraAudioPlayer::AnalyticEntries;


class FSimpleElectraAudioPlayerRenderer : public IMediaRenderer, public TSharedFromThis<FSimpleElectraAudioPlayerRenderer, ESPMode::ThreadSafe>
{
public:
	FSimpleElectraAudioPlayerRenderer(TSharedPtr<FSimpleElectraAudioPlayer, ESPMode::ThreadSafe> InParentPlayer)
	{
		Player = InParentPlayer;
	}
	virtual ~FSimpleElectraAudioPlayerRenderer()
	{ }

	//-------------------------------------------------------------------------
	// Methods for IMediaRenderer
	//
	const FParamDict& GetBufferPoolProperties() const override
	{ return BufferPoolProperties; }
	UEMediaError CreateBufferPool(const FParamDict& Parameters) override;
	UEMediaError AcquireBuffer(IBuffer*& OutBuffer, int32 TimeoutInMicroseconds, const FParamDict& InParameters) override;
	UEMediaError ReturnBuffer(IBuffer* Buffer, bool bRender, const FParamDict& InSampleProperties) override;
	UEMediaError ReleaseBufferPool() override;
	bool CanReceiveOutputFrames(uint64 NumFrames) const override;
	bool GetEnqueuedFrameInfo(int32& OutNumberOfEnqueuedFrames, FTimeValue& OutDurationOfEnqueuedFrames) const override;
	void SetRenderClock(TSharedPtr<IMediaRenderClock, ESPMode::ThreadSafe> RenderClock) override
	{ }
	void SetParentRenderer(TWeakPtr<IMediaRenderer, ESPMode::ThreadSafe> ParentRenderer) override;
	void SetNextApproximatePresentationTime(const FTimeValue& NextApproxPTS) override
	{ }
	UEMediaError Flush(const FParamDict& InOptions) override;
	void StartRendering(const FParamDict& InOptions) override
	{ }
	void StopRendering(const FParamDict& InOptions) override
	{ }
	void SampleReleasedToPool(IDecoderOutput* InDecoderOutput) override;
private:
	class FMediaBufferSharedPtrWrapper : public IMediaRenderer::IBuffer
	{
	public:
		explicit FMediaBufferSharedPtrWrapper(IAudioDecoderOutputPtr InDecoderOutput) : DecoderOutput(InDecoderOutput)
		{ }
		virtual ~FMediaBufferSharedPtrWrapper() = default;
		const FParamDict& GetBufferProperties() const override
		{ return BufferProperties; }
		FParamDict& GetMutableBufferProperties() override
		{ return BufferProperties; }

		FParamDict BufferProperties;
		IAudioDecoderOutputPtr DecoderOutput;
	};

	TWeakPtr<FSimpleElectraAudioPlayer, ESPMode::ThreadSafe> Player;
	TWeakPtr<IMediaRenderer, ESPMode::ThreadSafe> ParentRenderer;
	TDecoderOutputObjectPool<IAudioDecoderOutput, FElectraPlayerPlatformAudioDecoderOutputFactory> DecoderOutputPool;
	FParamDict BufferPoolProperties;
	uint32 MaxBufferSize = 0;
	int32 NumOutputAudioBuffersInUse = 0;
	int32 NumBuffers = 0;
	int32 NumBuffersAcquiredForDecoder = 0;
};


TSharedPtr<ISimpleElectraAudioPlayer, ESPMode::ThreadSafe> ISimpleElectraAudioPlayer::Create(const FCreateParams& InCreateParams)
{
	return FSimpleElectraAudioPlayer::Create(InCreateParams);
}

void ISimpleElectraAudioPlayer::SendAnalyticMetrics(const TSharedPtr<IAnalyticsProviderET>& InAnalyticsProvider)
{
	if (ElectraCSAudio_SendAnalytics)
	{
		FSimpleElectraAudioPlayer::SendAnalyticMetrics(InAnalyticsProvider);
	}
}

TSharedPtr<ISimpleElectraAudioPlayer::ICacheElementBase, ESPMode::ThreadSafe> ISimpleElectraAudioPlayer::GetCustomCacheElement(const FString& InForURL)
{
	return FSimpleElectraAudioPlayer::GetCustomCacheElement(InForURL);
}

void ISimpleElectraAudioPlayer::SetCustomCacheElement(const FString& InForURL, const TSharedPtr<ISimpleElectraAudioPlayer::ICacheElementBase, ESPMode::ThreadSafe>& InElement)
{
	FSimpleElectraAudioPlayer::SetCustomCacheElement(InForURL, InElement);
}

TSharedPtr<ISimpleElectraAudioPlayer::ICacheElementBase, ESPMode::ThreadSafe> FSimpleElectraAudioPlayer::GetCustomCacheElement(const FString& InForURL)
{
	return GetBlobCache().GetCustomData(InForURL);
}

void FSimpleElectraAudioPlayer::SetCustomCacheElement(const FString& InForURL, const TSharedPtr<ISimpleElectraAudioPlayer::ICacheElementBase, ESPMode::ThreadSafe>& InElement)
{
	GetBlobCache().AddCustomData(InForURL, InElement);
}

FSimpleElectraAudioPlayer::FSimpleElectraAudioPlayer(const FCreateParams& InCreateParams)
	: CreateParams(InCreateParams)
{
	if (!CreateParams.InstanceGUID.IsValid())
	{
		CreateParams.InstanceGUID = FGuid::NewGuid();
	}

	StreamFormat.NumChannels = 0;
	StreamFormat.SampleRate = 0;
	NextPTS = FTimespan::MinValue();

	InstanceLock.Lock();
	AllInstances.AddUnique(this);
	InstanceLock.Unlock();

	bIsReadyToTick = true;
}

FSimpleElectraAudioPlayer::~FSimpleElectraAudioPlayer()
{
	bIsReadyToTick = false;

	InstanceLock.Lock();
	check(AllInstances.Find(this) == INDEX_NONE);
	check(ActiveInstances.Find(this) == INDEX_NONE);
	check(StoppedInstances.Find(this) == INDEX_NONE);
	InstanceLock.Unlock();
	// The player instance must have been destroyed already!
	check(!Player.IsValid());
	ResourceProvider.Reset();
	Renderer.Reset();
	FlushAudio(true);
}

void FSimpleElectraAudioPlayer::Tick(float DeltaTime)
{
	if (bDestructionRequested)
	{
		if (!bDestructionIssued)
		{
			if (!bCloseInProgress)
			{
				FString Msg = FString::Printf(TEXT("%s: Closing"), *AssetName);
				UE_LOG(LogSimpleElectraPlayer, Verbose, TEXT("%s"), *Msg);

				bDestructionIssued = true;
				ClosePlayerInstance(true);
				InstanceLock.Lock();
				StoppedInstances.Remove(this);
				InstanceLock.Unlock();
			}
		}
		if (bDestructionComplete)
		{
			bIsReadyToTick = false;
			InstanceLock.Lock();
			AllInstances.Remove(this);
			ActiveInstances.Remove(this);
			StoppedInstances.Remove(this);
			InstanceLock.Unlock();

			if (ElectraCSAudio_ResumeStopped > 0)
			{
				MaybeRestartAStoppedStream(this);
			}
			delete this;
		}
		return;
	}

	switch(CurrentState)
	{
		case EState::OpeningStream:
		{
			HandleOpenStream(false);
			break;
		}
		case EState::TryReopeningStream:
		{
			CurrentState = EState::Stopped;
			HandleOpenStream(true);
			break;
		}
		case EState::CreatingPlayer:
		{
			return;
		}
		case EState::Errored:
		{
			return;
		}
		case EState::Stopped:
		{
			if (bMaybeReopenAfterSeek)
			{
				bMaybeReopenAfterSeek = false;
				StartPosition = NextPTS;
				HandleOpenStream(true);
			}
			return;
		}
	}

	if (bAskedToBeReleased && CurrentState != EState::CreatingPlayer)
	{
		FString Msg = FString::Printf(TEXT("%s: Stopping due to limit"), *AssetName);
		UE_LOG(LogSimpleElectraPlayer, Verbose, TEXT("%s"), *Msg);

		TimeStopped = FPlatformTime::Seconds();
		++Analytics.NumTimesSuspended;
		ClosePlayerInstance(false);
	}
}

void FSimpleElectraAudioPlayer::HandleOpenStream(bool bOnlyIfAvailable)
{
	int32 MaxAllowed = CreateParams.MaxTotalPlayerInstances;
	// CVar overrides unconditionally.
	if (ElectraCSAudio_MaxInstances > 0)
	{
		MaxAllowed = ElectraCSAudio_MaxInstances;
	}

	bool bCreateNow = false;

	// Is there a limit on the number of streams?
	if (MaxAllowed > 0)
	{
		// Check if we need to make room for this new stream.
		FScopeLock lock(&InstanceLock);
		
		int32 NumActive = ActiveInstances.Num();
		int32 NumRequestedToBeReleased = 0;
		for(auto& Inst : ActiveInstances)
		{
			NumRequestedToBeReleased += Inst->bAskedToBeReleased ? 1 : 0;
		}
		if (NumActive >= MaxAllowed)
		{
			if (!bOnlyIfAvailable)
			{
				int32 NumToRemove = NumActive - NumRequestedToBeReleased - MaxAllowed + 1;

				for(int32 i=0; NumToRemove>0 && i<NumActive; ++i)
				{
					if (!ActiveInstances[i]->bAskedToBeReleased)
					{
						ActiveInstances[i]->bAskedToBeReleased = true;
						--NumToRemove;
					}
				}
			}
		}
		else
		{
			bCreateNow = true;
		}
	}
	else
	{
		bCreateNow = true;
	}

	if (bCreateNow)
	{
		FString Msg = FString::Printf(TEXT("%s: %s playback"), *AssetName, !bAskedToBeReleased ? TEXT("Preparing") : TEXT("Resuming"));
		UE_LOG(LogSimpleElectraPlayer, Verbose, TEXT("%s"), *Msg);

		// Track use count
		if (bAskedToBeReleased)
		{
			++Analytics.NumTimesResumed;
		}
		else
		{
			++Analytics.NumTimesStarted;
		}

		// Discard any potential leftovers.
		Lock.Lock();
		NextPendingSampleBlocks.Empty();
		CurrentWriteSampleBlock.Reset();
		CurrentReadSampleBlock.Reset();
		NumActiveFramesTotal = 0;
		NumEnqueuedBlocks = 0;
		bIsFirstSampleBlock = true;
		Lock.Unlock();

		// Add to the list of active instances.
		InstanceLock.Lock();
		ActiveInstances.AddUnique(this);
		StoppedInstances.Remove(this);
		InstanceLock.Unlock();

		// Set state to active.
		bIsResuming = bAskedToBeReleased;
		CurrentState = EState::CreatingPlayer;
		bAskedToBeReleased = false;
		bMaybeReopenAfterSeek = false;
		TimeStopped = -1.0;

		TWeakPtr<FSimpleElectraAudioPlayer, ESPMode::ThreadSafe> This(AsShared());
		TFunction<void()> CreateTask = [This]()
		{
			TSharedPtr<FSimpleElectraAudioPlayer, ESPMode::ThreadSafe> That = This.Pin();
			if (That.IsValid())
			{
				That->CreatePlayerAsync();
			}
		};
		FMediaRunnable::EnqueueAsyncTask(MoveTemp(CreateTask));
	}
}

void FSimpleElectraAudioPlayer::CreatePlayerAsync()
{
	CreatePlayerInstance();
	InternalApplyOptions();
	Player->SetPlayerDataCache(PlayerDataCache.Pin());
	Player->LoadManifest(URL);
	if (bSetLooping)
	{
		IAdaptiveStreamingPlayer::FLoopParam lp;
		lp.bEnableLooping = true;
		Player->SetLooping(lp);
	}
	CurrentState = EState::Active;
}

void FSimpleElectraAudioPlayer::CreateIdleBufferIfNecessary()
{
	Lock.Lock();
	if (!CurrentReadSampleBlock.IsValid() && NextPendingSampleBlocks.IsEmpty())
	{
		TSharedPtr<FBlockSequence, ESPMode::ThreadSafe> NewSeq = MakeShared<FBlockSequence, ESPMode::ThreadSafe>();
		NewSeq->SequenceIndex = -1;
		NextPendingSampleBlocks.Emplace(NewSeq);
	}
	Lock.Unlock();
}


void FSimpleElectraAudioPlayer::MergeAnalytics(const FString& AssetId, const FAnalyticsEntry& InAnalytics)
{
	// If this entry was not active and had no failure downloading the blob we
	// do not merge it.
	if (!InAnalytics.NumTimesStarted && InAnalytics.ErrorCodeAndCount.IsEmpty())
	{
		return;
	}

	FScopeLock lock(&AnalyticsLock);

	// To conserve memory we restrict ourselves to keeping only a small number of stream analytics in memory.
	// This is to avoid growing indefinitely if the analytics are not periodically picked up by user code.
	const int32 MaxAnalytics = FMath::Max((int32)0, ElectraCSAudio_MaxStreamAnalytics);
	while(AnalyticEntries.Num() >= MaxAnalytics)
	{
		// Remove whichever entry comes first when enumerating.
		auto it = AnalyticEntries.CreateConstIterator();
		AnalyticEntries.Remove(it->Key);
	}

	FAnalyticsEntry* Entry = AnalyticEntries.Find(AssetId);
	if (!Entry)
	{
		AnalyticEntries.Emplace(AssetId, InAnalytics).PrepareAnalytics();
	}
	else
	{
		Entry->NumTimesSuspended += InAnalytics.NumTimesSuspended;
		Entry->NumTimesResumed += InAnalytics.NumTimesResumed;
		Entry->BestQualityLevel = InAnalytics.BestQualityLevel;
		Entry->StartLatency = (Entry->StartLatency * Entry->NumTimesStarted + InAnalytics.StartLatency) / (Entry->NumTimesStarted + 1);
		++Entry->NumTimesStarted;
		for(auto& it : InAnalytics.ErrorCodeAndCount)
		{
			Entry->ErrorCodeAndCount.FindOrAdd(it.Key) += it.Value;
		}
		for(auto& it : InAnalytics.NumSegmentsLoadedAtQuality)
		{
			Entry->NumSegmentsLoadedAtQuality.FindOrAdd(it.Key) += it.Value;
		}
		for(auto& it : InAnalytics.CacheHitsAtQuality)
		{
			Entry->CacheHitsAtQuality.FindOrAdd(it.Key) += it.Value;
		}
		Entry->PrepareAnalytics();
	}
}

void FSimpleElectraAudioPlayer::SendAnalyticMetrics(const TSharedPtr<IAnalyticsProviderET>& InAnalyticsProvider)
{
	if (InAnalyticsProvider.IsValid())
	{
		AnalyticsLock.Lock();
		TMap<FString, FAnalyticsEntry> CurrentEntries(MoveTemp(AnalyticEntries));
		AnalyticsLock.Unlock();

		for(auto& Elem : CurrentEntries)
		{
			TArray<FAnalyticsEventAttribute> ParamArray;
			ParamArray.Add(FAnalyticsEventAttribute(TEXT("URL"), Elem.Key));
			ParamArray.Add(FAnalyticsEventAttribute(TEXT("count"), Elem.Value.NumTimesStarted));
			ParamArray.Add(FAnalyticsEventAttribute(TEXT("nsusp"), Elem.Value.NumTimesSuspended));
			ParamArray.Add(FAnalyticsEventAttribute(TEXT("nresm"), Elem.Value.NumTimesResumed));
			ParamArray.Add(FAnalyticsEventAttribute(TEXT("bestq"), Elem.Value.BestQualityLevel));
			ParamArray.Add(FAnalyticsEventAttribute(TEXT("avgltcy"), Elem.Value.StartLatency));
			ParamArray.Add(FAnalyticsEventAttribute(TEXT("errlst"), Elem.Value.Analytics_ErrorCodeAndCount));
			ParamArray.Add(FAnalyticsEventAttribute(TEXT("loaded"), Elem.Value.Analytics_NumSegmentsLoadedAtQuality));
			ParamArray.Add(FAnalyticsEventAttribute(TEXT("chits"), Elem.Value.Analytics_CacheHitsAtQuality));
			InAnalyticsProvider->RecordEvent(TEXT("ElectraCSA.Report"), MoveTemp(ParamArray));
		}
	}
}

void FSimpleElectraAudioPlayer::MaybeRestartAStoppedStream(FSimpleElectraAudioPlayer* This)
{
	double TimeNow = FPlatformTime::Seconds();
	FScopeLock lock(&InstanceLock);
	for(auto& StoppedInst :	StoppedInstances)
	{
		// Don't restart ourselves.
		if (StoppedInst == This)
		{
			continue;
		}
		// Don't revive the living dead!
		if (StoppedInst->bDestructionRequested)
		{
			continue;
		}
		// Only those that are actually stopped and not in error or already restarting.
		if (StoppedInst->CurrentState != EState::Stopped)
		{
			continue;
		}

		// Calculate the remaining play time of that stream.
		FTimespan CurrentTime = StoppedInst->NextPTS < FTimespan::Zero() ? FTimespan::Zero() : StoppedInst->NextPTS;
		FTimespan AssetDuration;
		if (StoppedInst->EncodedDuration > FTimespan::Zero() && StoppedInst->EncodedDuration < FTimespan::MaxValue())
		{
			AssetDuration = StoppedInst->EncodedDuration;
		}
		else if (StoppedInst->Duration > FTimespan::Zero())
		{
			AssetDuration = StoppedInst->Duration;
		}
		else
		{
			continue;
		}
		double TimeRemaining = (AssetDuration - CurrentTime).GetTotalSeconds();
		// If this stream is about to end don't bother restarting it.
		if (TimeRemaining < ElectraCSAudio_MinRemainPlaytimeForReactivation)
		{
			continue;
		}

		// If the stream was stopped too long ago don't restart it either.
		if (TimeNow - StoppedInst->TimeStopped > ElectraCSAudio_MaxStoppedDurationForReactivation)
		{
			continue;
		}

		// Ask to open again.
		StoppedInst->StartPosition = CurrentTime;
		StoppedInst->CurrentState = EState::TryReopeningStream;
		break;
	}
}

void FSimpleElectraAudioPlayer::CreatePlayerInstance()
{
	if (!Player.IsValid())
	{
		// Create a renderer for the new player.
		Renderer = MakeShared<FSimpleElectraAudioPlayerRenderer, ESPMode::ThreadSafe>(SharedThis(this));

		// Create a resource provider for the new player.
		ResourceProvider = MakeShared<FResourceProvider, ESPMode::ThreadSafe>();

		TSharedPtr<IAdaptiveStreamingPlayer, ESPMode::ThreadSafe> NewPlayer;
		FParamDict NoOptions;
		IAdaptiveStreamingPlayer::FCreateParam cp;
		cp.ExternalPlayerGUID = CreateParams.InstanceGUID;
		cp.AudioRenderer = Renderer;
		NewPlayer = IAdaptiveStreamingPlayer::Create(cp);
		NewPlayer->AddMetricsReceiver(this);
		NewPlayer->SetStaticResourceProviderCallback(ResourceProvider);
		NewPlayer->Initialize(NoOptions);
		Player = NewPlayer;
	}
}

void FSimpleElectraAudioPlayer::DoClosePlayerAsync(TSharedPtr<FClosePlayerInstanceHelper, ESPMode::ThreadSafe>&& InPlayerHelper)
{
	TFunction<void()> CloseTask = [InPlayerHelper]()
	{
		if (InPlayerHelper->PlayerInstance.IsValid())
		{
			InPlayerHelper->PlayerInstance->Stop();
			InPlayerHelper->PlayerInstance.Reset();
		}
		if (!InPlayerHelper->bDeleteThis)
		{
			InPlayerHelper->This->CreateIdleBufferIfNecessary();
		}
		InstanceLock.Lock();
		ActiveInstances.Remove(InPlayerHelper->This);
		if (!InPlayerHelper->bDeleteThis)
		{
			StoppedInstances.AddUnique(InPlayerHelper->This);
		}
		InstanceLock.Unlock();
		if (InPlayerHelper->bDeleteThis)
		{
			MergeAnalytics(InPlayerHelper->This->BaseURL, InPlayerHelper->This->Analytics);
			InPlayerHelper->This->bDestructionComplete = true;
		}
		InPlayerHelper->This->bCloseInProgress = false;
	};

	if (GIsRunning)
	{
		FMediaRunnable::EnqueueAsyncTask(MoveTemp(CloseTask));
	}
	else
	{
		CloseTask();
	}
}

void FSimpleElectraAudioPlayer::ClosePlayerInstance(bool bDeleteThis)
{
	CurrentState = CurrentState == EState::Errored ? CurrentState : EState::Stopped;
	bCloseInProgress = true;

	TSharedPtr<FClosePlayerInstanceHelper, ESPMode::ThreadSafe> Helper = MakeShared<FClosePlayerInstanceHelper, ESPMode::ThreadSafe>();
	Helper->PlayerInstance = MoveTemp(Player);
	Helper->This = this;
	Helper->bDeleteThis = bDeleteThis;
	if (Helper->PlayerInstance.IsValid())
	{
		Helper->PlayerInstance->SetStaticResourceProviderCallback(nullptr);
		Helper->PlayerInstance->RemoveMetricsReceiver(this);
	}
	DoClosePlayerAsync(MoveTemp(Helper));
}

bool FSimpleElectraAudioPlayer::Open(const TMap<FString, FVariant>& InOptions, const FString& InManifestURL, const FTimespan& InStartPosition, const FTimespan& InEncodedDuration, bool bInAutoPlay, bool bInSetLooping, TSharedPtr<IElectraPlayerDataCache, ESPMode::ThreadSafe> InPlayerDataCache)
{
	if (TimeCreated < 0.0)
	{
		TimeCreated = FPlatformTime::Seconds();
	}

	URL = InManifestURL;
	Electra::FURL_RFC3986 UrlParser;
	if (UrlParser.Parse(URL))
	{
		BaseURL = UrlParser.Get(false, false);
	}

	EncodedDuration = InEncodedDuration;
	StartPosition = InStartPosition;
	bAutoPlay = bInAutoPlay;
	bSetLooping = bInSetLooping;
	Options.Append(InOptions);
	UpdateAssetName();

	// Check if this is a blob request. These can be performed without having to restrict the number of active players.
	// TBD: We may also elect to cache successful blob requests.
	static FString BlobParams(TEXT("blobparams"));
	const FVariant* BlobVar = nullptr;
	if ((BlobVar = Options.Find(BlobParams)) != nullptr && BlobVar->GetType() == EVariantTypes::String)
	{
		BlobParameters = BlobVar->GetValue<FString>();
		Options.Remove(BlobParams);
	}
	if (BlobParameters.IsSet())
	{
		PendingBlobRequest = MakeShared<FBlobRequest, ESPMode::ThreadSafe>();
		PendingBlobRequest->URL = InManifestURL;
		TSharedPtr<TArray<uint8>, ESPMode::ThreadSafe> BlobData = GetBlobCache().GetData(InManifestURL);
		if (BlobData.IsValid())
		{
			PendingBlobRequest->bWasCached = true;
			PendingBlobRequest->SetBlobData(BlobData);
			CurrentState = EState::OpenBlob;
		}
		else
		{
			FString Msg = FString::Printf(TEXT("%s: Loading blob"), *AssetName);
			UE_LOG(LogSimpleElectraPlayer, Verbose, TEXT("%s"), *Msg);
			CreatePlayerInstance();
			if (!PendingBlobRequest->Request->SetFromJSON(BlobParameters.GetValue()))
			{
				CurrentState = EState::Errored;
				bHasErrored = true;
				ErrorMessage = TEXT("Could not parse blob parameters");
				PendingBlobRequest.Reset();
				return false;
			}
			CurrentState = EState::OpenBlob;
			PendingBlobRequest->Request->URL(InManifestURL).Callback().BindThreadSafeSP(PendingBlobRequest.ToSharedRef(), &FBlobRequest::OnBlobRequestComplete);
			Player->LoadBlob(PendingBlobRequest->Request);
		}
		BlobParameters.Reset();
		return true;
	}

	// Finish the set up and change the state to opening to let the Tick() method take over.
	PendingBlobRequest.Reset();
	PlayerDataCache = InPlayerDataCache;

	CurrentState = EState::OpeningStream;
	return true;
}

void FSimpleElectraAudioPlayer::InternalApplyOptions()
{
	if (ResourceProvider.IsValid())
	{
		ResourceProvider->SetOptions(Options);
		ResourceProvider->SetPlaylistURL(URL);
	}

	FParamDict Opts, NoOpts;
	const FVariant* Mimetype = Options.Find(TEXT("mimetype"));
	if (Mimetype)
	{
		Opts.SetOrUpdate(TEXT("mime_type"), FVariantValue(Mimetype->GetValue<FString>()));
	}

	const FVariant* InitialBitrate = Options.Find(TEXT("initial_bitrate"));
	if (InitialBitrate)
	{
		Opts.SetOrUpdate(TEXT("initial_bitrate"), FVariantValue((int64) InitialBitrate->GetValue<int32>()));
	}

	const FVariant* DoNotTruncate = Options.Find(TEXT("do_not_truncate_at_presentation_end"));
	if (DoNotTruncate)
	{
		Opts.SetOrUpdate(TEXT("do_not_truncate_at_presentation_end"), FVariantValue(DoNotTruncate->GetValue<bool>()));
	}

	TSharedPtr<IAdaptiveStreamingPlayer, ESPMode::ThreadSafe> LockedPlayer(Player);
	if (LockedPlayer.IsValid())
	{
		LockedPlayer->ModifyOptions(Opts, NoOpts);
	}
}

void FSimpleElectraAudioPlayer::UpdateAssetName()
{
	FString AssetNameParam(TEXT("assetname"));
	const FVariant* AssetNameVar = nullptr;
	if ((AssetNameVar = Options.Find(AssetNameParam)) != nullptr && AssetNameVar->GetType() == EVariantTypes::String)
	{
		AssetName = AssetNameVar->GetValue<FString>();
	}
	else
	{
		AssetName = SimpleElectraAudioPlayerUtil::RedactMessage(BaseURL);
	}
}

void FSimpleElectraAudioPlayer::FResourceProvider::ProvideStaticPlaybackDataForURL(TSharedPtr<IAdaptiveStreamingPlayerResourceRequest, ESPMode::ThreadSafe> InOutRequest)
{
	if (InOutRequest->GetResourceType() == Electra::IAdaptiveStreamingPlayerResourceRequest::EPlaybackResourceType::Playlist)
	{
		FString URI = InOutRequest->GetResourceURL();
		if (URI == PlaylistURL)
		{
			const FVariant* PlaylistData = Options.Find(TEXT("playlistdata"));
			if (PlaylistData && PlaylistData->GetType() == EVariantTypes::String)
			{
				FString Playlist = PlaylistData->GetValue<FString>();
				TSharedPtr<TArray<uint8>, ESPMode::ThreadSafe> ResponseDataPtr = MakeShared<TArray<uint8>, ESPMode::ThreadSafe>((const uint8*)TCHAR_TO_UTF8(*Playlist), Playlist.Len());
				InOutRequest->SetPlaybackData(ResponseDataPtr, 0);
			}
		}
	}
	else if (InOutRequest->GetResourceType() == Electra::IAdaptiveStreamingPlayerResourceRequest::EPlaybackResourceType::LicenseKey)
	{
		const FVariant* KeysData = Options.Find(TEXT("keys"));
		if (KeysData && KeysData->GetType() == EVariantTypes::String)
		{
			FString Keys = KeysData->GetValue<FString>();
			TSharedPtr<TArray<uint8>, ESPMode::ThreadSafe> ResponseDataPtr = MakeShared<TArray<uint8>, ESPMode::ThreadSafe>((const uint8*)TCHAR_TO_UTF8(*Keys), Keys.Len());
			InOutRequest->SetPlaybackData(ResponseDataPtr, 0);
		}
	}
	InOutRequest->SignalDataReady();
}

int32 FSimpleElectraAudioPlayer::GetBinaryMetadata(TSharedPtr<TArray<uint8>, ESPMode::ThreadSafe>& OutMetadata) const
{
	FScopeLock lock(&Lock);
	if (PendingBlobRequest.IsValid())
	{
		if (PendingBlobRequest->bIsComplete)
		{
			if (PendingBlobRequest->ErrorCode != 0 && !PendingBlobRequest->bReportedError)
			{
				++Analytics.ErrorCodeAndCount.FindOrAdd((int32) PendingBlobRequest->ErrorCode);

				PendingBlobRequest->bReportedError = true;
				const_cast<FSimpleElectraAudioPlayer*>(this)->ReportError(FString::Printf(TEXT("Blob retrieval failed with error %d"), PendingBlobRequest->ErrorCode));
			}
			if (PendingBlobRequest->ErrorCode == 0 && !PendingBlobRequest->bWasCached)
			{
				GetBlobCache().AddData(PendingBlobRequest->URL, PendingBlobRequest->BlobData);
			}
			OutMetadata = PendingBlobRequest->BlobData;
			return PendingBlobRequest->ErrorCode;
		}
	}
	return -1;
}

bool FSimpleElectraAudioPlayer::GetStreamFormat(FStreamFormat& OutFormat) const
{
	FScopeLock lock(&Lock);
	OutFormat = StreamFormat;
	return StreamFormat.SampleRate && StreamFormat.NumChannels;
}

bool FSimpleElectraAudioPlayer::CanAcceptAudioFrames(int32 InNumFrames)
{
	FScopeLock lock(&Lock);
	if (StreamFormat.SampleRate)
	{
		double DurationInBuffer = (double)NumActiveFramesTotal / StreamFormat.SampleRate;
		return DurationInBuffer < SuggestedBufferDuration || NumActiveFramesTotal < MinNumSamplesInBuffer;
	}
	return NumActiveFramesTotal < MinNumSamplesInBuffer;
}

bool FSimpleElectraAudioPlayer::GetEnqueuedFrameInfo(int32& OutNumberOfEnqueuedFrames, FTimespan& OutDurationOfEnqueuedFrames) const
{
	FScopeLock lock(&Lock);
	OutNumberOfEnqueuedFrames = NumEnqueuedBlocks;
	if (StreamFormat.SampleRate)
	{
		double DurationInBuffer = (double)NumActiveFramesTotal / StreamFormat.SampleRate;
		OutDurationOfEnqueuedFrames = FTimespan::FromSeconds(DurationInBuffer);
	}
	else
	{
		OutDurationOfEnqueuedFrames.Zero();
	}
	return true;
}

void FSimpleElectraAudioPlayer::PrepareToLoopToBeginning()
{
	FScopeLock lock(&Lock);
	bool bIsSuspended = HasBeenSuspended();
	if (bIsSuspended)
	{
		bMaybeReopenAfterSeek = true;
	}

	// If we have a current block to read from then set its EOS state to not reached.
	// This allows to produce silence if the stream is suspended.
	if (CurrentReadSampleBlock.IsValid())
	{
		CurrentReadSampleBlock->bReachedEOS = false;
	}
	if (!bIsSuspended || (bIsSuspended && NextPendingSampleBlocks.Num()))
	{
		CurrentReadSampleBlock.Reset();
	}
}

bool FSimpleElectraAudioPlayer::EnqueueAudioFrames(const void* InBufferAddress, int32 InNumSamples, int32 InNumChannels, int32 InSampleRate, const FTimespan& InPTS, int64 InSequenceIndex, const FTimespan& InDuration)
{
	if (InBufferAddress && InNumSamples && InNumChannels)
	{
		if (TimeUntilReady < 0.0)
		{
			TimeUntilReady = FPlatformTime::Seconds() - TimeCreated;
			Analytics.StartLatency = TimeUntilReady;
			FString Msg = FString::Printf(TEXT("%s: ready after %.2fs"), *AssetName, TimeUntilReady);
			UE_LOG(LogSimpleElectraPlayer, Verbose, TEXT("%s"), *Msg);
		}

		FSampleBlock sb;
		int32 nb = InNumSamples * InNumChannels * sizeof(float);
		sb.Buffer = FMemory::Malloc(nb);
		sb.NumSamples = InNumSamples;
		sb.PTS = InPTS;
		FMemory::Memcpy(sb.Buffer, InBufferAddress, nb);

		FScopeLock lock(&Lock);

		// Get first PTS.
		if (NextPTS < FTimespan::Zero())
		{
			NextPTS = InPTS;
		}

		if (!CurrentWriteSampleBlock.IsValid() || CurrentWriteSampleBlock->SequenceIndex != InSequenceIndex)
		{
			TSharedPtr<FBlockSequence, ESPMode::ThreadSafe> NewSeq = MakeShared<FBlockSequence, ESPMode::ThreadSafe>();
			NewSeq->SequenceIndex = InSequenceIndex;
			NextPendingSampleBlocks.Emplace(NewSeq);
			CurrentWriteSampleBlock = MoveTemp(NewSeq);
		}
		CurrentWriteSampleBlock->SampleBlocks.Emplace(MoveTemp(sb));
		CurrentWriteSampleBlock->NumFramesAvailable += InNumSamples;
		NumActiveFramesTotal += InNumSamples;
		++NumEnqueuedBlocks;

		if (StreamFormat.SampleRate == 0 || StreamFormat.NumChannels == 0)
		{
			StreamFormat.SampleRate = InSampleRate;
			StreamFormat.NumChannels = InNumChannels;
		}
		return true;
	}
	return false;
}

void FSimpleElectraAudioPlayer::SetReceivedLastBuffer()
{
	FScopeLock lock(&Lock);
	check(CurrentWriteSampleBlock.IsValid());
	if (CurrentWriteSampleBlock.IsValid())
	{
		CurrentWriteSampleBlock->bReadEnded = true;
	}
}

void FSimpleElectraAudioPlayer::FlushAudio(bool bForceFlush)
{
	bool bIsSuspended = HasBeenSuspended();
	FScopeLock lock(&Lock);
	// Only flush when not terminated. Otherwise we want to return all the remaining samples we have
	if (!bIsSuspended || bForceFlush)
	{
		NextPendingSampleBlocks.Empty();
		CurrentWriteSampleBlock.Reset();
		CurrentReadSampleBlock.Reset();
		NumActiveFramesTotal = 0;
		NumEnqueuedBlocks = 0;
		NextPTS = FTimespan::MinValue();
	}
}

bool FSimpleElectraAudioPlayer::HasBeenSuspended() const
{
	return CurrentState == EState::Errored || CurrentState == EState::Stopped;
}

int64 FSimpleElectraAudioPlayer::GetNextSamples(FTimespan& OutPTS, bool& bOutIsFirstBlock, int16* OutBuffer, int32 InBufferSizeInFrames, int32 InNumFramesToGet, const FDefaultSampleInfo& InDefaultSampleInfo)
{
	bOutIsFirstBlock = false;
	if (!OutBuffer || InBufferSizeInFrames <= 0 || InNumFramesToGet <= 0)
	{
		return 0;
	}

	FScopeLock lock(&Lock);
	bool bIsSuspended = HasBeenSuspended();
	ActivateBlockReadSequence();
	if (!CurrentReadSampleBlock.IsValid())
	{
		return GetSamples_NotReady;
	}

	if (CurrentReadSampleBlock->SampleBlocks.Num() == 0)
	{
		// If the reading of the stream has not ended we are buffering and waiting for new data.
		if (!bIsSuspended && !CurrentReadSampleBlock->bReadEnded)
		{
			return GetSamples_NotReady;
		}

		// If all requested samples have been delivered already we are done.
		if (CurrentReadSampleBlock->bReachedEOS)
		{
			return GetSamples_AtEOS;
		}
		// Otherwise we need to continue "playing" by returning silence.
		else
		{
			int32 nc = StreamFormat.NumChannels > 0 ? StreamFormat.NumChannels : InDefaultSampleInfo.NumChannels;
			int32 sr = StreamFormat.SampleRate > 0 ? StreamFormat.SampleRate : InDefaultSampleInfo.SampleRate;

			if (InDefaultSampleInfo.ExpectedCurrentFramePos >= 0 && InDefaultSampleInfo.NumTotalFrames >= 0)
			{
				int64 RemainingToGo = InDefaultSampleInfo.NumTotalFrames - InDefaultSampleInfo.ExpectedCurrentFramePos;
				if (InNumFramesToGet > RemainingToGo)
				{
					InNumFramesToGet = RemainingToGo;
					CurrentReadSampleBlock->bReachedEOS = true;
				}
				OutPTS = FTimespan::FromSeconds((double)InDefaultSampleInfo.ExpectedCurrentFramePos / sr);
				NextPTS = FTimespan::FromSeconds((double)(InDefaultSampleInfo.ExpectedCurrentFramePos + InNumFramesToGet) / sr);
			}
			else
			{
				OutPTS = NextPTS;
				// Advance the PTS
				if (NextPTS < FTimespan::Zero())
				{
					NextPTS = StartPosition;
				}
				NextPTS = NextPTS + FTimespan::FromSeconds((double)InNumFramesToGet / sr);
				if (NextPTS >= Duration)
				{
					CurrentReadSampleBlock->bReachedEOS = true;
				}
			}
			// Fill the output with silence.
			FMemory::Memzero(OutBuffer, InNumFramesToGet * sizeof(int16) * nc);
			return InNumFramesToGet; 
		}
	}
	int32 NumFramesToGo = InNumFramesToGet < CurrentReadSampleBlock->NumFramesAvailable ? InNumFramesToGet : CurrentReadSampleBlock->NumFramesAvailable;
	int32 NumGot = 0;
	FTimespan FrontPTS = CurrentReadSampleBlock->SampleBlocks[0].PTS;
	int32 FrontPTSSampleOffset = CurrentReadSampleBlock->SampleBlocks[0].Offset;
	OutPTS = FrontPTS + FTimespan::FromSeconds((double)FrontPTSSampleOffset / StreamFormat.SampleRate);
	while(NumFramesToGo)
	{
		int32 NumFramesLeftInBlock = CurrentReadSampleBlock->SampleBlocks[0].NumSamples - CurrentReadSampleBlock->SampleBlocks[0].Offset;
		int32 NumBlockFrames = NumFramesToGo < NumFramesLeftInBlock ? NumFramesToGo : NumFramesLeftInBlock;
		const float* Src = reinterpret_cast<const float*>(CurrentReadSampleBlock->SampleBlocks[0].Buffer) + CurrentReadSampleBlock->SampleBlocks[0].Offset * StreamFormat.NumChannels;
		for(int32 i=0, iMax=NumBlockFrames*StreamFormat.NumChannels; i<iMax; ++i)
		{
			*OutBuffer++ = FMath::Clamp((int32)(*Src++ * 32768.0f), (int32)-32768, (int32)32767);
		}
		if ((CurrentReadSampleBlock->SampleBlocks[0].Offset += NumBlockFrames) >= CurrentReadSampleBlock->SampleBlocks[0].NumSamples)
		{
			CurrentReadSampleBlock->SampleBlocks[0].Free();
			CurrentReadSampleBlock->SampleBlocks.RemoveAt(0);
			--NumEnqueuedBlocks;
		}
		CurrentReadSampleBlock->NumFramesAvailable -= NumBlockFrames;
		NumActiveFramesTotal -= NumBlockFrames;
		NumFramesToGo -= NumBlockFrames;
		NumGot += NumBlockFrames;
	}
	bOutIsFirstBlock = bIsFirstSampleBlock;
	bIsFirstSampleBlock = false;
	NextPTS = FrontPTS + FTimespan::FromSeconds((double)(FrontPTSSampleOffset + NumGot) / StreamFormat.SampleRate);
	return NumGot;
}

TSharedPtr<TArray<uint8>, ESPMode::ThreadSafe> FSimpleElectraAudioPlayer::FBlobCache::GetData(const FString& InForURL)
{
	double Now = FPlatformTime::Seconds();
	FScopeLock lock(&Lock);

	TSharedPtr<FBlobCacheEntry, ESPMode::ThreadSafe>* Entry = EntryMap.Find(InForURL);
	if (Entry)
	{
		// Still good?
		if (Now - (*Entry)->TimeAdded < ElectraCSAudio_BlobCacheAge)
		{
			return (*Entry)->Data;
		}
		else
		{
			EntryLRU.Remove(*Entry);
			EntryMap.Remove(InForURL);
			SizeInUse -= (*Entry)->Data->Num();
		}
	}
	return nullptr;
}

void FSimpleElectraAudioPlayer::FBlobCache::AddData(const FString& InForURL, const TSharedPtr<TArray<uint8>, ESPMode::ThreadSafe>& InData)
{
	double Now = FPlatformTime::Seconds();
	FScopeLock lock(&Lock);

	TSharedPtr<FBlobCacheEntry, ESPMode::ThreadSafe>* Entry = EntryMap.Find(InForURL);
	if (!Entry)
	{
		CheckLimits(InData->Num());
		SizeInUse += InData->Num();
		TSharedPtr<FBlobCacheEntry, ESPMode::ThreadSafe> NewEntry = MakeShared<FBlobCacheEntry, ESPMode::ThreadSafe>();
		NewEntry->URL = InForURL;
		NewEntry->TimeAdded = Now;
		NewEntry->Data = InData;
		EntryMap.Emplace(InForURL, NewEntry);
		EntryLRU.Emplace(MoveTemp(NewEntry));
	}
	else
	{
		(*Entry)->TimeAdded = Now;
		EntryLRU.Remove(*Entry);
		EntryLRU.Emplace(*Entry);
	}
}

TSharedPtr<ISimpleElectraAudioPlayer::ICacheElementBase, ESPMode::ThreadSafe> FSimpleElectraAudioPlayer::FBlobCache::GetCustomData(const FString& InForURL)
{
	FScopeLock lock(&Lock);
	TSharedPtr<FBlobCacheEntry, ESPMode::ThreadSafe>* Entry = EntryMap.Find(InForURL);
	return Entry ? (*Entry)->CustomData : nullptr;
}

void FSimpleElectraAudioPlayer::FBlobCache::AddCustomData(const FString& InForURL, const TSharedPtr<ISimpleElectraAudioPlayer::ICacheElementBase, ESPMode::ThreadSafe>& InData)
{
	FScopeLock lock(&Lock);
	TSharedPtr<FBlobCacheEntry, ESPMode::ThreadSafe>* Entry = EntryMap.Find(InForURL);
	if (Entry)
	{
		(*Entry)->CustomData = InData;
	}
}


void FSimpleElectraAudioPlayer::FBlobCache::CheckLimits(int32 InBytesNeeded)
{
	while(EntryLRU.Num() && SizeInUse + InBytesNeeded > ElectraCSAudio_BlobCacheSize)
	{
		TSharedPtr<FBlobCacheEntry, ESPMode::ThreadSafe> Oldest = EntryLRU[0];
		EntryLRU.RemoveAt(0);
		EntryMap.Remove(Oldest->URL);
		SizeInUse -= Oldest->Data->Num();
	}
}





UEMediaError FSimpleElectraAudioPlayerRenderer::CreateBufferPool(const FParamDict& Parameters)
{
	DecoderOutputPool.Reset();
	NumOutputAudioBuffersInUse = 0;
	NumBuffers = (int32)Parameters.GetValue("num_buffers").SafeGetInt64(-1);
	if (NumBuffers <= 0)
	{
		return UEMEDIA_ERROR_BAD_ARGUMENTS;
	}
	BufferPoolProperties.Set("max_buffers", Electra::FVariantValue((int64)NumBuffers));
	MaxBufferSize = (uint32)Parameters.GetValue("max_buffer_size").SafeGetInt64(0);
	return MaxBufferSize ? UEMEDIA_ERROR_OK : UEMEDIA_ERROR_BAD_ARGUMENTS;
}

UEMediaError FSimpleElectraAudioPlayerRenderer::AcquireBuffer(IBuffer*& OutBuffer, int32 TimeoutInMicroseconds, const FParamDict& InParameters)
{
	if (TimeoutInMicroseconds != 0)
	{
		return UEMEDIA_ERROR_BAD_ARGUMENTS;
	}
	if (NumOutputAudioBuffersInUse >= NumBuffers)
	{
		return UEMEDIA_ERROR_INSUFFICIENT_DATA;
	}
	IAudioDecoderOutputPtr DecoderOutput = DecoderOutputPool.AcquireShared();
	DecoderOutput->SetOwner(SharedThis(this));
	FPlatformAtomics::InterlockedIncrement(&NumBuffersAcquiredForDecoder);
	DecoderOutput->Reserve(MaxBufferSize);
	FPlatformAtomics::InterlockedIncrement(&NumOutputAudioBuffersInUse);

	FMediaBufferSharedPtrWrapper* MediaBufferSharedPtrWrapper = new FMediaBufferSharedPtrWrapper(DecoderOutput);
	MediaBufferSharedPtrWrapper->BufferProperties.Set("size", FVariantValue((int64)DecoderOutput->GetReservedBufferBytes()));
	MediaBufferSharedPtrWrapper->BufferProperties.Set("address", FVariantValue(const_cast<void*>(DecoderOutput->GetBuffer())));
	OutBuffer = MediaBufferSharedPtrWrapper;
	return UEMEDIA_ERROR_OK;
}

UEMediaError FSimpleElectraAudioPlayerRenderer::ReturnBuffer(IBuffer* Buffer, bool bRender, const FParamDict& InSampleProperties)
{
	if (Buffer == nullptr)
	{
		return UEMEDIA_ERROR_BAD_ARGUMENTS;
	}
	FMediaBufferSharedPtrWrapper* MediaBufferSharedPtrWrapper = static_cast<FMediaBufferSharedPtrWrapper*>(Buffer);
	if (bRender)
	{
		int32 NumChannels = (int32)InSampleProperties.GetValue("num_channels").SafeGetInt64(-1);
		int32 SampleRate = (int32)InSampleProperties.GetValue("sample_rate").SafeGetInt64(-1);
		int32 UsedBufferBytes = (int32)InSampleProperties.GetValue("byte_size").SafeGetInt64(-1);
		FTimeValue decPTS = InSampleProperties.GetValue("pts").SafeGetTimeValue();
		FTimespan PTS = decPTS.GetAsTimespan();
		int64 InSequenceIndex = decPTS.GetSequenceIndex();

		FTimespan Duration = InSampleProperties.GetValue("duration").SafeGetTimeValue().GetAsTimespan();
		const void* BufferAddr = MediaBufferSharedPtrWrapper->BufferProperties.GetValue("address").SafeGetPointer();
		if (NumChannels <= 0 || SampleRate <= 0 || UsedBufferBytes <= 0)
		{
			return UEMEDIA_ERROR_BAD_ARGUMENTS;
		}

		IAudioDecoderOutputPtr DecoderOutput = MediaBufferSharedPtrWrapper->DecoderOutput;
		DecoderOutput->GetMutablePropertyDictionary() = InSampleProperties;
		DecoderOutput->Initialize(IAudioDecoderOutput::ESampleFormat::Float, NumChannels, SampleRate, Duration, FDecoderTimeStamp(PTS, InSequenceIndex), UsedBufferBytes);

		TSharedPtr<FSimpleElectraAudioPlayer, ESPMode::ThreadSafe> PinnedPlayer = Player.Pin();
		if (PinnedPlayer.IsValid())
		{
			PinnedPlayer->EnqueueAudioFrames(BufferAddr, UsedBufferBytes / (sizeof(float)*NumChannels), NumChannels, SampleRate, PTS, InSequenceIndex, Duration);
		}
	}
	else
	{
		if (InSampleProperties.GetValue("eos").SafeGetBool(false))
		{
			TSharedPtr<FSimpleElectraAudioPlayer, ESPMode::ThreadSafe> PinnedPlayer = Player.Pin();
			if (PinnedPlayer.IsValid())
			{
				PinnedPlayer->SetReceivedLastBuffer();
			}
		}
	}
	FPlatformAtomics::InterlockedDecrement(&NumBuffersAcquiredForDecoder);
	delete MediaBufferSharedPtrWrapper;
	return UEMEDIA_ERROR_OK;
}

UEMediaError FSimpleElectraAudioPlayerRenderer::ReleaseBufferPool()
{
	DecoderOutputPool.Reset();
	return UEMEDIA_ERROR_OK;
}

bool FSimpleElectraAudioPlayerRenderer::CanReceiveOutputFrames(uint64 NumFrames) const
{
	TSharedPtr<FSimpleElectraAudioPlayer, ESPMode::ThreadSafe> PinnedPlayer = Player.Pin();
	if (!PinnedPlayer.IsValid())
	{
		return false;
	}
	return PinnedPlayer->CanAcceptAudioFrames(NumFrames);
}

bool FSimpleElectraAudioPlayerRenderer::GetEnqueuedFrameInfo(int32& OutNumberOfEnqueuedFrames, FTimeValue& OutDurationOfEnqueuedFrames) const
{
	TSharedPtr<FSimpleElectraAudioPlayer, ESPMode::ThreadSafe> PinnedPlayer = Player.Pin();
	if (!PinnedPlayer.IsValid())
	{
		OutNumberOfEnqueuedFrames = 0;
		OutDurationOfEnqueuedFrames.SetToZero();
		return false;
	}
	FTimespan DurAvail(0);
	bool bGot = PinnedPlayer->GetEnqueuedFrameInfo(OutNumberOfEnqueuedFrames, DurAvail);
	if (bGot)
	{
		OutDurationOfEnqueuedFrames.SetFromTimespan(DurAvail);
	}
	return bGot;
}

void FSimpleElectraAudioPlayerRenderer::SetParentRenderer(TWeakPtr<IMediaRenderer, ESPMode::ThreadSafe> InParentRenderer)
{
	ParentRenderer = MoveTemp(InParentRenderer);
}

UEMediaError FSimpleElectraAudioPlayerRenderer::Flush(const FParamDict& InOptions)
{
	if (NumBuffersAcquiredForDecoder != 0)
	{
		return UEMEDIA_ERROR_INTERNAL;
	}
	if (TSharedPtr<FSimpleElectraAudioPlayer, ESPMode::ThreadSafe> PinnedPlayer = Player.Pin())
	{
		PinnedPlayer->FlushAudio(false);
	}
	return UEMEDIA_ERROR_OK;
}

void FSimpleElectraAudioPlayerRenderer::SampleReleasedToPool(IDecoderOutput* InDecoderOutput)
{
	FPlatformAtomics::InterlockedDecrement(&NumOutputAudioBuffersInUse);
	check(NumOutputAudioBuffersInUse >= 0 && NumOutputAudioBuffersInUse <= NumBuffers)

	TSharedPtr<IMediaRenderer, ESPMode::ThreadSafe> Parent = ParentRenderer.Pin();
	if (Parent.IsValid())
	{
		Parent->SampleReleasedToPool(InDecoderOutput);
	}
}


#if ENABLE_DEBUG_STATS

static TAutoConsoleVariable<bool> CVarSimpleElectraAudioPlayer_ShowOverlay(
	TEXT("ElectraCSAudio.ShowOverlay"),
	false,
	TEXT("If true, displays a debug overlay of active instances."),
	FConsoleVariableDelegate::CreateLambda([](IConsoleVariable* CVar)
		{ FSimpleElectraAudioPlayer::EnableDebugDraw(CVar->GetBool()); }),
	ECVF_Default);


FDelegateHandle FSimpleElectraAudioPlayer::DebugDrawDelegateHandle;
FVector2D FSimpleElectraAudioPlayer::DebugDrawTextPos;
UFont* FSimpleElectraAudioPlayer::DebugDrawFont = nullptr;
int32 FSimpleElectraAudioPlayer::DebugDrawFontRowHeight = 0;
const FLinearColor FSimpleElectraAudioPlayer::DebugColor_Orange { 1.0f, 0.5f, 0.0f };
const FLinearColor FSimpleElectraAudioPlayer::DebugColor_Green { 0.0f, 1.0f, 0.0f };
const FLinearColor FSimpleElectraAudioPlayer::DebugColor_Red { 1.0f, 0.0f, 0.0f };

void FSimpleElectraAudioPlayer::EnableDebugDraw(bool bEnable)
{
	if (bEnable && !DebugDrawDelegateHandle.IsValid())
	{
		DebugDrawDelegateHandle = UDebugDrawService::Register(TEXT("Game"), FDebugDrawDelegate::CreateStatic(&FSimpleElectraAudioPlayer::DebugDraw));
	}
	else if (!bEnable && DebugDrawDelegateHandle.IsValid())
	{
		UDebugDrawService::Unregister(DebugDrawDelegateHandle);
		DebugDrawDelegateHandle.Reset();
	}
}

void FSimpleElectraAudioPlayer::DebugDraw(UCanvas* InCanvas, APlayerController* /*InPlayerController*/)
{
	if (CVarSimpleElectraAudioPlayer_ShowOverlay.GetValueOnAnyThread() && InCanvas)
	{
		if(!DebugDrawFont)
		{
			// Pick a larger font on console.
			//DebugDrawFont = FPlatformProperties::SupportsWindowedMode() ? GEngine->GetSmallFont() : GEngine->GetMediumFont();
			DebugDrawFont = GEngine->GetMediumFont();
			DebugDrawFontRowHeight = FMath::TruncToInt(DebugDrawFont->GetMaxCharHeight() * 1.075f);
		}

		// Reset drawing pos...
		DebugDrawTextPos.X = 20.0f;
		DebugDrawTextPos.Y = 20.0f;


		InstanceLock.Lock();
		TArray<FSimpleElectraAudioPlayer*> active;
		TArray<FSimpleElectraAudioPlayer*> ready;
		TArray<FSimpleElectraAudioPlayer*> stopped;

		for(int32 i=0; i<AllInstances.Num(); ++i)
		{
			if (ActiveInstances.Contains(AllInstances[i]))
			{
				active.Emplace(AllInstances[i]);
			}
			else if (StoppedInstances.Contains(AllInstances[i]))
			{
				stopped.Emplace(AllInstances[i]);
			}
			else
			{
				ready.Emplace(AllInstances[i]);
			}
		}

		for(auto& act : active)
		{
			act->DebugDrawInst(InCanvas);
		}
		for(auto& rdy : ready)
		{
			rdy->DebugDrawInst(InCanvas);
		}
		for(auto& stp : stopped)
		{
			stp->DebugDrawInst(InCanvas);
		}
		InstanceLock.Unlock();
	}
}

void FSimpleElectraAudioPlayer::DebugDrawPrintLine(UCanvas* InCanvas, const FString& InString, const FLinearColor& InColor)
{
	if (InCanvas)
	{
		FCanvasTextItem textItem(DebugDrawTextPos, FText::FromString(InString), DebugDrawFont, InColor);
		const FLinearColor ShadowColor = FLinearColor::Black;
		textItem.EnableShadow(ShadowColor);
		InCanvas->DrawItem(textItem);
		DebugDrawTextPos.Y += DebugDrawFontRowHeight;
	}
}

void FSimpleElectraAudioPlayer::DebugDrawInst(UCanvas* InCanvas)
{
	FString Msg(AssetName);
	Msg += TEXT(": ");
	TSharedPtr<IAdaptiveStreamingPlayer, ESPMode::ThreadSafe> LockedPlayer(Player);

	FLinearColor Color;
	if (CurrentState == EState::Stopped) Color = DebugColor_Red;
	else if (CurrentState == EState::Active) Color = DebugColor_Green;
	else Color = DebugColor_Orange;

	if (LockedPlayer.IsValid())
	{
		auto GetOptionVal = [](const FString& InOptName, const TMap<FString, FVariant>& InOptions) -> int32
		{
			const FVariant* Var = nullptr;
			if ((Var = InOptions.Find(InOptName)) != nullptr && Var->GetType() == EVariantTypes::Int32)
			{
				return Var->GetValue<int32>();
			}
			return 0;
		};

		// Time until ready
		double ttr = TimeUntilReady >= 0.0 ? TimeUntilReady : FPlatformTime::Seconds() - TimeCreated;
		Msg += FString::Printf(TEXT("(%.2f) "), ttr);

		int32 Retry = GetOptionVal(TEXT("retrynum"), Options);
		int32 RetryMax = GetOptionVal(TEXT("retrymax"), Options);
		if (Retry)
		{
			Msg += FString::Printf(TEXT(" retry %d of %d "), Retry, RetryMax);
		}

		Electra::FErrorDetail Err = LockedPlayer->GetError();
		if (Err.IsOK())
		{
			if (PendingBlobRequest.IsValid())
			{
				Msg += TEXT("(Retrieving blob)");
			}
			else if (!LockedPlayer->HaveMetadata())
			{
				Msg += TEXT("(Opening)");
			}
			else
			{
				if (LockedPlayer->IsSeeking())
				{
					Msg += TEXT("(Seeking)");
				}
				else if (LockedPlayer->IsBuffering())
				{
					Msg += TEXT("(Buffering)");
				}
				double pts = NextPTS.GetTotalSeconds();
				Msg += FString::Printf(TEXT(" %.2f/%.2f"), pts>=0.0?pts:0.0, Duration.GetTotalSeconds());
			}
		}
		else
		{
			Msg += Err.GetMessage();
		}
	}
	else
	{
		double pts = NextPTS.GetTotalSeconds();
		Msg += FString::Printf(TEXT(" %.2f/%.2f"), pts>=0.0?pts:0.0, Duration.GetTotalSeconds());
	}
	DebugDrawPrintLine(InCanvas, Msg, Color);
}

#endif
