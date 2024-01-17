// Copyright Epic Games, Inc. All Rights Reserved.
#include "HarmonixMidi/MidiConstants.h"

DEFINE_LOG_CATEGORY(LogMidi);

FString MidiConstants::GetTextTypeName(uint8 TextType)
{
	switch (TextType)
	{
	case kMeta_Text: return TEXT("Text");
	case kMeta_Copyright: return TEXT("Copyright");
	case kMeta_TrackName: return TEXT("Track Name");
	case kMeta_InstrumentName: return TEXT("Instrument Name");
	case kMeta_Lyric: return TEXT("Lyric");
	case kMeta_Marker: return TEXT("Marker");
	case kMeta_CuePoint: return TEXT("Cue Point");
	default:
		checkNoEntry();
		break;
	}
	return TEXT("");
}

float MidiConstants::RoundToStandardBeatPrecision(float InBeat, int TimeSignatureDenominator)
{
	// this is the precision to which the "rounded" elapsed beats get presented.
	const double kQuarterNotePrecision = 0.01 * ((double)TimeSignatureDenominator / 4.0); // account for beat being a quarter note, eighth note, etc.
	double x = (double)InBeat;
	x += 0.5 * kQuarterNotePrecision;
	x *= 1.0 / kQuarterNotePrecision;
	x = FMath::Floor(x);
	x *= kQuarterNotePrecision;
	return x;
}

FString MidiConstants::GetControllerName(EControllerID ControllerId)
{
	switch (ControllerId)
	{
	case EControllerID::BankSelection:       return FString::Format(TEXT("({0}) BankSelection"), { ControllerId });
	case EControllerID::ModWheel:			 return FString::Format(TEXT("({0}) ModWheel"), { ControllerId });
	case EControllerID::Breath:				 return FString::Format(TEXT("({0}) Breath"), { ControllerId });
	case EControllerID::PortamentoTime:		 return FString::Format(TEXT("({0}) PortamentoTime"), { ControllerId });
	case EControllerID::DataCoarse:			 return FString::Format(TEXT("({0}) DataCoarse"), { ControllerId });
	case EControllerID::Volume:				 return FString::Format(TEXT("({0}) Volume"), { ControllerId });
	case EControllerID::Balance:			 return FString::Format(TEXT("({0}) Balance"), { ControllerId });
	case EControllerID::PanRight:			 return FString::Format(TEXT("({0}) Pan (0 = left, 1 = right)"), { ControllerId });
	case EControllerID::Expression:			 return FString::Format(TEXT("({0}) Expression"), { ControllerId });
	case EControllerID::BitCrushWetMix:		 return FString::Format(TEXT("({0}) BitCrushWetMix"), { ControllerId });
	case EControllerID::BitCrushLevel:		 return FString::Format(TEXT("({0}) BitCrushLevel"), { ControllerId });
	case EControllerID::BitCrushSampleHold:	 return FString::Format(TEXT("({0}) BitCrushSampleHold"), { ControllerId });
	case EControllerID::LFO0Frequency:		 return FString::Format(TEXT("({0}) LFO0Frequency"), { ControllerId });
	case EControllerID::LFO1Frequency:		 return FString::Format(TEXT("({0}) LFO1Frequency"), { ControllerId });
	case EControllerID::LFO0Depth:			 return FString::Format(TEXT("({0}) LFO0Depth"), { ControllerId });
	case EControllerID::LFO1Depth:			 return FString::Format(TEXT("({0}) LFO1Depth"), { ControllerId });
	case EControllerID::DelayTime:			 return FString::Format(TEXT("({0}) DelayTime"), { ControllerId });
	case EControllerID::DelayDryGain:		 return FString::Format(TEXT("({0}) DelayDryGain"), { ControllerId });
	case EControllerID::DelayWetGain:		 return FString::Format(TEXT("({0}) DelayWetGain"), { ControllerId });
	case EControllerID::DelayFeedback:		 return FString::Format(TEXT("({0}) DelayFeedback"), { ControllerId });
	case EControllerID::CoarsePitchBend:	 return FString::Format(TEXT("({0}) CoarsePitchBend"), { ControllerId });
	case EControllerID::SampleStartTime:	 return FString::Format(TEXT("({0}) SampleStartTime"), { ControllerId });
	case EControllerID::DataFine:			 return FString::Format(TEXT("({0}) DataFine"), { ControllerId });
	case EControllerID::SubStreamVol1:		 return FString::Format(TEXT("({0}) SubStreamVol1"), { ControllerId });
	case EControllerID::SubStreamVol2:		 return FString::Format(TEXT("({0}) SubStreamVol2"), { ControllerId });
	case EControllerID::SubStreamVol3:		 return FString::Format(TEXT("({0}) SubStreamVol3"), { ControllerId });
	case EControllerID::SubStreamVol4:		 return FString::Format(TEXT("({0}) SubStreamVol4"), { ControllerId });
	case EControllerID::SubStreamVol5:		 return FString::Format(TEXT("({0}) SubStreamVol5"), { ControllerId });
	case EControllerID::SubStreamVol6:		 return FString::Format(TEXT("({0}) SubStreamVol6"), { ControllerId });
	case EControllerID::SubStreamVol7:		 return FString::Format(TEXT("({0}) SubStreamVol7"), { ControllerId });
	case EControllerID::SubStreamVol8:		 return FString::Format(TEXT("({0}) SubStreamVol8"), { ControllerId });
	case EControllerID::DelayEQEnabled:		 return FString::Format(TEXT("({0}) DelayEQEnabled"), { ControllerId });
	case EControllerID::DelayEQType:		 return FString::Format(TEXT("({0}) DelayEQType"), { ControllerId });
	case EControllerID::DelayEQFreq:		 return FString::Format(TEXT("({0}) DelayEQFreq"), { ControllerId });
	case EControllerID::DelayEQQ:			 return FString::Format(TEXT("({0}) DelayEQQ"), { ControllerId });
	case EControllerID::Hold:				 return FString::Format(TEXT("({0}) Hold"), { ControllerId });
	case EControllerID::PortamentoSwitch:	 return FString::Format(TEXT("({0}) PortamentoSwitch"), { ControllerId });
	case EControllerID::Sustenuto:			 return FString::Format(TEXT("({0}) Sustenuto"), { ControllerId });
	case EControllerID::SoftPedal:			 return FString::Format(TEXT("({0}) SoftPedal"), { ControllerId });
	case EControllerID::Legato:				 return FString::Format(TEXT("({0}) Legato"), { ControllerId });
	case EControllerID::Hold2:				 return FString::Format(TEXT("({0}) Hold2"), { ControllerId });
	case EControllerID::FilterQ:			 return FString::Format(TEXT("({0}) FilterQ"), { ControllerId });
	case EControllerID::Release:			 return FString::Format(TEXT("({0}) Release"), { ControllerId });
	case EControllerID::Attack:				 return FString::Format(TEXT("({0}) Attack"), { ControllerId });
	case EControllerID::FilterFrequency:	 return FString::Format(TEXT("({0}) FilterFrequency"), { ControllerId });
	case EControllerID::TimeStretchEnvelopeOrder:		 
		return FString::Format(TEXT("({0}) TimeStretchEnvelopeOrder"), { ControllerId });
	case EControllerID::DelayLFOBeatSync:	 return FString::Format(TEXT("({0}) DelayLFOBeatSync"), { ControllerId });
	case EControllerID::DelayLFOEnabled:	 return FString::Format(TEXT("({0}) DelayLFOEnabled"), { ControllerId });
	case EControllerID::DelayLFORate:		 return FString::Format(TEXT("({0}) DelayLFORate"), { ControllerId });
	case EControllerID::DelayLFODepth:		 return FString::Format(TEXT("({0}) DelayLFODepth"), { ControllerId });
	case EControllerID::DelayStereoType:	 return FString::Format(TEXT("({0}) DelayStereoType"), { ControllerId });
	case EControllerID::DelayPanLeft:		 return FString::Format(TEXT("({0}) DelayPanLeft"), { ControllerId });
	case EControllerID::DelayPanRight:		 return FString::Format(TEXT("({0}) DelayPanRight"), { ControllerId });
	case EControllerID::RPNFine:			 return FString::Format(TEXT("({0}) RPNFine"), { ControllerId });
	case EControllerID::RPNCourse:			 return FString::Format(TEXT("({0}) RPNCourse"), { ControllerId });
	case EControllerID::AllSoundOff:		 return FString::Format(TEXT("({0}) AllSoundOff"), { ControllerId });
	case EControllerID::Reset:				 return FString::Format(TEXT("({0}) Reset"), { ControllerId });
	case EControllerID::AllNotesOff:		 return FString::Format(TEXT("({0}) AllNotesOff"), { ControllerId });
	}
	return FString::Format(TEXT("({0}) - <unassigned>"), { ControllerId });
}

void MidiConstants::GetControllerNames(TArray<FString>& Names)
{
	for (int32 i = 0; i < 128; ++i)
	{
		Names.Add(GetControllerName((EControllerID)i));
	}
}

const int8 MidiConstants::GetNoteNumberFromNoteName(const char* InName)
{
	if (isdigit(*InName))
		return (int8)atoi(InName);

	char Name[6];
	size_t Index = 0;
	for (; Index < 5 && Index < strlen(InName); Index++)
		Name[Index] = (char)toupper(InName[Index]);
	Name[Index] = 0;
	char* Walk = Name;

	int32 Note = -1;
	switch (*Walk)
	{
	case 'C': Note = 0;  break;
	case 'D': Note = 2;  break;
	case 'E': Note = 4;  break;
	case 'F': Note = 5;  break;
	case 'G': Note = 7;  break;
	case 'A': Note = 9;  break;
	case 'B': Note = 11; break;
	}
	if (Note == -1)
		return 0;
	Walk++;
	if (*Walk == '#') 
	{
		Note++; 
		Walk++; 
	}
	else if (*Walk == 'B')
	{
		Note--;
		Walk++; 
	}

	int32 Octave = 0;
	if (*Walk != '-')
		Octave = atoi(Walk) + 1;

	return int8(Note + (Octave * kNotesPerOctave));
}

const char* MidiConstants::GetNoteNameFromNoteNumber(uint8 MidiNoteNumber, ENoteNameEnharmonicStyle Style)
{
	typedef char const* CStr;
	static CStr sSharpAndFlat[kNotesPerOctave];
	static CStr sSharp[kNotesPerOctave];
	static CStr sFlat[kNotesPerOctave];

	sSharpAndFlat[0] = "C";
	sSharpAndFlat[1] = "C#/Db";
	sSharpAndFlat[2] = "D";
	sSharpAndFlat[3] = "D#/Eb";
	sSharpAndFlat[4] = "E";
	sSharpAndFlat[5] = "F";
	sSharpAndFlat[6] = "F#/Gb";
	sSharpAndFlat[7] = "G";
	sSharpAndFlat[8] = "G#/Ab";
	sSharpAndFlat[9] = "A";
	sSharpAndFlat[10] = "A#/Bb";
	sSharpAndFlat[11] = "B";

	sSharp[0] = "C";
	sSharp[1] = "C#";
	sSharp[2] = "D";
	sSharp[3] = "D#";
	sSharp[4] = "E";
	sSharp[5] = "F";
	sSharp[6] = "F#";
	sSharp[7] = "G";
	sSharp[8] = "G#";
	sSharp[9] = "A";
	sSharp[10] = "A#";
	sSharp[11] = "B";

	sFlat[0] = "C";
	sFlat[1] = "Db";
	sFlat[2] = "D";
	sFlat[3] = "Eb";
	sFlat[4] = "E";
	sFlat[5] = "F";
	sFlat[6] = "Gb";
	sFlat[7] = "G";
	sFlat[8] = "Ab";
	sFlat[9] = "A";
	sFlat[10] = "Bb";
	sFlat[11] = "B";

	CStr const* NameTable = nullptr;
	switch (Style)
	{
	case ENoteNameEnharmonicStyle::Flat:
		NameTable = sFlat;
		break;

	case ENoteNameEnharmonicStyle::Sharp:
		NameTable = sSharp;
		break;
	default:
	case ENoteNameEnharmonicStyle::SharpAndFlat:
		NameTable = sSharpAndFlat;
		break;
	}

	check(NameTable != nullptr);
	uint8 Index = MidiNoteNumber % kNotesPerOctave;
	return NameTable[Index];
}

int8 MidiConstants::GetNoteOctaveFromNoteNumber(uint8 MidiNoteNumber)
{
	return (int8)MidiNoteNumber / kNotesPerOctave - 1;
}
