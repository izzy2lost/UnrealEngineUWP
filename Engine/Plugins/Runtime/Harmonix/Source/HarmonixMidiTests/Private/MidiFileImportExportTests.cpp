// Copyright Epic Games, Inc. All Rights Reserved.

#include "MidiTestUtility.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/MemoryReader.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace HarmonixMidiTests::ImportExportTests
{
	using namespace Harmonix::Testing::Utility::MidiTestUtility;
		
		/**
	* Add some events to the input midi file for testing the ConformMidiFileLength() function
	* 
	************************
	* Midi File Structure: *
	************************
	*
	*For each Midi track / channel :
	*	1 Note On / Note Off pair will be added at the 1st beat of every bar, note duration is 1 beat(TicksPerQuarter)
	*For every EVEN bar number :
	*	1 CC event and 1 Text event will be added at the same position as the Note On events
	*For every ODD bar number :
	*	1 Pitch Bend event and 1 Poly Pres event will be added at the same position as the Note On events
	*If input file length(bars) is a fractional number :
	*	1 Note On / Note Off pair and 1 Text event will be added to the tick position of that fractional bar length
	*	(Note On and Note Off events are 1 tick away so they don't end up on the same tick)
	*/
	void AddEventsToTestMidiFile(UMidiFile* InMidiFile, int32 InNumTracks, int32 InNumChannels, float InFileLengthBars)
	{
		constexpr int32 DefaultNoteNumber = 60;//C4
		constexpr int32 DefaultNoteVelocity = 90;
		constexpr uint8 DefaultControllerID = 69; //Hold Pedal
		constexpr uint8 DefaultControlValue = 127;
		constexpr uint8 DefaultPitchBendValueLSB = 64;
		constexpr uint8 DefaultPitchBendValueMSB = 127;
		constexpr uint8 DefaultPolyPressNoteNumber = 62;
		constexpr uint8 DefaultPolyPresValue = 127;

		//Add events to track 1 - InNumTracks, Track 0 is the conductor track
		for (int32 TrackIndex = 1; TrackIndex < InNumTracks; ++TrackIndex)
		{
			FMidiTrack* CurrentTrack = InMidiFile->GetTrack(TrackIndex);
			for (int32 Channel = 0; Channel < InNumChannels; ++Channel)
			{
				for (int32 Bar = 0; Bar < FMath::CeilToInt32(InFileLengthBars); ++Bar)
				{
					AddNoteOnNoteOffPairsToFile(InMidiFile, DefaultNoteNumber, DefaultNoteVelocity, TrackIndex, Channel, (float)Bar);

					//Currently, Midi CC events & Midi Text events are added to bars with EVEN bar numbers,
					//Midi Poly Press & Pitch Bend event are added to bars with ODD bar numbers
					if (Bar % 2 == 0)
					{
						//Add Control Events to each channels/tracks
						AddCCEventsToFile(InMidiFile, DefaultControllerID, DefaultNoteNumber, TrackIndex, Channel, (float)Bar);
						//Add Text events to tracks
						AddTextEventsToFile(InMidiFile, TEXT("TextInMidiFile"), TrackIndex, Bar);
					}
					else
					{
						//Add pitch bend events to channels/tracks
						AddPitchEventsToFile(InMidiFile, DefaultPitchBendValueLSB, DefaultPitchBendValueMSB, TrackIndex, Channel, (float)Bar);
						//Add Poly Pres events to channels/tracks
						AddPolyPresEventsToFile(InMidiFile, DefaultPolyPressNoteNumber, DefaultPolyPresValue, TrackIndex, Channel, (float)Bar);
					}
				}
				//if bar length is a fractional number, add additional midi events after the last integer bar
				if (InFileLengthBars != (int32)InFileLengthBars)
				{
					//This will end up getting removed
					AddNoteOnNoteOffPairsToFile(InMidiFile, DefaultNoteNumber, DefaultNoteVelocity, TrackIndex, Channel, InFileLengthBars);
					//Add 2 CC Events (same controller ID) to test the conform function where it should remove events with the same type on the last tick
					AddCCEventsToFile(InMidiFile, DefaultControllerID, DefaultControlValue, TrackIndex, Channel, InFileLengthBars);
					AddCCEventsToFile(InMidiFile, DefaultControllerID, DefaultControlValue + 1, TrackIndex, Channel, InFileLengthBars);

					//Add 2 Pitch Bend Events to test the conform function where it should remove events with the same type on the last tick
					AddPitchEventsToFile(InMidiFile, DefaultPitchBendValueLSB, DefaultPitchBendValueMSB, TrackIndex, Channel, InFileLengthBars);
					AddPitchEventsToFile(InMidiFile, DefaultPitchBendValueLSB + 1, DefaultPitchBendValueMSB + 1, TrackIndex, Channel, InFileLengthBars);

					//Add 2 Pitch Bend Events to test the conform function where it should remove events with the same type on the last tick
					AddPolyPresEventsToFile(InMidiFile, DefaultPolyPressNoteNumber, DefaultPolyPresValue, TrackIndex, Channel, InFileLengthBars);
					AddPolyPresEventsToFile(InMidiFile, DefaultPolyPressNoteNumber, DefaultPolyPresValue + 1, TrackIndex, Channel, InFileLengthBars);
				}
			}
			InMidiFile->GetTrack(TrackIndex)->Sort();
		}

		//update file information in SongLengthData
		InMidiFile->TracksChanged();
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FTestMidiFileImportExport,
		"Harmonix.Midi.MidiFile.ImportExport",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
		bool FTestMidiFileImportExport::RunTest(const FString&)
	{
		//input test values for creating a midi file
		const float FileLengthBars = 5.5f;
		const int32 NumChannels = 2;
		const int32 NumTracks = 4;
		const int32 TimeSigNum6 = 6;
		const int32 TimeSigDenum8 = 8;
		const int32 TimeSigNum4 = 4;
		const int32 TimeSigDenum4 = 4;
		const int32 Tempo = 120;
		
		//create an empty midi file to test for rounding to nearest (down)
		UMidiFile* GeneratedMidiFile = BuildMidiFile(FileLengthBars, NumChannels, NumTracks, TimeSigNum4, TimeSigDenum4, Tempo);
		//Add some events to the midi file for testing 
		AddEventsToTestMidiFile(GeneratedMidiFile, NumTracks, NumChannels, FileLengthBars);

		TArray<uint8> StdMidiFileBytes;
		TSharedPtr<FMemoryWriter> StdMidiFileOut = MakeShared<FMemoryWriter>(StdMidiFileBytes, true);
		GeneratedMidiFile->SaveStdMidiFile(StdMidiFileOut);

		UMidiFile* ReimportedMidiFile = NewObject<UMidiFile>();
		TSharedPtr<FMemoryReader> StdMidiFileIn = MakeShared<FMemoryReader>(StdMidiFileBytes, true);
		ReimportedMidiFile->LoadStdMidiFile(StdMidiFileIn, TEXT("InMemoryMidiFile"));

		// TODO
		//TestTrue(TEXT("Generated midi file survives export/import cycle."),*GeneratedMidiFile == *ReimportedMidiFile);

		return true;
	}
}
#endif