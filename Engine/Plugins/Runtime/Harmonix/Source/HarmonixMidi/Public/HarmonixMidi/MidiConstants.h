// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once
#include "Logging/LogMacros.h"

DECLARE_LOG_CATEGORY_EXTERN(LogMidi, Log, All);

class HARMONIXMIDI_API MidiConstants
{
public:
	enum class EMidiTextEventEncoding
	{
		Latin1,
		UTF8
	};

	// helper enum for common time subdivisions
	enum class ESubdivisions
	{
		Downbeat      = 1,
		Half          = 2,
		ThreeOverFour = 3,
		Quarter       = 4,
		Eighth        = 8,
		Triplet       = 12,
		Sixteenth     = 16,
		ThirtySecond  = 32,
	};

	/////////////////////////////////////////////////////////////////////////////// 
	// Constants and functions for MIDI 1.0 standard
	//
	// Useful web resources:
	//  http://www.midi.org/about-midi/table1.shtml
	//  http://www.borg.com/~jglatt/tech/midispec.htm

	static const uint8 kNumChannels    = 16;
	static const uint8 kNotesPerOctave = 12;
	static const uint8 kMinNote        = 0;
	static const uint8 kMaxNote        = 127;
	static const uint8 kMaxNumNotes    = 128;
	static const uint8 kMiddleC        = 60;
	static const uint8 kMinVelocity    = 0;
	static const uint8 kMaxVelocity    = 127;

	static const int32   kTicksPerQuarterNoteInt  = 960;
	static constexpr float kTicksPerQuarterNote = 960.0f;
	static constexpr float kQuarterNotesPerTick = 1.0f / kTicksPerQuarterNote;

	//////////////////////////////////////////////////////////////////////////
	// Constants for the SMF (Standard Midi File) format.

	// Codes for special handling of System Exclusive messages:
	static const uint8 kFile_Escape      = 0xf7;
	static const uint8 kFile_SysEx       = 0xf0; 

	// Status code for SMF meta-events:
	static const uint8 kFile_Meta        = 0xff;

	// Meta-event IDs:
	static const uint8 kMeta_Text            = 0x01;
	static const uint8 kMeta_Copyright       = 0x02;
	static const uint8 kMeta_TrackName       = 0x03;
	static const uint8 kMeta_InstrumentName  = 0x04;
	static const uint8 kMeta_Lyric           = 0x05;
	static const uint8 kMeta_Marker          = 0x06;
	static const uint8 kMeta_CuePoint        = 0x07;
	static const uint8 kMeta_ChannelPrefix   = 0x20;
	static const uint8 kMeta_Port            = 0x21; // obsolete
	static const uint8 kMeta_EndOfTrack      = 0x2f;
	static const uint8 kMeta_Tempo           = 0x51;
	static const uint8 kMeta_SMPTE           = 0x54;
	static const uint8 kMeta_TimeSig         = 0x58;
	static const uint8 kMeta_KeySig          = 0x59;
	static const uint8 kMeta_Special         = 0x7f;

	// if FMidiMsg type is "Runtime" these are the possible status byte.
	// note: runtime messages are not serialized!
	static const uint8 kRuntimeAllNotesOffStatus = 0x01;  // Receiver should feel free to allow notes to "adsr release"
	static const uint8 kRuntimeAllNotesKillStatus = 0x02; // Receiver is expected to immediately free resources associated with any sounding notes and stop playing ASAP!

	static FString GetTextTypeName(uint8 TextType);

	///////////////////////////////////////////////////////////////////////////////
	// Status bytes indicate the type of message (3 bytes long except where noted).

	// Masks for disassembling MIDI Status bytes
	static const uint8 kStatusBitMask   = 0x80; // hi bit always set on Status byte
	static const uint8 kMessageTypeMask = 0xf0; // mask for message type
	static const uint8 kChannelMask     = 0x0f; // mask for channel
	static const uint8 kRealTimeMask    = 0xf8; // indicates real-time Status message

	static const uint8 kNoteOff  = 0x80; // Note Off
	static const uint8 kNoteOn   = 0x90; // Note On
	static const uint8 kPolyPres = 0xa0; // Polyphonic Key Pressure 
	static const uint8 kControl  = 0xb0; // Control Change
	static const uint8 kProgram  = 0xc0; // Program Change (2 bytes)
	static const uint8 kChanPres = 0xd0; // Channel Pressure (2 bytes)
	static const uint8 kPitch    = 0xe0; // Pitch Wheel Change
	static const uint8 kSystem   = 0xf0; // System


	static inline bool  IsStatus(uint8 Byte)     { return (Byte & kStatusBitMask) > 0;  }
	static inline uint8 GetChannel(uint8 Status) { check(IsStatus(Status));	check(!IsSystem(Status)); return Status & kChannelMask; }
	static inline uint8 GetType(uint8 Status)    { check(IsStatus(Status)); return Status & kMessageTypeMask; }

	static inline bool  IsNoteOff(uint8 Status)  { return GetType(Status) == kNoteOff;  }
	static inline bool  IsNoteOn(uint8 Status)   { return GetType(Status) == kNoteOn;   }
	static inline bool  IsPolyPres(uint8 Status) { return GetType(Status) == kPolyPres; }
	static inline bool  IsControl(uint8 Status)  { return GetType(Status) == kControl;  }
	static inline bool  IsProgram(uint8 Status)  { return GetType(Status) == kProgram;  }
	static inline bool  IsChanPres(uint8 Status) { return GetType(Status) == kChanPres; }
	static inline bool  IsPitch(uint8 Status)    { return GetType(Status) == kPitch;    }
	static inline bool  IsSystem(uint8 Status)   { return GetType(Status) == kSystem;   }

	// turns MIDI style tempo (microseconds per quarter note) into BPM...
	static inline float MidiTempoToBPM(int32 UsPerQuarterNote) { return UsPerQuarterNote== 0 ? 0 : 60000000.0f/(float)UsPerQuarterNote; }
	static inline int32 BPMToMidiTempo(float Bpm)       { return Bpm == 0.0 ? 0 : (int32)(60000000.0f / (float)Bpm); }

	static float RoundToStandardBeatPrecision(float InBeat, int TimeSIgnatureDenominator);

	///////////////////////////////////////////////////////////////////////////////
	// Controllers
	// 
	// In a Control Change message, data byte 1 is the controller ID, and data
	// byte 2 is the controller value.
	enum EControllerID
	{ // from midi specification...
	   // NOTE: If you add any here also add them to the GetControllerName function!
		BankSelection = 0,
		ModWheel = 1,
		Breath = 2,

		PortamentoTime = 5,
		DataCoarse = 6,
		Volume = 7,
		Balance = 8,
		PanRight = 10,
		Expression = 11,

		BitCrushWetMix = 14,
		BitCrushLevel = 15,
		BitCrushSampleHold = 16,

		LFO0Frequency = 22,
		LFO1Frequency = 23,
		LFO0Depth = 24,
		LFO1Depth = 25,

		DelayTime = 26,
		DelayDryGain = 27,
		DelayWetGain = 28,
		DelayFeedback = 29,

		CoarsePitchBend = 30,
		SampleStartTime = 31,
		DataFine = 38,

		SubStreamVol1 = 52,
		SubStreamVol2 = 53,
		SubStreamVol3 = 54,
		SubStreamVol4 = 55,
		SubStreamVol5 = 56,
		SubStreamVol6 = 57,
		SubStreamVol7 = 58,
		SubStreamVol8 = 59,

		DelayEQEnabled = 60,
		DelayEQType = 61,
		DelayEQFreq = 62,
		DelayEQQ = 63,

		Hold = 64,
		PortamentoSwitch = 65,
		Sustenuto = 66,
		SoftPedal = 67,
		Legato = 68,
		Hold2 = 69,
		FilterQ = 71,
		Release = 72,
		Attack = 73,
		FilterFrequency = 74,

		TimeStretchEnvelopeOrder = 79,

		DelayLFOBeatSync = 80,
		DelayLFOEnabled = 81,
		DelayLFORate = 82,
		DelayLFODepth = 83,

		DelayStereoType = 84,
		DelayPanLeft = 85,
		DelayPanRight = 86,

		RPNFine = 100,
		RPNCourse = 101,
		AllSoundOff = 120,
		Reset = 121,
		AllNotesOff = 123
	};

	static FString GetControllerName(EControllerID ControllerId);
	static void    GetControllerNames(TArray<FString>& Names);

	// allows choice between various naming conventions for enharmonic notes
	enum class ENoteNameEnharmonicStyle
	{
		Sharp,
		Flat,
		SharpAndFlat
	};

	static const int8 GetNoteNumberFromNoteName(const char* InName);
	static const char* GetNoteNameFromNoteNumber(uint8 MidiNoteNumber, ENoteNameEnharmonicStyle Style = ENoteNameEnharmonicStyle::SharpAndFlat);
	static int8 GetNoteOctaveFromNoteNumber(uint8 MidiNoteNumber);

	///////////////////////////////////////////////////////////////////////////////
	// RPNs (registered paramater numbers)
	static const uint8 kRPN_BendRangeC   = 0; 
	static const uint8 kRPN_BendRangeF   = 0; 


	///////////////////////////////////////////////////////////////////////////////
	// System messages:

	// System Common:
	static const uint8 kSys_MTC         = 0xf1; // 2 byte message
	static const uint8 kSys_SongPos     = 0xf2; // 3 byte message
	static const uint8 kSys_SongSelect  = 0xf3; // 2 byte message
	static const uint8 kSys_TuneRequest = 0xf6; // 1 byte message
	static const uint8 kSys_EOX         = 0xf7; // 1 byte message

	// System RealTime messages (all 1 byte)
	static const uint8 kSys_TimingClock = 0xf8;
	static const uint8 kSys_Start       = 0xfa;
	static const uint8 kSys_Continue    = 0xfb;
	static const uint8 kSys_Stop        = 0xfc;
	static const uint8 kSys_ActiveSense = 0xfe;
	static const uint8 kSys_Reset       = 0xff;
};
