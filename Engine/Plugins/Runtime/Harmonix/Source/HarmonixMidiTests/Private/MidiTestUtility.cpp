// Copyright Epic Games, Inc. All Rights Reserved.
#include "MidiTestUtility.h"

namespace Harmonix::Testing::Utility::MidiTestUtility
{
	UMidiFile* BuildMidiFile(float InFileLengthBars, int32 InNumChannels, int32 InNumTracks, int32 InTimeSigNum, int32 InTimeSigDenom, int32 InTempo)
	{
		UMidiFile* TheMidiFile = NewObject<UMidiFile>();

		//some default values for creating midi events
		constexpr int32 DefaultNoteNumber = 60;//C4
		constexpr int32 DefaultNoteVelocity = 90;
		constexpr int32 DefaultTicksPerQuarter = 960;
		constexpr uint8 DefaultControllerID = 69; //Hold Pedal
		constexpr uint8 DefaultControlValue = 127;
		constexpr uint8 DefaultPitchBendValueLSB = 64;
		constexpr uint8 DefaultPitchBendValueMSB = 127;
		constexpr uint8 DefaultPolyPresValue = 127;

		// Set initial tempo and time signature
		TheMidiFile->GetSongMaps()->GetTempoMap().AddTempoInfoPoint(MidiConstants::BPMToMidiTempo(InTempo), 0);
		TheMidiFile->GetSongMaps()->GetBarMap().AddTimeSignatureAtBarIncludingCountIn(0, InTimeSigNum, InTimeSigDenom);
		TheMidiFile->GetSongMaps()->GetBarMap().SetTicksPerQuarterNote(DefaultTicksPerQuarter);

		//Add tracks to Midi file according to input argument
		for (int32 Track = 1; Track < InNumTracks; ++Track)
		{
			//Add midi tracks 1 - InNumTracks to file, Track 0 is the conductor track
			FMidiTrack* CurrentTrack = TheMidiFile->AddTrack(FString::Printf(TEXT("TestMidiTrack%i"), Track));

			//Add a text message at the end of track 1, marking the file's last event tick
			if (Track == 1)
			{
				uint16 TextIndex = CurrentTrack->AddText("TheEndOfTheFile");
				int32 LastEventTickInFile = TheMidiFile->GetSongMaps()->GetBarMap().FractionalBarIncludingCountInToTick(InFileLengthBars);
				if (InFileLengthBars == (int32)InFileLengthBars && InFileLengthBars != 0.f)
				{
					//if the input bar length is an integer number, the last event tick is 1 tick before the next bar
					//(InFileLengthBars will end up being the very beginning of the next bar)
					LastEventTickInFile -= 1;
				}
				FMidiEvent TextEventAtTheEnd(LastEventTickInFile, FMidiMsg::CreateText(TextIndex, MidiConstants::kMeta_Text));
				CurrentTrack->AddEvent(TextEventAtTheEnd);
			}

		}
		//update song length information
		TheMidiFile->TracksChanged();

		//To keep up with the results of imported Midi files, we ALWAYS round the file's length up to an
		//interger number of bars
		TheMidiFile->ConformMidiFileLength(EMidiFileLengthConformOption::RoundUp);
		//if the fractional part of the file's length is greater than 0.5,
		//rounding up and rounding to the nearest will get the same results
		if(FGenericPlatformMath::Fractional(InFileLengthBars) >= 0.5f) TheMidiFile->bLengthRoundedToNearest = true;

		return TheMidiFile;
	}

	void AddNoteOnNoteOffPairsToFile(UMidiFile* InFile, int32 InNoteNumber, int32 InNoteVelocity, int32 InTrackIndex, int32 InChannel, float InBarIndex)
	{
		FMidiTrack* CurrentTrack = InFile->GetTrack(InTrackIndex);
		//tick number at the start of every bar for adding Note On/Note Off midi events
		int32 CurrentBarTick = InFile->GetSongMaps()->GetBarMap().BarIncludingCountInToTick(InBarIndex);
		int32 DefaultTicksPerQuarter = 960;
		//Note On / Note Off pair
		FMidiEvent CurrentNoteOnEvent(CurrentBarTick,
			FMidiMsg::CreateNoteOn(InChannel, InNoteNumber, InNoteVelocity));
		//Note Offs are 1 beat away from Note Ons (ticks per quarter note)
		FMidiEvent CurrentNoteOffEvent(CurrentBarTick + DefaultTicksPerQuarter,
			FMidiMsg::CreateNoteOff(InChannel, InNoteNumber));
		CurrentTrack->AddEvent(CurrentNoteOnEvent);
		CurrentTrack->AddEvent(CurrentNoteOffEvent);
	}

	void AddCCEventsToFile(UMidiFile* InFile, uint8 InControllerID, uint8 InControlValue, int32 InTrackIndex, int32 InChannel, float InBarIndex)
	{
		FMidiTrack* CurrentTrack = InFile->GetTrack(InTrackIndex);
		//tick number at the start of every bar for adding CC events
		int32 CurrentBarTick = InFile->GetSongMaps()->GetBarMap().BarIncludingCountInToTick(InBarIndex);
		//Add Control Events to each channels/tracks
		FMidiEvent CurrentCCEvent(CurrentBarTick,
			FMidiMsg(MidiConstants::kControl + InChannel, InControllerID, InControlValue)
		);
		CurrentTrack->AddEvent(CurrentCCEvent);
	}

	void AddTextEventsToFile(UMidiFile* InFile, FString InText, int32 InTrackIndex, float InBarIndex)
	{
		FMidiTrack* CurrentTrack = InFile->GetTrack(InTrackIndex);
		//tick number at the start of every bar for adding text events
		int32 CurrentBarTick = InFile->GetSongMaps()->GetBarMap().BarIncludingCountInToTick(InBarIndex);
		//text events
		uint16 TextIndex = CurrentTrack->AddText(InText);
		FMidiEvent CurrentTextEvent(CurrentBarTick, FMidiMsg::CreateText(TextIndex, MidiConstants::kMeta_Text));
		CurrentTrack->AddEvent(CurrentTextEvent);
	}

	void AddPitchEventsToFile(UMidiFile* InFile, uint8 InPitchValueLSB, uint8 InPitchValueMSB, int32 InTrackIndex, int32 InChannel, float InBarIndex)
	{
		FMidiTrack* CurrentTrack = InFile->GetTrack(InTrackIndex);
		//tick number at the start of every bar for adding text events
		int32 CurrentBarTick = InFile->GetSongMaps()->GetBarMap().BarIncludingCountInToTick(InBarIndex);
		//Add pitch bend events to channels/tracks
		FMidiEvent CurrentPitchEvent(CurrentBarTick,
			FMidiMsg(MidiConstants::kPitch + InChannel, InPitchValueLSB, InPitchValueMSB)
		);
		CurrentTrack->AddEvent(CurrentPitchEvent);
	}

	void AddPolyPresEventsToFile(UMidiFile* InFile, uint8 InNoteNumber, uint8 InPolyPresValue, int32 InTrackIndex, int32 InChannel, float InBarIndex)
	{
		FMidiTrack* CurrentTrack = InFile->GetTrack(InTrackIndex);
		//tick number at the start of every bar for adding text events
		//TODO: BarIndex is a whole number but 
		int32 CurrentBarTick = InFile->GetSongMaps()->GetBarMap().BarIncludingCountInToTick(InBarIndex);
		//Add Poly Pres events to channels/tracks
		FMidiEvent CurrentPolyPresEvent(CurrentBarTick,
			FMidiMsg(MidiConstants::kPolyPres + InChannel, InNoteNumber, InPolyPresValue)
		);
		CurrentTrack->AddEvent(CurrentPolyPresEvent);
	}
}