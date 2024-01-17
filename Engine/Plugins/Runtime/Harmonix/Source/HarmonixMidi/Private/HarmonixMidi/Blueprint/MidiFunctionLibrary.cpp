// Copyright Epic Games, Inc. All Rights Reserved.

#include "HarmonixMidi/Blueprint/MidiFunctionLibrary.h"

#include "HarmonixMidi/MidiConstants.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MidiFunctionLibrary)

FMidiNote UMidiNoteFunctionLibrary::GetMinMidiNote()
{
	return FMidiNote(MidiConstants::kMinNote);
}

FMidiNote UMidiNoteFunctionLibrary::GetMaxMidiNote()
{
	return FMidiNote(MidiConstants::kMaxNote);
}

uint8 UMidiNoteFunctionLibrary::GetMinNoteNumber()
{
	return MidiConstants::kMinNote;
}

uint8 UMidiNoteFunctionLibrary::GetMaxNoteNumber()
{
	return MidiConstants::kMaxNote;
}

int UMidiNoteFunctionLibrary::GetMaxNumNotes()
{
	return MidiConstants::kMaxNumNotes;
}

uint8 UMidiNoteFunctionLibrary::GetMinNoteVelocity()
{
	return MidiConstants::kMinVelocity;
}

uint8 UMidiNoteFunctionLibrary::GetMaxNoteVelocity()
{
	return MidiConstants::kMaxVelocity;
}

float UMusicalTickFunctionLibrary::GetTicksPerQuarterNote()
{
	return MidiConstants::kTicksPerQuarterNote;
}

int32 UMusicalTickFunctionLibrary::GetTicksPerQuarterNoteInt()
{
	return MidiConstants::kTicksPerQuarterNoteInt;
}

float UMusicalTickFunctionLibrary::GetQuarterNotesPerTick()
{
	return MidiConstants::kQuarterNotesPerTick;
}

float UMusicalTickFunctionLibrary::TickToQuarterNote(float InTick)
{
	return InTick * MidiConstants::kQuarterNotesPerTick;
}

float UMusicalTickFunctionLibrary::QuarterNoteToTick(float InQuarterNote)
{
	return InQuarterNote * MidiConstants::kTicksPerQuarterNote;
}

