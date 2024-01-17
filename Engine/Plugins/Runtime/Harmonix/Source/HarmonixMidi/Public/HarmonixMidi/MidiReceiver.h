// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once
#include "Algo/ForEach.h"
#include "Containers/Array.h"
#include "Containers/UnrealString.h"

/**
 * MidiReceivers are used in conjunction with a IMidiReaders to read data from a
 * MIDI file. As the IMidiReader processes the file, the appropriate member 
 * functions on the receiver are called.
 * 
 * FSongMaps has a type specific IMidiReceiver that takes callbacks from the IMidiReader
 * and constructs its internal maps. 
 * 
 * Games can implement their own midi receivers if they want to process midi messages 
 * when midi files are read. In this way, the game doesn't need to worry about the 
 * low level byte wrangling and can respond to callbacks that receive well formed 
 * midi messages
 */

class IMidiReader;

class IMidiReceiver
{
public:
	virtual ~IMidiReceiver() {}

	/** The reader is about to read a new MIDI file. Now is your chance to reset your state. */
	virtual void Reset() = 0;

	/** Called when the reader has determined how many tracks are in the midi data */
	virtual void SetNumTracks(int32 Num) {}

	/** called when all tracks have been read. */
	virtual void Finalize(int32 InLastFileTick) {}

	// For Midi type 0 files
	virtual void AddDummyConductorTrack() {};

	virtual void OnNewTrack(int32 NewTrackIndex) = 0;
	virtual void OnEndOfTrack(int32 LastTick) = 0;
	virtual void OnAllTracksRead() = 0;

	/** For standard 1- or 2-byte MIDI message */
	virtual void OnMidiMessage(int32 Tick, uint8 Status, uint8 Data1, uint8 Data2) = 0;

	/**
	 * Text, Copyright, TrackName, InstName, Lyric, Marker, CuePoint meta-event:
	 * "type" is the type of meta-event (constants defined in MidiConstants.h) 
	 */
	virtual void OnText(int32 Tick, const FString& Str, uint8 Type) = 0;

	/**
	 * Tempo Change meta - event:
	 * tempo is in microseconds per quarter-note
	 */
	virtual void OnTempo(int32 Tick, int32 Tempo) {}

	/** Time Signature meta - event:
	 *   time signature is numerator/denominator
	 */
	virtual void OnTimeSignature(int32 Tick, int32 Numerator, int32 Denominator, bool FailOnError = true) {}

	/**
	 * Called by the reader before it starts processing the midi data stream.
	 * This saves off a back pointer that is very useful for things like writing 
	 * good error messages when a receiver encounters unexpected data, or for 
	 * telling the reader to skip the current track and move on.
	 */
	virtual void SetMidiReader(IMidiReader* InReader) { Reader = InReader; }

	/** Tells the reader to Skip the rest of this track and go to the next one */
	void SkipCurrentTrack();

	const IMidiReader* GetReader() const { return Reader; }

protected:
	IMidiReader* Reader = nullptr;
};

/**
 * An implementation of IMidiReceiver that allows one to parse a midi file
 * and send the found data to many receivers at the same time.
 * 
 * Again, this can be very useful if a game has multiple systems that want to 
 * build their own runtime data based on data found in a standard midi file.
 *
 * @see IMidiReceiver
 */
class FMidiReceiverList : public IMidiReceiver
{
#define _FOR_EACH_MIDI_RECEIVER(func_body) Algo::ForEach(ReceiverList, [=](IMidiReceiver* recvr)func_body);
public:
	virtual void Reset()
	{
		_FOR_EACH_MIDI_RECEIVER({ recvr->Reset(); });
	}

	virtual void Finalize(int32 LastFileTick)
	{
		_FOR_EACH_MIDI_RECEIVER({ recvr->Finalize(LastFileTick); });
	}

	virtual void OnNewTrack(int32 NewTrackIndex)
	{
		_FOR_EACH_MIDI_RECEIVER({ recvr->OnNewTrack(NewTrackIndex); });
	}

	virtual void OnEndOfTrack(int32 LastTick)
	{
		_FOR_EACH_MIDI_RECEIVER({ recvr->OnEndOfTrack(LastTick); });
	}

	virtual void OnAllTracksRead()
	{
		_FOR_EACH_MIDI_RECEIVER({ recvr->OnAllTracksRead(); });
	}

	virtual void OnMidiMessage(int32 Tick, uint8 Status, uint8 Data1, uint8 Data2)
	{
		_FOR_EACH_MIDI_RECEIVER({ recvr->OnMidiMessage(Tick, Status, Data1, Data2); });
	}

	virtual void OnText(int32 Tick, const FString& Str, uint8 Type)
	{
		_FOR_EACH_MIDI_RECEIVER({ recvr->OnText(Tick, Str, Type); });
	}

	virtual void OnTempo(int32 Tick, int32 Tempo)
	{
		_FOR_EACH_MIDI_RECEIVER({ recvr->OnTempo(Tick, Tempo); });
	}

	virtual void OnTimeSignature(int32 Tick, int32 Numerator, int32 Denominator, bool FailOnError = true)
	{
		_FOR_EACH_MIDI_RECEIVER({ recvr->OnTimeSignature(Tick, Numerator, Denominator, FailOnError); });
	}

	virtual void SetMidiReader(IMidiReader* InReader)
	{
		IMidiReceiver::SetMidiReader(InReader);
		_FOR_EACH_MIDI_RECEIVER({ recvr->SetMidiReader(InReader); });
	}

	void Add(IMidiReceiver* recvr) { ReceiverList.Add(recvr); }

	void Empty() { ReceiverList.Empty(); }

	int32 Num() { return ReceiverList.Num(); }

private:
	TArray<IMidiReceiver*> ReceiverList;
#undef _FOR_EACH_MIDI_RECEIVER
};
