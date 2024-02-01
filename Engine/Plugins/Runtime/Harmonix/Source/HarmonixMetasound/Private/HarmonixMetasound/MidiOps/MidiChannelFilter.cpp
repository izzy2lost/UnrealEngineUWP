// Copyright Epic Games, Inc. All Rights Reserved.

#include "HarmonixMetasound/MidiOps/MidiChannelFilter.h"

namespace Harmonix::Midi::Ops
{
	void FMidiChannelFilter::Process(const HarmonixMetasound::FMidiStream& InStream, HarmonixMetasound::FMidiStream& OutStream)
	{
		OutStream.Copy(
			InStream,
			[this](const HarmonixMetasound::FMidiStreamEvent& Event)
			{
				if (Event.MidiMessage.IsStd())
				{
					const uint8 ChannelIdx = Event.MidiMessage.GetStdChannel();
					check(ChannelIdx < 16);

					const bool ChannelIsEnabled = GetChannelEnabled(ChannelIdx + 1);

					// If this is a note on or off, keep track of active voices so we can unstick notes if the filter changes later
					if (ChannelIsEnabled)
					{
						if (Event.MidiMessage.IsNoteOn())
						{
							ActiveVoices[ChannelIdx].AddUnique(Event.GetVoiceId());
						}
						else if (Event.MidiMessage.IsNoteOff())
						{
							ActiveVoices[ChannelIdx].Remove(Event.GetVoiceId());
						}
					}
					
					return ChannelIsEnabled;
				}

				// Pass non-std messages
				return true;
			},
			true /* include transport events */);

		// If we turned off some channels, send note offs for any active voices in that channel
		for (uint8 ChannelIdx = 0; ChannelIdx < MidiConstants::kNumChannels; ++ChannelIdx)
		{
			if (!GetChannelEnabled(ChannelIdx + 1))
			{
				for (const FMidiVoiceId& VoiceId : ActiveVoices[ChannelIdx])
				{
					HarmonixMetasound::FMidiStreamEvent NoteOffEvent{ static_cast<uint32>(0), FMidiMsg::CreateNoteOff(ChannelIdx, 0) };
					NoteOffEvent.SetVoiceId(VoiceId);
					OutStream.InsertMidiEvent(NoteOffEvent);
				}

				ActiveVoices[ChannelIdx].Reset();
			}
		}
	}

	void FMidiChannelFilter::SetChannelEnabled(const uint8 Channel, const bool Enabled)
	{
		const uint16 ChannelMask = MidiChannelToBitmask(Channel);

		if (Enabled)
		{
			EnabledChannels |= ChannelMask;
		}
		else
		{
			EnabledChannels &= ~ChannelMask;
		}
	}

	bool FMidiChannelFilter::GetChannelEnabled(const uint8 Channel) const
	{
		const uint16 ChannelMask = MidiChannelToBitmask(Channel);
		return (EnabledChannels & ChannelMask) == ChannelMask;
	}
}
