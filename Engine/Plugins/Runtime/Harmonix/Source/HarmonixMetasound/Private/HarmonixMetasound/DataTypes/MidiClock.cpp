// Copyright Epic Games, Inc. All Rights Reserved.
#include "HarmonixMetasound/DataTypes/MidiClock.h"

#include "MetasoundDataTypeRegistrationMacro.h"
#include "Engine/Engine.h"
#include "HarmonixMetasound/Subsystems/MidiClockUpdateSubsystem.h"

DEFINE_LOG_CATEGORY_STATIC(LogHarmonixMidiClock, Log, All);

#define LOCTEXT_NAMESPACE "HarmonixMetaSound"

namespace Metasound
{
	DEFINE_METASOUND_ENUM_BEGIN(EMidiClockSubdivisionQuantization, FEnumMidiClockSubdivisionQuantizationType, "SubdivisionQuantizationType")
		DEFINE_METASOUND_ENUM_ENTRY(EMidiClockSubdivisionQuantization::Bar, "BarDesc", "Bar", "BarTT", "Bar"),
		DEFINE_METASOUND_ENUM_ENTRY(EMidiClockSubdivisionQuantization::Beat, "BeatDesc", "Beat", "BeatTT", "Beat"),
		DEFINE_METASOUND_ENUM_ENTRY(EMidiClockSubdivisionQuantization::ThirtySecondNote, "ThirtySecondNoteDesc", "1/32", "ThirtySecondNoteTT", "1/32"),
		DEFINE_METASOUND_ENUM_ENTRY(EMidiClockSubdivisionQuantization::SixteenthNote, "SixteenthNoteDesc", "1/16", "SixteenthNoteTT", "1/16"),
		DEFINE_METASOUND_ENUM_ENTRY(EMidiClockSubdivisionQuantization::EighthNote, "EighthNoteDesc", "1/8", "EighthNoteTT", "1/8"),
		DEFINE_METASOUND_ENUM_ENTRY(EMidiClockSubdivisionQuantization::QuarterNote, "QuarterNoteDesc", "1/4", "QuarterNoteTT", "1/4"),
		DEFINE_METASOUND_ENUM_ENTRY(EMidiClockSubdivisionQuantization::HalfNote, "HalfNoteDesc", "Half", "HalfNoteTT", "Half"),
		DEFINE_METASOUND_ENUM_ENTRY(EMidiClockSubdivisionQuantization::WholeNote, "WholeNoteDesc", "Whole", "WholeNoteTT", "Whole"),
		DEFINE_METASOUND_ENUM_ENTRY(EMidiClockSubdivisionQuantization::DottedSixteenthNote, "DottedSixteenthNoteDesc", "(dotted) 1/16", "DottedSixteenthNoteTT", "(dotted) 1/16"),
		DEFINE_METASOUND_ENUM_ENTRY(EMidiClockSubdivisionQuantization::DottedEighthNote, "DottedEighthNoteDesc", "(dotted) 1/8", "DottedEighthNoteTT", "(dotted) 1/8"),
		DEFINE_METASOUND_ENUM_ENTRY(EMidiClockSubdivisionQuantization::DottedQuarterNote, "DottedQuarterNoteDesc", "(dotted) 1/4", "DottedQuarterNoteTT", "(dotted) 1/4"),
		DEFINE_METASOUND_ENUM_ENTRY(EMidiClockSubdivisionQuantization::DottedHalfNote, "DottedHalfNoteDesc", "(dotted) Half", "DottedHalfNoteTT", "(dotted) Half"),
		DEFINE_METASOUND_ENUM_ENTRY(EMidiClockSubdivisionQuantization::DottedWholeNote, "DottedWholeNoteDesc", "(dotted) Whole", "DottedWholeNoteTT", "(dotted) Whole"),
		DEFINE_METASOUND_ENUM_ENTRY(EMidiClockSubdivisionQuantization::SixteenthNoteTriplet, "SixteenthNoteTripletDesc", "1/16 (triplet)", "SixteenthNoteTripletTT", "1/16 (triplet)"),
		DEFINE_METASOUND_ENUM_ENTRY(EMidiClockSubdivisionQuantization::EighthNoteTriplet, "EighthNoteTripletDesc", "1/8 (triplet)", "EighthNoteTripletTT", "1/8 (triplet)"),
		DEFINE_METASOUND_ENUM_ENTRY(EMidiClockSubdivisionQuantization::QuarterNoteTriplet, "QuarterNoteTripletDesc", "1/4 (triplet)", "QuarterNoteTripletTT", "1/4 (triplet)"),
		DEFINE_METASOUND_ENUM_ENTRY(EMidiClockSubdivisionQuantization::HalfNoteTriplet, "HalfNoteTripletDesc", "1/2 (triplet)", "HalfNoteTripletTT", "1/2 (triplet)"),
	DEFINE_METASOUND_ENUM_END()
}

int32 SubdivisionToMidiTicks(const EMidiClockSubdivisionQuantization Division, const int32 CurrentTick, const FSongMaps& SongMap)
{
	int32 BarMapPointIndex = SongMap.GetBarMap().GetPointIndexForTick(CurrentTick);
	if (BarMapPointIndex < 0)
	{
		return 0;
	}
	const FTimeSignature* TimeSignature =  SongMap.GetTimeSignatureAtTick(CurrentTick);
	if (!TimeSignature)
	{
		return 0;
	}
	switch (Division)
	{
	case EMidiClockSubdivisionQuantization::Bar: 					return SongMap.GetBarMap().GetTicksInBarAfterPoint(BarMapPointIndex);
	case EMidiClockSubdivisionQuantization::Beat:					return (MidiConstants::kTicksPerQuarterNoteInt * 4) / TimeSignature->Denominator;
	case EMidiClockSubdivisionQuantization::ThirtySecondNote:		return MidiConstants::kTicksPerQuarterNoteInt / 8;
	case EMidiClockSubdivisionQuantization::SixteenthNote:			return MidiConstants::kTicksPerQuarterNoteInt / 4;
	case EMidiClockSubdivisionQuantization::EighthNote:				return MidiConstants::kTicksPerQuarterNoteInt / 2;
	case EMidiClockSubdivisionQuantization::QuarterNote:			return MidiConstants::kTicksPerQuarterNoteInt;
	case EMidiClockSubdivisionQuantization::HalfNote:				return MidiConstants::kTicksPerQuarterNoteInt * 2;
	case EMidiClockSubdivisionQuantization::WholeNote:				return MidiConstants::kTicksPerQuarterNoteInt * 4;
	case EMidiClockSubdivisionQuantization::DottedSixteenthNote:	return (MidiConstants::kTicksPerQuarterNoteInt / 4) + (MidiConstants::kTicksPerQuarterNoteInt / 8);
	case EMidiClockSubdivisionQuantization::DottedEighthNote:		return (MidiConstants::kTicksPerQuarterNoteInt / 2) + (MidiConstants::kTicksPerQuarterNoteInt / 4);
	case EMidiClockSubdivisionQuantization::DottedQuarterNote:		return (MidiConstants::kTicksPerQuarterNoteInt)     + (MidiConstants::kTicksPerQuarterNoteInt / 2);
	case EMidiClockSubdivisionQuantization::DottedHalfNote:			return (MidiConstants::kTicksPerQuarterNoteInt * 2) + (MidiConstants::kTicksPerQuarterNoteInt);
	case EMidiClockSubdivisionQuantization::DottedWholeNote:		return (MidiConstants::kTicksPerQuarterNoteInt * 4) + (MidiConstants::kTicksPerQuarterNoteInt * 2);
	case EMidiClockSubdivisionQuantization::SixteenthNoteTriplet:   return (MidiConstants::kTicksPerQuarterNoteInt / 2) / 3;
	case EMidiClockSubdivisionQuantization::EighthNoteTriplet:		return MidiConstants::kTicksPerQuarterNoteInt / 3;
	case EMidiClockSubdivisionQuantization::QuarterNoteTriplet:		return (MidiConstants::kTicksPerQuarterNoteInt * 2) / 3;
	case EMidiClockSubdivisionQuantization::HalfNoteTriplet:        return (MidiConstants::kTicksPerQuarterNoteInt * 4) / 3;
	default:	/* Beat */											return (MidiConstants::kTicksPerQuarterNoteInt * 4) / TimeSignature->Denominator;
	}
}

REGISTER_METASOUND_DATATYPE(HarmonixMetasound::FMidiClock, "MidiClock")

namespace HarmonixMetasound
{
	using namespace Metasound;
	
	FMidiClock::FMidiClock(const FOperatorSettings& InSettings)
		: TempoChangesCursor(this)
		, BlockSize(InSettings.GetNumFramesPerBlock())
		, CurrentBlockFrameIndex(0)
		, SampleRate(InSettings.GetSampleRate())
		, HasSpeedChangeInBlock(false)
		, HasTempoChangeInBlock(false)
	{
		DrivingMidiPlayCursorMgr = MakeShared<FMidiPlayCursorMgr>();
		DrivingMidiPlayCursorMgr->RegisterHiResPlayCursor(&TempoChangesCursor);
		IOwnThePlayCursorMgr = true;
		SpeedChangesInBlock.Add({0, 0.0f, 1.0f});
		TempoChangesInBlock.Add({0, 0.0f, 120.0f});

		RegisterForGameThreadUpdates();
	}

	FMidiClock::FMidiClock(const FMidiClock& Other)
	{
		*this = Other;
	}

	FMidiClock& FMidiClock::operator=(const FMidiClock& Other)
	{
		if (&Other != this)
		{
			bool NeedReRegister = false;
			if (DrivingMidiPlayCursorMgr && DrivingMidiPlayCursorMgr != Other.DrivingMidiPlayCursorMgr)
			{
				NeedReRegister = true;
				DrivingMidiPlayCursorMgr->UnregisterPlayCursor(&TempoChangesCursor);
				DrivingMidiPlayCursorMgr = nullptr;
			}

			BlockSize = Other.BlockSize;
			CurrentBlockFrameIndex = Other.CurrentBlockFrameIndex;
			SampleRate = Other.SampleRate;
			CurrentTransportState = Other.CurrentTransportState;

			TransportChangesInBlock = Other.TransportChangesInBlock;
			HasSpeedChangeInBlock = Other.HasSpeedChangeInBlock;
			SpeedChangesInBlock = Other.SpeedChangesInBlock;
			HasTempoChangeInBlock = Other.HasTempoChangeInBlock;
			TempoChangesInBlock = Other.TempoChangesInBlock;
			SmoothingEnabled = Other.SmoothingEnabled;

			if (NeedReRegister)
			{
				DrivingMidiPlayCursorMgr = Other.DrivingMidiPlayCursorMgr;
				if (DrivingMidiPlayCursorMgr)
				{
					DrivingMidiPlayCursorMgr->RegisterHiResPlayCursor(&TempoChangesCursor);
				}
			}

			RegisterForGameThreadUpdates();
		}
		return *this;
	}

	FMidiClock::FMidiClock(FMidiClock&& Other)
		: TempoChangesCursor(this)
	{
		BlockSize = Other.BlockSize;
		CurrentBlockFrameIndex = Other.CurrentBlockFrameIndex;
		SampleRate = Other.SampleRate;
		CurrentTransportState = Other.CurrentTransportState;

		TransportChangesInBlock = MoveTemp(Other.TransportChangesInBlock);
		HasSpeedChangeInBlock = Other.HasSpeedChangeInBlock;
		SpeedChangesInBlock = MoveTemp(Other.SpeedChangesInBlock);
		HasTempoChangeInBlock = Other.HasTempoChangeInBlock;
		TempoChangesInBlock = MoveTemp(Other.TempoChangesInBlock);
		SmoothingEnabled = Other.SmoothingEnabled;
		
		if (Other.DrivingMidiPlayCursorMgr)
		{
			Other.DrivingMidiPlayCursorMgr->UnregisterPlayCursor(&Other.TempoChangesCursor);
			DrivingMidiPlayCursorMgr = Other.DrivingMidiPlayCursorMgr;
		}
		else
		{
			DrivingMidiPlayCursorMgr = MakeShared<FMidiPlayCursorMgr>();
		}
		DrivingMidiPlayCursorMgr->RegisterHiResPlayCursor(&TempoChangesCursor);

		RegisterForGameThreadUpdates();
	}

	FMidiClock::~FMidiClock()
	{
		UnregisterForGameThreadUpdates();
	
		DrivingMidiPlayCursorMgr->UnregisterPlayCursor(&TempoChangesCursor, false);
	}

	FMidiClock::FTempoChangesCursor::FTempoChangesCursor()
	{
		SetMessageFilter(FMidiPlayCursor::EFilterPassFlags::Tempo);
	}

	FMidiClock::FTempoChangesCursor::FTempoChangesCursor(FMidiClock* MidiClock) : MyMidiClock(MidiClock)
	{
		check(MyMidiClock);
		SetMessageFilter(FMidiPlayCursor::EFilterPassFlags::Tempo);
	}

	void FMidiClock::FTempoChangesCursor::OnTempo(int32 TrackIndex, int32 Tick, int32 Tempo, bool IsPreroll /*= false*/)
	{
		check(MyMidiClock);
		int32 BlockFrameIndex = MyMidiClock->CurrentBlockFrameIndex;

		check(BlockFrameIndex >= MyMidiClock->TempoChangesInBlock.Last().BlockSampleFrameIndex);
		MyMidiClock->HasTempoChangeInBlock = true;
		float Bpm = MidiConstants::MidiTempoToBPM(Tempo);
		if (MyMidiClock->TempoChangesInBlock.Last().BlockSampleFrameIndex == BlockFrameIndex)
		{
			MyMidiClock->TempoChangesInBlock.Last().Tempo = Bpm;
		}
		else
		{ 
			MyMidiClock->TempoChangesInBlock.Add({BlockFrameIndex, 0.0f, Bpm});
		}
	}

	void FMidiClock::RegisterForGameThreadUpdates()
	{
		if (nullptr == GEngine)
		{
			UE_LOG(LogHarmonixMidiClock, Error, TEXT("GEngine didn't exist. Failed to register a MIDI clock for game thread updates."))
			return;
		}

		UMidiClockUpdateSubsystem* UpdateSubsystem = GEngine->GetEngineSubsystem<UMidiClockUpdateSubsystem>();

		if (nullptr == UpdateSubsystem)
		{
			UE_LOG(LogHarmonixMidiClock, Error, TEXT("Failed to get UMidiClockUpdateSubsystem. Low resolution MIDI clock updates won't happen."));
			return;
		}

		UpdateSubsystem->TrackClock(this);
	}

	void FMidiClock::UnregisterForGameThreadUpdates()
	{
		if (nullptr == GEngine)
		{
			UE_LOG(LogHarmonixMidiClock, Warning, TEXT("GEngine didn't exist while attempting to unregister a MIDI clock with the game thread updater."))
			return;
		}

		UMidiClockUpdateSubsystem* UpdateSubsystem = GEngine->GetEngineSubsystem<UMidiClockUpdateSubsystem>();

		if (nullptr == UpdateSubsystem)
		{
			UE_LOG(LogHarmonixMidiClock, Warning, TEXT("Failed to get UMidiClockUpdateSubsystem while attempting to unregister a MIDI clock with the game thread updater."));
			return;
		}

		UpdateSubsystem->StopTrackingClock(this);
	}


	void FMidiClock::ResetAndStart(int32 FrameIndex, bool SeekToStart)
	{
		if (SeekToStart)
		{
			SampleCount = 0;
			DrivingMidiPlayCursorMgr->SeekTo(0, 0, true, false);
		}
	
		AddTransportStateChangeToBlock({FrameIndex,0.0f, EMusicPlayerTransportState::Playing});
		
		HasSpeedChangeInBlock = false;
		SpeedChangesInBlock.SetNum(1);
		SpeedChangesInBlock[0] = {0, 0.0f, 1.0f};

		HasTempoChangeInBlock = false;
		TempoChangesInBlock.SetNum(1);
		TempoChangesInBlock[0] = { 0, 0.0f, 120.0f };

		CurrentBlockFrameIndex = FrameIndex;
		FramesUntilNextProcess = 0;
	}

	void FMidiClock::PrepareBlock()
	{
		TransportChangesInBlock.Empty(4);
		if (SpeedChangesInBlock.Num() > 1)
		{
			SpeedChangesInBlock[0].Speed = SpeedChangesInBlock.Last().Speed;
			SpeedChangesInBlock.SetNum(1, false);
		}
		HasSpeedChangeInBlock = false;
		if (TempoChangesInBlock.Num() > 1)
		{
			TempoChangesInBlock[0].Tempo = TempoChangesInBlock.Last().Tempo;
			TempoChangesInBlock.SetNum(1, false);
		}
		HasTempoChangeInBlock = false;
		CurrentBlockFrameIndex = 0;

		MidiClockEventsInBlock.Reset();
	}

	bool FMidiClock::HasLowResCursors() const
	{
		return DrivingMidiPlayCursorMgr->HasLowResCursors();
	}

	void FMidiClock::UpdateLowResCursors()
	{
		DrivingMidiPlayCursorMgr->AdvanceLowResCursors();
	}

	void FMidiClock::AddTransportStateChangeToBlock(const FMidiTimestampTransportState& NewTransportState)
	{
		if (TransportChangesInBlock.IsEmpty() || TransportChangesInBlock.Last().BlockSampleFrameIndex <= NewTransportState.BlockSampleFrameIndex)
		{
			CurrentTransportState = NewTransportState;
			TransportChangesInBlock.Add(NewTransportState);
		}
	}

	void FMidiClock::AddSpeedChangeToBlock(const FMidiTimestampSpeed& NewSpeed)
	{
		check(SpeedChangesInBlock.IsEmpty() || SpeedChangesInBlock.Last().BlockSampleFrameIndex <= NewSpeed.BlockSampleFrameIndex);
		if (SpeedChangesInBlock.Last().BlockSampleFrameIndex == NewSpeed.BlockSampleFrameIndex)
		{
			SpeedChangesInBlock.Last().Speed = NewSpeed.Speed;
		}
		else
		{
			SpeedChangesInBlock.Add(NewSpeed);
		}
		HasSpeedChangeInBlock = true;
		DrivingMidiPlayCursorMgr->InformOfCurrentAdvanceRate(NewSpeed.Speed);
	}

	const TArray<FMidiClockEvent>& FMidiClock::GetMidiClockEventsInBlock() const
	{
		return MidiClockEventsInBlock;
	}

	EMusicPlayerTransportState FMidiClock::GetTransportStateAtBlockSampleFrame(int32 FrameIndex) const
	{
		return GetTransportTimestampForBlockSampleFrame(FrameIndex).TransportState;
	}

	EMusicPlayerTransportState FMidiClock::GetTransportStateAtEndOfBlock() const
	{
		return CurrentTransportState.TransportState;
	}

	const FMidiTimestampTransportState& FMidiClock::GetTransportTimestampForBlockSampleFrame(int32 FrameIndex) const
	{
		if (TransportChangesInBlock.IsEmpty())
		{
			return CurrentTransportState;
		}
		int32 Index = Algo::UpperBoundBy(TransportChangesInBlock, FrameIndex, [](const FMidiTimestampTransportState& t) { return t.BlockSampleFrameIndex; }) - 1;
		if (Index < 0)
		{
			return CurrentTransportState;
		}
		return TransportChangesInBlock[Index];
	}

	float FMidiClock::GetSpeedAtBlockSampleFrame(int32 FrameIndex) const
	{
		return GetSpeedTimestampForBlockSampleFrame(FrameIndex).Speed;
	}

	float FMidiClock::GetSpeedAtEndOfBlock() const
	{
		return SpeedChangesInBlock.Last().Speed;
	}

	const FMidiTimestampSpeed& FMidiClock::GetSpeedTimestampForBlockSampleFrame(int32 FrameIndex) const
	{
		int32 Index = Algo::UpperBoundBy(SpeedChangesInBlock, FrameIndex, [](const FMidiTimestampSpeed& t) { return t.BlockSampleFrameIndex; }) - 1;
		if (Index < 0)
		{
			Index = 0;
		}
		return SpeedChangesInBlock[Index];
	}

	float FMidiClock::GetTempoAtBlockSampleFrame(int32 FrameIndex) const
	{
		int32 Index = Algo::UpperBoundBy(TempoChangesInBlock, FrameIndex, [](const FMidiTimestampTempo& t) { return t.BlockSampleFrameIndex; }) - 1;
		if (Index < 0)
		{
			Index = 0;
		}
		return TempoChangesInBlock[Index].Tempo;
	}

	float FMidiClock::GetTempoAtEndOfBlock() const
	{
		return TempoChangesInBlock.Last().Tempo;
	}

	int32 FMidiClock::GetNumTempoChangesInBlock() const
	{
		return TempoChangesInBlock.Num();
	}

	FMidiTimestampTempo FMidiClock::GetTempoChangeByIndex(int32 Index) const
	{
		if (Index >= 0 && Index < TempoChangesInBlock.Num())
		{
			return TempoChangesInBlock[Index]; // intentional copy
		}

		return InvalidMidiTimestampTempo;
	}

	int32 FMidiClock::GetCurrentMidiTick() const
	{
		return DrivingMidiPlayCursorMgr->GetCurrentHiResTick();
	}

	int32 FMidiClock::GetCurrentBlockFrameIndex() const
	{
		return CurrentBlockFrameIndex;
	}

	void FMidiClock::AdvanceHiResToMs(int32 BlockFrameIndex, float Ms, bool Broadcast)
	{
		CurrentBlockFrameIndex = BlockFrameIndex;
		DrivingMidiPlayCursorMgr->AdvanceHiResToMs(Ms, Broadcast);
	}

	void FMidiClock::AttachToTimeAuthority(const FMidiClock& MidiClockRef)
	{
		DrivingMidiPlayCursorMgr->AttachToTimeAuthority(MidiClockRef.DrivingMidiPlayCursorMgr);
	}

	float FMidiClock::GetQuarterNoteIncludingCountIn() const
	{
		int32 Tick = DrivingMidiPlayCursorMgr->GetCurrentHiResTick();
		return (float)Tick / (float)DrivingMidiPlayCursorMgr->GetSongMaps().GetTicksPerQuarterNote();
	}

	FMusicTimestamp FMidiClock::GetCurrentMusicTimestamp() const
	{
		return DrivingMidiPlayCursorMgr->GetMusicTimestampAtMs(GetCurrentHiResMs());
	}

	FMusicTimestamp FMidiClock::GetMusicTimestampAtBlockOffset(const int32 Offset) const
	{
		const float MsAtOffset = GetMsAtBlockOffset(Offset);
		return DrivingMidiPlayCursorMgr->GetMusicTimestampAtMs(MsAtOffset);
	}

	float FMidiClock::GetMsAtBlockOffset(int32 Offset) const
	{
		const float EndMs = GetCurrentHiResMs();
		const float MsPerFrame = 1000 / SampleRate;
		const int32 InvOffset = BlockSize - Offset - 1;
		return EndMs - InvOffset * MsPerFrame;
	}

	void FMidiClock::LockForMidiDataChanges()
	{

		DrivingMidiPlayCursorMgr->LockForMidiDataChanges();
	}

	void FMidiClock::MidiDataChangesComplete(FMidiPlayCursorMgr::EMidiChangePositionCorrectMode Mode /*= FMidiPlayCursorMgr::EMidiChangePositionCorrectMode::MaintainTick*/)
	{
		DrivingMidiPlayCursorMgr->MidiDataChangeComplete(Mode);
	}

	void FMidiClock::SeekTo(const FMusicSeekTarget& Target, int32 PreRollBars)
	{
		int32 Tick = 0;
		switch (Target.Type)
		{
		case ESeekPointType::BarBeat:
			Tick = DrivingMidiPlayCursorMgr->GetBarMap().MusicTimestampToTick(Target.BarBeat);
			break;
		default:
		case ESeekPointType::Millisecond:
			Tick = DrivingMidiPlayCursorMgr->GetSongMaps().MsToTick(Target.Ms);
			break;
		}
		DrivingMidiPlayCursorMgr->SeekTo(Tick, PreRollBars,true,false);
	}

	void FMidiClock::InformOfCurrentAdvanceRate(float AdvanceRate)
	{
		DrivingMidiPlayCursorMgr->InformOfCurrentAdvanceRate(AdvanceRate);
	}

	void FMidiClock::CopySpeedAndTempoChanges(const FMidiClock* InClock, float InSpeedMult)
	{
		HasTempoChangeInBlock = InClock->HasTempoChangeInBlock;
		TempoChangesInBlock = InClock->TempoChangesInBlock;

		HasSpeedChangeInBlock = InClock->HasSpeedChangeInBlock;
		SpeedChangesInBlock = InClock->SpeedChangesInBlock;

		for (FMidiTimestampSpeed& SpeedChange : SpeedChangesInBlock)
		{
			SpeedChange.Speed *= InSpeedMult;
		}
	}

	void FMidiClock::WriteAdvance(int32 StartFrameIndex, int32 EndFrameIndex, float InSpeed /*= 1.0f*/)
	{
		int32 FramesToProcess = EndFrameIndex - StartFrameIndex;
		const FTempoMap& TempoMap = DrivingMidiPlayCursorMgr->GetSongMaps().GetTempoMap();
		while (FramesToProcess > FramesUntilNextProcess)
		{
			StartFrameIndex += FramesUntilNextProcess;
			FSampleCount AdvanceToFrame = SampleCount + (FSampleCount)((float)kMidiGranularity * InSpeed);
			FramesUntilNextProcess = kMidiGranularity;
			float AdvanceToMs = ((float)AdvanceToFrame * 1000.0f) / SampleRate;
			DrivingMidiPlayCursorMgr->InformOfCurrentAdvanceRate(InSpeed);
			AdvanceHiResToMs(StartFrameIndex, AdvanceToMs, true);

			
			// Account for looping
			// recalculate sampleCount based on our new time, if clock looped back, this will be different from AdvanceToFrame
			if (GetCurrentHiResMs() < AdvanceToMs)
			{
				SampleCount = FMath::Max<FSampleCount>(FSampleCount(GetCurrentHiResMs() / 1000.0f * SampleRate), 0);
			}
			else
			{
				SampleCount = AdvanceToFrame;
			}
			

			FramesToProcess = EndFrameIndex - StartFrameIndex;
		}
		FramesUntilNextProcess -= FramesToProcess;
	}

	void FMidiClock::WriteNoAdvance(int32 StartFrameIndex, int32 EndFrameIndex)
	{
		EMusicPlayerTransportState ClockTransport = GetTransportStateAtBlockSampleFrame(StartFrameIndex);
		check(ClockTransport != EMusicPlayerTransportState::Playing);
	}

	void FMidiClock::SeekTo(int32 BlockFrameIndex, const FMusicSeekTarget& InTarget, int32 InPrerollBars)
	{
		CurrentBlockFrameIndex = BlockFrameIndex;
		SeekTo(InTarget, InPrerollBars);
		SampleCount = FMath::Max<FSampleCount>(FSampleCount(GetCurrentHiResMs() / 1000.0f * SampleRate), 0);
		FramesUntilNextProcess = 0;
	}

	TSharedPtr<FMidiFileData> FMidiClock::MakeClockConductorMidiData(float InTempoBPM, int32 InTimeSigNum, int32 InTimeSigDen)
	{
		TSharedPtr<FMidiFileData> OutMidiData = MakeShared<FMidiFileData>();

		// clear it all out for good measure...
		FTempoMap& TempoMap = OutMidiData->SongMaps.GetTempoMap();
		TempoMap.Empty();
		FBarMap& BarMap = OutMidiData->SongMaps.GetBarMap();
		BarMap.Empty();
		OutMidiData->Tracks.Empty();

		// create conductor track
		FMidiTrack& Track = OutMidiData->Tracks.Add_GetRef(FMidiTrack(TEXT("conductor")));
		OutMidiData->LastEventTick = std::numeric_limits<int32>::max();

		// add time sig info
		int32 TimeSigNum = FMath::Clamp(InTimeSigNum, 1, 64);
		int32 TimeSigDen = FMath::Clamp(InTimeSigDen, 1, 64);
		Track.AddEvent(FMidiEvent(0, FMidiMsg((uint8)TimeSigNum, (uint8)TimeSigDen)));
		BarMap.AddTimeSignatureAtBarIncludingCountIn(0, TimeSigNum, TimeSigDen);

		// add tempo info
		float TempoBPM = FMath::Max(1.0f, InTempoBPM);
		int32 MidiTempo = MidiConstants::BPMToMidiTempo(TempoBPM);
		Track.AddEvent(FMidiEvent(0, FMidiMsg(MidiTempo)));
		TempoMap.AddTempoInfoPoint(MidiTempo, 0);

		Track.Sort();

		// max out song length data so the midi can play indefinitely
		OutMidiData->SongMaps.GetSongLengthData().LastTick = std::numeric_limits<int32>::max();
		OutMidiData->SongMaps.GetSongLengthData().LengthTicks = std::numeric_limits<int32>::max();
		OutMidiData->SongMaps.GetSongLengthData().LengthBars = std::numeric_limits<int32>::max();

		return OutMidiData;
	}


	FMidiClockEventCursor::FMidiClockEventCursor(FMidiClockWriteRef InClock)
		: MidiClock(InClock)
	{
		MidiClock->RegisterHiResPlayCursor(this);
	}

	FMidiClockEventCursor::~FMidiClockEventCursor()
	{

		MidiClock->UnregisterPlayCursor(this);
	}

	void FMidiClockEventCursor::Reset(bool ForceNoBroadcast /*= false*/)
	{
		FMidiPlayCursor::Reset(ForceNoBroadcast);
		AddEvent(FMidiClockEvent::MakeResetEvent(MidiClock->GetCurrentBlockFrameIndex(), MidiClock->GetCurrentHiResTick(), ForceNoBroadcast));
	}

	void FMidiClockEventCursor::OnLoop(int32 LoopStartTick, int32 LoopEndTick)
	{
		FMidiPlayCursor::OnLoop(LoopStartTick, LoopEndTick);
		AddEvent(FMidiClockEvent::MakeLoopEvent(MidiClock->GetCurrentBlockFrameIndex(), LoopStartTick, LoopEndTick));
	}

	void FMidiClockEventCursor::SeekToTick(int32 Tick)
	{
		FMidiPlayCursor::SeekToTick(Tick);
		AddEvent(FMidiClockEvent::MakeSeekToEvent(MidiClock->GetCurrentBlockFrameIndex(), Tick));
	}

	void FMidiClockEventCursor::SeekThruTick(int32 Tick)
	{
		FMidiPlayCursor::SeekThruTick(Tick);
		AddEvent(FMidiClockEvent::MakeSeekThruEvent(MidiClock->GetCurrentBlockFrameIndex(), Tick));
	}

	void FMidiClockEventCursor::AdvanceThruTick(int32 Tick, bool IsPreRoll)
	{
		FMidiPlayCursor::AdvanceThruTick(Tick, IsPreRoll);
		AddEvent(FMidiClockEvent::MakeAdvanceThruEvent(MidiClock->GetCurrentBlockFrameIndex(), Tick, IsPreRoll));
	}

	void FMidiClockEventCursor::AddEvent(const FMidiClockEvent& InEvent)
	{
		TArray<FMidiClockEvent>& Events = MidiClock->MidiClockEventsInBlock;
		if (!Events.IsEmpty())
		{
			check(Events.Last().BlockFrameIndex <= InEvent.BlockFrameIndex);
		}

		Events.Add(InEvent);
	}

}

#undef LOCTEXT_NAMESPACE
