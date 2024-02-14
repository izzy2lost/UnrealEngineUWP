// Copyright Epic Games, Inc. All Rights Reserved.
#include "HarmonixMidi/MidiReader.h"
#include "HarmonixMidi/MidiConstants.h"
#include "HarmonixMidi/MidiReceiver.h"
#include "HarmonixMidi/VarLenNumber.h"
#include "HarmonixMidi/MusicTimeSpecifier.h"
#include "HarmonixMidi/BarMap.h"
#include "HarmonixMidi/TempoMap.h"
#include "Misc/Paths.h"

// A 4-character IFF file chunk ID.  (Only "MThd" and "MTrk" are recognized
// by the SMF file format.)
const int32 kMidiChunkIDSize = 4;

class FMidiChunkID
{
public:
	FMidiChunkID(const char* InStr) { memcpy(Str, InStr, kMidiChunkIDSize); }
	FMidiChunkID(FArchive& Archive) { Archive.Serialize(Str, 4); }

	friend FArchive& operator<<(FArchive& Archive, FMidiChunkID& Id);

	bool operator==(const FMidiChunkID& rhs) const
	{
		return !FCStringAnsi::Strncmp(Str, rhs.Str, kMidiChunkIDSize);
	}

	bool operator!=(const FMidiChunkID& rhs) const { return !operator==(rhs); }

	static const FMidiChunkID kMThd; // "MThd": MIDI file header chunk
	static const FMidiChunkID kMTrk; // "MTrk": MIDI track chunk

private:
	char Str[kMidiChunkIDSize]; // storage for the 4 bytes
};

const FMidiChunkID FMidiChunkID::kMThd("MThd");
const FMidiChunkID	FMidiChunkID::kMTrk("MTrk");

FArchive& operator<<(FArchive& Archive, FMidiChunkID& Id)
{
	Archive.Serialize(Id.Str, 4);
	return Archive;
}

// Contains a chunk ID and a length.  The length is the number of bytes of
// data in the chunk AFTER the header.

class FMidiChunkHeader
{
public:
	FMidiChunkHeader(FArchive& Archive)
		: ID(Archive)
		, Length(0)
	{
		Archive << Length;
	}
	FMidiChunkHeader(const FMidiChunkID& InId, uint32 InLength) :
		ID(InId), Length(InLength) {}

	// Access chunk-header information
	uint32 GetLength() const { return Length; } // length of data following
	const FMidiChunkID& GetID() const { return ID; } // the chunk's 4-byte ID

	friend FArchive& operator<<(FArchive& Archive, FMidiChunkHeader& Header);

private:
	FMidiChunkID ID; // Chunk's 4-byte id
	uint32 Length; // length of data part of this chunk (not including header)
};

FArchive& operator<<(FArchive& Archive, FMidiChunkHeader& Header)
{
	Archive << Header.ID;
	Archive << Header.Length;
	return Archive;
}

///////////////////////////////////////////////////////////////////////////////
// FStdMidiFileReader
FStdMidiFileReader::FStdMidiFileReader(
	void* Buffer,
	int32 BufferSize,
	 const FString& InFileName,
	 IMidiReceiver* InReceiver,
	 int32 TicksPerQuarterNote,
	 Harmonix::Midi::Constants::EMidiTextEventEncoding InTextEncoding)
	: Filename(InFileName)
	, Receiver(InReceiver)
	, TextEncoding(InTextEncoding)
	, DestinationTicksPerQuarterNote(TicksPerQuarterNote)
{
	check(Receiver);
	BufferArchive = MakeShared<FBufferReader>(Buffer, BufferSize, false);
	InputArchive = BufferArchive;
	Init();
}

FStdMidiFileReader::FStdMidiFileReader(
	const FString& FilePath,
	IMidiReceiver* InReceiver,
	int32 TicksPerQuarterNote,
	Harmonix::Midi::Constants::EMidiTextEventEncoding InTextEncoding)
	: Receiver(InReceiver)
	, TextEncoding(InTextEncoding)
	, DestinationTicksPerQuarterNote(TicksPerQuarterNote)
{
	check(Receiver);
	Filename = FPaths::GetCleanFilename(FilePath);
	IPlatformFile& PlatformFileApi = FPlatformFileManager::Get().GetPlatformFile();
	IFileHandle* FileHandle = PlatformFileApi.OpenRead(*FilePath);
	FileArchive = MakeShared<FArchiveFileReaderGeneric>(FileHandle, *Filename, FileHandle->Size());
	InputArchive = FileArchive;
	Init();
}

FStdMidiFileReader::FStdMidiFileReader(
	TSharedPtr<FArchive> Archive,
	const FString& InFilename,
	IMidiReceiver* InReceiver,
	int32 TicksPerQuarterNote,
	Harmonix::Midi::Constants::EMidiTextEventEncoding InTextEncoding)
	: Filename(InFilename)
	, InputArchive(Archive)
	, Receiver(InReceiver)
	, TextEncoding(InTextEncoding)
	, DestinationTicksPerQuarterNote(TicksPerQuarterNote)
{
	check(Receiver);
	Init();
}

bool FStdMidiFileReader::ReadStdMidiFileForReceiver(
	void* Buffer,
	int32 BufferSize,
	const FString& FileName,
	 IMidiReceiver* Receiver,
	  int32 TicksPerQuarterNote,
	  Harmonix::Midi::Constants::EMidiTextEventEncoding InTextEncoding)
{
	bool Success = true;
	FStdMidiFileReader Reader(Buffer, BufferSize, FileName, Receiver, TicksPerQuarterNote, InTextEncoding);

	if (Reader.GetFailed())
	{
		UE_LOG(LogMidi, Error, TEXT("Unable to read MIDI file"));
		Success = false;
	}
	else
	{
		Receiver->Reset();
		Reader.ReadAllTracks();
		Receiver->Finalize(Reader.LastTick);

		if (Reader.GetFailed())
		{
			UE_LOG(LogMidi, Error, TEXT("opened but failed to parse MIDI file"));
			Success = false;
		}
	}

	return Success;
}

void FStdMidiFileReader::ReadAllTracks()
{
	// read from the top of the stream
	InputArchive->Seek(0);

	bool TrackReadSuccessfully = false;
	do 
	{
		TrackReadSuccessfully = ReadTrack();
	} while (!Failed && TrackReadSuccessfully);

	Receiver->Finalize(LastTick);
}

bool FStdMidiFileReader::ReadSomeEvents(int32 NumEvents)
{
	for (int32 i = 0; i < NumEvents; ++i)
	{
		ReadNextEvent();
		if ((State == EState::End) || Failed)
		{
			return true;
		}
	}
	return false;
}

bool FStdMidiFileReader::ReadTrack()
{
	do
	{
		ReadNextEvent();
	} while (State != EState::End && State != EState::NewTrack && !Failed);

	return (State == EState::NewTrack);
}

bool FStdMidiFileReader::ReadEvents(int32 Count)
{
	for (int32 i = 0; i < Count; ++i)
	{
		if (State == EState::End || Failed)
		{
			return false;
		}
		ReadNextEvent();
	}
	return true;
}

// skips the processing of the current track and moves on to the next track.
// Has no effect if called from within EndOfTrack().
void FStdMidiFileReader::SkipCurrentTrack()
{
	if (State == EState::InTrack)
	{
		if (CurrentTrackIndex == NumTracks - 1)
		{
			State = EState::End;
			Receiver->OnEndOfTrack(CurrentTick);
			Receiver->OnAllTracksRead();
		}
		else
		{
			State = EState::NewTrack;
			Receiver->OnEndOfTrack(CurrentTick);
			InputArchive->Seek(TrackEndPos);
		}
	}
}

void FStdMidiFileReader::Init()
{
	BarMap = MakeShared<FBarMap>();
	check(InputArchive);
	InputArchive->ArForceByteSwapping = PLATFORM_LITTLE_ENDIAN == 1;
	Receiver->SetMidiReader(this);
}

void FStdMidiFileReader::ReadNextEvent()
{
	ReadNextEventImpl();
}

void FStdMidiFileReader::ReadNextEventImpl()
{
	if (Failed)
	{
		return;
	}

	switch (State)
	{
	case EState::InTrack:
		ReadEvent(*InputArchive);
		break;
	case EState::NewTrack:
		if (Format == 0 && CurrentTrackIndex == 0)
		{
			// we already ready the one and only track as the "conductor track".
			// we have to read it again as "track 1", so...
			InputArchive->Seek(LastTracksFilePosition);
			TrackFilteringMode = ETrackFilteringMode::NonConductorEvents;
		}
		else 
		{
			LastTracksFilePosition = InputArchive->Tell();
		}
		ReadTrackHeader(*InputArchive);
		break;
	case EState::Start:
		ReadFileHeader(*InputArchive);
		break;
	case EState::End:
		break;
	}
}

// Read the standard Midi file header chunk (MThd)
void FStdMidiFileReader::ReadFileHeader(FArchive& Archive)
{
	check(State == EState::Start);

	FMidiChunkHeader Header(Archive);

	// Verify the MIDI header is properly formed.
	ensureAlwaysMsgf(Header.GetID() == FMidiChunkID::kMThd && Header.GetLength() == 6, TEXT("%s: MIDI file header is corrupt"), *Filename);

	// SMF format:
	//   0: A single multi-channel track
	//   1: 1 or more simultaneous tracks
	//   2: 1 or more sequentially independent tracks
	// we only support type 0 & 1.
	Archive << Format;
	ensureMsgf(Format == 1 || Format == 0, TEXT("%s: Only type 0 or 1 MIDI files are supported; this file is type %d"), *Filename, Format);

	if (Format == 0)
	{
		// We are starting to read a format 0 midi file, so on the first pass
		// over the one and only track in the file gather the conductor type events...
		TrackFilteringMode = ETrackFilteringMode::ConductorEvents;
	}
	else
	{
		TrackFilteringMode = ETrackFilteringMode::None;
	}

	Archive << NumTracks;

	if (!ensureMsgf(Format != 0 || NumTracks == 1, TEXT("%s: Format 0 file expected only one track but found %d"), *Filename, NumTracks))
	{
		Failed = true;
		return;
	}
	if (Format == 0)
	{
		// we are going to convert it to a format 1 file, with a conductor track
		// and one track for non-conductor events...
		NumTracks = 2;
	}

	if (NumTracks <= 0)
	{
		UE_LOG(LogMidi, Error, TEXT("%s: MIDI file has no tracks"), *Filename);
	}
	else
	{
		Receiver->SetNumTracks(NumTracks);
	}

	// Hi bit set on ticksPerQuarter indicates a SMPTE division.
	// We don't support that.
	int16 TicksPerQuarter;
	Archive << TicksPerQuarter;
	if (TicksPerQuarter & 0x8000)
	{
		UE_LOG(LogMidi, Error, TEXT("%s: MIDI file uses SMPTE time division; this is not supported"), *Filename);
	}
	TickConversionFactor = float(DestinationTicksPerQuarterNote) / (float)TicksPerQuarter;

	// If there are header failures, don't continue compilation. Just quit here!
	if ((NumTracks == 0) || (Format != 1 && Format != 0) || (TicksPerQuarter & 0x8000))
	{
		Failed = true;
	}
	else
	{
		State = EState::NewTrack;
	}
}

// Read the standard Midi track chunk header (MTrk)
void FStdMidiFileReader::ReadTrackHeader(FArchive& Archive)
{
	check(State == EState::NewTrack);

	FMidiChunkHeader TrackHeader(Archive);

	if (TrackHeader.GetID() != FMidiChunkID::kMTrk)
	{
		UE_LOG(LogMidi, Error, TEXT("%s: MIDI track header for track %d is corrupt"), *Filename, CurrentTrackIndex);
		Failed = true;
		return;
	}

	TrackEndPos = int32(Archive.Tell() + TrackHeader.GetLength());
	if (TrackEndPos > (int32)Archive.TotalSize())
	{
		UE_LOG(LogMidi, Error, TEXT("%s: MIDI track %d data length as recorded in the header exceeds the amount of data in the file!"), *Filename, CurrentTrackIndex);
		Failed = true;
		return;
	}

	++CurrentTrackIndex; // update track num

	// reset state for reading a new track
	PrevStatus = 0;
	CurrentFileTick = 0;
	CurrentTick = 0;    // units of file's Tick
	MidiListTick = -1;  // units of desired Tick (for sorting)
	State = EState::InTrack;

	// Let the midi receiver know that this is a new track.
	Receiver->OnNewTrack(CurrentTrackIndex);

	if (Format == 0)
	{
		if (CurrentTrackIndex == 0)
		{
			CurrentTrackName = TEXT("conductor");
		}
		else
		{
			CurrentTrackName = TEXT("Track-0");
		}
		Receiver->OnText(0, CurrentTrackName, Harmonix::Midi::Constants::GMeta_TrackName);
	}
}

// Read an event from the track
void FStdMidiFileReader::ReadEvent(FArchive& Archive)
{
	check(State == EState::InTrack);

	uint8 Status, Data1 = 0;    // MIDI message bytes
	bool RunningStatus;     // true if running status is detected

	CurrentFileTick += Midi::VarLenNumber::Read(Archive); // accumulate tick
	CurrentTick = int32(CurrentFileTick * TickConversionFactor);  // convert to kTicksPerQuarterNoteInt

	if (CurrentTick > LastTick)
	{
		LastTick = CurrentTick;
	}

	if (CurrentTick != MidiListTick)
	{
		// we have a new tick; sort all midi from previous tick and send it out
		ProcessMidiList();

		if (State != EState::InTrack)
		{
			return;
		}

		MidiListTick = CurrentTick;
	}

	Archive << Status;
	if (Harmonix::Midi::Constants::IsStatus(Status))  // this byte is truly a status byte
	{
		RunningStatus = false;
		if (!Harmonix::Midi::Constants::IsSystem(Status))
		{
			// save the current status byte, in case the next message uses running
			// status.  Note that this is only for midi events; running status can
			// run across meta-events and system exclusive events.
			PrevStatus = Status;
		}
	}
	else
	{
		// whoops, that wasn't actually a status byte we read, but a data byte;
		// the status byte comes from running status.
		RunningStatus = true;
		Data1 = Status;
		Status = PrevStatus;
	}

	// Check if this is a system (ie, non-channel) message
	if (Harmonix::Midi::Constants::IsSystem(Status))
	{
		ReadSystemEvent(CurrentTick, Status, Archive);
	}                   // All other messages are channel (non-system) messages
	else
	{
		// Running status means that data1 has already been read above
		if (!RunningStatus)
		{
			Archive << Data1;
		}
		ReadMidiEvent(CurrentTick, Status, Data1, Archive);
	}
}

void FStdMidiFileReader::ReadMidiEvent(int32 Tick, uint8 Status, uint8 Data1, FArchive& Archive)
{
	using namespace Harmonix::Midi::Constants;
	uint8 Data2 = 0;
	bool ValidMidiEvent = false;
	switch (Status & GMessageTypeMask)
	{
		// special case for NoteOn (3 byte message)
	case GNoteOn:
		Archive << Data2;
		ValidMidiEvent = true;
		// convert note-on vel=0 -> note offs
		if (Data2 == 0)
		{
			Status = GNoteOff | (Status & GChannelMask);
		}
		break;

		// other 3 byte messages
	case GNoteOff:
	case GControl:
	case GPitch:
	case GPolyPres:
		Archive << Data2;
		ValidMidiEvent = true;
		break;

		// 2 byte messages
	case GProgram:
	case GChanPres:
		Data2 = 0;
		ValidMidiEvent = true;
		break;

	default:
		UE_LOG(LogMidi, Error, TEXT("%s (%s): Cannot parse event %i"), *Filename, *CurrentTrackName, (Status & GMessageTypeMask));
		break;
	}

	if (ValidMidiEvent && TrackFilteringMode != ETrackFilteringMode::ConductorEvents)
	{
		// At this time, we only care about Note On / Note Off events.
		QueueChannelMsg(Tick, Status, Data1, Data2);
	}
}

// Read system event from the stream.  Status byte has already been read.
void FStdMidiFileReader::ReadSystemEvent(int32 Tick, uint8 Status, FArchive& Archive)
{
	using namespace Harmonix::Midi::Constants;
	
	switch (Status)
	{
	case GFile_SysEx:
	case GFile_Escape:
		{
			// All system events start with length. We'll
			// seek past that length and ignore these events
			// entirely.
			int32 PacketLength = Midi::VarLenNumber::Read(Archive);
			Archive.Seek(Archive.Tell() + PacketLength);
		}
		break;
	case GFile_Meta:
		{
			uint8 type;
			Archive << type;
			ReadMetaEvent(Tick, type, Archive);
		}
		break;
	default:
		UE_LOG(LogMidi, Error, TEXT("%s (%s): Cannot parse system event %i"), *Filename, *CurrentTrackName, Status);
		break;
	}
}

// Read a Meta Event from stream.
// Status byte and event type have already been read.
void FStdMidiFileReader::ReadMetaEvent(int32 Tick, uint8 Type, FArchive& Archive)
{
	using namespace Harmonix::Midi::Constants;
	
	int32 Length = Midi::VarLenNumber::Read(Archive);
	int32 StartPos = int32(Archive.Tell());
	FString WorkingString;

	switch (Type)
	{
	case GMeta_TrackName:
		CurrentTrackName = ReadText(Archive, Length);
		Receiver->OnText(Tick, CurrentTrackName, Type);
		break;
	case GMeta_Copyright:
	case GMeta_Marker:
	case GMeta_CuePoint:
		WorkingString = ReadText(Archive, Length);
		if (TrackFilteringMode != ETrackFilteringMode::NonConductorEvents)
		{
			Receiver->OnText(Tick, WorkingString, Type);
		}
		break;
	case GMeta_Text:
	case GMeta_Lyric:
		WorkingString = ReadText(Archive, Length);
		if (TrackFilteringMode != ETrackFilteringMode::ConductorEvents)
		{
			Receiver->OnText(Tick, WorkingString, Type);
		}
		break;
	case GMeta_Tempo:
		{
			uint8 Byte1, Byte2, Byte3;
			Archive << Byte1 << Byte2 << Byte3;
			int32 Tempo = (Byte1 << 16) + (Byte2 << 8) + Byte3;
			if (TrackFilteringMode != ETrackFilteringMode::NonConductorEvents)
			{
				Receiver->OnTempo(Tick, Tempo);
			}
		}
		break;

	case GMeta_EndOfTrack:
		ProcessMidiList();  // in case there is anything left to send out.
		Receiver->OnEndOfTrack(Tick);
		if (CurrentTrackIndex == NumTracks - 1)
		{
			State = EState::End;
			Receiver->OnAllTracksRead();
		}
		else
		{
			State = EState::NewTrack;
		}
		break;

	case GMeta_TimeSig:
		{
			uint8 Numerator, DenominatorExp;
			Archive << Numerator << DenominatorExp;

			if (DenominatorExp > 6)
			{
				UE_LOG(LogMidi, Error, TEXT("%s (%s): Time signature at %s has invalid denominator (2^%d); max is 64 (2^6)"), *Filename, *CurrentTrackName, *MidiTickFormat(Tick, BarMap.Get(), Midi::EMusicTimeStringFormat::Position), DenominatorExp);

				// Continuing past here could lead to an integer overflow below.
				break;
			}

			// time signature denominator is given as a power of 2
			//  e.g. for 6/8 time, numerator=6 and denominator=3
			int32 Denominator = int32(FMath::Pow(2.0f, DenominatorExp));

			if (Numerator == 0)
			{
				UE_LOG(LogMidi, Error, TEXT("%s (%s): Time signature %d/%d at %s has invalid numerator (%d)"), *Filename, *CurrentTrackName, Numerator, Denominator,
					*MidiTickFormat(Tick, BarMap.Get(), Midi::EMusicTimeStringFormat::Position), Numerator);
			}

			if (TrackFilteringMode != ETrackFilteringMode::NonConductorEvents)
			{
				// before telling any receivers about the time signature we update our own local tempo map
				// so we can print better error and warning messages...
				check(Tick == 0 || BarMap->GetNumTimeSignaturePoints() > 0);
				int32 BarIndex = BarMap->TickToBarIncludingCountIn(Tick);
				if (!BarMap->AddTimeSignatureAtBarIncludingCountIn(BarIndex, Numerator, Denominator, true, false))
				{
					UE_LOG(LogMidi, Warning, TEXT("%s (%s): Time signature %d/%d at %s overlaps or conflicts with nearby time signatures"),
						*Filename, *CurrentTrackName, Numerator, Denominator, *MidiTickFormat(Tick, BarMap.Get(), Midi::EMusicTimeStringFormat::Position));
				}
				// fill in our bar map
				Receiver->OnTimeSignature(Tick, Numerator, Denominator, false);
			}

			// skip over next two bytes, "MIDI clocks per metronome click"
			// and "32nd notes per MIDI quarter note"; they're not too useful
			Archive.Seek(Archive.Tell() + 2);
		}
		break;

	case GMeta_ChannelPrefix:
	case GMeta_Port:
	case GMeta_KeySig:
	case GMeta_SMPTE:
	case GMeta_InstrumentName:
		// these are valid events, but not currently supported by
		// MidiReceiver; do nothing
		break;

	default:
		UE_LOG(LogMidi, Warning, TEXT("%s (%s): Cannot parse meta event %i"), *Filename, *CurrentTrackName, Type);
		break;
	}

	// 
	if (State != EState::InTrack)
	{
		return;
	}

	// skip to end of meta event
	Archive.Seek(StartPos + Length);
}

FString FStdMidiFileReader::ReadText(FArchive& Archive, int32 Length)
{
	unsigned char* AsUtf8 = nullptr;
	if (TextEncoding == Harmonix::Midi::Constants::EMidiTextEventEncoding::Latin1)
	{
		unsigned char* AsLatin = (unsigned char*)FMemory::Malloc(Length + 1);
		Archive.Serialize(AsLatin, Length);
		AsLatin[Length] = 0;
		AsUtf8 = (unsigned char*)FMemory::Malloc(Length * 2 + 2);

		unsigned char* in = AsLatin;
		unsigned char* out = AsUtf8;
		while (*in)
		{
			if (*in < 128)
			{
				*out++ = *in++;
			}
			else
			{
				*out++ = 0xc0 | *in >> 6;
				*out++ = 0x80 | (*in++ & 0x3f);
			}
		}
		*out = 0;

		FMemory::Free(AsLatin);
	}
	else
	{
		AsUtf8 = (unsigned char*)FMemory::Malloc(Length + 1);
		Archive.Serialize(AsUtf8, Length);
		AsUtf8[Length] = 0;
	}

	FString AsFString = StringCast<TCHAR>((const UTF8CHAR*)AsUtf8).Get();
	FMemory::Free(AsUtf8);
	return AsFString;
}

// Queue all midi that is on the same tick, for sorting. 
// Note that tick is already in terms of desired ticks per quarter note.
void FStdMidiFileReader::QueueChannelMsg(int32 Tick, uint8 Status, uint8 Data1, uint8 Data2)
{
	// stick this message in the MidiList
	EventsOnSameTick.Add({Status, Data1, Data2});
}

// Check if MidiList has anything in it. If so, sort and send out.
void FStdMidiFileReader::ProcessMidiList()
{
	if (TrackFilteringMode != ETrackFilteringMode::ConductorEvents)
	{
		EventsOnSameTick.StableSort(RawMidiLess());
		for (auto& MidiItem : EventsOnSameTick)
		{
			Receiver->OnMidiMessage(MidiListTick, MidiItem.Status, MidiItem.Data1, MidiItem.Data2);

			// state changed because SkipCurrentTrack was called. Stop processing.
			if (State != EState::InTrack)
			{
				break;
			}
		}
	}
	EventsOnSameTick.Empty();
}
