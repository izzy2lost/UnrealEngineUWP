// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Playlist/AvalanchePage.h"
#include "Playlist/AvalanchePlaylist.h"
#include "Templates/SharedPointer.h"
#include "XmlSerializationDefines.h"

class UAvalanchePlaylist;

namespace UE::AvaPlaylistEditor::Utils
{
	FString GetImportFilepath(const TCHAR* InFileDescription, const TCHAR* InExtension);
	FString GetExportFilepath(const UObject* InObjectToExport, const TCHAR* InFileDescription, const TCHAR* InExtension);
	FString GetSaveAssetAsPath(const FString& InDefaultPath, const FString& InDefaultAssetName);
	
	FString SerializePagesToJson(UAvalanchePlaylist* InPlaylist, const TArray<int32>& InPageIds);
	TArray<FAvalanchePage> DeserializePagesFromJson(const FString& InJsonString);

	/**
	 * @brief Creates a transient playlist with only the specified pages.
	 * @param InPlaylist Source Playlist.
	 * @param InPageIds Pages to copy to the new playlist.
	 * @return Created playlist.
	 */
	TStrongObjectPtr<UAvalanchePlaylist> ExportPagesToPlaylist(const UAvalanchePlaylist* InPlaylist, const TArray<int32>& InPageIds);

	bool SavePlaylistToXml(const UAvalanchePlaylist* InPlaylist, FArchive& InArchive, EXmlSerializationEncoding InXmlEncoding);
	bool SavePlaylistToXml(const UAvalanchePlaylist* InPlaylist, const TCHAR* InFilepath);

	bool SavePlaylistToJson(const UAvalanchePlaylist* InPlaylist, FArchive& InArchive);
	bool SavePlaylistToJson(const UAvalanchePlaylist* InPlaylist, const TCHAR* InFilepath);

	bool LoadPlaylistFromJson(UAvalanchePlaylist* InPlaylist, FArchive& InArchive);
	bool LoadPlaylistFromJson(UAvalanchePlaylist* InPlaylist, const TCHAR* InFilepath);

	/**
	 * Check the file extenstion to see if it is a supported format to use 
	 * with the deserializing functions (LoadPlaylistFrom...).
	 */
	bool CanLoadPlaylistFromFile(const TCHAR* InFilepath);

	/**
	 * Map of "Source TemplateId" to "Destination Template Id".
	 * 
	 * During the import of new pages, we need to keep track of the
	 * destination template Id, i.e. Source Page's template id -> actual id in playlist.
	 */
	struct FImportTemplateMap
	{
		TMap<int32, int32> TemplateIds;

		void Add(int32 InSourceTemplateId, int32 InDestinationTemplateId)
		{
			TemplateIds.Add(InSourceTemplateId, InDestinationTemplateId);
		}

		bool HasSourceTemplateId(int32 InSourceTemplateId) const
		{
			return TemplateIds.Contains(InSourceTemplateId);
		}

		int32 GetTemplateId(int32 InSourceTemplateId) const
		{
			const int32* FoundId = TemplateIds.Find(InSourceTemplateId);
			return FoundId ? *FoundId : InSourceTemplateId;
		}
	};
	
	TArray<int32> ImportTemplatePages(UAvalanchePlaylist* InPlaylist, const TArray<FAvalanchePage>& InSourceTemplates, FImportTemplateMap& OutImportedTemplateIds);

	/**
	 * @brief Import the given pages to the playlist.
	 * @param InPlaylist Playlist that receives the new pages. 
	 * @param InPageListReference Reference to the page list to which the pages will be added.
	 * @param InSourcePages Source pages
	 * @param InSourceTemplates Source templates, if available, for the source pages. 
	 * @param InOutImportedTemplateIds Imported template ids to make the correspondence between the source page template id and what was imported.
	 * @param InInsertPosition Indicate where in the page list to insert the new pages.
	 * @return Array of added page ids.
	 *
	 * Importing pages is a non-trivial operation. It includes adding missing templates. The biggest issue
	 * is maintaining the link between the page's template id and the imported templates. If available, the
	 * source templates can be used to reconstruct this relationship by finding an exact match with the templates
	 * of the destination playlist.
	 */
	TArray<int32> ImportInstancedPages(
		UAvalanchePlaylist* InPlaylist,
		const FAvaPageListReference& InPageListReference,
		const TArray<FAvalanchePage>& InSourcePages,
		const TArray<FAvalanchePage>& InSourceTemplates,
		FImportTemplateMap& InOutImportedTemplateIds,
		const FAvaPageInsertPosition& InInsertPosition = FAvaPageInsertPosition());

	/**
	 * @brief Import/Merge a playlist into another playlist.
	 * @param InPlaylist Destination playlist.
	 * @param InSourcePlaylist Source playlist to merge in the destination.
	 * @param InInsertPosition Indicate where in the page list to insert the new pages.
	 * @return Array of added page ids.
	 */
	TArray<int32> ImportInstancedPagesFromPlaylist(
		UAvalanchePlaylist* InPlaylist,
		const UAvalanchePlaylist* InSourcePlaylist,
		const FAvaPageInsertPosition& InInsertPosition = FAvaPageInsertPosition());
	
	UAvalanchePlaylist* SaveDuplicatePlaylist(UAvalanchePlaylist* InSourcePlaylist, const FString& InAssetName, const FString& InPackagePath);
}