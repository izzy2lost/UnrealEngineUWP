// Copyright Epic Games, Inc. All Rights Reserved.
#include "HarmonixMetasound/Components/MusicClockComponent.h"
#include "HarmonixMetasound/Components/MetasoundMusicClockDriver.h"
#include "HarmonixMetasound/Components/WallClockMusicClockDriver.h"
#include "Harmonix.h"
#include "HarmonixMidi/MusicTimeSpan.h"
#include "Components/AudioComponent.h"
#include "Templates/SharedPointer.h"
#include "Misc/Optional.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MusicClockComponent)

DEFINE_LOG_CATEGORY(LogMusicClock)

UMusicClockComponent::UMusicClockComponent()
{
	MakeDefaultSongMap();
	PrimaryComponentTick.bCanEverTick = true;
	// We want the music player and clocks to tick before other components, which default to the TG_DuringPhysics group.
	// Though, this means any other TG_PrePhysics group actors or components that care about accurate song time will
	// want to manually add a tick prerequisite on this component.
	PrimaryComponentTick.TickGroup = TG_PrePhysics;
}

UMusicClockComponent* UMusicClockComponent::CreateMetasoundDrivenMusicClock(UObject* WorldContextObject, UAudioComponent* InAudioComponent, FName MetasoundOutputPinName, bool Start)
{
	UMusicClockComponent* NewClock = NewObject<UMusicClockComponent>(WorldContextObject);
	if (!NewClock->GetOwner())
	{
		UE_LOG(LogMusicClock, Warning, TEXT("Can't create a music clock in a non-actor context!"));
		return nullptr;
	}
	NewClock->RegisterComponent();
	NewClock->DriveMethod = EMusicClockDriveMethod::MetaSound;
	NewClock->MetasoundOutputName = MetasoundOutputPinName;
	NewClock->ConnectToMetasoundOnAudioComponent(InAudioComponent);
	if (Start)
	{
		NewClock->Start();
	}
	return NewClock;
}

UMusicClockComponent* UMusicClockComponent::CreateWallClockDrivenMusicClock(UObject* WorldContextObject, UMidiFile* InTempoMap, bool Start)
{
	UMusicClockComponent* NewClock = NewObject<UMusicClockComponent>(WorldContextObject);
	if (!NewClock->GetOwner())
	{
		UE_LOG(LogMusicClock, Warning, TEXT("Can't create a music clock in a non-actor context!"));
		return nullptr;
	}
	NewClock->RegisterComponent();
	NewClock->ConnectToWallClockForMidi(InTempoMap);
	if (Start)
	{
		NewClock->Start();
	}
	return NewClock;
}

bool UMusicClockComponent::ConnectToMetasoundOnAudioComponent(UAudioComponent* InAudioComponent)
{
	DriveMethod = EMusicClockDriveMethod::MetaSound;
	MetasoundsAudioComponent = InAudioComponent;
	return ConnectToMetasound();
}

void UMusicClockComponent::ConnectToWallClockForMidi(UMidiFile* InTempoMap)
{
	DriveMethod = EMusicClockDriveMethod::WallClock;
	TempoMap = InTempoMap;
	ConnectToWallClock();
}

void UMusicClockComponent::UpdateClockIfNeeded()
{
	//	Not for use outside the game thread.
	if (ensureMsgf(
		IsInGameThread(),
		TEXT("%hs called from non-game thread.  This is not supported!"), __FUNCTION__) == false)
	{
		return;
	}

	if (GFrameCounter == LastUpdateFrame)
	{
		return;
	}

	//	Run the actual clock update.
	if (State == EMusicClockState::Running && ClockDriver)
	{
		// Make note of what will soon become the "previous position"...	
		PrevAudioRenderSongPos = CurrentSmoothedAudioRenderSongPos;
		PrevPlayerExperiencedSongPos = CurrentPlayerExperiencedSongPos;
		PrevVideoRenderSongPos = CurrentVideoRenderSongPos;

		if (ClockDriver->RefreshCurrentSongPos())
		{
			CalcDelta();
			LastUpdateFrame = GFrameCounter;
		}
	}
}

void UMusicClockComponent::CreateClockDriver()
{
	if (DriveMethod == EMusicClockDriveMethod::WallClock || !::IsValid(MetasoundsAudioComponent))
	{
		ConnectToWallClock();
	}
	else
	{
		ConnectToMetasound();
	}
}

bool UMusicClockComponent::ConnectToMetasound()
{
	check(DriveMethod == EMusicClockDriveMethod::MetaSound);
	if (!::IsValid(MetasoundsAudioComponent))
	{
		return false;
	}
	if (ClockDriver)
	{
		ClockDriver->Disconnect();
		ClockDriver = nullptr;
	}
	TUniquePtr<FMetasoundMusicClockDriver> MetasoundClockDriver = MakeUnique<FMetasoundMusicClockDriver>(this);
	bool Connected = MetasoundClockDriver->ConnectToAudioComponentsMetasound(MetasoundsAudioComponent, MetasoundOutputName);
	ClockDriver = MoveTemp(MetasoundClockDriver);
	return Connected;
}

void UMusicClockComponent::ConnectToWallClock()
{
	// we don't 'check' that the driver mode is wall clock here because if the driver mode is metasound and we can't 
	// connect for some reason we will fall back to this clock driver!
	if (ClockDriver)
	{
		ClockDriver->Disconnect();
	}
	ClockDriver = MakeUnique<FWallClockMusicClockDriver>(this, TempoMap);
}

FMidiSongPos UMusicClockComponent::CalculateSongPosWithOffset(float MsOffset, ECalibratedMusicTimebase Timebase)
{
	check(ClockDriver);

	UpdateClockIfNeeded();

	FMidiSongPos Result;
	if (ClockDriver->CalculateSongPosWithOffset(MsOffset, Timebase, Result))
	{
		return Result;
	}

	// otherwise, use our song maps copy
	const FSongMaps* Maps = &DefaultMaps;
	switch (Timebase)
	{
	case ECalibratedMusicTimebase::AudioRenderTime:
		Result.SetByTime((CurrentSmoothedAudioRenderSongPos.SecondsIncludingCountIn * 1000.0f) + MsOffset, *Maps);
		break;
	case ECalibratedMusicTimebase::ExperiencedTime:
		Result.SetByTime((CurrentPlayerExperiencedSongPos.SecondsIncludingCountIn * 1000.0f) + MsOffset, *Maps);
		break;
	case ECalibratedMusicTimebase::VideoRenderTime:
	default:
		Result.SetByTime((CurrentVideoRenderSongPos.SecondsIncludingCountIn * 1000.0f) + MsOffset, *Maps);
		break;
	}

	return Result;
}

void UMusicClockComponent::BeginPlay()
{
	if (!ClockDriver)
	{
		CreateClockDriver();
	}
	Super::BeginPlay();
}

void UMusicClockComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);
	if (ClockDriver)
	{
		ClockDriver->Disconnect();
		ClockDriver = nullptr;
	}
}

void UMusicClockComponent::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	UpdateClockIfNeeded();
	Broadcast();
}

void UMusicClockComponent::Start()
{
	MakeDefaultSongMap();
	if (!ClockDriver)
	{
		CreateClockDriver();
	}
	ClockDriver->OnStart();
	LastBroadcastBeat = -1;
	LastBroadcastBar  = -1;
	CurrentSmoothedAudioRenderSongPos.Reset();
	CurrentVideoRenderSongPos.Reset();
	CurrentPlayerExperiencedSongPos.Reset();
	RawUnsmoothedAudioRenderPos.Reset();
	State = EMusicClockState::Running;
	PlayStateEvent.Broadcast(State);
}

void UMusicClockComponent::Pause()
{
	if (State != EMusicClockState::Running)
	{
		return;
	}

	if (ClockDriver)
	{
		ClockDriver->OnPause();
	}

	State = EMusicClockState::Paused;
	PlayStateEvent.Broadcast(State);
}

void UMusicClockComponent::Continue()
{
	if (State != EMusicClockState::Paused)
	{
		return;
	}

	if (ClockDriver)
	{
		ClockDriver->OnContinue();
	}

	State = EMusicClockState::Running;
	PlayStateEvent.Broadcast(State);
}

void UMusicClockComponent::Stop()
{
	if (ClockDriver)
	{
		ClockDriver->OnStop();
	}
	State = EMusicClockState::Stopped;
	CurrentSmoothedAudioRenderSongPos.Reset();
	CurrentVideoRenderSongPos.Reset();
	CurrentPlayerExperiencedSongPos.Reset();
	RawUnsmoothedAudioRenderPos.Reset();
	PlayStateEvent.Broadcast(State);
}

float UMusicClockComponent::GetSecondsIncludingCountIn(ECalibratedMusicTimebase Timebase)
{
	UpdateClockIfNeeded();
	switch (Timebase)
	{
	case ECalibratedMusicTimebase::AudioRenderTime:	return CurrentSmoothedAudioRenderSongPos.SecondsIncludingCountIn;
	case ECalibratedMusicTimebase::ExperiencedTime:	return CurrentPlayerExperiencedSongPos.SecondsIncludingCountIn;
	case ECalibratedMusicTimebase::VideoRenderTime:
	default:                                        return CurrentVideoRenderSongPos.SecondsIncludingCountIn;
	}
}

float UMusicClockComponent::GetSecondsFromBarOne(ECalibratedMusicTimebase Timebase)
{
	UpdateClockIfNeeded();
	switch (Timebase)
	{
	case ECalibratedMusicTimebase::AudioRenderTime:	return CurrentSmoothedAudioRenderSongPos.SecondsFromBarOne;
	case ECalibratedMusicTimebase::ExperiencedTime:	return CurrentPlayerExperiencedSongPos.SecondsFromBarOne;
	case ECalibratedMusicTimebase::VideoRenderTime:
	default:                                        return CurrentVideoRenderSongPos.SecondsFromBarOne;
	}
}

float UMusicClockComponent::GetBarsIncludingCountIn(ECalibratedMusicTimebase Timebase)
{
	UpdateClockIfNeeded();
	switch (Timebase)
	{
	case ECalibratedMusicTimebase::AudioRenderTime:	return CurrentSmoothedAudioRenderSongPos.BarsIncludingCountIn;
	case ECalibratedMusicTimebase::ExperiencedTime:	return CurrentPlayerExperiencedSongPos.BarsIncludingCountIn;
	case ECalibratedMusicTimebase::VideoRenderTime:
	default:                                        return CurrentVideoRenderSongPos.BarsIncludingCountIn;
	}
}

float UMusicClockComponent::GetBeatsIncludingCountIn(ECalibratedMusicTimebase Timebase)
{
	UpdateClockIfNeeded();
	switch (Timebase)
	{
	case ECalibratedMusicTimebase::AudioRenderTime: return CurrentSmoothedAudioRenderSongPos.BeatsIncludingCountIn;
	case ECalibratedMusicTimebase::ExperiencedTime:	return CurrentPlayerExperiencedSongPos.BeatsIncludingCountIn;
	case ECalibratedMusicTimebase::VideoRenderTime:
	default:                                        return CurrentVideoRenderSongPos.BeatsIncludingCountIn;
	}
}

FMusicTimestamp UMusicClockComponent::GetCurrentTimestamp(ECalibratedMusicTimebase Timebase)
{
	UpdateClockIfNeeded();
	switch (Timebase)
	{
	case ECalibratedMusicTimebase::AudioRenderTime:	return CurrentSmoothedAudioRenderSongPos.Timestamp;
	case ECalibratedMusicTimebase::ExperiencedTime:	return CurrentPlayerExperiencedSongPos.Timestamp;
	case ECalibratedMusicTimebase::VideoRenderTime:
	default:                                        return CurrentVideoRenderSongPos.Timestamp;
	}
}

FString UMusicClockComponent::GetCurrentSectionName(ECalibratedMusicTimebase Timebase /*= ECalibratedMusicTimebase::VideoRenderTime*/)
{
	const FMidiSongPos& SongPos = GetSongPos(Timebase);
	return SongPos.CurrentSongSection.Name;
}

int32 UMusicClockComponent::GetCurrentSectionIndex(ECalibratedMusicTimebase Timebase /*= ECalibratedMusicTimebase::VideoRenderTime*/)
{
	const FMidiSongPos& SongPos = GetSongPos(Timebase);
	return GetSongMaps().GetSectionMap().TickToSectionIndex(SongPos.CurrentSongSection.StartTick);
}

float UMusicClockComponent::GetCurrentSectionStartMs(ECalibratedMusicTimebase Timebase /*= ECalibratedMusicTimebase::VideoRenderTime*/)
{
	const FMidiSongPos& SongPos = GetSongPos(Timebase);
	return GetSongMaps().TickToMs(SongPos.CurrentSongSection.StartTick);
}

float UMusicClockComponent::GetCurrentSectionLengthMs(ECalibratedMusicTimebase Timebase /*= ECalibratedMusicTimebase::VideoRenderTime*/)
{
	const FMidiSongPos& SongPos = GetSongPos(Timebase);
	return GetSongMaps().TickToMs(SongPos.CurrentSongSection.LengthTicks);
}

float UMusicClockComponent::GetDistanceFromCurrentBeat(ECalibratedMusicTimebase Timebase /*= ECalibratedMusicTimebase::VideoRenderTime*/)
{
	return FMath::Fractional(GetSongPos(Timebase).BeatsIncludingCountIn);
}

float UMusicClockComponent::GetDistanceToNextBeat(ECalibratedMusicTimebase Timebase /*= ECalibratedMusicTimebase::VideoRenderTime*/)
{
	return 1 - GetDistanceFromCurrentBeat(Timebase);
}

float UMusicClockComponent::GetDistanceToClosestBeat(ECalibratedMusicTimebase Timebase /*= ECalibratedMusicTimebase::VideoRenderTime*/)
{
	return FMath::Min(GetDistanceFromCurrentBeat(Timebase), GetDistanceToNextBeat(Timebase));
}

float UMusicClockComponent::GetDistanceFromCurrentBar(ECalibratedMusicTimebase Timebase /*= ECalibratedMusicTimebase::VideoRenderTime*/)
{
	return FMath::Fractional(GetSongPos(Timebase).BarsIncludingCountIn);
}

float UMusicClockComponent::GetDistanceToNextBar(ECalibratedMusicTimebase Timebase /*= ECalibratedMusicTimebase::VideoRenderTime*/)
{
	return 1 - GetDistanceFromCurrentBar(Timebase);
}

float UMusicClockComponent::GetDistanceToClosestBar(ECalibratedMusicTimebase Timebase /*= ECalibratedMusicTimebase::VideoRenderTime*/)
{
	return FMath::Min(GetDistanceFromCurrentBar(Timebase), GetDistanceToNextBar(Timebase));
}

float UMusicClockComponent::GetDeltaBar(ECalibratedMusicTimebase Timebase)
{
	UpdateClockIfNeeded();
	switch (Timebase)
	{
	case ECalibratedMusicTimebase::AudioRenderTime: return AudioRenderDeltaBarF;
	case ECalibratedMusicTimebase::ExperiencedTime:	return PlayerExperienceDeltaBarF;
	case ECalibratedMusicTimebase::VideoRenderTime:
	default:                                        return VideoRenderDeltaBarF;
	}
}

float UMusicClockComponent::GetDeltaBeat(ECalibratedMusicTimebase Timebase)
{
	UpdateClockIfNeeded();
	switch (Timebase)
	{
	case ECalibratedMusicTimebase::AudioRenderTime: return AudioRenderDeltaBeatF;
	case ECalibratedMusicTimebase::ExperiencedTime:	return PlayerExperienceDeltaBeatF;
	case ECalibratedMusicTimebase::VideoRenderTime:
	default:                                        return VideoRenderDeltaBeatF;
	}
}

const TArray<FSongSection>& UMusicClockComponent::GetSongSections() const
{
	return GetSongMaps().GetSectionMap().GetSections();
}

float UMusicClockComponent::GetCountInSeconds() const
{
	return GetSongMaps().GetCountInSeconds();
}

float UMusicClockComponent::TickToMs(float Tick) const
{
	return GetSongMaps().TickToMs(Tick);
}

float UMusicClockComponent::BeatToMs(float Beat) const
{
	return GetSongMaps().GetMsAtBeat(Beat);
}

float UMusicClockComponent::GetMsPerBeatAtMs(float Ms) const
{
	return GetSongMaps().GetMsPerBeatAtMs(Ms);
}

float UMusicClockComponent::GetNumBeatsInBarAtMs(float Ms) const
{
	return GetSongMaps().GetNumBeatsInPulseBarAtMs(Ms);
}

float UMusicClockComponent::GetBeatInBarAtMs(float Ms) const
{
	return GetSongMaps().GetBeatInPulseBarAtMs(Ms);
}

float UMusicClockComponent::BarToMs(float Bar) const
{
	if (const FTimeSignature* TimeSigAtBar = GetSongMaps().GetTimeSignatureAtBar(static_cast<int32>(Bar)))
	{
		return BeatToMs(static_cast<float>(TimeSigAtBar->Numerator) * Bar);
	}

	return 0.0f;
}

float UMusicClockComponent::GetMsPerBarAtMs(float Ms) const
{
	return GetSongMaps().GetMsPerBarAtMs(Ms);
}

FString UMusicClockComponent::GetSectionNameAtMs(float Ms) const
{
	return GetSongMaps().GetSectionNameAtMs(Ms);
}

float UMusicClockComponent::GetSectionLengthMsAtMs(float Ms) const
{
	return GetSongMaps().GetSectionLengthMsAtMs(Ms);
}

float UMusicClockComponent::GetSectionStartMsAtMs(float Ms) const
{
	return GetSongMaps().GetSectionStartMsAtMs(Ms);
}

float UMusicClockComponent::GetSectionEndMsAtMs(float Ms) const
{
	return GetSongMaps().GetSectionEndMsAtMs(Ms);
}

float UMusicClockComponent::GetNumSections() const
{
	return GetSongMaps().GetSectionMap().GetNumSections();
}

float UMusicClockComponent::GetSongLengthMs() const
{
	return GetSongMaps().GetSongLengthMs();
}

float UMusicClockComponent::GetSongLengthBeats() const
{
	return GetSongMaps().GetSongLengthBeats();
}

int32 UMusicClockComponent::GetSongLengthBars() const
{
	return GetSongMaps().GetSongLengthBars();
}

float UMusicClockComponent::GetSongRemainingTime(ECalibratedMusicTimebase Timebase /*= ECalibratedMusicTimebase::VideoRenderTime*/)
{
	const float SongLengthMs = GetSongMaps().GetSongLengthMs();
	return SongLengthMs <= 0.f ? 0.f : SongLengthMs - (GetSongPos(Timebase).SecondsIncludingCountIn * 1000.0f);
}

const FSongMaps& UMusicClockComponent::GetSongMaps() const
{
	const FSongMaps* SongMaps = ClockDriver ? ClockDriver->GetCurrentSongMaps() : nullptr;
	return SongMaps ? *SongMaps : DefaultMaps;
}

const FMidiSongPos& UMusicClockComponent::GetSongPos(ECalibratedMusicTimebase Timebase)
{
	UpdateClockIfNeeded();

	switch (Timebase)
	{
	case ECalibratedMusicTimebase::AudioRenderTime:
		return CurrentSmoothedAudioRenderSongPos;
	case ECalibratedMusicTimebase::ExperiencedTime:
		return CurrentPlayerExperiencedSongPos;
	case ECalibratedMusicTimebase::VideoRenderTime:
	default:
		return CurrentVideoRenderSongPos;
	}
}

FMidiSongPos UMusicClockComponent::GetCurrentSmoothedAudioRenderSongPos()
{
	return GetSongPos(ECalibratedMusicTimebase::AudioRenderTime);
}

FMidiSongPos UMusicClockComponent::GetCurrentVideoRenderSongPos()
{
	return GetSongPos(ECalibratedMusicTimebase::VideoRenderTime);
}

FMidiSongPos UMusicClockComponent::GetCurrentPlayerExperiencedSongPos()
{
	return GetSongPos(ECalibratedMusicTimebase::ExperiencedTime);
}

float UMusicClockComponent::MeasureSpanProgress(const FMusicalTimeSpan& Span, ECalibratedMusicTimebase Timebase)
{
	UpdateClockIfNeeded();

	const FSongMaps* Maps = ClockDriver ? ClockDriver->GetCurrentSongMaps() : &DefaultMaps;
	if (!Maps)
	{
		return 0.0f;
	}

	return Span.CalcPositionInSpan(CurrentSmoothedAudioRenderSongPos, *Maps);
}

void UMusicClockComponent::CalcDelta()
{
	AudioRenderDeltaBarF = CurrentSmoothedAudioRenderSongPos.BarsIncludingCountIn - PrevAudioRenderSongPos.BarsIncludingCountIn;
	AudioRenderDeltaBeatF = CurrentSmoothedAudioRenderSongPos.BeatsIncludingCountIn - PrevAudioRenderSongPos.BeatsIncludingCountIn;
	PlayerExperienceDeltaBarF = CurrentPlayerExperiencedSongPos.BarsIncludingCountIn - PrevPlayerExperiencedSongPos.BarsIncludingCountIn;
	PlayerExperienceDeltaBeatF = CurrentPlayerExperiencedSongPos.BeatsIncludingCountIn - PrevPlayerExperiencedSongPos.BeatsIncludingCountIn;
	VideoRenderDeltaBarF = CurrentVideoRenderSongPos.BarsIncludingCountIn - PrevVideoRenderSongPos.BarsIncludingCountIn;
	VideoRenderDeltaBeatF = CurrentVideoRenderSongPos.BeatsIncludingCountIn - PrevVideoRenderSongPos.BeatsIncludingCountIn;
}

void UMusicClockComponent::Broadcast()
{
	const FMidiSongPos* Basis = &CurrentVideoRenderSongPos;
	switch (TimebaseForBarAndBeatEvents)
	{
	case ECalibratedMusicTimebase::AudioRenderTime:
		Basis = &CurrentSmoothedAudioRenderSongPos;
		break;
	case ECalibratedMusicTimebase::ExperiencedTime:
		Basis = &CurrentPlayerExperiencedSongPos;
		break;
	}
	int32 CurrBar = FMath::FloorToInt32(Basis->BarsIncludingCountIn);
	if (LastBroadcastBar != CurrBar)
	{
		BarEvent.Broadcast(Basis->Timestamp.Bar);
		LastBroadcastBar = CurrBar;
	}
	int32 CurrBeat = FMath::FloorToInt32(Basis->BeatsIncludingCountIn);
	if (LastBroadcastBeat != CurrBeat)
	{
		BeatEvent.Broadcast(CurrBeat, FMath::FloorToInt32(Basis->Timestamp.Beat));
		LastBroadcastBeat = CurrBeat;
	}
	const FSongSection& SongSection = Basis->CurrentSongSection;
	if (LastBroadcastSongSection.StartTick != SongSection.StartTick || LastBroadcastSongSection.LengthTicks != SongSection.LengthTicks)
	{
		SectionEvent.Broadcast(SongSection.Name, SongSection.StartTick, SongSection.LengthTicks);
		LastBroadcastSongSection = FSongSection(SongSection.Name, SongSection.StartTick, SongSection.LengthTicks);
	}
}

void UMusicClockComponent::MakeDefaultSongMap()
{
	DefaultMaps.EmptyAllMaps();
	DefaultMaps.Init(MidiConstants::kTicksPerQuarterNoteInt);
	DefaultMaps.GetTempoMap().AddTempoInfoPoint(MidiConstants::BPMToMidiTempo(Tempo), 0);
	DefaultMaps.GetBarMap().AddTimeSignatureAtBarIncludingCountIn(0, TimeSignatureNum, TimeSignatureDenom);
}
