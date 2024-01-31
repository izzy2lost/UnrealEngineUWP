// Copyright Epic Games, Inc. All Rights Reserved.

#include "Playlist/AvaPlaylistEditorUtils.h"

#include "Backends/JsonStructDeserializerBackend.h"
#include "Backends/JsonStructSerializerBackend.h"
#include "Backends/XmlStructSerializerBackend.h"
#include "ContentBrowserModule.h"
#include "DesktopPlatformModule.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/FileManager.h"
#include "IAssetTools.h"
#include "IContentBrowserSingleton.h"
#include "JsonObjectConverter.h"
#include "Misc/PathViews.h"
#include "Playlist/AvalanchePlaylist.h"
#include "StructDeserializer.h"
#include "StructSerializer.h"

#define LOCTEXT_NAMESPACE "AvaPlaylistEditor_Utils"

namespace UE::AvaPlaylistEditor::Utils::Private
{
	static FString LastExportPath;
	static FString LastImportPath;
	
	static const FString PageEntriesName = TEXT("Pages");

	// Don't serialize the transient properties.
	static auto TransientPropertyFiler = [](const FProperty* InCurrentProp, const FProperty* InParentProp)
	{
		const bool bIsTransient = InCurrentProp && InCurrentProp->HasAnyPropertyFlags(CPF_Transient); 
		return !bIsTransient; 
	};

	struct FPlaylistSerializerPolicies : public FStructSerializerPolicies
	{
		FPlaylistSerializerPolicies()
		{
			PropertyFilter = TransientPropertyFiler;
		}
	};

	struct FPlaylistDeserializerPolicies : public FStructDeserializerPolicies
	{
		FPlaylistDeserializerPolicies()
		{
			PropertyFilter = TransientPropertyFiler;
		}
	};
	
	TArray<TSharedPtr<FJsonValue>> PagesToJsonObjects(UAvalanchePlaylist* InPlaylist, const TArray<int32>& InPageIds)
	{
		check(InPlaylist);
		TArray<TSharedPtr<FJsonValue>> PageEntries;
		PageEntries.Reserve(InPageIds.Num());

		for (const int32 PageId : InPageIds)
		{
			const FAvalanchePage& Page = InPlaylist->GetPage(PageId);

			if (Page.IsValidPage())
			{
				TSharedRef<FJsonObject> PageObject = MakeShared<FJsonObject>();
				FJsonObjectConverter::UStructToJsonObject(FAvalanchePage::StaticStruct(), &Page, PageObject, 0 /* CheckFlags */, 0 /* SkipFlags */);
				PageEntries.Add(MakeShared<FJsonValueObject>(PageObject));
			}
		}

		return PageEntries;
	}

	const FAvalanchePage& FindPage(const TArray<FAvalanchePage>& InPages, int32 InPageIdToFind)
	{
		for (const FAvalanchePage& Page : InPages)
		{
			if (Page.GetPageId() == InPageIdToFind)
			{
				return Page;
			}
		}
		return FAvalanchePage::NullPage;
	}
	
	const FAvalanchePage& FindTemplateForSourcePage(const UAvalanchePlaylist* InPlaylist, const FAvalanchePage& InSourcePage,
		const TArray<FAvalanchePage>& InSourceTemplates, FImportTemplateMap& InOutImportedTemplateIds)
	{
		{
			// Check if the template is already imported/existing at the given TemplateId.
			const FAvalanchePage& ExistingTemplate = InPlaylist->GetPage(InOutImportedTemplateIds.GetTemplateId(InSourcePage.GetTemplateId()));
			if (ExistingTemplate.IsValidPage() && ExistingTemplate.IsTemplate() && ExistingTemplate.GetAvalancheAssetPathDirect() == InSourcePage.GetAvalancheAssetPathDirect())
			{
				return ExistingTemplate;
			}
		}

		// Fallback: Try to find a match using the source template if available.
		const FAvalanchePage& SourceTemplate = FindPage(InSourceTemplates, InSourcePage.GetTemplateId());
		if (SourceTemplate.IsValidPage())
		{
			// Try to find that template in the playlist with an exact match (rc values, asset, etc).
			const FAvalanchePageCollection& PageCollection = InPlaylist->GetTemplatePages();
			for (const FAvalanchePage& ExistingTemplate : PageCollection.Pages)
			{
				if (ExistingTemplate.IsTemplateMatchingByValue(SourceTemplate))
				{
					// Keep track of the match we made for next time.
					InOutImportedTemplateIds.Add(InSourcePage.GetTemplateId(), ExistingTemplate.GetPageId());
					return ExistingTemplate;
				}
			}
		}
		
		return FAvalanchePage::NullPage;
	}

	bool CopyPageInPlace(UAvalanchePlaylist* InPlaylist, int32 InPageId, const FAvalanchePage& InSourcePage, int32 InTemplateId)
	{
		FAvalanchePage& DestinationPage = InPlaylist->GetPage(InPageId);
		if (DestinationPage.IsValidPage())
		{
			DestinationPage = InSourcePage;
			DestinationPage.SetPageId(InPageId);		// Restore page id.
			DestinationPage.SetTemplateId(InTemplateId);
			return true;
		}
		UE_LOG(LogAvaPlaylist, Error, TEXT("Failed to copy page in plage: page id %d is not found in destination rundown."), InPageId);
		return false;
	}
}

FString UE::AvaPlaylistEditor::Utils::GetImportFilepath(const TCHAR* InFileDescription, const TCHAR* InExtension)
{
	// Reference: UAssetToolsImpl::ExportAssetsInternal.
	TArray<FString> OpenFilenames;
	IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
	bool bOpened = false;
	if (DesktopPlatform)
	{
		if (Private::LastImportPath.IsEmpty())
		{
			Private::LastImportPath = FPaths::ProjectSavedDir();
		}
		
		const FString FileType = FString::Printf(TEXT("%s (*.%s)|*.%s"), InFileDescription, InExtension, InExtension);
		
		bOpened = DesktopPlatform->OpenFileDialog(
			FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr),
			FText::Format(LOCTEXT("Import_F", "Import {0}"), FText::FromString(InFileDescription)).ToString(),
			*Private::LastImportPath,
			TEXT(""),
			*FileType,
			EFileDialogFlags::None,
			OpenFilenames
		);
	}
	if (bOpened && OpenFilenames.Num() > 0 && OpenFilenames[0].IsEmpty() == false)
	{
		Private::LastImportPath = OpenFilenames[0];
		return OpenFilenames[0];
	}
	return FString();
}

FString UE::AvaPlaylistEditor::Utils::GetExportFilepath(const UObject* InObjectToExport, const TCHAR* InFileDescription, const TCHAR* InExtension)
{
	// Reference: UAssetToolsImpl::ExportAssetsInternal.
	TArray<FString> SaveFilenames;
	IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
	bool bSave = false;
	if (DesktopPlatform)
	{
		if (Private::LastExportPath.IsEmpty())
		{
			Private::LastExportPath = FPaths::ProjectSavedDir();
		}
		
		const FString FileType = FString::Printf(TEXT("%s (*.%s)|*.%s"), InFileDescription, InExtension, InExtension);
		
		bSave = DesktopPlatform->SaveFileDialog(
			FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr),
			FText::Format(LOCTEXT("Export_F", "Export: {0}"), FText::FromString(InObjectToExport->GetName())).ToString(),
			*Private::LastExportPath,
			*InObjectToExport->GetName(),
			*FileType,
			EFileDialogFlags::None,
			SaveFilenames
		);
	}
	if (bSave && SaveFilenames.Num() > 0)
	{
		Private::LastExportPath = SaveFilenames[0];
		return SaveFilenames[0];
	}
	return FString();
}

FString UE::AvaPlaylistEditor::Utils::GetSaveAssetAsPath(const FString& InDefaultPath, const FString& InDefaultAssetName)
{
	FSaveAssetDialogConfig SaveAssetDialogConfig;
	{
		SaveAssetDialogConfig.DefaultPath = InDefaultPath;
		SaveAssetDialogConfig.DefaultAssetName = InDefaultAssetName;
		SaveAssetDialogConfig.AssetClassNames.Add(UAvalanchePlaylist::StaticClass()->GetClassPathName());
		SaveAssetDialogConfig.ExistingAssetPolicy = ESaveAssetDialogExistingAssetPolicy::AllowButWarn;
		SaveAssetDialogConfig.DialogTitleOverride = LOCTEXT("SaveAssetDialogTitle", "Save Asset As");
	}
	
	const FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
	return ContentBrowserModule.Get().CreateModalSaveAssetDialog(SaveAssetDialogConfig);
}

FString UE::AvaPlaylistEditor::Utils::SerializePagesToJson(UAvalanchePlaylist* InPlaylist, const TArray<int32>& InPageIds)
{
	if (!IsValid(InPlaylist))
	{
		return FString();
	}
	
	const TArray<TSharedPtr<FJsonValue>> PageEntries = Private::PagesToJsonObjects(InPlaylist, InPageIds);
	
	if (PageEntries.IsEmpty())
	{
		return FString();
	}

	const TSharedRef<FJsonObject> RootJsonObject = MakeShared<FJsonObject>();
	RootJsonObject->SetArrayField(Private::PageEntriesName, PageEntries);

	FString SerializedString;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&SerializedString);

	FJsonSerializer::Serialize(RootJsonObject, Writer);
	
	return SerializedString;
}

TArray<FAvalanchePage> UE::AvaPlaylistEditor::Utils::DeserializePagesFromJson(const FString& InJsonString)
{
	TSharedPtr<FJsonObject> RootJsonObject;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(InJsonString);

	if (!FJsonSerializer::Deserialize(Reader, RootJsonObject))
	{
		UE_LOG(LogAvaPlaylist, Warning, TEXT("Unable to serialize the pasted text into Json format"));
		return {};
	}

	const TArray<TSharedPtr<FJsonValue>>* PageEntries;
	if (!RootJsonObject->TryGetArrayField(Private::PageEntriesName, PageEntries))
	{
		UE_LOG(LogAvaPlaylist, Warning, TEXT("Missing %s entry field in pasted text"), *Private::PageEntriesName);
		return {};
	}

	TArray<FAvalanchePage> Pages;
	Pages.Reserve(PageEntries->Num());
	for (const TSharedPtr<FJsonValue>& PageEntry : *PageEntries)
	{
		if (!PageEntry.IsValid() || PageEntry->Type != EJson::Object)
		{
			UE_LOG(LogAvaPlaylist, Warning, TEXT("Invalid page entry. Not an object"));
			continue;
		}

		const TSharedPtr<FJsonObject>& PageObject = PageEntry->AsObject();
		check(PageObject.IsValid());

		FAvalanchePage Page;
		if (FJsonObjectConverter::JsonObjectToUStruct(PageObject.ToSharedRef(), FAvalanchePage::StaticStruct(), &Page, 0 /* CheckFlags */, 0 /* SkipFlags */))
		{
			Pages.Emplace(MoveTemp(Page));
		}
		else
		{
			UE_LOG(LogAvaPlaylist, Warning, TEXT("Unable to convert Page Entry Json Object to Motion Design Page Struct"));
		}
	}
	return Pages;
}

TStrongObjectPtr<UAvalanchePlaylist> UE::AvaPlaylistEditor::Utils::ExportPagesToPlaylist(const UAvalanchePlaylist* InPlaylist, const TArray<int32>& InPageIds)
{
	if (!InPlaylist || InPageIds.IsEmpty())
	{
		return nullptr;
	}

	TSet<int32> AddedTemplates;
	TArray<FAvalanchePage> SourcePages;
	TArray<FAvalanchePage> SourceTemplates;
	bool bExportTemplates = false;
	
	for (const int32 PageId : InPageIds)
	{
		const FAvalanchePage& Page = InPlaylist->GetPage(PageId);
		if (!Page.IsValidPage())
		{
			continue;
		}

		if (Page.IsTemplate() && !AddedTemplates.Contains(Page.GetPageId()))
		{
			SourceTemplates.Add(Page);
			AddedTemplates.Add(Page.GetTemplateId());
			bExportTemplates = true; 
			continue;
		}
		
		SourcePages.Add(Page);
		if (!AddedTemplates.Contains(Page.GetTemplateId()))
		{
			const FAvalanchePage& Template = InPlaylist->GetPage(Page.GetTemplateId());
			if (Template.IsValidPage())
			{
				SourceTemplates.Add(Template);
				AddedTemplates.Add(Template.GetPageId());
			}
		}
	}

	if (!SourcePages.IsEmpty())
	{
		const TStrongObjectPtr<UAvalanchePlaylist> NewPlaylist(NewObject<UAvalanchePlaylist>());
		FImportTemplateMap ImportedTemplateIds;
		
		if (bExportTemplates)
		{
			ImportTemplatePages(NewPlaylist.Get(), SourceTemplates, ImportedTemplateIds);
		}
		
		ImportInstancedPages(NewPlaylist.Get(), UAvalanchePlaylist::InstancePageList, SourcePages, SourceTemplates, ImportedTemplateIds);
		return NewPlaylist;
	}

	return nullptr;
}

bool UE::AvaPlaylistEditor::Utils::SavePlaylistToXml(const UAvalanchePlaylist* InPlaylist, FArchive& InArchive, EXmlSerializationEncoding InXmlEncoding)
{
	if (IsValid(InPlaylist))
	{
		// Note: using the StructSerializerBackend produces a more compact format and is more suitable for exporting compared to
		// FXmlArchiveOutputFormatter. However, it doesn't support serializing UObject in place and the xml deserializer hasn't been
		// implemented yet. Support for serialization of UObjects in place is not planned to be needed for playlist at the moment.
		FXmlStructSerializerBackend Backend(InArchive, EStructSerializerBackendFlags::Default);
		FStructSerializer::Serialize(InPlaylist, *InPlaylist->GetClass(), Backend, Private::FPlaylistSerializerPolicies());
		Backend.SaveDocument(InXmlEncoding);
		return true;
	}
	return false;
}

bool UE::AvaPlaylistEditor::Utils::SavePlaylistToXml(const UAvalanchePlaylist* InPlaylist, const TCHAR* InFilepath)
{
	if (IsValid(InPlaylist))
	{
		const TUniquePtr<FArchive> FileWriter(IFileManager::Get().CreateFileWriter(InFilepath));
		if (FileWriter)	
		{
			const bool bSaved = SavePlaylistToXml(InPlaylist, *FileWriter, EXmlSerializationEncoding::Utf8);
			FileWriter->Close();
			return bSaved;
		}
	}
	return false;
}

bool UE::AvaPlaylistEditor::Utils::SavePlaylistToJson(const UAvalanchePlaylist* InPlaylist, FArchive& InArchive)
{
	if (IsValid(InPlaylist))
	{
		FJsonStructSerializerBackend Backend(InArchive, EStructSerializerBackendFlags::Default);
		FStructSerializer::Serialize(InPlaylist, *InPlaylist->GetClass(), Backend, Private::FPlaylistSerializerPolicies());
		return true;
	}
	return false;
}

bool UE::AvaPlaylistEditor::Utils::SavePlaylistToJson(const UAvalanchePlaylist* InPlaylist, const TCHAR* InFilepath)
{
	if (IsValid(InPlaylist))
	{
		const TUniquePtr<FArchive> FileWriter(IFileManager::Get().CreateFileWriter(InFilepath));
		if (FileWriter)
		{
			const bool bSerialized = SavePlaylistToJson(InPlaylist, *FileWriter);
			FileWriter->Close();
			return bSerialized;
		}
	}
	return false;
}

bool UE::AvaPlaylistEditor::Utils::LoadPlaylistFromJson(UAvalanchePlaylist* InPlaylist, FArchive& InArchive)
{
	if (IsValid(InPlaylist))
	{
		// Serializing doesn't reset content, it will add to it, so we need
		// to explicitly make the playlist empty first.
		if (!InPlaylist->Empty())
		{
			return false;
		}
		
		FJsonStructDeserializerBackend Backend(InArchive);
		const bool bLoaded = FStructDeserializer::Deserialize(InPlaylist, *InPlaylist->GetClass(), Backend, Private::FPlaylistDeserializerPolicies());
		if (bLoaded)
		{
			InPlaylist->PostLoad();
		}
		return bLoaded;
	}
	return false;
}

bool UE::AvaPlaylistEditor::Utils::LoadPlaylistFromJson(UAvalanchePlaylist* InPlaylist, const TCHAR* InFilepath)
{
	if (IsValid(InPlaylist))
	{
		const TUniquePtr<FArchive> FileReader(IFileManager::Get().CreateFileReader(InFilepath));
		if (FileReader)
		{
			const bool bLoaded = LoadPlaylistFromJson(InPlaylist, *FileReader);
			FileReader->Close();
			return bLoaded;
		}
	}
	return false;
}

bool UE::AvaPlaylistEditor::Utils::CanLoadPlaylistFromFile(const TCHAR* InFilepath)
{
	const FStringView Extension = FPathViews::GetExtension(InFilepath);
	
	// We only support loading from json file (LoadPlaylistFromJson).
	return Extension.Equals(TEXT("json"), ESearchCase::IgnoreCase);
}

TArray<int32> UE::AvaPlaylistEditor::Utils::ImportTemplatePages(UAvalanchePlaylist* InPlaylist, const TArray<FAvalanchePage>& InSourceTemplates, FImportTemplateMap& OutImportedTemplateIds)
{
	if (!IsValid(InPlaylist))
	{
		return {};
	}

	TArray<int32> OutTemplateIds;
	OutTemplateIds.Reserve(InSourceTemplates.Num());

	for (const FAvalanchePage& SourceTemplate : InSourceTemplates)
	{
		const int32 SourceTemplateId = SourceTemplate.IsTemplate() ? SourceTemplate.GetPageId() : SourceTemplate.GetTemplateId();
		// Try to add the template with the id it had in the original list.
		int32 ImportedTemplateId = InPlaylist->AddTemplate(FAvaPageIdGeneratorParams(SourceTemplateId));
		
		if (ImportedTemplateId != FAvalanchePage::InvalidPageId)
		{
			OutImportedTemplateIds.Add(SourceTemplateId, ImportedTemplateId);
			ensure(Private::CopyPageInPlace(InPlaylist, ImportedTemplateId, SourceTemplate, FAvalanchePage::InvalidPageId));
			OutTemplateIds.Add(ImportedTemplateId);
		}
	}
	return OutTemplateIds;
}

TArray<int32> UE::AvaPlaylistEditor::Utils::ImportInstancedPages(
	UAvalanchePlaylist* InPlaylist,
	const FAvaPageListReference& InPageListReference,
	const TArray<FAvalanchePage>& InSourcePages,
	const TArray<FAvalanchePage>& InSourceTemplates,
	FImportTemplateMap& InOutImportedTemplateIds,
	const FAvaPageInsertPosition& InInsertPosition)
{
	if (!IsValid(InPlaylist))
	{
		return {};
	}
	
	TArray<int32> OutPageIds;

	FAvaPageInsertPosition InsertPosition = InInsertPosition;

	// If we are adding above, iteration should be reversed, so the last is added first
	// and the next to last added above that, etc.
	const bool bReverseIteration = InsertPosition.IsValid() && !InsertPosition.bAddBelow;
	
	for (int32 PageIndex = 0; PageIndex < InSourcePages.Num(); ++PageIndex)
	{
		const FAvalanchePage& SourcePage = InSourcePages[bReverseIteration ? InSourcePages.Num() - PageIndex - 1 : PageIndex];
		
		if (!SourcePage.IsValidPage())
		{
			continue;
		}

		// Attempt to find/create template for this page
		int32 ImportedTemplateId = FAvalanchePage::InvalidPageId;

		if (SourcePage.IsTemplate())
		{
			// Todo: untested case.
			// Suspect this of being wrong. if the page is a template, then it needs to be imported
			// as a template, i.e. ImportTemplatePages.
			ImportedTemplateId = SourcePage.GetPageId();
		}
		else
		{
			// The source page has a source template id. It may not match the destination, this is
			// why we rely on the ImportedTemplateIds to translate that. As a fallback, if the source
			// templates are provided, it will try to match templates by values.
			const FAvalanchePage& ExistingTemplate = Private::FindTemplateForSourcePage(InPlaylist, SourcePage, InSourceTemplates, InOutImportedTemplateIds);

			if (ExistingTemplate.IsValidPage())
			{
				ImportedTemplateId = ExistingTemplate.GetPageId();
			}
			else
			{
				// Try to add the template with the id it had in the original list.
				ImportedTemplateId = InPlaylist->AddTemplate(FAvaPageIdGeneratorParams(SourcePage.GetTemplateId()));
				
				if (ImportedTemplateId != FAvalanchePage::InvalidPageId)
				{
					// Keep track of correspondence for next pages in the list.
					InOutImportedTemplateIds.Add(SourcePage.GetTemplateId(), ImportedTemplateId);
					
					// We either add the current page "as template", or use the source templates if provided.
					const FAvalanchePage& SourceTemplate = Private::FindPage(InSourceTemplates, SourcePage.GetTemplateId());
					
					ensure(Private::CopyPageInPlace(InPlaylist, ImportedTemplateId, SourceTemplate.IsValidPage() ? SourceTemplate : SourcePage, FAvalanchePage::InvalidPageId));
				}
				else
				{
					// We are unable to find/create a template for this page.
					continue;
				}
			}
		}

		// There is a valid imported template at this point.
		check(ImportedTemplateId != FAvalanchePage::InvalidPageId);

		// We're pasting to the instance list, so just add the page
		if (InPageListReference.Type == EAvaPageListType::Instance)
		{
			// We want to preserve the source PageId if possible.
			const FAvaPageIdGeneratorParams NewPageIdParams =
				FAvaPageIdGeneratorParams::FromInsertPositionOrSourceId(SourcePage.GetPageId(), InsertPosition);
			
			int32 ImportedPageId = InPlaylist->AddPageFromTemplate(ImportedTemplateId, NewPageIdParams, InsertPosition);

			if (ImportedPageId != FAvalanchePage::InvalidPageId)
			{
				ensure(Private::CopyPageInPlace(InPlaylist, ImportedPageId, SourcePage, ImportedTemplateId));
				OutPageIds.Add(ImportedPageId);			
				InsertPosition.ConditionalUpdateAdjacentId(ImportedPageId); // Update for next insertion.
			}

			continue;
		}

		// We're pasting to a sub list, so we need to check if the page exists in the instance list first
		const FAvalanchePage& InstancedPage = InPlaylist->GetPage(SourcePage.GetPageId());
		int32 InstancedPageId = InstancedPage.GetPageId();

		// Add it if it's missing
		if (!InstancedPage.IsValidPage() || InstancedPage.IsTemplate())
		{
			// We want to preserve the source PageId if possible.
			const FAvaPageIdGeneratorParams NewPageIdParams =
				FAvaPageIdGeneratorParams::FromInsertPositionOrSourceId(!InstancedPage.IsTemplate() ? SourcePage.GetPageId() : FAvalanchePage::InvalidPageId, InsertPosition);

			// Note: If the page is a template, it will already be imported as templateId.
			InstancedPageId = InPlaylist->AddPageFromTemplate(ImportedTemplateId, NewPageIdParams, InsertPosition);

			if (InstancedPageId != FAvalanchePage::InvalidPageId)
			{
				ensure(Private::CopyPageInPlace(InPlaylist, InstancedPageId, SourcePage, ImportedTemplateId));
				InsertPosition.ConditionalUpdateAdjacentId(InstancedPageId); // Update for next insertion.
			}
			else
			{
				// We were unable to create the instance page, so it cannot be added to a sub list
				continue;
			}
		}

		// There is a valid imported instanced page at this point.
		check(InstancedPageId != FAvalanchePage::InvalidPageId);

		// Now we have our instance page reference, add it to the sublist
		if (InPlaylist->AddPageToSubList(InPageListReference.SubListIndex, InstancedPageId))
		{
			OutPageIds.Add(InstancedPageId);
		}
	}

	if (bReverseIteration)
	{
		Algo::Reverse(OutPageIds);
	}
	
	return OutPageIds;
}

TArray<int32> UE::AvaPlaylistEditor::Utils::ImportInstancedPagesFromPlaylist(UAvalanchePlaylist* InPlaylist, const UAvalanchePlaylist* InSourcePlaylist, const FAvaPageInsertPosition& InInsertPosition)
{
	if (!IsValid(InPlaylist) || !IsValid(InSourcePlaylist))
	{
		return {};
	}
	
	const FAvalanchePageCollection& SourceTemplates = InSourcePlaylist->GetTemplatePages();
	const FAvalanchePageCollection& SourcePages = InSourcePlaylist->GetInstancedPages();
	FImportTemplateMap ImportedTemplateIds;
	
	return ImportInstancedPages(InPlaylist, UAvalanchePlaylist::InstancePageList, SourcePages.Pages, SourceTemplates.Pages, ImportedTemplateIds, InInsertPosition);
}

UAvalanchePlaylist* UE::AvaPlaylistEditor::Utils::SaveDuplicatePlaylist(UAvalanchePlaylist* InSourcePlaylist, const FString& InAssetName, const FString& InPackagePath)
{
	return Cast<UAvalanchePlaylist>(IAssetTools::Get().DuplicateAsset(InAssetName, InPackagePath, InSourcePlaylist));
}

#undef LOCTEXT_NAMESPACE
