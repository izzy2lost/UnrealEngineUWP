// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MetasoundParamHelper.h"
#include "Internationalization/Internationalization.h"
#include "Internationalization/Text.h"
#include "UObject/NameTypes.h"
#include "UObject/ObjectMacros.h"

// TODO: move these into MetasoundParamHelper.h
#define METASOUND_GET_PARAM_METADATA_ADVANCED(NAME) FDataVertexMetadata { NAME##Tooltip, NAME##DisplayName, true }
#define METASOUND_GET_PARAM_NAME_AND_METADATA_ADVANCED(NAME) METASOUND_GET_PARAM_NAME(NAME), METASOUND_GET_PARAM_METADATA_ADVANCED(NAME)

namespace HarmonixMetasound
{
	HARMONIXMETASOUND_API extern const FName HarmonixNodeNamespace;

	namespace MetasoundNodeCategories
	{
		HARMONIXMETASOUND_API extern const FText Harmonix;
		HARMONIXMETASOUND_API extern const FText Modulation;
	}

#define EXTERN_METASOUND_PARAM(NAME)      \
	HARMONIXMETASOUND_API extern const TCHAR* NAME##Name;       \
	HARMONIXMETASOUND_API extern const FText NAME##Tooltip;     \
	HARMONIXMETASOUND_API extern const FText NAME##DisplayName; 

#if WITH_EDITOR
#define LOC_DEFINE_REGION
#define EXTERN_METASOUND_PARAM_D(NAME, NAME_TEXT, TOOLTIP_TEXT)              \
	const TCHAR* NAME##Name = TEXT(NAME_TEXT);                               \
	const FText NAME##Tooltip = LOCTEXT(#NAME "Tooltip", TOOLTIP_TEXT);      \
	const FText NAME##DisplayName = LOCTEXT(#NAME "DisplayName", NAME_TEXT); 
#undef LOC_DEFINE_REGION
#else 
#define EXTERN_METASOUND_PARAM_D(NAME, NAME_TEXT, TOOLTIP_TEXT) \
	const TCHAR* NAME##Name = TEXT(NAME_TEXT);                  \
	const FText NAME##Tooltip = FText::GetEmpty();              \
	const FText NAME##DisplayName = FText::GetEmpty();
#endif // WITH_EDITOR

// Alias an existing param definition. Helpful for re-using definitions while providing a public node API.
#define EXTERN_METASOUND_PARAM_ALIAS(ALIAS_NAME, TARGET_NAME) \
	const TCHAR* ALIAS_NAME##Name = TARGET_NAME##Name; \
	const FText ALIAS_NAME##Tooltip = TARGET_NAME##Tooltip; \
	const FText ALIAS_NAME##DisplayName = TARGET_NAME##DisplayName; 
	
	namespace CommonPinNames
	{
		namespace Inputs
		{
			EXTERN_METASOUND_PARAM(Enable)
			EXTERN_METASOUND_PARAM(MidiStream)
			EXTERN_METASOUND_PARAM(MidiChannelNumber)
			EXTERN_METASOUND_PARAM(MidiChannelFilterSpecifier)
			EXTERN_METASOUND_PARAM(MidiTrackNumber)
			EXTERN_METASOUND_PARAM(MidiTrackIndexFilterSpecifier)
			EXTERN_METASOUND_PARAM(MinMidiNote)
			EXTERN_METASOUND_PARAM(MaxMidiNote)
			EXTERN_METASOUND_PARAM(MinMidiVelocity)
			EXTERN_METASOUND_PARAM(MaxMidiVelocity)
			EXTERN_METASOUND_PARAM(MidiFileAsset)
			EXTERN_METASOUND_PARAM(Transport)
			EXTERN_METASOUND_PARAM(MidiClock)
			EXTERN_METASOUND_PARAM(Tempo)
			EXTERN_METASOUND_PARAM(TimeSigNumerator)
			EXTERN_METASOUND_PARAM(TimeSigDenominator)
			EXTERN_METASOUND_PARAM(Speed)
			EXTERN_METASOUND_PARAM(Loop)
			EXTERN_METASOUND_PARAM(LoopLengthBars)
			EXTERN_METASOUND_PARAM(TransportPrepare)
			EXTERN_METASOUND_PARAM(TransportPlay)
			EXTERN_METASOUND_PARAM(TransportPause)
			EXTERN_METASOUND_PARAM(TransportContinue)
			EXTERN_METASOUND_PARAM(TransportStop)
			EXTERN_METASOUND_PARAM(TransportKill)
			EXTERN_METASOUND_PARAM(TriggerSeek)
			EXTERN_METASOUND_PARAM(SeekDestination)
			EXTERN_METASOUND_PARAM(PrerollBars)
			EXTERN_METASOUND_PARAM(GridSizeUnits)
			EXTERN_METASOUND_PARAM(GridSizeMult)
			EXTERN_METASOUND_PARAM(OffsetUnits)
			EXTERN_METASOUND_PARAM(OffsetMult)
			EXTERN_METASOUND_PARAM(Bar)
			EXTERN_METASOUND_PARAM(FloatBeat)
			EXTERN_METASOUND_PARAM(SynthPatch)
			EXTERN_METASOUND_PARAM(Transposition)
			EXTERN_METASOUND_PARAM(ClockSpeedToPitch)
			EXTERN_METASOUND_PARAM(ClockSpeedToFrequency)
			EXTERN_METASOUND_PARAM(LFOSyncType);
			EXTERN_METASOUND_PARAM(LFOFrequency);
			EXTERN_METASOUND_PARAM(LFOInvert);
			EXTERN_METASOUND_PARAM(LFOShape);
			EXTERN_METASOUND_PARAM(AudioMono);
		}
		namespace Outputs
		{
			EXTERN_METASOUND_PARAM(Transport)
			EXTERN_METASOUND_PARAM(MidiClock)
			EXTERN_METASOUND_PARAM(MidiFileAsset)
			EXTERN_METASOUND_PARAM(MidiStream)
			EXTERN_METASOUND_PARAM(NoteOn)
			EXTERN_METASOUND_PARAM(NoteOff)
			EXTERN_METASOUND_PARAM(MidiNoteNumber)
			EXTERN_METASOUND_PARAM(Frequency)
			EXTERN_METASOUND_PARAM(MidiVelocity)
			EXTERN_METASOUND_PARAM(NormalizedVelocity)
			EXTERN_METASOUND_PARAM(TransportPrepare)
			EXTERN_METASOUND_PARAM(TransportPlay)
			EXTERN_METASOUND_PARAM(TransportPause)
			EXTERN_METASOUND_PARAM(TransportContinue)
			EXTERN_METASOUND_PARAM(TransportStop)
			EXTERN_METASOUND_PARAM(TransportKill)
			EXTERN_METASOUND_PARAM(MusicTimestamp)
			EXTERN_METASOUND_PARAM(SeekTarget)
			EXTERN_METASOUND_PARAM(AudioMono)
			EXTERN_METASOUND_PARAM(AudioLeft)
			EXTERN_METASOUND_PARAM(AudioRight)
			EXTERN_METASOUND_PARAM(AudioCenter)
			EXTERN_METASOUND_PARAM(AudioLFE)
			EXTERN_METASOUND_PARAM(AudioLeftBack)
			EXTERN_METASOUND_PARAM(AudioRightBack)
			EXTERN_METASOUND_PARAM(AudioLeftSide)
			EXTERN_METASOUND_PARAM(AudioRightSide)
			EXTERN_METASOUND_PARAM(MusicTimespanBar)
			EXTERN_METASOUND_PARAM(MusicTimespanBeat)
			EXTERN_METASOUND_PARAM(Tempo)
			EXTERN_METASOUND_PARAM(Speed)
			EXTERN_METASOUND_PARAM(TimeSigNumerator)
			EXTERN_METASOUND_PARAM(TimeSigDenominator)
			EXTERN_METASOUND_PARAM(SecsPerQuarter)
			EXTERN_METASOUND_PARAM(SecsPerBeat)
		}
	}
}
