// Copyright Epic Games, Inc. All Rights Reserved.

#include "HarmonixMetasound/Common.h"

#include "MetasoundNodeRegistrationMacro.h"

#define LOCTEXT_NAMESPACE "HarmonixMetaSound"

namespace HarmonixMetasound
{
	const FName HarmonixNodeNamespace = TEXT("HarmonixNodes");

	namespace MetasoundNodeCategories
	{
		const FText Harmonix = METASOUND_LOCTEXT("Metasound_HarmonixCategory", "Harmonix");
		const FText Modulation = METASOUND_LOCTEXT("Metasound_ModulationCategory", "Modulation");
	}
	
	namespace CommonPinNames
	{
		namespace Inputs
		{
			EXTERN_METASOUND_PARAM_D(Enable,                        "Enable",                             "Enables processing.")
			EXTERN_METASOUND_PARAM_D(MidiStream,                    "Midi Stream",                        "Midi event input stream.")
			EXTERN_METASOUND_PARAM_D(MidiChannelNumber,             "Midi Channel",                       "Midi channel to render (0 for all/omni).")
			EXTERN_METASOUND_PARAM_D(MidiChannelFilterSpecifier,    "Midi Channel Filter",                "Midi channels to process. '*' for all, or a string like '1,3,4-8' to identify specific channels.")
			EXTERN_METASOUND_PARAM_D(MidiTrackNumber,               "Track Number",                       "Track number (1 based).")
			EXTERN_METASOUND_PARAM_D(MidiTrackIndexFilterSpecifier, "Midi Track Filter",                  "Midi tracks to process. '*' for all, or a string like '1,3,4-8' to identify specific tracks.")
			EXTERN_METASOUND_PARAM_D(MinMidiNote,                   "Min Note #",                         "Minimum midi note number (0-127).")
			EXTERN_METASOUND_PARAM_D(MaxMidiNote,                   "Max Note #",                         "Maximum midi note number (0-127).")
			EXTERN_METASOUND_PARAM_D(MinMidiVelocity,               "Min Velocity",                       "Minimum Velocity (0-127).")
			EXTERN_METASOUND_PARAM_D(MaxMidiVelocity,               "Max Velocity",                       "Maximum Velocity (0-127).")
			EXTERN_METASOUND_PARAM_D(MidiFileAsset,                 "Midi File",                          "Standard Midi file.")
			EXTERN_METASOUND_PARAM_D(Transport,                     "Transport",                          "Play, pause, continue, stop, etc.")
			EXTERN_METASOUND_PARAM_D(MidiClock,                     "Midi Clock",                         "Midi timing information.")
			EXTERN_METASOUND_PARAM_D(Tempo,                         "Tempo",                              "Tempo in BPM. This is MIDI bpm... ALWAYS QUARTER NOTES PER MINUTE regardless of time signature!")
			EXTERN_METASOUND_PARAM_D(TimeSigNumerator,	            "Time Sig. Numerator",                "Time Signature Numerator")
			EXTERN_METASOUND_PARAM_D(TimeSigDenominator,            "Time Sig. Denominator",              "TimeSignature Denominator")
			EXTERN_METASOUND_PARAM_D(Speed,                         "Speed",                              "Playback speed multiplier.")
			EXTERN_METASOUND_PARAM_D(Loop,				            "Simple Loop",			              "Whether looping is enabled for this node. Loop length will be rounded to the nearest bar.")
			EXTERN_METASOUND_PARAM_D(LoopLengthBars,                "Loop Length (Bars)",                 "The number of bars to loop for simple looping")
			EXTERN_METASOUND_PARAM_D(TransportPrepare,              "Prepare",                            "Causes receiving nodes to prepare to play (pre-buffer, cache, etc.)")
			EXTERN_METASOUND_PARAM_D(TransportPlay,                 "Play",                               "Causes a transport play request.")
			EXTERN_METASOUND_PARAM_D(TransportPause,                "Pause",                              "Causes a transport pause request.")
			EXTERN_METASOUND_PARAM_D(TransportContinue,             "Continue",                           "Causes a transport contiunue request.")
			EXTERN_METASOUND_PARAM_D(TransportStop,                 "Stop",                               "Causes a transport stop request.")
			EXTERN_METASOUND_PARAM_D(TransportKill,                 "Kill",                               "Causes a transport kill request.")
			EXTERN_METASOUND_PARAM_D(TriggerSeek,                   "Trigger Seek",                       "Causes a transport seek request. Seek destination provided on a separate input.")
			EXTERN_METASOUND_PARAM_D(SeekDestination,               "Seek Target",                        "Destination for the next seek when it is triggered.")
			EXTERN_METASOUND_PARAM_D(PrerollBars,                   "Pre-roll Bars",                      "Number of bars to look back when seeking midi to find note-on messages that should hold over.")
			EXTERN_METASOUND_PARAM_D(GridSizeUnits,                 "Base Grid Size",                     "Base size of the qrid squares in musical terms.")
			EXTERN_METASOUND_PARAM_D(GridSizeMult,                  "Grid Size Multiplier",               "Grid size multiplier.")
			EXTERN_METASOUND_PARAM_D(OffsetUnits,                   "Base Offset Size ",                  "Base size of the qrid offset.")
			EXTERN_METASOUND_PARAM_D(OffsetMult,                    "Offset Multiplier",                  "Offset multiplier.")
			EXTERN_METASOUND_PARAM_D(Bar,		   	                "Bar",                                "Musical bar (or measure) index (0 based!)")
			EXTERN_METASOUND_PARAM_D(FloatBeat,			            "Beat",                               "Musical beat index. 0 based and in units of the time signature denomninator! The value is rounded to the nearest 32nd note.")
			EXTERN_METASOUND_PARAM_D(SynthPatch,                    "Patch",                              "Synthesizer patch asset.")
			EXTERN_METASOUND_PARAM_D(Transposition,                 "Transposition",                      "Transposition in semitones.")
			EXTERN_METASOUND_PARAM_D(ClockSpeedToPitch,             "Midi Clock Speed Affects Pitch",     "When true the clock speed of the incoming midi stream will affect the pitch of the synth.")
			EXTERN_METASOUND_PARAM_D(ClockSpeedToFrequency,         "Midi Clock Speed Affects Frequency", "When true the clock speed of the incoming midi stream will affect the calculated frequency.")
			EXTERN_METASOUND_PARAM_D(LFOSyncType,					"LFO Sync Type",					  "Specifies if and how the LFO should sync to a clock");
			EXTERN_METASOUND_PARAM_D(LFOFrequency,					"LFO Frequency",					  "The period of the LFO. If Sync Type is TempoSync, the unit is cycles per quarter note. Otherwise, the unit is Hz (cycles per second).");
			EXTERN_METASOUND_PARAM_D(LFOInvert,						"Invert LFO",						  "Toggle to invert the LFO");
			EXTERN_METASOUND_PARAM_D(LFOShape,						"LFO Shape",						  "The shape of the LFO. 0.0-1.0 = morph between square and triangle. 1.0-2.0 = morph between triangle and sawtooth");
			EXTERN_METASOUND_PARAM_D(AudioMono,						"Audio Mono",						  "The mono audio input");
		}
		namespace Outputs
		{
			EXTERN_METASOUND_PARAM_D(Transport,          "Transport",                   "Play, pause, continue, stop, etc.")
			EXTERN_METASOUND_PARAM_D(MidiClock,          "Midi Clock",                  "Midi timing information.")
			EXTERN_METASOUND_PARAM_D(MidiFileAsset,      "Midi File",                   "Standard Midi file.")
			EXTERN_METASOUND_PARAM_D(MidiStream,         "Midi Stream",                 "Midi event output stream.")
			EXTERN_METASOUND_PARAM_D(NoteOn,             "Note On",                     "Midi note on.")
			EXTERN_METASOUND_PARAM_D(NoteOff,            "Note Off",                    "Midi note off.")
			EXTERN_METASOUND_PARAM_D(MidiNoteNumber,     "Midi Note #",                 "Midi note number (0 - 127).")
			EXTERN_METASOUND_PARAM_D(Frequency,          "Frequency",                   "Midi note number converted to frequency.")
			EXTERN_METASOUND_PARAM_D(MidiVelocity,       "Velocity",                    "Midi velocity (0 - 127).")
			EXTERN_METASOUND_PARAM_D(NormalizedVelocity, "Normalized Velocity",         "Velocity (0.0 - 1.0).")
			EXTERN_METASOUND_PARAM_D(TransportPrepare,   "Prepare",                     "Causes receiving nodes to prepare to play (pre-buffer, cache, etc.)")
			EXTERN_METASOUND_PARAM_D(TransportPlay,      "Play",                        "Causes a transport play request.")
			EXTERN_METASOUND_PARAM_D(TransportPause,     "Pause",                       "Causes a transport pause request.")
			EXTERN_METASOUND_PARAM_D(TransportContinue,  "Continue",                    "Causes a transport continue request.")
			EXTERN_METASOUND_PARAM_D(TransportStop,      "Stop",                        "Causes a transport stop request.")
			EXTERN_METASOUND_PARAM_D(TransportKill,      "Kill",                        "Causes a transport kill request.")
			EXTERN_METASOUND_PARAM_D(MusicTimestamp,     "Music Timestamp",             "A single structure representing a musical time (bar & beat).")
			EXTERN_METASOUND_PARAM_D(SeekTarget,         "Seek Target",                 "A well formed musical seek target.")
			EXTERN_METASOUND_PARAM_D(AudioMono,          "Audio Mono",                  "The mono audio output.")
			EXTERN_METASOUND_PARAM_D(AudioLeft,          "Audio Left",                  "The left channel audio output.")
			EXTERN_METASOUND_PARAM_D(AudioRight,         "Audio Right",                 "The right channel audio output.")
			EXTERN_METASOUND_PARAM_D(AudioCenter,        "Audio Center",                "5.1/7.1: The center channel audio output.")
			EXTERN_METASOUND_PARAM_D(AudioLFE,           "Audio LFE",                   "5.1/7.1: The LFE channel audio output.")
			EXTERN_METASOUND_PARAM_D(AudioLeftSide,      "Audio Left Side",             "Quad/5.1: Left Surround, 7.1: Left Side.")
			EXTERN_METASOUND_PARAM_D(AudioRightSide,     "Audio Right Side",            "Quad/5.1: Right Surround, 7.1: Right Side.")
			EXTERN_METASOUND_PARAM_D(AudioLeftBack,      "Audio Left Back - Surround",  "7.1: Left Back.")
			EXTERN_METASOUND_PARAM_D(AudioRightBack,     "Audio Right Back - Surround", "7.1: Right Back.")
			EXTERN_METASOUND_PARAM_D(MusicTimespanBar,   "Bar",                         "Bar (Measure). 1 based!")
			EXTERN_METASOUND_PARAM_D(MusicTimespanBeat,  "Beat",                        "Beat. 1 based!")
			EXTERN_METASOUND_PARAM_D(Tempo,              "Tempo",                       "BPM. This is MIDI bpm... ALWAYS QUARTER NOTES PER MINUTE regardless of time signature!")
			EXTERN_METASOUND_PARAM_D(Speed,              "Speed",                       "Speed Multiplier")
			EXTERN_METASOUND_PARAM_D(TimeSigNumerator,   "Time Sig. Numerator",         "Time Signature Numerator")
			EXTERN_METASOUND_PARAM_D(TimeSigDenominator, "Time Sig. Denominator",       "TimeSignature Denominator")
			EXTERN_METASOUND_PARAM_D(SecsPerQuarter,     "Secs Per Quarter Note",       "Seconds per quarter note.")
			EXTERN_METASOUND_PARAM_D(SecsPerBeat,        "Secs Per Beat",               "Seconds per beat. Equals seconds per beat if time signature denominator is 4.")
		}
	}
}

#undef LOCTEXT_NAMESPACE // "HarmonixMetaSound"
