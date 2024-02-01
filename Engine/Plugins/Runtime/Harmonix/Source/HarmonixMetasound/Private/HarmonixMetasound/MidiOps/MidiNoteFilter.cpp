// Copyright Epic Games, Inc. All Rights Reserved.

#include "HarmonixMetasound/MidiOps/MidiNoteFilter.h"

namespace Harmonix::Midi::Ops
{
	void FMidiNoteFilter::Process(const HarmonixMetasound::FMidiStream& InStream, HarmonixMetasound::FMidiStream& OutStream)
	{
		OutStream.Copy(
			InStream,
			[this](const HarmonixMetasound::FMidiStreamEvent& Event)
			{
				// Note ons must pass note number and velocity check
				if (Event.MidiMessage.IsNoteOn())
				{
					const FMidiVoiceId VoiceId = Event.GetVoiceId();
					const uint8 Channel = Event.MidiMessage.GetStdChannel();
					const uint8 NoteNumber = Event.MidiMessage.GetStdData1();
					const uint8 Velocity = Event.MidiMessage.GetStdData2();
					const bool bPassesFilter =
						NoteNumber >= MinNoteNumber
						&& NoteNumber <= MaxNoteNumber
						&& Velocity >= MinVelocity
						&& Velocity <= MaxVelocity;

					if (bPassesFilter)
					{
						ActiveVoices.FindOrAdd(VoiceId) = { Channel, NoteNumber, Velocity };
					}

					return bPassesFilter;
				}

				// Note offs must pass note number check
				if (Event.MidiMessage.IsNoteOff())
				{
					const FMidiVoiceId VoiceId = Event.GetVoiceId();
					const uint8 NoteNumber = Event.MidiMessage.GetStdData1();
					const bool bPassesFilter = NoteNumber >= MinNoteNumber && NoteNumber <= MaxNoteNumber;

					if (bPassesFilter)
					{
						ActiveVoices.Remove(VoiceId);
					}
					
					return bPassesFilter;
				}

				// Handle all notes off/kill
				if (Event.MidiMessage.IsNoteMessage())
				{
					ActiveVoices.Reset();
					return true;
				}

				return IncludeOtherEvents.Get();
			},
			true /* include transport events */);

		// If active notes we're tracking no longer pass the filter, send a note off for each
		for (auto It = ActiveVoices.CreateIterator(); It; ++It)
		{
			const uint8 NoteNumber = It.Value().NoteNumber;
			const uint8 Velocity = It.Value().Velocity;
			const bool bPassesFilter =
				NoteNumber >= MinNoteNumber
				&& NoteNumber <= MaxNoteNumber
				&& Velocity >= MinVelocity
				&& Velocity <= MaxVelocity;

			if (!bPassesFilter)
			{
				// Send a note off
				const uint8 Channel = It.Value().Channel;
				HarmonixMetasound::FMidiStreamEvent NoteOffEvent{ static_cast<uint32>(0), FMidiMsg::CreateNoteOff(Channel, NoteNumber) };
				NoteOffEvent.SetVoiceId(It.Key());
				OutStream.InsertMidiEvent(NoteOffEvent);
				
				// Stop tracking
				It.RemoveCurrent();
			}
		}
	}
}
