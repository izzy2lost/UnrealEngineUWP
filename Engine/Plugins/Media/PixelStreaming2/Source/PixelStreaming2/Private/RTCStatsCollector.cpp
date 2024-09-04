// Copyright Epic Games, Inc. All Rights Reserved.

#include "RTCStatsCollector.h"
#include "Logging.h"
#include "PixelStreaming2StatNames.h"
#include "ToStringExtensions.h"

// TODO (Migration) RTCP-6453 Retrieve stats from EpicRtc when EpicRtc supports it
#if 0

namespace UE::PixelStreaming2
{

	// ------------- FRTCStatsCollector-------------------

	FRTCStatsCollector::FRTCStatsCollector()
		: FRTCStatsCollector(INVALID_PLAYER_ID)
	{
	}

	FRTCStatsCollector::FRTCStatsCollector(FPixelStreaming2PlayerId PlayerId)
		: AssociatedPlayerId(PlayerId)
		, LastCalculationCycles(FPlatformTime::Cycles64())
		, bIsEnabled(!Settings::CVarPixelStreaming2WebRTCDisableStats.GetValueOnAnyThread())
	{

		// Add each of the sinks for each of the stat types we are interested in tracking
		RTCStatSinks.Add(MakeShared<FTrackStatsSink>());
		RTCStatSinks.Add(MakeShared<FRTPMediaStatsSink>(RTCStatTypes::InboundRTP, "video"));
		RTCStatSinks.Add(MakeShared<FRTPMediaStatsSink>(RTCStatTypes::OutboundRTP, "video"));
		RTCStatSinks.Add(MakeShared<FRTPMediaStatsSink>(RTCStatTypes::InboundRTP, "audio"));
		RTCStatSinks.Add(MakeShared<FRTPMediaStatsSink>(RTCStatTypes::OutboundRTP, "audio"));
		RTCStatSinks.Add(MakeShared<FVideoSourceStatsSink>());
		RTCStatSinks.Add(MakeShared<FDataChannelStatsSink>());
	}

	void FRTCStatsCollector::AddRef() const
	{
		FPlatformAtomics::InterlockedIncrement(&RefCount);
	}

	rtc::RefCountReleaseStatus FRTCStatsCollector::Release() const
	{
		if (FPlatformAtomics::InterlockedDecrement(&RefCount) == 0)
		{
			return rtc::RefCountReleaseStatus::kDroppedLastRef;
		}

		return rtc::RefCountReleaseStatus::kOtherRefsRemained;
	}

	void FRTCStatsCollector::OnStatsDelivered(const rtc::scoped_refptr<const webrtc::RTCStatsReport>& Report)
	{
		FStats* PSStats = FStats::Get();

		if (!bIsEnabled || !PSStats || !Report)
		{
			return;
		}

		uint64 CyclesNow = FPlatformTime::Cycles64();
		double SecondsDelta = FGenericPlatformTime::ToSeconds64(CyclesNow - LastCalculationCycles);

		for (const webrtc::RTCStats& Stats : *Report)
		{
			for (TSharedPtr<FStatsSink> Sink : RTCStatSinks)
			{
				if (Sink && Sink->Wants(Stats))
				{
					FString PeerId = Sink->DerivePeerId(Stats, AssociatedPlayerId);
					Sink->Process(Stats, PeerId);
					Sink->PostProcess(PeerId, SecondsDelta);
				}
			}

			// For debugging to see all stats in the log
			// UE_LOG(LogPixelStreaming2, Log, TEXT("------------%s---%s------------"), *FString(Stats.id().c_str()), *FString(Stats.type()));
			// for (const webrtc::RTCStatsMemberInterface* StatMember : StatMembers)
			// {
			// 	UE_LOG(LogPixelStreaming2, Log, TEXT("%s"), *FString(StatMember->name()));
			// }
		}

		LastCalculationCycles = FPlatformTime::Cycles64();
	}

	/*
	* ---------------- FStatsSink --------------------
	*/

	void FRTCStatsCollector::FStatsSink::Process(const webrtc::RTCStats& InStats, FString PeerId)
	{
		FStats* PSStats = FStats::Get();

		if (!PSStats)
		{
			return;
		}

		std::vector<const webrtc::RTCStatsMemberInterface*> StatMembers = InStats.Members();

		for (const webrtc::RTCStatsMemberInterface* StatMember : StatMembers)
		{
			const FName StatName = FName(StatMember->name());

			FRTCTrackedStat* StatToEmit = Get(StatName);
			if (!StatToEmit)
			{
				continue;
			}

			if (ExtractValueAndSet(StatMember, StatToEmit))
			{
				PSStats->StorePeerStat(PeerId, SinkType, StatToEmit->GetLatestStat());
			}
		}
	}

	void FRTCStatsCollector::FStatsSink::PostProcess(FString PeerId, double SecondsDelta)
	{
		FStats* PSStats = FStats::Get();

		if (!PSStats)
		{
			return;
		}

		// Run all the stat calculators
		for (auto& Calculator : Calculators)
		{
			TOptional<FStatData> OptStatData = Calculator(*this, SecondsDelta);
			if (OptStatData.IsSet())
			{
				FStatData& StatData = *OptStatData;
				CalculatedStats.Add(StatData.StatName, StatData);
				PSStats->StorePeerStat(PeerId, SinkType, StatData);
			}
		}
	}

	bool FRTCStatsCollector::FStatsSink::ExtractValueAndSet(const webrtc::RTCStatsMemberInterface* ExtractFrom, FRTCTrackedStat* SetValueHere)
	{
		const bool bIsDefined = ExtractFrom->is_defined();
		if (!bIsDefined)
		{
			return false;
		}

		const bool bZeroInitially = SetValueHere->GetLatestStat().StatValue == 0.0;
		FString StatValueStr = ToString(ExtractFrom->ValueToString());
		double StatValueDouble = FCString::Atod(*StatValueStr);
		SetValueHere->SetLatestValue(StatValueDouble);
		const bool bZeroStill = SetValueHere->GetLatestStat().StatValue == 0.0;
		return !(bZeroInitially && bZeroStill);
	}

	void FRTCStatsCollector::FStatsSink::AddAliased(FName StatName, FName AliasedName, int NDecimalPlaces, uint8 DisplayFlags)
	{
		FRTCTrackedStat Stat = FRTCTrackedStat(StatName, AliasedName, NDecimalPlaces, DisplayFlags);
		Stats.Add(StatName, Stat);
	}

	/*
	* ---------------- FRTPMediaStatsSink --------------------
	*/

	FRTCStatsCollector::FRTPMediaStatsSink::FRTPMediaStatsSink(FName InSinkType, FString InMediaKind)
		: FStatsSink(InSinkType)
		, MediaKind(InMediaKind)
	{
		// Todo: These stats could be split up across more specific classes once we want start tracking audio stats.

		// These stats will be extracted from the stat reports and emitted straight to screen
		Add(PixelStreaming2StatNames::FirCount, 0);
		Add(PixelStreaming2StatNames::PliCount, 0);
		Add(PixelStreaming2StatNames::NackCount, 0);
		Add(PixelStreaming2StatNames::SliCount, 0);
		Add(PixelStreaming2StatNames::RetransmittedBytesSent, 0);
		Add(PixelStreaming2StatNames::TotalEncodeBytesTarget, 0);
		Add(PixelStreaming2StatNames::KeyFramesEncoded, 0);
		Add(PixelStreaming2StatNames::FrameWidth, 0);
		Add(PixelStreaming2StatNames::FrameHeight, 0);
		Add(PixelStreaming2StatNames::HugeFramesSent, 0);
		Add(PixelStreaming2StatNames::AvgSendDelay, 0);

		// These are values used to calculate extra values (stores time deltas etc)
		AddNonRendered(PixelStreaming2StatNames::TargetBitrate);
		AddNonRendered(PixelStreaming2StatNames::FramesSent);
		AddNonRendered(PixelStreaming2StatNames::FramesReceived);
		AddNonRendered(PixelStreaming2StatNames::BytesSent);
		AddNonRendered(PixelStreaming2StatNames::BytesReceived);
		AddNonRendered(PixelStreaming2StatNames::QPSum);
		AddNonRendered(PixelStreaming2StatNames::TotalEncodeTime);
		AddNonRendered(PixelStreaming2StatNames::FramesEncoded);
		AddNonRendered(PixelStreaming2StatNames::FramesDecoded);

		// Calculated stats below:

		// FrameSent Per Second
		AddStatCalculator([](FStatsSink& StatSource, double Period) -> TOptional<FStatData> {
			FRTCTrackedStat* FramesSentStat = StatSource.Get(PixelStreaming2StatNames::FramesSent);
			if (FramesSentStat && FramesSentStat->GetLatestStat().StatValue > 0)
			{
				const double FramesSentPerSecond = FramesSentStat->CalculateDelta(Period);
				FStatData FpsStat = FStatData(PixelStreaming2StatNames::FramesSentPerSecond, FramesSentPerSecond, 0);
				FpsStat.DisplayFlags = FStatData::EDisplayFlags::TEXT | FStatData::EDisplayFlags::GRAPH;
				return FpsStat;
			}
			return {};
			});

		// FramesReceived Per Second
		AddStatCalculator([](FStatsSink& StatSource, double Period) -> TOptional<FStatData> {
			FRTCTrackedStat* FramesReceivedStat = StatSource.Get(PixelStreaming2StatNames::FramesReceived);
			if (FramesReceivedStat && FramesReceivedStat->GetLatestStat().StatValue > 0)
			{
				const double FramesReceivedPerSecond = FramesReceivedStat->CalculateDelta(Period);
				return FStatData(PixelStreaming2StatNames::FramesReceivedPerSecond, FramesReceivedPerSecond, 0);
			}
			return {};
			});

		// Megabits sent Per Second
		AddStatCalculator([](FStatsSink& StatSource, double Period) -> TOptional<FStatData> {
			FRTCTrackedStat* BytesSentStat = StatSource.Get(PixelStreaming2StatNames::BytesSent);
			if (BytesSentStat && BytesSentStat->GetLatestStat().StatValue > 0)
			{
				const double BytesSentPerSecond = BytesSentStat->CalculateDelta(Period);
				const double MegabitsPerSecond = BytesSentPerSecond / 1'000'000.0 * 8.0;
				return FStatData(PixelStreaming2StatNames::BitrateMegabits, MegabitsPerSecond, 2);
			}
			return {};
			});

		// Bits sent Per Second
		AddStatCalculator([](FStatsSink& StatSource, double Period) -> TOptional<FStatData> {
			FRTCTrackedStat* BytesSentStat = StatSource.Get(PixelStreaming2StatNames::BytesSent);
			if (BytesSentStat && BytesSentStat->GetLatestStat().StatValue > 0)
			{
				const double BytesSentPerSecond = BytesSentStat->CalculateDelta(Period);
				const double BitsPerSecond = BytesSentPerSecond * 8.0;
				FStatData Stat = FStatData(PixelStreaming2StatNames::Bitrate, BitsPerSecond, 0);
				Stat.DisplayFlags = FStatData::EDisplayFlags::HIDDEN; // We don't want to display bits per second (too many digits)
				return Stat;
			}
			return {};
			});

		// Target megabits sent Per Second
		AddStatCalculator([](FStatsSink& StatSource, double Period) -> TOptional<FStatData> {
			FRTCTrackedStat* TargetBpsStats = StatSource.Get(PixelStreaming2StatNames::TargetBitrate);
			if (TargetBpsStats && TargetBpsStats->GetLatestStat().StatValue > 0)
			{
				const double TargetBps = TargetBpsStats->Average();
				const double MegabitsPerSecond = TargetBps / 1'000'000.0;
				return FStatData(PixelStreaming2StatNames::TargetBitrateMegabits, MegabitsPerSecond, 2);
			}
			return {};
			});

		// Megabits received Per Second
		AddStatCalculator([](FStatsSink& StatSource, double Period) -> TOptional<FStatData> {
			FRTCTrackedStat* BytesReceivedStat = StatSource.Get(PixelStreaming2StatNames::BytesReceived);
			if (BytesReceivedStat && BytesReceivedStat->GetLatestStat().StatValue > 0)
			{
				const double BytesReceivedPerSecond = BytesReceivedStat->CalculateDelta(Period);
				const double MegabitsPerSecond = BytesReceivedPerSecond / 1000.0 * 8.0;
				return FStatData(PixelStreaming2StatNames::Bitrate, MegabitsPerSecond, 2);
			}
			return {};
			});

		// Encoded fps
		AddStatCalculator([](FStatsSink& StatSource, double Period) -> TOptional<FStatData> {
			FRTCTrackedStat* EncodedFramesStat = StatSource.Get(PixelStreaming2StatNames::FramesEncoded);
			if (EncodedFramesStat && EncodedFramesStat->GetLatestStat().StatValue > 0)
			{
				const double EncodedFramesPerSecond = EncodedFramesStat->CalculateDelta(Period);
				return FStatData(PixelStreaming2StatNames::EncodedFramesPerSecond, EncodedFramesPerSecond, 0);
			}
			return {};
			});

		// Decoded fps
		AddStatCalculator([](FStatsSink& StatSource, double Period) -> TOptional<FStatData> {
			FRTCTrackedStat* DecodedFramesStat = StatSource.Get(PixelStreaming2StatNames::FramesDecoded);
			if (DecodedFramesStat && DecodedFramesStat->GetLatestStat().StatValue > 0)
			{
				const double DecodedFramesPerSecond = DecodedFramesStat->CalculateDelta(Period);
				return FStatData(PixelStreaming2StatNames::DecodedFramesPerSecond, DecodedFramesPerSecond, 0);
			}
			return {};
			});

		// Avg QP Per Second
		AddStatCalculator([](FStatsSink& StatSource, double Period) -> TOptional<FStatData> {
			FRTCTrackedStat* QPSumStat = StatSource.Get(PixelStreaming2StatNames::QPSum);
			FStatData* EncodedFramesPerSecond = StatSource.GetCalculatedStat(PixelStreaming2StatNames::EncodedFramesPerSecond);
			if (QPSumStat && QPSumStat->GetLatestStat().StatValue > 0
				&& EncodedFramesPerSecond && EncodedFramesPerSecond->StatValue > 0.0)
			{
				const double QPSumDeltaPerSecond = QPSumStat->CalculateDelta(Period);
				const double MeanQPPerFrame = QPSumDeltaPerSecond / EncodedFramesPerSecond->StatValue;
				FName StatName = PixelStreaming2StatNames::MeanQPPerSecond;
				return FStatData(StatName, MeanQPPerFrame, 0);
			}
			return {};
			});

		// Mean EncodeTime (ms) Per Frame
		AddStatCalculator([](FStatsSink& StatSource, double Period) -> TOptional<FStatData> {
			FRTCTrackedStat* TotalEncodeTimeStat = StatSource.Get(PixelStreaming2StatNames::TotalEncodeTime);
			FStatData* EncodedFramesPerSecond = StatSource.GetCalculatedStat(PixelStreaming2StatNames::EncodedFramesPerSecond);
			if (TotalEncodeTimeStat && TotalEncodeTimeStat->GetLatestStat().StatValue > 0
				&& EncodedFramesPerSecond && EncodedFramesPerSecond->StatValue > 0.0)
			{
				const double TotalEncodeTimePerSecond = TotalEncodeTimeStat->CalculateDelta(Period);
				const double MeanEncodeTimePerFrameMs = TotalEncodeTimePerSecond / EncodedFramesPerSecond->StatValue * 1000.0;
				return FStatData(PixelStreaming2StatNames::MeanEncodeTime, MeanEncodeTimePerFrameMs, 2);
			}
			return {};
			});

		// Mean SendDelay (ms) Per Frame
		AddStatCalculator([](FStatsSink& StatSource, double Period) -> TOptional<FStatData> {
			FRTCTrackedStat* TotalSendDelayStat = StatSource.Get(PixelStreaming2StatNames::TotalPacketSendDelay);
			FStatData* FramesSentPerSecond = StatSource.GetCalculatedStat(PixelStreaming2StatNames::FramesSentPerSecond);
			if (TotalSendDelayStat && TotalSendDelayStat->GetLatestStat().StatValue > 0
				&& FramesSentPerSecond && FramesSentPerSecond->StatValue > 0.0)
			{
				const double TotalSendDelayPerSecond = TotalSendDelayStat->CalculateDelta(Period);
				const double MeanSendDelayPerFrameMs = TotalSendDelayPerSecond / FramesSentPerSecond->StatValue * 1000.0;
				return FStatData(PixelStreaming2StatNames::MeanSendDelay, MeanSendDelayPerFrameMs, 2);
			}
			return {};
			});

		// JitterBufferDelay (ms)
		AddStatCalculator([](FStatsSink& StatSource, double Period) -> TOptional<FStatData> {
			FRTCTrackedStat* JitterBufferDelayStat = StatSource.Get(PixelStreaming2StatNames::JitterBufferDelay);
			FStatData* FramesReceivedPerSecond = StatSource.GetCalculatedStat(PixelStreaming2StatNames::FramesReceivedPerSecond);
			if (JitterBufferDelayStat && JitterBufferDelayStat->GetLatestStat().StatValue > 0
				&& FramesReceivedPerSecond && FramesReceivedPerSecond->StatValue > 0.0)
			{
				const double TotalJitterBufferDelayPerSecond = JitterBufferDelayStat->CalculateDelta(Period);
				const double MeanJitterBufferDelayMs = TotalJitterBufferDelayPerSecond / FramesReceivedPerSecond->StatValue * 1000.0;
				return FStatData(PixelStreaming2StatNames::JitterBufferDelay, MeanJitterBufferDelayMs, 2);
			}
			return {};
			});
	}

	bool FRTCStatsCollector::FRTPMediaStatsSink::Wants(const webrtc::RTCStats& InStats) const
	{
		bool bWants = FStatsSink::Wants(InStats);

		if (!bWants)
		{
			return false;
		}
		// Check the `kind` field to see if it matches what we want
		std::vector<const webrtc::RTCStatsMemberInterface*> StatMembers = InStats.Members();
		for (const webrtc::RTCStatsMemberInterface* StatMember : StatMembers)
		{
			const FString StatName = FString(StatMember->name());
			if (StatName == "kind")
			{
				FString StatMediaKind = StatMember->is_defined() ? ToString(StatMember->ValueToString()) : TEXT("");
				return MediaKind == StatMediaKind;
			}
		}
		return false;
	}

	FString FRTCStatsCollector::FRTPMediaStatsSink::DerivePeerId(const webrtc::RTCStats& InStats, FPixelStreaming2PlayerId PlayerId) const
	{
		std::vector<const webrtc::RTCStatsMemberInterface*> StatMembers = InStats.Members();
		const FString StatsType = FString(InStats.type());

		if (PlayerId == SFU_PLAYER_ID && (StatsType == RTCStatTypes::OutboundRTP || StatsType == RTCStatTypes::InboundRTP))
		{
			// Extract the `ssrc` to uniquely id the stream
			for (const webrtc::RTCStatsMemberInterface* StatMember : StatMembers)
			{
				const FString StatName = FString(StatMember->name());
				if (StatName == "ssrc")
				{
					FString Ssrc = StatMember->is_defined() ? ToString(StatMember->ValueToString()) : TEXT("");
					return Ssrc;
				}
			}
		}

		return PlayerId;
	}

	/*
	* ---------------- FTrackStatsSink ----------------
	*/
	FRTCStatsCollector::FTrackStatsSink::FTrackStatsSink() : FStatsSink(RTCStatTypes::Track)
	{
		// Basic stats we wish to store and emit
		Add(PixelStreaming2StatNames::JitterBufferDelay, 2);
		Add(PixelStreaming2StatNames::FramesPerSecond, 0);
		Add(PixelStreaming2StatNames::FramesDecoded, 0);
		Add(PixelStreaming2StatNames::FramesDropped, 0);
		Add(PixelStreaming2StatNames::FramesCorrupted, 0);
		Add(PixelStreaming2StatNames::PartialFramesLost, 0);
		Add(PixelStreaming2StatNames::FullFramesLost, 0);
		Add(PixelStreaming2StatNames::JitterBufferTargetDelay, 2);
		Add(PixelStreaming2StatNames::InterruptionCount, 0);
		Add(PixelStreaming2StatNames::TotalInterruptionDuration, 2);
		Add(PixelStreaming2StatNames::FreezeCount, 0);
		Add(PixelStreaming2StatNames::PauseCount, 0);
		Add(PixelStreaming2StatNames::TotalFreezesDuration, 2);
		Add(PixelStreaming2StatNames::TotalPausesDuration, 2);
	}

	/*
	* ---------------- FVideoSourceStatsSink ----------------
	*/
	FRTCStatsCollector::FVideoSourceStatsSink::FVideoSourceStatsSink() : FStatsSink(RTCStatTypes::MediaSource)
	{
		// Track video source fps
		Add(PixelStreaming2StatNames::SourceFps, 0);
	}

	bool FRTCStatsCollector::FVideoSourceStatsSink::Wants(const webrtc::RTCStats& InStats) const
	{
		const FString StatsType = FString(InStats.type());
		if (StatsType != RTCStatTypes::MediaSource)
		{
			return false;
		}
		for (const webrtc::RTCStatsMemberInterface* StatMember : InStats.Members())
		{
			const FString StatName = FString(StatMember->name());
			if (StatName == "kind")
			{
				FString StatMediaKind = StatMember->is_defined() ? ToString(StatMember->ValueToString()) : TEXT("");
				return StatMediaKind == "video";
			}
		}
		return false;
	}

	/*
	* ---------- FRTCTrackedStat -------------------
	*/
	FRTCStatsCollector::FRTCTrackedStat::FRTCTrackedStat(FName StatName, FName Alias, int NDecimalPlaces, uint8 DisplayFlags)
		: LatestStat(StatName, 0.0, NDecimalPlaces)
	{
		LatestStat.DisplayFlags = DisplayFlags;
		LatestStat.Alias = Alias;
	}

	double FRTCStatsCollector::FRTCTrackedStat::CalculateDelta(double Period) const
	{
		return (LatestStat.StatValue - PrevValue) * Period;
	}

	double FRTCStatsCollector::FRTCTrackedStat::Average() const
	{
		return (LatestStat.StatValue + PrevValue) * 0.5;
	}

	void FRTCStatsCollector::FRTCTrackedStat::SetLatestValue(double InValue)
	{
		PrevValue = LatestStat.StatValue;
		LatestStat.StatValue = InValue;
	}

	/*
	* ----------- FDataChannelStatsSink -----------
	*/
	FRTCStatsCollector::FDataChannelStatsSink::FDataChannelStatsSink() : FStatsSink("data-channel")
	{
		// These names are added as aliased names because `bytesSent` is ambiguous stat that is used across inbound-rtp, outbound-rtp, and data-channel
		// so to disambiguate which state we are referring to we record the `bytesSent` stat for the data-channel but store and report it as `data-channel-bytesSent`
		AddAliased(PixelStreaming2StatNames::MessagesSent, PixelStreaming2StatNames::DataChannelMessagesSent, 0, FStatData::EDisplayFlags::TEXT | FStatData::EDisplayFlags::GRAPH);
		AddAliased(PixelStreaming2StatNames::MessagesReceived, PixelStreaming2StatNames::DataChannelBytesReceived, 0, FStatData::EDisplayFlags::TEXT | FStatData::EDisplayFlags::GRAPH);
		AddAliased(PixelStreaming2StatNames::BytesSent, PixelStreaming2StatNames::DataChannelBytesSent, 0, FStatData::EDisplayFlags::TEXT | FStatData::EDisplayFlags::GRAPH);
		AddAliased(PixelStreaming2StatNames::BytesReceived, PixelStreaming2StatNames::DataChannelMessagesReceived, 0, FStatData::EDisplayFlags::TEXT | FStatData::EDisplayFlags::GRAPH);
	}

} // namespace UE::PixelStreaming2

#endif // (Migration)
