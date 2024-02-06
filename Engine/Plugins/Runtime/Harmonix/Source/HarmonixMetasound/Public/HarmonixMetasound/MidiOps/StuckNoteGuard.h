// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HarmonixMetasound/DataTypes/MidiStream.h"

namespace Harmonix::Midi::Ops
{
	/**
	 * Tracks incoming note messages and sends note offs when a note would become "stuck" because of a filter change
	 */
	class HARMONIXMETASOUND_API FStuckNoteGuard
	{
	public:
		using FIncludeNotePredicate = TFunctionRef<bool(const HarmonixMetasound::FMidiStreamEvent& Event)>;

		/**
		 * Track incoming note messages and send note offs for notes that would have become "stuck" because the caller's filter changed.
		 * @param InStream The input MIDI stream
		 * @param OutStream The output MIDI stream
		 * @param Predicate The predicate which indicates whether an event is still included in the caller's filter
		 */
		void Process(const HarmonixMetasound::FMidiStream& InStream, HarmonixMetasound::FMidiStream& OutStream, const FIncludeNotePredicate& Predicate);

	private:
		TArray<HarmonixMetasound::FMidiStreamEvent> ActiveNotes;
	};
}
