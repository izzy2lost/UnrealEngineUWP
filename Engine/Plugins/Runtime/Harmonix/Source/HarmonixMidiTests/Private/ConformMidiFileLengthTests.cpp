// Copyright Epic Games, Inc. All Rights Reserved.

#include "MidiTestUtility.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace HarmonixMidiTests::ConformMidiFileLength
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
		}

		//update file information in SongLengthData
		InMidiFile->TracksChanged();
	}

	/**
	 * Check if a Midi file has excessive midi events on the last tick after its length is conformed by ROUNDING DOWN
	 * ConformMidiFileLength(EMidiFileLengthConformOption::RoundDown) first move all the events exceeding the last integer bar to the last tick of 
	 * the last integer bar, and then remove Note On/Note Off pairs, CC events with the same controller ID, Poly Pres events with the same note number
	 * and chan pres/pitch bend events that are on the same (last) tick
	 * this function validates these results
	 */
	bool ContainsExcessiveEventsAtLastEventTick(UMidiFile* ConformedMidiFile)
	{
		int32 ConformedLastEventTick = ConformedMidiFile->GetLastEventTick();

		for (FMidiTrack& Track : ConformedMidiFile->GetTracks())
		{
			const FMidiEventList& Events = Track.GetEvents();

			for (int EventIndex = Events.Num() - 1; EventIndex >= 0 && Events[EventIndex].GetTick() == ConformedLastEventTick; --EventIndex)
			{
				const FMidiMsg& CurrentMsg = Events[EventIndex].GetMsg();
				uint8 MsgStatus = CurrentMsg.Status;

				//check for Note On/Note Off pairs on the last tick
				if (CurrentMsg.IsNoteOff())
				{
					for (int32 i = EventIndex - 1; i > 0 && Events[i].GetTick() == ConformedLastEventTick; --i)
					{
						//check equality of note on/note off events' midi note number (data1) 
						if (Events[i].GetMsg().IsNoteOn() && Events[i].GetMsg().GetStdData1() == CurrentMsg.GetStdData1())
						{
							return true;
						}
					}
				}

				//check for pitch bend events on the last tick
				if (MsgStatus == MidiConstants::kPitch)
				{
					//check if there exist multiple events with same status on the last tick
					for (int32 i = EventIndex - 1; i >= 0 && Events[i].GetTick() == ConformedLastEventTick; --i)
					{
						if (Events[i].GetMsg().Status == MsgStatus)
						{
							return true;
						}
					}
				}

				//check for CC events and Poly Press on the last tick with the same controller ID
				if (MsgStatus == MidiConstants::kControl || MsgStatus == MidiConstants::kPolyPres)
				{
					uint8 CurrentEventControllerId = CurrentMsg.Data1;
					for (int32 i = EventIndex - 1; i >= 0 && Events[i].GetTick() == ConformedLastEventTick; --i)
					{
						//check control change events for identical controller ID (data1)
						//check poly pres events for the same note number (data1)
						if (Events[i].GetMsg().Data1 == CurrentEventControllerId)
						{
							return true;
						}
					}
				}
			}
		}

		return false;
	}

	/**
	 * Test the results of the Midi file length conform options after calling the ConformMidiFileLength() function on the input midi file
	 */
	static bool RunTestConformed(FAutomationTestBase& InTest, EMidiFileLengthConformOption Option, UMidiFile* InConformedMidiFile, float InFileLengthBars, int32 OriginalLastEventTick)
	{
		//Get the conformed Last Event Tick (should be the very last tick of the last integer bar)
		int32 LastEventTickConformed = InConformedMidiFile->GetLastEventTick();
		//Get the conformed Last Tick from the Song Length Data
		int32 SongLengthLastTickConformed = InConformedMidiFile->GetSongMaps()->GetSongLengthData().LastTick;
		//Expected last event tick after rounding up/down(nearest)
		int32 ExpectedLastTickRoundUp = InConformedMidiFile->GetSongMaps()->GetBarMap().BarIncludingCountInToTick(FMath::CeilToInt32(InFileLengthBars)) - 1;
		int32 ExpectedLastTickRoundDown = InConformedMidiFile->GetSongMaps()->GetBarMap().BarIncludingCountInToTick((int32)InFileLengthBars) - 1;

		//tests for each conform options:
		switch (Option) {
		case EMidiFileLengthConformOption::RoundUp:
			//round up doesn't do anything to the events, meaning the last event tick will still be the same as before, but song length 
			//data will change to the last tick of the next integer bar
			if (InFileLengthBars < 1.0)
			{
				//after conforming by rounding up, all rounding options should be disabled if file length is less than 1 bar
				if (!InTest.TestFalse("Cannot conform by rounding up down, or nearest", (InConformedMidiFile->ShouldConformMidiFileLength(EMidiFileLengthConformOption::RoundUp) ||
					InConformedMidiFile->ShouldConformMidiFileLength(EMidiFileLengthConformOption::RoundDown) || InConformedMidiFile->ShouldConformMidiFileLength(EMidiFileLengthConformOption::Nearest))))
				{
					return false;
				}
			}
			else {
				//if file length is greater than 1 bar, round down should still be available after rounding up, nearest shouldn't be 
				if (!InTest.TestTrue("Can conform by rounding down", InConformedMidiFile->ShouldConformMidiFileLength(EMidiFileLengthConformOption::RoundDown)))
				{
					return false;
				}
				if (!InTest.TestFalse("Cannot conform by rounding up or nearest", (InConformedMidiFile->ShouldConformMidiFileLength(EMidiFileLengthConformOption::RoundUp) ||
					InConformedMidiFile->ShouldConformMidiFileLength(EMidiFileLengthConformOption::Nearest))))
				{
					return false;
				}
			}
			//events should have no changes
			if (!InTest.TestEqual("Last event tick should be the same after rounding up", LastEventTickConformed, OriginalLastEventTick))
			{
				return false;
			}

			if (!InTest.TestEqual("Last tick in song length data should be rounded up to the nearest integer bar", SongLengthLastTickConformed, ExpectedLastTickRoundUp))
			{
				return false;
			}
			break;

		case EMidiFileLengthConformOption::RoundDown:
			//Last tick in song length data should be at the end of the last integer bar
			if (!InTest.TestEqual("Last tick in song length data after conforming should be at the end of the last integer bar of the file", SongLengthLastTickConformed,ExpectedLastTickRoundDown))
			{
				return false;
			}

			//conformed last event tick should be less than or equal to the last tick of song length data in the last integer bar
			//NOTE: we check 'less than or equal to' for a special case:
			//if a Midi file has a fractional bar length,
			//and the only Midi events exceeding the last integer bar are Note On/Note Off pairs,
			//while there is no Midi event on the very last tick of the last integer bar ---------
			//they'll first be moved to the last tick of the last integer bar and then REMOVED since we don't want Note On/Note Off pairs on 
			//the same tick. After this, the last event tick in file might end up being less than expected (the end of the last integer bar)
			//(The last tick in song length data will still be at the end of the last integer bar after calling TracksChanged())
			if (!InTest.TestTrue("Conformed last event tick should be less than or equal to the last tick in song length data", LastEventTickConformed <= SongLengthLastTickConformed))
			{
				return false;
			}

			//after conforming by rounding down, all rounding options should be disabled 
			if (!InTest.TestFalse("Cannot conform by rounding up down, or nearest", (InConformedMidiFile->ShouldConformMidiFileLength(EMidiFileLengthConformOption::RoundUp) ||
				InConformedMidiFile->ShouldConformMidiFileLength(EMidiFileLengthConformOption::RoundDown) || InConformedMidiFile->ShouldConformMidiFileLength(EMidiFileLengthConformOption::Nearest))))
			{
				return false;
			}

			//check for excessive events on the last tick after conforming 
			if (!InTest.TestFalse("There should not be excessive Pitch Bend/CC/Poly Press or Note On/Note Off pairs on the last tick after conforming", ContainsExcessiveEventsAtLastEventTick(InConformedMidiFile)))
			{
				return false;
			}
			break;
		case EMidiFileLengthConformOption::Nearest:
			//round to nearest is either rounding down or rounding up
			;
				break;
		}
		return true;
	}



	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FTestConformMidiFileLength,
		"Harmonix.Midi.ConformMidiFileLength",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
		bool FTestConformMidiFileLength::RunTest(const FString&)
	{
		//input test values for creating a midi file
		constexpr float InFileLengthBars = 5.5f;
		constexpr int32 InNumChannels = 2;
		constexpr int32 InNumTracks = 4;
		constexpr int32 InTimeSigNum6 = 6;
		constexpr int32 InTimeSigDenum8 = 8;
		constexpr int32 InTimeSigNum4 = 4;
		constexpr int32 InTimeSigDenum4 = 4;
		constexpr int32 InTempo = 120;
		

		//if a Midi file only has events on tick 0 (the very beginning), it should still be able to be conformed by rounding up
		constexpr float FileLength0Bar = 0.0;
		UMidiFile* MidiFile0Bar = BuildMidiFile(FileLength0Bar, InNumChannels, InNumTracks, InTimeSigNum4, InTimeSigDenum4, InTempo);
		//no need to round up again since we did it upon creating the file
		int32 ExpectedLastEventTick0BarRoundedUp = MidiFile0Bar->GetSongMaps()->GetBarMap().BarIncludingCountInToTick(1) - 1;
		int32 SongLengthDataLastEventTick0BarRoundedUp = MidiFile0Bar->GetSongMaps()->GetSongLengthData().LastTick;
		//last tick in song length data should be at the end of bar 1, but the last event tick should still be 0
		UTEST_TRUE("A Midi file that only has events on tick 0 should still be rounded up to 1 Bar (in song length data)",
			ExpectedLastEventTick0BarRoundedUp == SongLengthDataLastEventTick0BarRoundedUp);
		//verify the last event tick doesn't change
		UTEST_TRUE("Verify the last event tick is still 0",
			MidiFile0Bar->GetLastEventTick() == 0
		);

		//create an empty midi file to test for rounding up
		UMidiFile* MidiFileToTestRoundUp = BuildMidiFile(InFileLengthBars, InNumChannels, InNumTracks, InTimeSigNum6, InTimeSigDenum8, InTempo);
		//Add some events to the midi file for testing 
		AddEventsToTestMidiFile(MidiFileToTestRoundUp, InNumTracks, InNumChannels, InFileLengthBars);
		//get the last event tick before conforming
		int32 LastEventTickNotConformed = MidiFileToTestRoundUp->GetLastEventTick();
		//conform by rounding up
		MidiFileToTestRoundUp->ConformMidiFileLength(EMidiFileLengthConformOption::RoundUp);
		//check results
		if (!RunTestConformed(*this, EMidiFileLengthConformOption::RoundUp, MidiFileToTestRoundUp, InFileLengthBars,LastEventTickNotConformed))
		{
			return false;
		}

		//create an empty midi file to test for rounding down
		UMidiFile* MidiFileToTestRoundDown = BuildMidiFile(InFileLengthBars, InNumChannels, InNumTracks, InTimeSigNum4, InTimeSigDenum4, InTempo);
		//Add some events to the midi file for testing 
		AddEventsToTestMidiFile(MidiFileToTestRoundDown, InNumTracks, InNumChannels, InFileLengthBars);
		//conform by rounding down
		MidiFileToTestRoundDown->ConformMidiFileLength(EMidiFileLengthConformOption::RoundDown);
		//check results
		if (!RunTestConformed(*this, EMidiFileLengthConformOption::RoundDown, MidiFileToTestRoundDown, InFileLengthBars, LastEventTickNotConformed))
		{
			return false;
		}

		//round to nearest, if the fractional part is greater than 0.5, results will be the same as round up
		//otherwise equivalent to round down

		constexpr float TestFileLengthNearest = 5.25;
		//create an empty midi file to test for rounding to nearest (down)
		UMidiFile* MidiFileToTestRoundToNearestDown = BuildMidiFile(TestFileLengthNearest, InNumChannels, InNumTracks, InTimeSigNum4, InTimeSigDenum4, InTempo);
		//Add some events to the midi file for testing 
		AddEventsToTestMidiFile(MidiFileToTestRoundToNearestDown, InNumTracks, InNumChannels, TestFileLengthNearest);
		UTEST_FALSE("After rounding up upon creating the file, it cannot be conformed by rounding up again or to the nearest.",
			MidiFileToTestRoundToNearestDown->ShouldConformMidiFileLength(EMidiFileLengthConformOption::RoundUp) ||
			MidiFileToTestRoundToNearestDown->ShouldConformMidiFileLength(EMidiFileLengthConformOption::Nearest)
		);
		//Ideally, this file can still be rounded down, but calling ConformMidiFileLength(EMidiFileLengthConformOption::Nearest) won't do it,
		//because we automatically round the file up upon creating it, and it shouldn't be able to 'round to nearest anymore'
		UTEST_TRUE("A file with fractional portion of its length (bars) less than 0.5 can be rounded down, but not by any other conform options",
			MidiFileToTestRoundToNearestDown->ShouldConformMidiFileLength(EMidiFileLengthConformOption::RoundDown)
			);
		//since the fractional portion is greater than 0.5, this is equivalent to rounding up
		constexpr float TestFileLengthNearest2 = 5.7;
		//create an empty midi file to test for rounding to nearest (up)
		UMidiFile* MidiFileToTestRoundToNearestUp = BuildMidiFile(TestFileLengthNearest2, InNumChannels, InNumTracks, InTimeSigNum4, InTimeSigDenum4, InTempo);
		//Add some events to the midi file for testing 
		AddEventsToTestMidiFile(MidiFileToTestRoundToNearestUp, InNumTracks, InNumChannels, TestFileLengthNearest2);
		//get the last event tick before conforming
		int32 LastEventTickNotConformedNearestUp = MidiFileToTestRoundToNearestUp->GetLastEventTick();
		//conform by rounding to nearest (up)
		MidiFileToTestRoundToNearestUp->ConformMidiFileLength(EMidiFileLengthConformOption::Nearest);
		//since we know this result will be the same as round up, just test for round up
		if (!RunTestConformed(*this, EMidiFileLengthConformOption::RoundUp, MidiFileToTestRoundToNearestUp, TestFileLengthNearest2, LastEventTickNotConformedNearestUp))
		{
			return false;
		}

		//special case: bar length is less than 1 bar
		constexpr float TestFileLength1Bar = 0.6;
		//create an empty midi file for testing bar length less than 1 bar
		UMidiFile* MidiFileToTest1Bar = BuildMidiFile(TestFileLength1Bar, InNumChannels, InNumTracks, InTimeSigNum4, InTimeSigDenum4, InTempo);
		//Add some events to the midi file for testing 
		AddEventsToTestMidiFile(MidiFileToTest1Bar, InNumTracks, InNumChannels, TestFileLength1Bar);
		//get the last event tick before conforming
		int32 LastEventTickNotConformed1Bar = MidiFileToTest1Bar->GetLastEventTick();
		//in this case, this file's length cannot round down or nearest, only can round up
		//(we rounded up already when creating the file, so no option should be conformable
		UTEST_FALSE("A Midi file with length less than 1 bar can only be conformed by rounding up",
			MidiFileToTest1Bar->ShouldConformMidiFileLength(EMidiFileLengthConformOption::RoundUp) || 
			MidiFileToTest1Bar->ShouldConformMidiFileLength(EMidiFileLengthConformOption::RoundDown) ||
			MidiFileToTest1Bar->ShouldConformMidiFileLength(EMidiFileLengthConformOption::Nearest)
			);

		//conform by rounding up
		MidiFileToTest1Bar->ConformMidiFileLength(EMidiFileLengthConformOption::RoundUp);
		//check results
		if (!RunTestConformed(*this, EMidiFileLengthConformOption::RoundUp, MidiFileToTest1Bar, TestFileLength1Bar, LastEventTickNotConformed1Bar))
		{
			return false;
		}

		//test a midi file that should not be conformed (having an integer bar length)
		constexpr float IntegerFileLength = 5.0;
		//create a midi file with an integer bar length
		UMidiFile* MidiFileToTestIntegerLength = BuildMidiFile(IntegerFileLength, InNumChannels, InNumTracks, InTimeSigNum4, InTimeSigDenum4, InTempo);
		//Add some events to the midi file for testing 
		AddEventsToTestMidiFile(MidiFileToTestIntegerLength, InNumTracks, InNumChannels, IntegerFileLength);
		//get the last event tick
		int32 LastEventTickIntegerLength = MidiFileToTestIntegerLength->GetLastEventTick();

		//No conform option should be enabled
		UTEST_FALSE("A Midi file with an integer bar length cannot be conformed",
			MidiFileToTestIntegerLength->ShouldConformMidiFileLength(EMidiFileLengthConformOption::RoundUp) ||
			MidiFileToTestIntegerLength->ShouldConformMidiFileLength(EMidiFileLengthConformOption::RoundDown) || 
			MidiFileToTestIntegerLength->ShouldConformMidiFileLength(EMidiFileLengthConformOption::Nearest)
		);

		//"conform" file length (expects nothing to change in file)
		//(we can still call the ConformMidiFileLength() function but nothing will happen to the file)
		MidiFileToTestIntegerLength->ConformMidiFileLength(EMidiFileLengthConformOption::RoundUp);
		MidiFileToTestIntegerLength->ConformMidiFileLength(EMidiFileLengthConformOption::RoundDown);
		MidiFileToTestIntegerLength->ConformMidiFileLength(EMidiFileLengthConformOption::Nearest);
		//check results 
		UTEST_EQUAL("Nothing should change to a midi file that cannot be conformed despite calling the ConformMidiFileLength function on it",
			LastEventTickIntegerLength,
			MidiFileToTestIntegerLength->GetSongMaps()->GetBarMap().BarIncludingCountInToTick(IntegerFileLength) - 1);
		UTEST_EQUAL("Last tick in song length data should also equal to last event tick",
			LastEventTickIntegerLength,
			MidiFileToTestIntegerLength->GetSongMaps()->GetSongLengthData().LastTick);

		return true;
	}

}
#endif