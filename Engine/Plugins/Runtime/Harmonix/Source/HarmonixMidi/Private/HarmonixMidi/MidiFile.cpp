// Copyright Epic Games, Inc. All Rights Reserved.
#include "HarmonixMidi/MidiFile.h"
#include "HarmonixMidi/MidiTrack.h"
#include "HarmonixMidi/MidiReader.h"
#include "HarmonixMidi/MidiReceiver.h"
#include "HarmonixMidi/MidiMsg.h"
#include "HarmonixMidi/MidiEvent.h"
#include "HarmonixMidi/MidiWriter.h"
#include "HarmonixMidi/BarMap.h"
#include "HarmonixMidi/SongMaps.h"
#include "HarmonixMidi/SongMapReceiver.h"
#include "Misc/Paths.h"
#include "Algo/ForEach.h"
#include "UObject/AssetRegistryTagsContext.h"
#if WITH_EDITORONLY_DATA
#include "EditorFramework/AssetImportData.h"
#endif

//////////////////////////////////////////////////////////////
//
// private helper class for reading a std midi file into a UMidiFile
//

class FMidiEventReceiver : public IMidiReceiver
{
public:
	FMidiEventReceiver(UMidiFile& target);
	int32 GetLastTick() const { return LastTick; }

private:
	virtual void Reset() override
	{
		LastTick = 0;
		CurrentTrackHasName = false;
	}
	virtual void OnNewTrack(int32 NewTrackIndex) override;
	virtual void OnEndOfTrack(int32 InLastTick) override;
	virtual void OnAllTracksRead() override {}
	virtual void OnMidiMessage(int32 Tick, uint8 Status, uint8 Data1, uint8 Data2) override;
	virtual void OnTempo(int32 Tick, int32 Tempo) override;
	virtual void OnText(int32 Tick, const FString& Str, uint8 Type) override;
	virtual void OnTimeSignature(int32 Tick, int32 Numerator, int32 Denominator, bool FailOnError = true) override;

	virtual void AddDummyConductorTrack() override;

	UMidiFile& File;
	int32  LastTick = 0;
	bool   CurrentTrackHasName = false;
};


////////////////////////////////////////
UMidiFile::UMidiFile()
{
	// add a single conductor track as the 0th track:
	TheMidiData.Tracks.Emplace("Conductor");
}

void UMidiFile::GetAssetRegistryTags(FAssetRegistryTagsContext Context) const
{
	Super::GetAssetRegistryTags(Context);
	
	Context.AddTag(UObject::FAssetRegistryTag("NumTracks", FString::FromInt(GetNumTracks()), UObject::FAssetRegistryTag::TT_Numerical));
	
	FString Tempo = FString::Printf(TEXT("%.2f BPM"), TheMidiData.SongMaps.GetTempoAtTick(0));
	Context.AddTag(UObject::FAssetRegistryTag("InitialTempo",  Tempo, UObject::FAssetRegistryTag::TT_Numerical));
	
	FString Length = FString::Printf(TEXT("%d Bars"), TheMidiData.SongMaps.GetSongLengthData().LengthBars);
	Context.AddTag(UObject::FAssetRegistryTag("Length", Length, UObject::FAssetRegistryTag::TT_Numerical));

#if WITH_EDITORONLY_DATA
	// this seems useful
	// this would allow us to inspect import data about this asset without fully loading the asset
	// storing it as a JSON is something that is done in other places (Like in StaticMesh)
	if (AssetImportData)
	{
		Context.AddTag(FAssetRegistryTag(
			GET_MEMBER_NAME_CHECKED(UMidiFile, AssetImportData),
			AssetImportData->GetSourceData().ToJson(),
			UObject::FAssetRegistryTag::TT_Hidden)
		);
	}
#endif

}

#if WITH_EDITORONLY_DATA
FString UMidiFile::GetImportedSrcFilePath() const
{
	return AssetImportData->GetFirstFilename();
}

#endif

TSharedPtr<Audio::IProxyData> UMidiFile::CreateProxyData(const Audio::FProxyDataInitParams& InitParams)
{
	if (!RenderableCopyOfMidiFileData)
	{
		RenderableCopyOfMidiFileData = MakeShared<FMidiFileData>(TheMidiData);
	}
	TSharedPtr<FMidiFileProxy> Proxy = MakeShared<FMidiFileProxy>(RenderableCopyOfMidiFileData);
	return Proxy;
}


void UMidiFile::BeginDestroy()
{
	RenderableCopyOfMidiFileData = nullptr;
	Super::BeginDestroy();
}

void UMidiFile::PostInitProperties()
{
	Super::PostInitProperties();

#if WITH_EDITORONLY_DATA
	if (!AssetImportData && !HasAnyFlags(RF_ClassDefaultObject))
	{
		AssetImportData = NewObject<UAssetImportData>(this, TEXT("AssetImportData"));
	}
#endif
}

void UMidiFile::LoadStdMidiFile(const FString& FilePath, int32 DesiredTicksPerQuarterNote, MidiConstants::EMidiTextEventEncoding InTextEncoding)
{
	FString Filename = FPaths::GetCleanFilename(FilePath);
	IPlatformFile& PlatformFileApi = FPlatformFileManager::Get().GetPlatformFile();
	IFileHandle* FileHandle = PlatformFileApi.OpenRead(*FilePath);
	TSharedPtr<FArchiveFileReaderGeneric> Archive = MakeShared<FArchiveFileReaderGeneric>(FileHandle, *Filename, FileHandle->Size());
	LoadStdMidiFile(Archive, Filename, DesiredTicksPerQuarterNote, InTextEncoding);

#if WITH_EDITORONLY_DATA
	if (!AssetImportData)
	{
		AssetImportData = NewObject<UAssetImportData>(this, TEXT("AssetImportData"));
	}
	AssetImportData->Update(FilePath);
#endif
}

void UMidiFile::LoadStdMidiFile(void* Buffer, int32 BufferSize, const FString& Filename, int32 DesiredTicksPerQuarterNote, MidiConstants::EMidiTextEventEncoding InTextEncoding)
{
	TSharedPtr<FBufferReader> BufferArchive = MakeShared<FBufferReader>(Buffer, BufferSize, false);
	LoadStdMidiFile(BufferArchive, Filename, DesiredTicksPerQuarterNote, InTextEncoding);
}

void UMidiFile::LoadStdMidiFile(TSharedPtr<FArchive> Archive, const FString& Filename, int32 DesiredTicksPerQuarterNote, MidiConstants::EMidiTextEventEncoding InTextEncoding)
{
	TheMidiData.TicksPerQuarterNote = DesiredTicksPerQuarterNote;
	TheMidiData.SongMaps.Init(TheMidiData.TicksPerQuarterNote);
	TheMidiData.Tracks.Empty();
	// Create a receiver list...
	FMidiReceiverList AllReceivers;
	// Add the event receiver that will populate the midi tracks with midi events...
	FMidiEventReceiver EventReceiver(*this);
	AllReceivers.Add(&EventReceiver);
	// Add the map receiver that will populate the song maps...
	FSongMapReceiver MapReceiver(&TheMidiData.SongMaps);
	AllReceivers.Add(&MapReceiver);

	// Create the reader and read all the data...
	FStdMidiFileReader reader(Archive, Filename, &AllReceivers, TheMidiData.TicksPerQuarterNote, InTextEncoding);
	reader.ReadAllTracks();

	TheMidiData.LastEventTick = EventReceiver.GetLastTick();
	if (reader.GetFailed())
	{
		UE_LOG(LogMidi, Error, TEXT("MIDI import failed. Midi data is malformed."));
	}

	// we are now "dirty"... so make sure any new requests for renderable data 
	// get a new copy...
	RenderableCopyOfMidiFileData = nullptr;
}

void UMidiFile::SaveStdMidiFile(const FString& FilePath)
{
	FString Filename = FPaths::GetCleanFilename(FilePath);
	IPlatformFile& PlatformFileApi = FPlatformFileManager::Get().GetPlatformFile();
	IFileHandle* FileHandle = PlatformFileApi.OpenWrite(*FilePath);
	TSharedPtr<FArchiveFileWriterGeneric> Archive = MakeShared<FArchiveFileWriterGeneric>(FileHandle, *Filename, 0);
	SaveStdMidiFile(Archive, Filename);
}

void UMidiFile::SaveStdMidiFile(TSharedPtr<FArchive> Archive, const FString& Filename)
{
	FMidiWriter Writer(*Archive, TheMidiData.TicksPerQuarterNote);
	Algo::ForEach(TheMidiData.Tracks, [&](FMidiTrack& track) { track.WriteStdMidi(Writer); });
}

FMidiTrack* UMidiFile::AddTrack(const FString& Name)
{
	TheMidiData.Tracks.Emplace(Name);
	return &TheMidiData.Tracks.Last();
}

void UMidiFile::Empty()
{
	TheMidiData.Tracks.Empty();
	TheMidiData.Tracks.Emplace("Conductor");
}

void UMidiFile::SetConductorTrack(const FTempoMap* TempoMap, const FBarMap* BarMap)
{
	check(!TheMidiData.Tracks.IsEmpty());
	// use assignment operator to properly clear string table, events, and other data.
	TheMidiData.Tracks[0] = FMidiTrack("Conductor");

	int32 numTempoChanges = TempoMap->GetNumTempoChangePoints();

	int32 i;
	for (i = 0; i < numTempoChanges; ++i)
	{
		int32 tick = TempoMap->GetTempoChangePointTick(i);
		float msPerQuarterNote = TempoMap->GetMsPerQuarterNoteAtTick(tick);
		int32 usecPerQuarterNote = int32(msPerQuarterNote * 1000);
		TheMidiData.Tracks[0].AddEvent(FMidiEvent(tick, FMidiMsg(usecPerQuarterNote)));
	}

	int32 numTimeSigChanges = BarMap->GetNumTimeSignaturePoints();

	for (i = 0; i < numTimeSigChanges; ++i)
	{
		const FTimeSignaturePoint& sig = BarMap->GetTimeSignaturePoint(i);
		TheMidiData.Tracks[0].AddEvent(FMidiEvent(sig.StartTick, FMidiMsg(sig.TimeSignature.Numerator, sig.TimeSignature.Denominator)));
	}

	TheMidiData.Tracks[0].Sort();
}

void UMidiFile::SortAllTracks()
{
	for (auto& Track : TheMidiData.Tracks)
	{
		Track.Sort();
	}
}

const FMidiTrack* UMidiFile::FindTrackByName(const FString& Name) const
{
	int32 trackIndex = FindTrackIndexByName(Name);
	if (trackIndex == INDEX_NONE)
	{
		return nullptr;
	}
	return &TheMidiData.Tracks[trackIndex];
}

int32 UMidiFile::FindTrackIndexByName(const FString& Name) const
{
	for (int32 i = 0; i < TheMidiData.Tracks.Num(); ++i)
	{
		const FMidiTrack& Track = TheMidiData.Tracks[i];
		if (Track.GetName() && *Track.GetName() == Name)
		{
			return i;
		}
	}
	return INDEX_NONE;
}

int32 UMidiFile::FindTextEvent(const FString& eventText, const FString* TrackName)
{
	if (TrackName)
	{
		int32 TrackIndex = FindTrackIndexByName(*TrackName);
		if (TrackIndex == INDEX_NONE)
		{
			return INDEX_NONE;
		}
		return FindTextEvent(eventText, TrackIndex);
	}
	else
	{
		for (int32 i = 0; i < TheMidiData.Tracks.Num(); ++i)
		{
			int32 FoundIndex = FindTextEvent(eventText, i);
			if (FoundIndex >= 0)
			{
				return FoundIndex;
			}
		}
	}
	return INDEX_NONE;
}

int32 UMidiFile::FindTextEvent(const FString& EventText, int32 TrackIndex)
{
	if (TrackIndex >= TheMidiData.Tracks.Num())
	{
		return INDEX_NONE;
	}
	if (!TheMidiData.Tracks[TrackIndex].GetTextRepository())
	{
		return INDEX_NONE;
	}

	// first lets see if the request text is even in the track's repository...
	int32 StringIndex = 0;
	FMidiTextRepository* TextRepo = TheMidiData.Tracks[TrackIndex].GetTextRepository();
	for (; StringIndex < TextRepo->Num(); StringIndex++)
	{
		if ((*TextRepo)[StringIndex] == EventText)
		{
			break;
		}
	}
	if (StringIndex == TextRepo->Num())
	{
		return INDEX_NONE;
	}

	// it's in the list, so now let's find the event...
	for (const FMidiEvent& m : TheMidiData.Tracks[TrackIndex].GetEvents())
	{
		if (m.GetMsg().MsgType() == FMidiMsg::EType::Text && m.GetMsg().GetTextIndex() == StringIndex)
		{
			return m.GetTick();
		}
	}
	// not found? How can this be if it was in the textRepo?
	return INDEX_NONE;
}

TArray<int32> UMidiFile::FindAllTextEvents(const FString& EventText, const FString* TrackName)
{
	TArray<int32> ticks;

	if (TrackName)
	{
		int32 TrackIndex = FindTrackIndexByName(*TrackName);
		if (TrackIndex != INDEX_NONE)
		{
			FindAllTextEvents(EventText, TrackIndex, ticks);
		}
	}
	else
	{
		for (int32 i = 0; i < TheMidiData.Tracks.Num(); ++i)
		{
			FindAllTextEvents(EventText, i, ticks);
		}
	}
	return ticks;
}

void UMidiFile::FindAllTextEvents(const FString& EventText, int32 TrackIndex, TArray<int32> OutIndexes)
{
	// first lets see if the request text is even in the track's repository...
	int32 StringIndex = 0;
	FMidiTextRepository* TextRepo = TheMidiData.Tracks[TrackIndex].GetTextRepository();
	for (; StringIndex < TextRepo->Num(); StringIndex++)
	{
		if ((*TextRepo)[StringIndex] == EventText)
		{
			break;
		}
	}

	// if we found it, then look through the events...
	if (StringIndex != TextRepo->Num())
	{
		for (const FMidiEvent& m : TheMidiData.Tracks[TrackIndex].GetEvents())
		{
			if (m.GetMsg().MsgType() == FMidiMsg::EType::Text && m.GetMsg().GetTextIndex() == StringIndex)
			{
				OutIndexes.Add(m.GetTick());
			}
		}
	}
}

void UMidiFile::TracksChanged()
{
	int32 NewLastEventTick = 0;
	bool bIsFileEmpty = true;
	for (auto& Track : TheMidiData.Tracks)
	{
		FMidiEventList& Events = Track.GetRawEvents();
		//check if the file is empty
		if (bIsFileEmpty && !Events.IsEmpty())
		{
			bIsFileEmpty = false;
		}
		int32 LastTickOnTrack = Events.IsEmpty() ? 0 : Events.Last().GetTick();
		if (NewLastEventTick < LastTickOnTrack)
		{
			NewLastEventTick = LastTickOnTrack;
		}
	}

	if (NewLastEventTick == 0 && !bIsFileEmpty)
	{
		TheMidiData.LastEventTick = NewLastEventTick;
		FSongLengthData& LengthData = TheMidiData.SongMaps.GetSongLengthData();
		//if the last event tick in a file is 0 and the file is not empty, we know that the length in tick is 1
		LengthData.LengthTicks = 1;
		LengthData.LastTick = NewLastEventTick;
		LengthData.LengthBars = TheMidiData.SongMaps.GetBarMap().TickToBarIncludingCountIn(LengthData.LengthTicks);
		return;
	}

	//update length data in song length data if needed 
	if (NewLastEventTick != TheMidiData.LastEventTick)
	{
		TheMidiData.LastEventTick = NewLastEventTick;
		FMusicTimestamp Timestamp = TheMidiData.SongMaps.GetBarMap().TickToMusicTimestamp(NewLastEventTick + 1);
		FSongLengthData& LengthData = TheMidiData.SongMaps.GetSongLengthData();
		LengthData.LengthTicks = TheMidiData.SongMaps.GetBarMap().MusicTimestampToTick(Timestamp);
		LengthData.LastTick = LengthData.LengthTicks - 1;
		LengthData.LengthBars = TheMidiData.SongMaps.GetBarMap().TickToBarIncludingCountIn(LengthData.LengthTicks);
	}
}

bool UMidiFile::ShouldConformMidiFileLength(EMidiFileLengthConformOption Option)
{
	//if file length is already rounded down, there's nothing more can be done
	if (bLengthRoundedDown) return false;

	//if a file's length is rounded up, it can be rounded down once more, but nothing else 
	if (bLengthRoundedUp)
	{
		if (Option == EMidiFileLengthConformOption::RoundDown)
		{
			//if the file's length is less than 1, cannot round down
			return TheMidiData.SongMaps.GetSongLengthData().LengthBars > 1 ? true : false;
		}
		else
		{
			return false;
		}
	}

	if (bLengthRoundedToNearest && Option == EMidiFileLengthConformOption::Nearest) return false;

	//if bar length is 1 bar (or less, it will round up to 1 bar if the file is imported, 0 if the file is manually created), 
	//it CANNOT be rounded down to 0 bar, can only be rounded up
	if (TheMidiData.SongMaps.GetSongLengthData().LengthBars <= 1 && Option != EMidiFileLengthConformOption::RoundUp)
	{
		return false;
	}

	//Special case: a manually created Midi file only has events on tick 0
	//This should still be able to be conformed by rounding up ONLY
	bool bIsFileEmpty = true;
	for (auto& Track : TheMidiData.Tracks)
	{
		FMidiEventList& Events = Track.GetRawEvents();
		//check if the file is empty
		if (bIsFileEmpty && !Events.IsEmpty())
		{
			bIsFileEmpty = false;
			break;
		}
	}

	//get the last event tick in file
	int32 LastEventTick = TheMidiData.LastEventTick;

	if (LastEventTick == 0 && !bIsFileEmpty && Option == EMidiFileLengthConformOption::RoundUp) return true;

	//if the last event tick is at the very end of an integer bar, the next tick should be the at the next bar, beat 1.0 
	FMusicTimestamp Timestamp = TheMidiData.SongMaps.GetBarMap().TickToMusicTimestamp(LastEventTick + 1);
	//for manually created Midi files, we can specify that the last event tick is at the very end of an integer bar
	//however,some DAWs places the very last event of an integer bar at the very beginning of the next bar, causing a 1-tick difference
	//the current solution is to tolerate this 1-tick error when checking the last event tick
	double OneTickTimeStampBeat = TheMidiData.SongMaps.GetBarMap().TickToMusicTimestamp(1).Beat;
	double OneTickError = OneTickTimeStampBeat - 1.0;
	if (!FMath::IsNearlyEqual(Timestamp.Beat, 1.0f, OneTickError))
	{
		return true;
	}
	return false;
}

void UMidiFile::ConformMidiFileLength(EMidiFileLengthConformOption Option)
{
	//check whether midi file length needs to be conformed
	if (!ShouldConformMidiFileLength(Option))
	{
		UE_LOG(LogMidi, Log, TEXT("Midi file does not need to be conformed."));
		return;
	}
	
	int32 LastTickInSongLengthData = TheMidiData.SongMaps.GetSongLengthData().LastTick;
	//Currently, file length is automatically rounded up to the nearest integer bar for imported midi files, if the conform option is Round Up, early out
	//(NOTE: Round Up doesn't add/change/remove anything to the midi file or events)
	if (Option == EMidiFileLengthConformOption::RoundUp)
	{
		//for manually created Midi files, update the song length data to keep up with 
		//results of imported Midi files WITHOUT adding/changing/removing events
		FMusicTimestamp Timestamp = TheMidiData.SongMaps.GetBarMap().TickToMusicTimestamp(TheMidiData.LastEventTick + 1);
		if (!FMath::IsNearlyEqual(Timestamp.Beat, 1.0f))
		{
			Timestamp.Bar++;
			Timestamp.Beat = 1.0f;
			FSongLengthData& LengthData = TheMidiData.SongMaps.GetSongLengthData();
			LengthData.LengthTicks = TheMidiData.SongMaps.GetBarMap().MusicTimestampToTick(Timestamp);
			LengthData.LastTick = LengthData.LengthTicks - 1;
			LengthData.LengthBars = TheMidiData.SongMaps.GetBarMap().TickToBarIncludingCountIn(LengthData.LengthTicks);
		}
		
		bLengthRoundedUp = true;
		return;
	}

	//Get file's fractional bar length (not rounded up)
	float OriginalLengthFractionalBar = TheMidiData.SongMaps.GetBarIncludingCountInAtTick(TheMidiData.LastEventTick);

	//Round Down to the nearest integer bar
	int32 ConformedLastBar = (int32)OriginalLengthFractionalBar;

	//rounding to nearest is still either round down or round up
	//depending on the result from FMath::RoundToInt()
	if (Option == EMidiFileLengthConformOption::Nearest)
	{
		ConformedLastBar = FMath::RoundToInt(OriginalLengthFractionalBar);
		EMidiFileLengthConformOption NearestRoundingOption = ConformedLastBar > (int32)OriginalLengthFractionalBar ? EMidiFileLengthConformOption::RoundUp : EMidiFileLengthConformOption::RoundDown;
		
		//if round to nearest ends up being rounding up, early out
		if (NearestRoundingOption == EMidiFileLengthConformOption::RoundUp)
		{
			//round the length up by updating the song length data
			FMusicTimestamp Timestamp = TheMidiData.SongMaps.GetBarMap().TickToMusicTimestamp(TheMidiData.LastEventTick + 1);
			if (!FMath::IsNearlyEqual(Timestamp.Beat, 1.0f))
			{
				Timestamp.Bar++;
				Timestamp.Beat = 1.0f;
				FSongLengthData& LengthData = TheMidiData.SongMaps.GetSongLengthData();
				LengthData.LengthTicks = TheMidiData.SongMaps.GetBarMap().MusicTimestampToTick(Timestamp);
				LengthData.LastTick = LengthData.LengthTicks - 1;
				LengthData.LengthBars = TheMidiData.SongMaps.GetBarMap().TickToBarIncludingCountIn(LengthData.LengthTicks);
			}
			bLengthRoundedUp = true;
			bLengthRoundedToNearest = true;
			return;
		}
	}

	//the actual tick to conform to by rounding down
	int32 ConformedLastEventTick = TheMidiData.SongMaps.GetBarMap().BarIncludingCountInToTick(ConformedLastBar) - 1;

	//Retrieve midi file name for logging after moving/removing midi events
	FString MidiFileName = this->GetName();

	//keep track of the events that are moved to the last tick and excessive events removed after moving
	int32 NumEventsMovedToLastTick = 0;
	int32 TotalNumEventsRemoved = 0;
	int32 NumNoteOnNoteOffPairsRemoved = 0;
	int32 NumAftertouchEventsRemoved = 0;
	int32 NumPitchEventsRemoved = 0;
	int32 NumControlEventsRemoved = 0;

	//go through midi events in each midi track 
	for (int32 TrackIndex = 0; TrackIndex < TheMidiData.Tracks.Num(); ++TrackIndex)
	{
		//Get the raw midi events and the last tick of the last event
		FMidiTrack& Track = TheMidiData.Tracks[TrackIndex];
		FMidiEventList& Events = Track.GetRawEvents();
		int32 LastEventTick = Events.Last().GetTick();
	
		//if the last tick of this event is less than the tick of the conformed last tick,
		//this midi track does not need to be conformed, go to the next midi track
		if (LastEventTick <= ConformedLastEventTick) continue;

		//move all events with tick > ConfromedLastEventTick to ConformedLastTick
		for (int32 EventIndex = Events.Num() - 1; EventIndex >= 0 && Events[EventIndex].GetTick() > ConformedLastEventTick; --EventIndex)
		{
			// NOTE: It is safe for us to do this BECAUSE...
			// We do not have to re-sort the midi data on the tracks because while we may have slid events earlier,
			// their relative position to each other MUST be unchanged!
			Events[EventIndex].Tick = ConformedLastEventTick;
			NumEventsMovedToLastTick++;
		}

		//filter the midi events (at the new conformed last tick) after moving them, remove note on/note off pairs on the last tick, and other excessive events
		int32 NumItemsRemoved = 0;
		int32 CurrentEventIndex = Events.Num() - 1;

		while (CurrentEventIndex >= 0 && Events[CurrentEventIndex].GetTick() == ConformedLastEventTick)
		{
			FMidiEvent& Event = Events[CurrentEventIndex];
			FMidiMsg& Msg = Event.GetMsg();

			//filter Note On/Note off pairs on the last tick
			if (Msg.IsNoteOff())
			{
				int32 NoteOnEventIndex = -1;
				for (int32 i = CurrentEventIndex - 1; i > 0 && Events[i].GetTick() == ConformedLastEventTick; --i)
				{
					//check equality of note on/note off events' midi note number (data1) 
					if (Events[i].GetMsg().IsNoteOn() && Events[i].GetMsg().GetStdData1() == Msg.GetStdData1())
					{
						NoteOnEventIndex = i;
						break;
					}
				}
				//remove note on/note off pairs on the conformed last tick
				if (NoteOnEventIndex != -1) 
				{
					Events.RemoveAt(CurrentEventIndex);
					NumItemsRemoved++;
					Events.RemoveAt(NoteOnEventIndex);
					NumItemsRemoved++;
					NumNoteOnNoteOffPairsRemoved++;
					TotalNumEventsRemoved += 2;
				}
			}
			uint8 MsgStatus = Msg.Status;

			//filter chan press/pitch bend/program change events
			if (MsgStatus == MidiConstants::kChanPres|| MsgStatus == MidiConstants::kPitch || MsgStatus == MidiConstants::kProgram)
			{
				int32 OtherEventIndexToRemove = -1;
				//check if there exist multiple events with same status on the last tick
				for (int32 i = CurrentEventIndex - 1; i >= 0 && Events[i].GetTick() == ConformedLastEventTick; --i)
				{
					if (Events[i].GetMsg().Status == MsgStatus) {
						OtherEventIndexToRemove = CurrentEventIndex;
						break;
					}
				}
				//remove excessive events on the conformed last tick
				if (OtherEventIndexToRemove != -1)
				{
					Events.RemoveAt(OtherEventIndexToRemove);
					NumItemsRemoved++;
					if (MsgStatus == MidiConstants::kChanPres || MsgStatus == MidiConstants::kPolyPres)
					{
						NumAftertouchEventsRemoved++;
					}
					else {
						NumPitchEventsRemoved++;
					}
					TotalNumEventsRemoved++;
				}

			}

			//filter Control Change events and Poly Pres events
			if (MsgStatus == MidiConstants::kControl || MsgStatus == MidiConstants::kPolyPres)
			{
				uint8 CurrentEventControllerId = Msg.Data1;
				int32 ControlEventIndexToRemove = -1; 
				for (int32 i = CurrentEventIndex - 1; i >= 0 && Events[i].GetTick() == ConformedLastEventTick; --i)
				{
					//check control change events for identical controller ID (data1) on the same tick and remove the later one
					//check poly pres events for the same note number (data1) on the same tick and remove the later one
					if (Events[i].GetMsg().Data1 == CurrentEventControllerId) {
						ControlEventIndexToRemove = CurrentEventIndex;
						break;
					}
				}
				//remove excessive control changes or poly pressure on conformed last tick
				if (ControlEventIndexToRemove != -1)
				{
					Events.RemoveAt(ControlEventIndexToRemove);
					NumItemsRemoved++;
					NumControlEventsRemoved++;
					TotalNumEventsRemoved++;
				}
			}

			//update indices after removing excessive events
			if (NumItemsRemoved == 0)
			{
				CurrentEventIndex -= 1;
			}
			else {
				CurrentEventIndex -= NumItemsRemoved;
				NumItemsRemoved = 0;
			}
		}
	}

	TracksChanged();

	//update the conformed length in SongLengthData if conformed to an integer bar, and marks the file is being conformed with the current conform option(s)
	if (Option == EMidiFileLengthConformOption::RoundDown || Option == EMidiFileLengthConformOption::Nearest)
	{
		FSongLengthData& LengthData = TheMidiData.SongMaps.GetSongLengthData();
		LengthData.LengthTicks = ConformedLastEventTick + 1;
		LengthData.LastTick = ConformedLastEventTick;
		LengthData.LengthBars = TheMidiData.SongMaps.GetBarMap().TickToBarIncludingCountIn(LengthData.LengthTicks);
		bLengthRoundedDown = true;

		//if we get here by rounding up first (or round up by rounding to the nearest integer) and then round down
		//this means the file is ultimately rounded down, NOT rounded up
		if (bLengthRoundedUp) bLengthRoundedUp = false;

		bLengthRoundedToNearest = Option == EMidiFileLengthConformOption::Nearest ? true : false;
		
		//Log conform summary 
		FString MidiFileLengthConformSummary = FString::Printf(
			TEXT("Midi file '%s.mid' length conformance summary: original length: %f bars, rounded down to new length: %d bar(s), moved %d Midi event(s) to tick %d"),
			*MidiFileName,
			OriginalLengthFractionalBar,
			ConformedLastBar,
			NumEventsMovedToLastTick,
			ConformedLastEventTick
		);
		if (TotalNumEventsRemoved > 0)
		{
			MidiFileLengthConformSummary.Append(FString::Printf(TEXT("; removed %d Midi event(s):"),TotalNumEventsRemoved));
			if (NumNoteOnNoteOffPairsRemoved > 0) MidiFileLengthConformSummary.Append(FString::Printf(TEXT(" %d Note On/Note off event pair(s),"), NumNoteOnNoteOffPairsRemoved));
			if (NumAftertouchEventsRemoved > 0) MidiFileLengthConformSummary.Append(FString::Printf(TEXT(" %d Aftertouch event(s),"), NumAftertouchEventsRemoved));
			if (NumPitchEventsRemoved > 0) MidiFileLengthConformSummary.Append(FString::Printf(TEXT(" %d Pitch Bend event(s),"), NumPitchEventsRemoved));
			if (NumControlEventsRemoved > 0) MidiFileLengthConformSummary.Append(FString::Printf(TEXT(" %d Control Change event(s),"), NumControlEventsRemoved));
			MidiFileLengthConformSummary.Append(FString::Printf(TEXT(" at tick %d "), ConformedLastEventTick));
		}
		UE_LOG(LogMidi, Log, TEXT("%s"),*MidiFileLengthConformSummary);
	}

	//This prevents the current conform option affecting any midi file that is currently being obtained from RenderableCopyOfMidiFileData
	//e.g. if file length is being conformed while the un-conformed file is occupied during playback, changes to the midi file won't be reflected until the 
	//current playback is stopped and starts playing again (we only create a new RenderableCopyOfMidiFileData if there isn't one existed already)
	RenderableCopyOfMidiFileData = nullptr;
}

#if WITH_EDITORONLY_DATA
void UMidiFile::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{	
	Super::PostEditChangeProperty(PropertyChangedEvent);

	FName StartBarPropertyName = GET_MEMBER_NAME_CHECKED(UMidiFile, StartBar);
	
	if (PropertyChangedEvent.Property->GetName() == StartBarPropertyName)
	{
		TheMidiData.SongMaps.GetBarMap().SetStartBar(StartBar);
	}
}
#endif

FMidiEventReceiver::FMidiEventReceiver(UMidiFile& file)
	: File(file)
{
}

void FMidiEventReceiver::AddDummyConductorTrack()
{
	// Add a dummy conductor track for midi type 0 files
	File.GetTracks().Emplace("Conductor");
}

void FMidiEventReceiver::OnNewTrack(int32 newTrackIndex)
{
	File.GetTracks().Emplace();
	CurrentTrackHasName = false;
}

void FMidiEventReceiver::OnEndOfTrack(int InLastTick)
{
	if (InLastTick > LastTick)
	{
		LastTick = InLastTick;
	}
	if (!CurrentTrackHasName)
	{
		check(!File.GetTracks().IsEmpty());

		FMidiTrack& Track = File.GetTracks().Last();
		uint16 StringIndex = Track.AddText("track_1");

		Track.AddEvent(FMidiEvent(0, FMidiMsg::CreateText(StringIndex, MidiConstants::kMeta_TrackName)));
		Track.Sort();
		CurrentTrackHasName = true;
	}
}

void FMidiEventReceiver::OnMidiMessage(int32 Tick, uint8 Status, uint8 Data1, uint8 Data2)
{
	check(!File.GetTracks().IsEmpty());

	File.GetTracks().Last().AddEvent(FMidiEvent(Tick, FMidiMsg(Status, Data1, Data2)));
}

void FMidiEventReceiver::OnTempo(int32 Tick, int32 Tempo)
{
	check(!File.GetTracks().IsEmpty());

	File.GetTracks().Last().AddEvent(FMidiEvent(Tick, FMidiMsg(Tempo)));
}

void FMidiEventReceiver::OnText(int32 Tick, const FString& Str, uint8 Type)
{
	check(!File.GetTracks().IsEmpty());

	FMidiTrack& Track = File.GetTracks().Last();
	uint16 StringIndex = Track.AddText(Str);

	if (Type == MidiConstants::kMeta_TrackName)
	{
		// Track name should always appear at tick 0!
		if (Tick != 0 && !CurrentTrackHasName)
		{
			UE_LOG(LogMidi, Warning, TEXT("MIDI track name (\"%s\") event found at tick %d but none found at tick 0! The first track name event on a midi track should be on tick 0. Shifting it to tick 0."),
				*Str, Tick);
			Tick = 0;
		}
		else if (CurrentTrackHasName && Str != *Track.GetName())
		{
			UE_LOG(LogMidi, Warning, TEXT("2nd Track Name event found on MIDI track %s --> %s at tick %d"),
				**Track.GetName(), *Str , Tick);
		}
		CurrentTrackHasName = true;
	}

	Track.AddEvent(FMidiEvent(Tick, FMidiMsg::CreateText(StringIndex, Type)));
}

void FMidiEventReceiver::OnTimeSignature(int32 Tick, int32 Numerator, int32 Denominator, bool FailOnError)
{
	check(!File.GetTracks().IsEmpty());

	File.GetTracks().Last().AddEvent(FMidiEvent(Tick, FMidiMsg(Numerator, Denominator)));
}

int32 FMidiFileData::FindTrackIndexByName(const FString& TrackName)
{
	for (int32 i = 0; i < Tracks.Num(); ++i)
	{
		const FMidiTrack& Track = Tracks[i];
		if (Track.GetName() && *Track.GetName() == TrackName)
		{
			return i;
		}
	}
	return INDEX_NONE;
}

void FMidiFileData::PostSerialize(const FArchive& Ar)
{
	if (Ar.IsLoading() && SongMaps.BarMapIsEmpty() && Tracks.Num() > 0)
	{
		UE_LOG(LogMidi, Warning, TEXT("Empty bar map. Rebuilding."));
		const FMidiEventList& Events = Tracks[0].GetEvents();
		FBarMap& BarMap = SongMaps.GetBarMap();
		BarMap.SetStartBar(1);
		for (auto& Event : Events)
		{
			if (Event.GetMsg().MsgType() == FMidiMsg::EType::TimeSig)
			{
				int32 Tick = Event.GetTick();
				int32 BarIndex;
				check(Tick == 0 || BarMap.GetNumTimeSignaturePoints() > 0);
				if (Tick == 0)
				{
					BarIndex = 0;
				}
				else
				{
					BarIndex = BarMap.TickToBarIncludingCountIn(Tick);
				}
				BarMap.AddTimeSignatureAtBarIncludingCountIn(BarIndex, Event.GetMsg().GetTimeSigNumerator(), Event.GetMsg().GetTimeSigDenominator());
			}
		}
		BarMap.Finalize(LastEventTick);
	}
}

void FMidiFileData::AddTempoChange(int32 TrackIdx, int32 Tick, float TempoBPM)
{
	check(Tracks.IsValidIndex(TrackIdx));

	FTempoMap& TempoMap = SongMaps.GetTempoMap();
	int32 MidiTempo = MidiConstants::BPMToMidiTempo(TempoBPM);
	Tracks[TrackIdx].AddEvent(FMidiEvent(Tick, FMidiMsg(MidiTempo)));
	Tracks[TrackIdx].Sort();
	TempoMap.AddTempoInfoPoint(MidiTempo, Tick);
}

void FMidiFileData::AddTimeSigChange(int32 TrackIdx, int32 Tick, int32 InTimeSigNum, int32 InTimeSigDenom)
{
	check(Tracks.IsValidIndex(TrackIdx));

	FBarMap& BarMap = SongMaps.GetBarMap();

	// Time signature changes can only happen at the beginning of a bar,
	// so round up to the next bar boundary...
	int32 AbsoluteBar = FMath::CeilToInt32(SongMaps.GetBarIncludingCountInAtTick(Tick));
	Tick = BarMap.BarBeatTickIncludingCountInToTick(AbsoluteBar, 1, 0);
	int32 TimeSigNum = FMath::Clamp(InTimeSigNum, 1, 64);
	int32 TimeSigDenom = FMath::Clamp(InTimeSigDenom, 1, 64);
	Tracks[TrackIdx].AddEvent(FMidiEvent(Tick, FMidiMsg((uint8)TimeSigNum, (uint8)TimeSigDenom)));
	Tracks[TrackIdx].Sort();
	BarMap.AddTimeSignatureAtBarIncludingCountIn(AbsoluteBar, TimeSigNum, TimeSigDenom);
}

