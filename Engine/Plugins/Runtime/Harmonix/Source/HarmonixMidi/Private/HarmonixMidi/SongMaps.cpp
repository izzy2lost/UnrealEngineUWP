// Copyright Epic Games, Inc. All Rights Reserved.
#include "HarmonixMidi/SongMaps.h"
#include "HarmonixMidi/SongMapReceiver.h"
#include "HarmonixMidi/MidiFile.h"
#include "HarmonixMidi/MidiReader.h"
#include "Misc/RuntimeErrors.h"

FSongMaps::FSongMaps()
{
}

bool FSongMaps::operator==(const FSongMaps& Other) const
{
	if (TrackNames.Num() != Other.TrackNames.Num())
	{
		return false;
	}
	for (int32 TrackIndex = 0; TrackIndex < TrackNames.Num(); ++TrackIndex)
	{
		if (TrackNames[TrackIndex] != Other.TrackNames[TrackIndex])
		{
			return false;
		}
	}
	return	TicksPerQuarterNote == Other.TicksPerQuarterNote &&
		TempoMap == Other.TempoMap &&
		BarMap == Other.BarMap &&
		BeatMap == Other.BeatMap &&
		SectionMap == Other.SectionMap &&
		ChordMap == Other.ChordMap &&
		LengthData == Other.LengthData;
}

void FSongMaps::Init(int32 InTicksPerQuarterNote)
{
	TicksPerQuarterNote = InTicksPerQuarterNote;
	TempoMap.SetTicksPerQuarterNote(InTicksPerQuarterNote);
	TempoMap.Empty();
	BarMap.SetTicksPerQuarterNote(InTicksPerQuarterNote);
	BarMap.Empty();
	BeatMap.SetTicksPerQuarterNote(InTicksPerQuarterNote);
	BeatMap.Empty();
	SectionMap.SetTicksPerQuarterNote(InTicksPerQuarterNote);
	SectionMap.Empty();
	ChordMap.SetTicksPerQuarterNote(InTicksPerQuarterNote);
	ChordMap.Empty();
}

void FSongMaps::Copy(const FSongMaps& Other, int32 StartTick, int32 EndTick)
{
	EmptyAllMaps();

	TicksPerQuarterNote = Other.TicksPerQuarterNote;

	TempoMap.Copy(Other.GetTempoMap(), StartTick, EndTick);
	BarMap.Copy(Other.GetBarMap(), StartTick, EndTick);
	BeatMap.Copy(Other.GetBeatMap(), StartTick, EndTick);
	SectionMap.Copy(Other.GetSectionMap(), StartTick, EndTick);
	ChordMap.Copy(Other.GetChordMap(), StartTick, EndTick);

	int32 LastTick = EndTick == -1 ? Other.LengthData.LastTick : EndTick;
	TempoMap.Finalize(LastTick);
	BarMap.Finalize(LastTick);
	BeatMap.Finalize(LastTick);
	SectionMap.Finalize(LastTick);
	ChordMap.Finalize(LastTick);

	memset(&LengthData, 0, sizeof(LengthData));
	LengthData.LastTick = LastTick;
	LengthData.LengthTicks = LengthData.LastTick + 1;
	FMusicTimestamp Timestamp;
	Timestamp = BarMap.TickToMusicTimestamp(float(LengthData.LastTick));
	LengthData.LengthBars = FMath::IsNearlyZero(Timestamp.Beat-1.0f,1.0E-4) ? Timestamp.Bar - 1 : Timestamp.Bar;
	if (LengthData.LengthBars == Timestamp.Bar)
	{
		LengthData.LengthTicks = int32(BarMap.MusicTimestampToTick({LengthData.LengthBars + BarMap.GetStartBar(), 1.0f}));
		LengthData.LastTick = LengthData.LengthTicks - 1;
	}
}

bool FSongMaps::LoadFromStdMidiFile(const FString& FilePath)
{
	FSongMapReceiver MapReceiver(this);
	FStdMidiFileReader Reader(FilePath, &MapReceiver);
	return ReadWithReader(Reader);
}

bool FSongMaps::LoadFromStdMidiFile(void* Buffer, int32 BufferSize, const FString& Filename)
{
	FSongMapReceiver MapReceiver(this);
	FStdMidiFileReader Reader(Buffer, BufferSize, Filename, &MapReceiver);
	return ReadWithReader(Reader);
}

bool FSongMaps::LoadFromStdMidiFile(TSharedPtr<FArchive> Archive, const FString& Filename)
{
	FSongMapReceiver MapReceiver(this);
	FStdMidiFileReader Reader(Archive, Filename, &MapReceiver);
	return ReadWithReader(Reader);
}

float FSongMaps::TickToMs(float Tick) const
{
	if (TempoMap.IsEmpty())
	{
		UE_LOG(LogMIDI, Log, TEXT("SongMaps does not contain a Tempo Map."));
		return 0.0f;
	}
	return TempoMap.TickToMs(Tick);
}

float FSongMaps::MsToTick(float Ms) const
{
	if (TempoMap.IsEmpty())
	{
		UE_LOG(LogMIDI, Log, TEXT("SongMaps does not contain a Tempo Map."));
		return 0;
	}
	return TempoMap.MsToTick(Ms);
}

float FSongMaps::GetCountInSeconds() const
{
	float BarOneBeatOneTick = BarMap.MusicTimestampToTick({ 1, 1.0f });
	return TempoMap.TickToMs(BarOneBeatOneTick) / 1000.f;
}

bool FSongMaps::FinalizeRead(IMidiReader* Reader)
{
	int32 LastTick = Reader->GetLastTick();

	if (Reader->IsFailed())
	{
		EmptyAllMaps();
		return false;
	}

	TempoMap.Finalize(LastTick);
	BarMap.Finalize(LastTick);
	BeatMap.Finalize(LastTick);
	SectionMap.Finalize(LastTick);
	ChordMap.Finalize(LastTick);

	memset(&LengthData, 0, sizeof(LengthData));
	LengthData.LastTick = LastTick;
	LengthData.LengthTicks = LengthData.LastTick + 1;
	FMusicTimestamp Timestamp;
	Timestamp = BarMap.TickToMusicTimestamp(float(LengthData.LastTick));
	LengthData.LengthBars = FMath::IsNearlyZero(Timestamp.Beat - 1.0f, 1.0E-4) ? Timestamp.Bar - 1 : Timestamp.Bar;
	if (LengthData.LengthBars == Timestamp.Bar)
	{
		LengthData.LengthTicks = int32(BarMap.MusicTimestampToTick({ LengthData.LengthBars + BarMap.GetStartBar(), 1.0f }));
		LengthData.LastTick = LengthData.LengthTicks - 1;
	}
	return true;
}

bool FSongMaps::ReadWithReader(FStdMidiFileReader& Reader)
{
	Reader.ReadAllTracks();
	return FinalizeRead(&Reader);
}

FArchive& operator<<(FArchive& Archive, FSongLengthData& LengthData)
{
	Archive << LengthData.LengthTicks;  
	Archive << LengthData.LengthBars;   
	Archive << LengthData.LastTick;
	return Archive;
} 

void FSongMaps::EmptyAllMaps()
{
	TempoMap.Empty();
	BarMap.Empty();
	BeatMap.Empty();
	SectionMap.Empty();
	ChordMap.Empty();
	TrackNames.Empty();
	LengthData.LastTick = 0;
	LengthData.LengthBars = 0;
	LengthData.LengthTicks = 0;
}

bool FSongMaps::IsEmpty() const
{
	return 	TempoMap.IsEmpty() &&
			BarMap.IsEmpty() &&
			BeatMap.IsEmpty() &&
			SectionMap.IsEmpty() &&
			ChordMap.IsEmpty() &&
			TrackNames.IsEmpty() &&
			LengthData.LastTick == 0 &&
			LengthData.LengthBars == 0 &&
			LengthData.LengthTicks == 0;
}

float FSongMaps::GetSongLengthMs() const
{
	return TickToMs(float(GetSongLengthData().LengthTicks));
}

int32 FSongMaps::GetSongLengthBeats() const
{
	return BeatMap.GetNumMapPoints();
}

int32 FSongMaps::GetSongLengthBars() const
{
	return GetSongLengthData().LengthBars;
}

void FSongMaps::StringLengthToMT(const FString& LengthString, int32& OutBars, int32& OutTicks)
{
	if (BarMap.IsEmpty())
	{
		UE_LOG(LogMIDI, Log, TEXT("SongMaps does not contain a Bar Map."));
		return;
	}

	// ASSUMES WE DON'T HAVE A COMPLICATED TIME SIGNATURE.
	// IF WE DID, THEN WE WOULD NEVER GET HERE, AS WE WOULD
	// HAVE DETERMINED THE LENGTH FROM THE MIDI FILE!
	
	// ALSO: LengthString, being a 'length', is specifying 0 based bars and beats!
	//       And, the return values are lengths, so also 0 based.
	const TCHAR* Walk = *LengthString;
	OutBars = 0;
	int32 Beats = 0;
	OutTicks = 0;

	// get bars...
	int32 Count = 0;
	while (*Walk != ':' && *Walk != 0 && Count < 3)
	{
		OutBars = (OutBars * 10) + ((int32)(*Walk) - (int32)'0');
		Walk++;
		Count++;
	}
	if (*Walk == ':')
	{
		// get beats...
		Count = 0;
		Walk++;
		while (*Walk != ':' && *Walk != 0 && Count < 3)
		{
			Beats = (Beats * 10) + ((int32)(*Walk) - (int32)'0');
			Walk++;
			Count++;
		}
		if (*Walk == ':')
		{
			// get ticks...
			Count = 0;
			Walk++;
			while (*Walk != ':' && *Walk != 0 && Count < 3)
			{
				OutTicks = (OutTicks * 10) + ((int32)(*Walk) - (int32)'0');
				Walk++;
				Count++;
			}
		}
	}
	const FTimeSignaturePoint* TimeSignaturePoint = &BarMap.GetTimeSignaturePoint(0);
	if (!TimeSignaturePoint)
	{
		UE_LOG(LogMIDI, Log, TEXT("No Time Signature found in SongMaps."));
		return;
	}
	OutBars += (Beats == 0 && OutTicks == 0) ? 0 : 1;
	int32 TicksPerBeat   = TicksPerQuarterNote / (TimeSignaturePoint->TimeSignature.Denominator / 4);
	int32 TicksPerBar = TicksPerBeat * TimeSignaturePoint->TimeSignature.Numerator;
	OutTicks = (TicksPerBar * OutBars)
		+ (TicksPerBeat * Beats)
		+ OutTicks;
}

FString FSongMaps::GetTrackName(int32 Index) const
{
	if (Index < 0 || Index >= TrackNames.Num())
		return FString();

	return TrackNames[Index];
}

///////////////////////////////////////////////////////////////////////////////////
// TEMPO
const FTempoInfoPoint* FSongMaps::GetTempoInfoForMs(float Ms) const
{
	if (TempoMap.IsEmpty())
	{
		UE_LOG(LogMIDI, Log, TEXT("SongMaps does not contain a Tempo Map."));
		return nullptr;
	}
	int32 Tick = int32(MsToTick(Ms));
	return TempoMap.GetTempoPointAtTick(Tick);
}

const FTempoInfoPoint* FSongMaps::GetTempoInfoForTick(int32 Tick) const
{
	if (TempoMap.IsEmpty())
	{
		UE_LOG(LogMIDI, Log, TEXT("SongMaps does not contain a Tempo Map."));
		return nullptr;
	}
	return TempoMap.GetTempoPointAtTick(Tick);
}

float FSongMaps::GetTempoAtMs(float Ms) const
{
	if (TempoMap.IsEmpty())
	{
		UE_LOG(LogMIDI, Log, TEXT("SongMaps does not contain a Tempo Map."));
		return 0.0f;
	}
	int32 Tick = int32(MsToTick(Ms));
	return TempoMap.GetTempoAtTick(Tick);
}

float FSongMaps::GetTempoAtTick(int32 Tick) const
{
	if (TempoMap.IsEmpty())
	{
		UE_LOG(LogMIDI, Log, TEXT("SongMaps does not contain a Tempo Map."));
		return 0.0f;
	}
	return TempoMap.GetTempoAtTick(Tick);
}

///////////////////////////////////////////////////////////////////////////////////
// BEAT
const FBeatMapPoint* FSongMaps::GetBeatAtMs(float Ms) const
{
	if (BeatMap.IsEmpty())
	{
		UE_LOG(LogMIDI, Log, TEXT("SongMaps does not contain a Beat Map."));
		return nullptr;
	}
	int32 Tick = int32(MsToTick(Ms));
	return BeatMap.GetPointInfoForTick(Tick);
}

float FSongMaps::GetMsAtBeat(float Beat) const
{
	if (!BeatMap.IsEmpty() && Beat < BeatMap.GetNumMapPoints())
	{
		float Tick = BeatMap.GetFractionalTickAtBeat(Beat);
		return TickToMs(Tick);
	}

	if (BarMap.IsEmpty())
	{
		return 0.0f;
	}

	const FTimeSignaturePoint& Point1 = BarMap.GetTimeSignaturePoint(0);
	if (BarMap.GetNumTimeSignaturePoints() == 1)
	{
		float Tick = Beat * TicksPerQuarterNote / (Point1.TimeSignature.Denominator / 4);
		return TickToMs(Tick);
	}

	const FTimeSignaturePoint* FromPoint = &Point1;
	for (int i = 1; i < BarMap.GetNumTimeSignaturePoints(); ++i)
	{
		const FTimeSignaturePoint& Point2 = BarMap.GetTimeSignaturePoint(i);
		if (Beat < Point2.BeatIndex)
		{
			break;
		}
		FromPoint = &Point2;
	}

	float BeatsAtTimeSignature = Beat - (float)FromPoint->BeatIndex;
	float Bar = (float)FromPoint->BarIndex + (BeatsAtTimeSignature / (float)FromPoint->TimeSignature.Numerator);
	return TickToMs(BarMap.FractionalBarIncludingCountInToTick(Bar));
}

const FBeatMapPoint* FSongMaps::GetBeatAtTick(int32 Tick) const
{
	if (BeatMap.IsEmpty())
	{
		UE_LOG(LogMIDI, Log, TEXT("SongMaps does not contain a Beat Map."));
		return nullptr;
	}
	return BeatMap.GetPointInfoForTick(Tick);
}

float FSongMaps::GetMsPerBeatAtMs(float Ms) const
{
	int32 Tick = int32(MsToTick(Ms));
	return GetMsPerBeatAtTick(Tick);
}

float FSongMaps::GetMsPerBeatAtTick(int32 Tick) const
{
	const FBeatMapPoint* BeatInfo = GetBeatAtTick(Tick);
	if (!BeatInfo)
	{
		// In midi, tempo is always quarter notes per minute. Without a beat map,
		// a beat is the divisor in the time signature, which might not be a 
		// quarter note. So we will convert here...
		float QuarterNotesPerMinute = GetTempoAtTick(Tick);
		const FTimeSignature* TimeSignature = GetTimeSignatureAtTick(Tick);
		float BeatsPerMinute = QuarterNotesPerMinute / ((float)TimeSignature->Denominator / 4.0f /* quarter note / 4 == 1 */);
		return 60000.0f / BeatsPerMinute;
	}
	return TickToMs(float(BeatInfo->StartTick + BeatInfo->LengthTicks)) - TickToMs(float(BeatInfo->StartTick));
}

float FSongMaps::GetFractionalBeatAtMs(float Ms) const
{
	float Tick = MsToTick(Ms);
	return GetFractionalBeatAtTick(Tick);
}

float FSongMaps::GetFractionalBeatAtTick(float Tick) const
{
	int32 BeatIndex = GetBeatIndexAtTick(int32(Tick));
	if (BeatIndex < 0 || BeatMap.IsEmpty())
	{
		return 1.0f; // 1 based position!
	}

	const FBeatMapPoint* BeatInfo = &BeatMap.GetBeatPointInfo(BeatIndex);
	check(BeatInfo);
	float TickInBeat = Tick - BeatInfo->StartTick;
	float FractionalPart = TickInBeat / BeatInfo->LengthTicks;
	return BeatIndex + FractionalPart + 1.0f; // +1 for musical position
}

int32 FSongMaps::GetBeatIndexAtMs(float Ms) const
{
	int32 Tick = int32(MsToTick(Ms));
	return GetBeatIndexAtTick(Tick);
}

int32 FSongMaps::GetBeatIndexAtTick(int32 Tick) const
{
	if (BeatMap.IsEmpty())
	{
		UE_LOG(LogMIDI, Log, TEXT("SongMaps does not contain a Beat Map."));
		return -1;
	}
	return BeatMap.GetPointIndexForTick(Tick);
}

EMusicalBeatType FSongMaps::GetBeatTypeAtMs(float Ms) const
{
	int32 Tick = int32(MsToTick(Ms));
	return GetBeatTypeAtTick(Tick);
}

EMusicalBeatType FSongMaps::GetBeatTypeAtTick(int32 Tick) const
{
	if (BeatMap.IsEmpty())
	{
		UE_LOG(LogMIDI, Log, TEXT("SongMaps does not contain a Beat Map."));
		return EMusicalBeatType::Normal;
	}
	return BeatMap.GetBeatTypeAtTick(Tick);
}

float FSongMaps::GetBeatInPulseBarAtMs(float Ms) const
{
	float Tick = MsToTick(Ms);
	return GetBeatInPulseBarAtTick(Tick);
}

float FSongMaps::GetBeatInPulseBarAtTick(float Tick) const
{
	if (BeatMap.IsEmpty())
	{
		UE_LOG(LogMIDI, Log, TEXT("SongMaps does not contain a Beat Map."));
		return 0.0f;
	}
	return BeatMap.GetBeatInPulseBarAtTick(Tick);
}

int32 FSongMaps::GetNumBeatsInPulseBarAtMs(float Ms) const
{
	int32 Tick = int32(MsToTick(Ms));
	return GetNumBeatsInPulseBarAtTick(Tick);
}

int32 FSongMaps::GetNumBeatsInPulseBarAtTick(int32 Tick) const
{
	if (BeatMap.IsEmpty())
	{
		UE_LOG(LogMIDI, Log, TEXT("SongMaps does not contain a Beat Map."));
		return 0;
	}
	return BeatMap.GetNumBeatsInPulseBarAt(Tick);
}

///////////////////////////////////////////////////////////////////////////////////
// Time Signature

const FTimeSignature* FSongMaps::GetTimeSignatureAtMs(float Ms) const
{
	int32 Tick = int32(MsToTick(Ms));
	return GetTimeSignatureAtTick(Tick);
}

const FTimeSignature* FSongMaps::GetTimeSignatureAtTick(int32 Tick) const
{
	if (BarMap.IsEmpty())
	{
		UE_LOG(LogMIDI, Log, TEXT("SongMaps does not contain a Bar Map."));
		return nullptr;
	}
	return &BarMap.GetTimeSignatureAtTick(Tick);
}

const FTimeSignature* FSongMaps::GetTimeSignatureAtBar(int32 Bar) const
{
	if (Bar < 1)
	{
		UE_LOG(LogMIDI, Warning, TEXT("Bar < 1 (%d) specified as a musical position! Bars are '1' based in musical positions. Using bar 1!"), Bar);
	}

	if (BarMap.IsEmpty())
	{
		UE_LOG(LogMIDI, Log, TEXT("SongMaps does not contain a Bar Map."));
		return nullptr;
	}
	return &BarMap.GetTimeSignatureAtBar(Bar);
}

float FSongMaps::GetBarIncludingCountInAtMs(float Ms) const
{
	float Tick = MsToTick(Ms);
	return GetBarIncludingCountInAtTick(Tick);
}

float FSongMaps::GetBarIncludingCountInAtTick(float Tick) const
{
	if (BarMap.IsEmpty())
	{
		UE_LOG(LogMIDI, Log, TEXT("SongMaps does not contain a Bar Map."));
		return 1.0f; // 1 based music positions!
	}
	return BarMap.TickToFractionalBarIncludingCountIn(Tick);
}

float FSongMaps::GetMsPerBarAtMs(float Ms) const
{
	float Tick = MsToTick(Ms);
	return GetMsPerBarAtTick(Tick);
}

float FSongMaps::GetMsPerBarAtTick(float Tick) const
{
	if (TempoMap.IsEmpty())
	{
		UE_LOG(LogMIDI, Log, TEXT("SongMaps does not contain a Tempo Map."));
		return 0.0f;
	}
	if (BarMap.IsEmpty())
	{
		UE_LOG(LogMIDI, Log, TEXT("SongMaps does not contain a Bar Map."));
		return 0.0f;
	}
	float Bpm = TempoMap.GetTempoAtTick(int32(Tick)); // quarter notes per minute
	const FTimeSignature* TimeSignature = GetTimeSignatureAtTick(int32(Tick));
	float QuarterNotesInBar;
	if (TimeSignature)
	{
		QuarterNotesInBar = (float)TimeSignature->Numerator / ((float)TimeSignature->Denominator / 4.0f);
	}
	else
	{
		QuarterNotesInBar = 4.0f;
	}
	float MsPerQuarterNote = 1000.0f / (Bpm / 60.0f);
	return QuarterNotesInBar * MsPerQuarterNote;
}

void FSongMaps::SetLengthTotalBars(int32 Bars)
{
	check(Bars >= 0);
	memset(&LengthData, 0, sizeof(LengthData));
	LengthData.LengthTicks = BarMap.BarIncludingCountInToTick(Bars);
	LengthData.LastTick = LengthData.LengthTicks - 1;
	LengthData.LengthBars = Bars;
}

int32 FSongMaps::CalculateMidiTick(const FMusicTimestamp& Timestamp, const EMidiClockSubdivisionQuantization Quantize) const
{
	int32 TriggerTick = 0;
	if (Quantize == EMidiClockSubdivisionQuantization::None)
	{
		TriggerTick = FMath::RoundToInt32(BarMap.MusicTimestampToTick(Timestamp));
	}
	else
	{
		int32 RawTick = FMath::RoundToInt32(BarMap.MusicTimestampToTick(Timestamp));
		int32 BarTick = BarMap.MusicTimestampBarToTick(Timestamp.Bar);
		int32 TicksPerQuantizationUnit = SubdivisionToMidiTicks(Quantize, RawTick);
		if (ensure(TicksPerQuantizationUnit > 0))
		{
			float NumUnits = ((float)(RawTick - BarTick)) / (float)TicksPerQuantizationUnit;
			int32 NumWholeUnits = FMath::RoundToInt32(NumUnits);
			TriggerTick = BarTick + (NumWholeUnits * TicksPerQuantizationUnit);
		}
		else
		{
			TriggerTick = FMath::RoundToInt32(BarMap.MusicTimestampToTick(Timestamp));
		}
	}
	return TriggerTick;
}

int32 FSongMaps::SubdivisionToMidiTicks(const EMidiClockSubdivisionQuantization Division, const int32 Tick) const
{
	int32 BarMapPointIndex = GetBarMap().GetPointIndexForTick(Tick);
	if (BarMapPointIndex < 0)
	{
		return 1;
	}
	const FTimeSignature* TimeSignature = GetTimeSignatureAtTick(Tick);
	if (!TimeSignature)
	{
		return 1;
	}

	using namespace Harmonix::Midi::Constants;

	switch (Division)
	{
	case EMidiClockSubdivisionQuantization::None:                   return 1;
	case EMidiClockSubdivisionQuantization::Bar: 					return GetBarMap().GetTicksInBarAfterPoint(BarMapPointIndex);
	case EMidiClockSubdivisionQuantization::Beat:					return (GTicksPerQuarterNoteInt * 4) / TimeSignature->Denominator;
	case EMidiClockSubdivisionQuantization::ThirtySecondNote:		return GTicksPerQuarterNoteInt / 8;
	case EMidiClockSubdivisionQuantization::SixteenthNote:			return GTicksPerQuarterNoteInt / 4;
	case EMidiClockSubdivisionQuantization::EighthNote:				return GTicksPerQuarterNoteInt / 2;
	case EMidiClockSubdivisionQuantization::QuarterNote:			return GTicksPerQuarterNoteInt;
	case EMidiClockSubdivisionQuantization::HalfNote:				return GTicksPerQuarterNoteInt * 2;
	case EMidiClockSubdivisionQuantization::WholeNote:				return GTicksPerQuarterNoteInt * 4;
	case EMidiClockSubdivisionQuantization::DottedSixteenthNote:	return (GTicksPerQuarterNoteInt / 4) + (GTicksPerQuarterNoteInt / 8);
	case EMidiClockSubdivisionQuantization::DottedEighthNote:		return (GTicksPerQuarterNoteInt / 2) + (GTicksPerQuarterNoteInt / 4);
	case EMidiClockSubdivisionQuantization::DottedQuarterNote:		return (GTicksPerQuarterNoteInt)+(GTicksPerQuarterNoteInt / 2);
	case EMidiClockSubdivisionQuantization::DottedHalfNote:			return (GTicksPerQuarterNoteInt * 2) + (GTicksPerQuarterNoteInt);
	case EMidiClockSubdivisionQuantization::DottedWholeNote:		return (GTicksPerQuarterNoteInt * 4) + (GTicksPerQuarterNoteInt * 2);
	case EMidiClockSubdivisionQuantization::SixteenthNoteTriplet:   return (GTicksPerQuarterNoteInt / 2) / 3;
	case EMidiClockSubdivisionQuantization::EighthNoteTriplet:		return GTicksPerQuarterNoteInt / 3;
	case EMidiClockSubdivisionQuantization::QuarterNoteTriplet:		return (GTicksPerQuarterNoteInt * 2) / 3;
	case EMidiClockSubdivisionQuantization::HalfNoteTriplet:        return (GTicksPerQuarterNoteInt * 4) / 3;
	default:	                                             		checkNoEntry();	return 1;
	}
}

float FSongMaps::SubdivisionToBeats(EMidiClockSubdivisionQuantization Subdivision, const FTimeSignature& TimeSignature)
{
	// Easy cases first
	if (Subdivision == EMidiClockSubdivisionQuantization::Bar)
	{
		return TimeSignature.Numerator;
	}

	if (Subdivision == EMidiClockSubdivisionQuantization::Beat)
	{
		return 1;
	}

	const float BeatsPerQuarter = TimeSignature.Denominator / 4.0f;

	switch (Subdivision)
	{
	case EMidiClockSubdivisionQuantization::ThirtySecondNote:
		return BeatsPerQuarter / 8;
	case EMidiClockSubdivisionQuantization::SixteenthNote:
		return BeatsPerQuarter / 4;
	case EMidiClockSubdivisionQuantization::EighthNote:
		return BeatsPerQuarter / 2;
	case EMidiClockSubdivisionQuantization::QuarterNote:
		return BeatsPerQuarter;
	case EMidiClockSubdivisionQuantization::HalfNote:
		return BeatsPerQuarter * 2;
	case EMidiClockSubdivisionQuantization::WholeNote:
		return BeatsPerQuarter * 4;
	case EMidiClockSubdivisionQuantization::DottedSixteenthNote:
		return BeatsPerQuarter / 4 + BeatsPerQuarter / 8;
	case EMidiClockSubdivisionQuantization::DottedEighthNote:
		return BeatsPerQuarter / 2 + BeatsPerQuarter / 4;
	case EMidiClockSubdivisionQuantization::DottedQuarterNote:
		return BeatsPerQuarter + BeatsPerQuarter / 2;
	case EMidiClockSubdivisionQuantization::DottedHalfNote:
		return BeatsPerQuarter * 3;
	case EMidiClockSubdivisionQuantization::DottedWholeNote:
		return BeatsPerQuarter * 6;
	case EMidiClockSubdivisionQuantization::SixteenthNoteTriplet:
		return (BeatsPerQuarter / 4) * 2 / 3;
	case EMidiClockSubdivisionQuantization::EighthNoteTriplet:
		return (BeatsPerQuarter / 2) * 2 / 3;
	case EMidiClockSubdivisionQuantization::QuarterNoteTriplet:
		return BeatsPerQuarter * 2 / 3;
	case EMidiClockSubdivisionQuantization::HalfNoteTriplet:
		return BeatsPerQuarter * 4 / 3;
	default:
		checkNoEntry();
		return 0;
	}
}

///////////////////////////////////////////////////////////////////////////////////
// Sections

const FSongSection* FSongMaps::GetSectionAtMs(float Ms) const
{
	int32 Tick = int32(MsToTick(Ms));
	return GetSectionAtTick(Tick);
}

const FSongSection* FSongMaps::GetSectionAtTick(int32 Tick) const
{
	if (SectionMap.IsEmpty())
	{
		UE_LOG(LogMIDI, Log, TEXT("SongMaps does not contain a Section Map."));
		return nullptr;
	}
	return SectionMap.TickToSection(Tick);
}

const FSongSection* FSongMaps::GetSectionWithName(const FString& Name) const
{
	if (SectionMap.IsEmpty())
	{
		UE_LOG(LogMIDI, Log, TEXT("SongMaps does not contain a Section Map."));
		return nullptr;
	}
	return SectionMap.FindSectionInfo(Name);
}

FString FSongMaps::GetSectionNameAtMs(float Ms) const
{
	int32 Tick = int32(MsToTick(Ms));
	return GetSectionNameAtTick(Tick);
}

FString FSongMaps::GetSectionNameAtTick(int32 Tick) const
{
	if (SectionMap.IsEmpty())
	{
		UE_LOG(LogMIDI, Log, TEXT("SongMaps does not contain a Section Map."));
		return FString();
	}
	return SectionMap.GetSectionNameAtTick(Tick);
}

float FSongMaps::GetSectionLengthMsAtMs(float Ms) const
{
	int32 Tick = int32(MsToTick(Ms));
	return GetSectionLengthMsAtTick(Tick);
}

float FSongMaps::GetSectionStartMsAtMs(float Ms) const
{
	const int32 Tick = int32(MsToTick(Ms));
	const FSongSection* SectionAtTick = GetSectionAtTick(Tick);
	if (!SectionAtTick)
	{
		return 0.0f;
	}

	return TickToMs(float(SectionAtTick->StartTick));
}

float FSongMaps::GetSectionEndMsAtMs(float Ms) const
{
	const int32 Tick = int32(MsToTick(Ms));
	const FSongSection* SectionAtTick = GetSectionAtTick(Tick);
	if (!SectionAtTick)
	{
		return 0.0f;
	}

	return TickToMs(float(SectionAtTick->EndTick()));
}

float FSongMaps::GetSectionLengthMsAtTick(int32 Tick) const
{
	const FSongSection* SectionAtTick = GetSectionAtTick(Tick);
	if (!SectionAtTick)
	{
		return 0.0f;
	}

	float StartMs = TickToMs(float(SectionAtTick->StartTick));
	float EndMs = TickToMs(float(SectionAtTick->EndTick()));
	return EndMs - StartMs;
}

///////////////////////////////////////////////////////////////////////////////////
// Chords

const FChordMapPoint* FSongMaps::GetChordAtMs(float Ms) const
{
	int32 Tick = int32(MsToTick(Ms));
	return GetChordAtTick(Tick);
}

const FChordMapPoint* FSongMaps::GetChordAtTick(int32 Tick) const
{
	if (ChordMap.IsEmpty())
	{
		UE_LOG(LogMIDI, Log, TEXT("SongMaps does not contain a Chord Map."));
		return nullptr;
	}
	return ChordMap.GetPointInfoForTick(Tick);
}

FName FSongMaps::GetChordNameAtMs(float Ms) const
{
	int32 Tick = int32(MsToTick(Ms));
	return GetChordNameAtTick(Tick);
}

FName FSongMaps::GetChordNameAtTick(int32 Tick) const
{
	if (ChordMap.IsEmpty())
	{
		UE_LOG(LogMIDI, Log, TEXT("SongMaps does not contain a Chord Map."));
		return FName();
	}
	return ChordMap.GetChordNameAtTick(Tick);
}

float FSongMaps::GetChordLengthMsAtMs(float Ms) const
{
	int32 Tick = int32(MsToTick(Ms));
	return GetChordLengthMsAtTick(Tick);
}

float FSongMaps::GetChordLengthMsAtTick(int32 Tick) const
{
	const FChordMapPoint* ChordInfo = GetChordAtTick(Tick);
	if (!ChordInfo)
	{
		return 0.0f;
	}
	float ChordStartMs = TickToMs(float(ChordInfo->StartTick));
	float ChordEndMs = TickToMs(float(ChordInfo->EndTick()));
	return ChordEndMs - ChordStartMs;
}

