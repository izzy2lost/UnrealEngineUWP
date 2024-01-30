// Copyright Epic Games, Inc. All Rights Reserved.

#include "Playlist/AvaPlaylistExporter.h"

#include "AvaSerializationUtils.h"
#include "Playlist/AvaPlaylistEditorUtils.h"
#include "Playlist/AvalanchePlaylist.h"
#include "Serialization/MemoryWriter.h"

UAvaPlaylistExporter::UAvaPlaylistExporter()
{
	FormatExtension.Add(TEXT("json"));
	FormatDescription.Add(TEXT("JavaScript Object Notation file"));
	FormatExtension.Add(TEXT("xml"));
	FormatDescription.Add(TEXT("eXtensible Markup Language file"));
	SupportedClass = UAvalanchePlaylist::StaticClass();
	bText = true;
}

bool UAvaPlaylistExporter::ExportText(const FExportObjectInnerContext* InContext, UObject* InObject, const TCHAR* InType, FOutputDevice& InAr, FFeedbackContext* InWarn, uint32 InPortFlags)
{
	const UAvalanchePlaylist* Playlist = Cast<UAvalanchePlaylist>(InObject);
	if (!Playlist)
	{
		return false;
	}

	TArray<uint8> OutputBytes;
	FMemoryWriter Writer(OutputBytes);
	bool bSavedToBytes = false;

	if ( FCString::Stricmp(InType, TEXT("json")) == 0 )
	{
		bSavedToBytes = UE::AvaPlaylistEditor::Utils::SavePlaylistToJson(Playlist, Writer);
	}
	else if (FCString::Stricmp(InType, TEXT("xml")) == 0 )
	{
		// Note: using WChar encoding for compatibility with BytesToString below.
		bSavedToBytes = UE::AvaPlaylistEditor::Utils::SavePlaylistToXml(Playlist, Writer, EXmlSerializationEncoding::WChar);
	}

	if (bSavedToBytes)
	{
		FString OutputString;
		UE::AvaSerializationUtils::JsonValueConversion::BytesToString(OutputBytes, OutputString);
		InAr.Log(OutputString);
		return true;
	}
	
	return false;	
}