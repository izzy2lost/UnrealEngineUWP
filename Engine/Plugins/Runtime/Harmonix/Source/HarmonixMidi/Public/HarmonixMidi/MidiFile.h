// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once
#include "HarmonixMidi/TempoMap.h"
#include "HarmonixMidi/MidiMsg.h"
#include "HarmonixMidi/BarMap.h"
#include "HarmonixMidi/SongMaps.h"
#include "HarmonixMidi/MidiTrack.h"
#include "IAudioProxyInitializer.h"


#include "MidiFile.generated.h"

struct FSongMaps;
class UAssetImportData;

class FMidiFileProxy;
using FMidiFileProxyPtr = TSharedPtr<FMidiFileProxy, ESPMode::ThreadSafe>;

//midi file length conform options,rounding file length to a whole bar up or down 
enum class EMidiFileLengthConformOption : uint8
{
	RoundDown,
	RoundUp,
	Nearest
};

USTRUCT(BlueprintType)
struct HARMONIXMIDI_API FMidiFileData
{
	GENERATED_BODY()

	FMidiFileData()
		: TicksPerQuarterNote(MidiConstants::kTicksPerQuarterNoteInt)
		, LastEventTick(0)
	{}
	UPROPERTY(BlueprintReadOnly, Category = "MidiFile")
	FString MidiFileName;
	UPROPERTY(BlueprintReadOnly, Category = "MidiFile")
	int32 TicksPerQuarterNote;
	UPROPERTY(BlueprintReadOnly, Category = "MidiFile")
	FSongMaps SongMaps;
	UPROPERTY(BlueprintReadOnly, Category = "MidiFile")
	TArray<FMidiTrack> Tracks;
	UPROPERTY(BlueprintReadOnly, Category = "MidiFile")
	int32 LastEventTick;

	int32 FindTrackIndexByName(const FString& TrackName);
	
	void PostSerialize(const FArchive& Ar);

	/**
	* Adds a tempo change to the midi data at the given tick for the given track idx
	* 
	* asserts valid TrackIdx
	*/
	void AddTempoChange(int32 TrackIdx, int32 Tick, float TempoBPM);
	
	/**
	* Adds a Time Signature change to the next bar boundary from the input tick for the given track
	* 
	* Time signature changes can only happen at the beginning of a bar
	* So input Tick will be rounded UP to the next bar boundary.
	* 
	* asserts valid TrackIdx
	*/
	void AddTimeSigChange(int32 TrackIdx, int32 Tick, int32 TimeSigNum, int32 TimeSigDenom);

};

template<>
struct TStructOpsTypeTraits<FMidiFileData> : public TStructOpsTypeTraitsBase2<FMidiFileData>
{
	enum
	{
		WithPostSerialize = true,
	};
};

/**
 * An FMidFile is primarily a container for FMidiTracks. 
 * 
 * This class can handle loading and saving standard midi files, as well
 * as serializing itself to standard Unreal Engine FArchives.
 */
UCLASS(BlueprintType, Category="Music")
class HARMONIXMIDI_API UMidiFile : public UObject, public IAudioProxyDataFactory
{
	GENERATED_BODY()

public:
	using FMidiTrackList = TArray<FMidiTrack>;

	UMidiFile();
	virtual void GetAssetRegistryTags(FAssetRegistryTagsContext Context) const override;
	virtual void PostInitProperties() override;

	/** A method for importing a standard midi file, with the option of providing a pointer to an FSongMaps instance that will be populated during the load. */
	void LoadStdMidiFile(const FString& FilePath, int32 DesiredTicksPerQuarterNote = MidiConstants::kTicksPerQuarterNoteInt,
		MidiConstants::EMidiTextEventEncoding InTextEncoding = MidiConstants::EMidiTextEventEncoding::UTF8);
	/** A method for importing a standard midi file, with the option of providing a pointer to an FSongMaps instance that will be populated during the load. */
	void LoadStdMidiFile(void* Buffer, int32 BufferSize, const FString& FileName, int32 DesiredTicksPerQuarterNote = MidiConstants::kTicksPerQuarterNoteInt,
		MidiConstants::EMidiTextEventEncoding InTextEncoding = MidiConstants::EMidiTextEventEncoding::UTF8);
	/** A method for importing a standard midi file, with the option of providing a pointer to an FSongMaps instance that will be populated during the load. */
	void LoadStdMidiFile(TSharedPtr<FArchive> Archive, const FString& Filename, int32 DesiredTicksPerQuarterNote = MidiConstants::kTicksPerQuarterNoteInt,
		MidiConstants::EMidiTextEventEncoding InTextEncoding = MidiConstants::EMidiTextEventEncoding::UTF8);

	/** A method for exporting the midi track data to a standard midi file. */
	void SaveStdMidiFile(const FString& FilePath);
	/** A method for exporting the midi track data to a standard midi file. */
	void SaveStdMidiFile(TSharedPtr<FArchive> Archive, const FString& Filename);

	FMidiTrack* AddTrack(const FString& Name);

	void Empty();

	void SetConductorTrack(const FTempoMap*, const FBarMap*);

	void SortAllTracks();

	int32 GetNumTracks() const { return TheMidiData.Tracks.Num(); }
	const FMidiTrack* GetTrack(int32 Index) const { return &TheMidiData.Tracks[Index]; }
	FMidiTrack* GetTrack(int32 Index) { return &TheMidiData.Tracks[Index]; }
	const FMidiTrack* FindTrackByName(const FString& TrackName) const;
	int32 FindTrackIndexByName(const FString& TrackName) const;

	// Find the text event and return the tick or -1 if not found.
	int32 FindTextEvent(const FString& EventText, const FString* TrackName = nullptr);
	// Find the text event and return the tick or -1 if not found.
	int32 FindTextEvent(const FString& EventText, int32 TrackIndex);

	// Find all the ticks at which the text event occurs
	TArray<int32> FindAllTextEvents(const FString& EventText, const FString* TrackName = nullptr);
	void FindAllTextEvents(const FString& EventText, int32 TrackIndex, TArray<int32> OutIndexes);

	FMidiTrackList& GetTracks() { return TheMidiData.Tracks; }
	const FMidiTrackList& GetTracks() const { return TheMidiData.Tracks; }

	/**
	 * This function must be called if any changes are made to any of the tracks of this midi file. It will 
	 * do various internal tasks necessary to assure the midi data is consistent and "playable".
	 */
	void TracksChanged();

	int32  GetLastEventTick() const { return TheMidiData.LastEventTick; }

#if WITH_EDITORONLY_DATA
	// Import data for this MidiFileAsset
	// updated during Import in MidiFile
	UPROPERTY(VisibleAnywhere, Instanced, Category = ImportSettings)
	TObjectPtr<UAssetImportData> AssetImportData;
	FString GetImportedSrcFilePath() const;

	//The Start Bar of a Midi File
	UPROPERTY(EditDefaultsOnly,BlueprintReadOnly,Category = "Midi File Start Bar")
	int32 StartBar = 1;

	//StartBar UPROPERTY callback 
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	int32 GetStartBar() { return TheMidiData.SongMaps.GetBarMap().GetStartBar(); };

	//Midi File Conform Option, keep track if the files' length are conformed during import
	UPROPERTY(BlueprintReadOnly, Category = "MidiFile")
	bool bLengthRoundedDown = false;

	UPROPERTY(BlueprintReadOnly, Category = "MidiFile")
	bool bLengthRoundedUp = false;

	UPROPERTY(BlueprintReadOnly, Category = "MidiFile")
	bool bLengthRoundedToNearest = false;

	/**
	* Takes a file conform option (currently the last integer bar or the last integer beat),determine whether or not 
	* the file needs to be conformed/are already conformed.
	* @param A midi file conform option (last bar or last beat)
	* @return A bool indicating the file should or should not be conformed
	*/
	bool ShouldConformMidiFileLength(EMidiFileLengthConformOption Option);

	/**
	* Conform Midi File length(bars) to an integer number of bars:
	* LengthTicks and LengthBars in SongLengthData are rounded up to an integer number of bars
	* if the midi file has a fractional bar length or ends on a fractional beat, 
	* this function takes a midi conform option (Round Down, and Round Up,Round to Nearest):
	* Round Down: modifies/moves the raw midi events in every midi track of a midi file to the previous integer bar
	*			  and remove excessive note on/note off pairs, control change events, pitch bend events, etc. at the tick they are moved to
	* Round Up:	since file length is automatically rounded up, this option does NOT do or change anything to the Midi events in file
	* Round to Nearest: rounding the file length to the nearest integer (either round down or round up), depending on the file's fractional length
	* e.g. if a midi file has length 5.875 bars, Round Down conforms it to 5 bars, Round Up conforms it to 6 bars,
	* and Round To Nearest conforms it to 6 bars (5.875 > 5.5)
	*/
	void ConformMidiFileLength(EMidiFileLengthConformOption Option);

	const FSongMaps* GetSongMaps() const { return &TheMidiData.SongMaps; }
	FSongMaps* GetSongMaps() { return &TheMidiData.SongMaps; }

	// IAudioProxyDataFactory
	virtual TSharedPtr<Audio::IProxyData> CreateProxyData(const Audio::FProxyDataInitParams& InitParams) override;

	// UObject
	void BeginDestroy() override;

protected:
	UPROPERTY()
	FMidiFileData TheMidiData;


	TSharedPtr<FMidiFileData> RenderableCopyOfMidiFileData;
};

class HARMONIXMIDI_API FMidiFileProxy : public Audio::TProxyData<FMidiFileProxy>
{
public:
	IMPL_AUDIOPROXY_CLASS(FMidiFileProxy);

	explicit FMidiFileProxy(TSharedPtr<FMidiFileData>& Data)
		: MidiFileDataPtr(Data)
	{}

	TSharedPtr<FMidiFileData> GetMidiFile()
	{
		return MidiFileDataPtr;
	}

private:
	TSharedPtr<FMidiFileData> MidiFileDataPtr;
};
