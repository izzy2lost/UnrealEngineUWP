// Copyright Epic Games, Inc. All Rights Reserved.
#include "AssetDefinition_MidiFile.h"

#include "Algo/AnyOf.h"
#include "ContentBrowserMenuContexts.h"
#include "HarmonixMidi/MidiFile.h"
#include "ToolMenus.h"
#include "DesktopPlatformModule.h"
#include "Framework/Application/SlateApplication.h"
#include "Misc/MessageDialog.h"
#include "GenericPlatform/GenericPlatformMisc.h"

#define LOCTEXT_NAMESPACE "Harmonix_Midi"

FString UAssetDefinition_MidiFile::LastMidiExportFolder;

TSoftClassPtr<UObject> UAssetDefinition_MidiFile::GetAssetClass() const
{
	return UMidiFile::StaticClass();
}

FText UAssetDefinition_MidiFile::GetAssetDisplayName() const
{
	return NSLOCTEXT("AssetTypeActions", "MIDIFileDefinition", "Standard MIDI File");
}

FLinearColor  UAssetDefinition_MidiFile::GetAssetColor() const
{

	return FLinearColor(1.0f, 0.5f, 0.0f);
}

TConstArrayView<FAssetCategoryPath> UAssetDefinition_MidiFile::GetAssetCategories() const
{
	static const auto Categories = { EAssetCategoryPaths::Audio / NSLOCTEXT("Harmonix", "HmxAssetCategoryName", "Harmonix") };
	return Categories;
}

bool UAssetDefinition_MidiFile::CanImport() const
{
	return true;
}

void UAssetDefinition_MidiFile::RegisterContextMenu()
{
	UToolMenu* Menu = UToolMenus::Get()->ExtendMenu("ContentBrowser.AssetContextMenu.MidiFile");
	FToolMenuSection& Section = Menu->FindOrAddSection("GetAssetActions");

	Section.AddDynamicEntry("MidiFile_ExportMid",
		FNewToolMenuSectionDelegate::CreateLambda([](FToolMenuSection& InSection)
			{
				const TAttribute<FText> Label = LOCTEXT("MidiFile_ExportMid", "Export Standard MIDI File (.mid)");
				const TAttribute<FText> ToolTip = LOCTEXT("MidiFile_ExportMidToolTip", "Exports standard MIDI file(s)");
				const FSlateIcon Icon = FSlateIcon(FAppStyle::GetAppStyleSetName(), "ClassIcon.MidiFile");
				const FToolMenuExecuteAction UIAction = FToolMenuExecuteAction::CreateStatic(&UAssetDefinition_MidiFile::ExecuteExportMidiFile);
				InSection.AddMenuEntry("MidiFile_ExportMid", Label, ToolTip, Icon, UIAction);
			}));
}

void UAssetDefinition_MidiFile::ExecuteExportMidiFile(const FToolMenuContext& MenuContext)
{
	if (const UContentBrowserAssetContextMenuContext* Context = UContentBrowserAssetContextMenuContext::FindContextWithAssets(MenuContext))
	{
		if (Context->SelectedAssets.Num() > 1)
		{
			ExportAllMidiToFolder(Context);
		}
		else
		{
			IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
			if (DesktopPlatform)
			{
				UMidiFile* MidiFile = Context->LoadFirstSelectedObject<UMidiFile>();
				FString	DefaultFileName = MidiFile->GetName() + TEXT(".mid");
				TArray<FString> SaveFileNames;
				const bool bFileSelected = DesktopPlatform->SaveFileDialog(
					FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr),
					LOCTEXT("MidiFile_ExportMid_SaveFileDialogTitle", "Save Standard MIDI File as...").ToString(),
					LastMidiExportFolder.IsEmpty() ? FPaths::ProjectDir() : LastMidiExportFolder,
					DefaultFileName,
					TEXT("Standard MIDI File (*.mid)|*.mid"),
					EFileDialogFlags::None,
					SaveFileNames);

				if (!bFileSelected)
				{
					return;
				}

				if (ensure(SaveFileNames.Num() == 1))
				{
					FString OutputFileName = SaveFileNames[0];
					MidiFile->SaveStdMidiFile(OutputFileName);
					LastMidiExportFolder = FPaths::GetPath(OutputFileName);
				}
			}
		}
	}
}

void UAssetDefinition_MidiFile::ExportAllMidiToFolder(const UContentBrowserAssetContextMenuContext* Context)
{
	IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
	if (DesktopPlatform)
	{
		FString SelectedFolderName;
		if (DesktopPlatform->OpenDirectoryDialog(FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr), "Select destination for Standard MIDI Files...", LastMidiExportFolder.IsEmpty() ? FPaths::ProjectDir() : LastMidiExportFolder, SelectedFolderName))
		{
			LastMidiExportFolder = SelectedFolderName;
			bool bKeepWarning = true;
			for (UMidiFile* MidiFile : Context->LoadSelectedObjects<UMidiFile>())
			{
				FString OutFilePath = FPaths::Combine(LastMidiExportFolder, MidiFile->GetName() + TEXT(".mid"));
				if (bKeepWarning && FPaths::FileExists(OutFilePath))
				{
					EAppReturnType::Type AskResult = AskOverwrite(OutFilePath);
					if (AskResult == EAppReturnType::NoAll)
					{
						break;
					}
					if (AskResult == EAppReturnType::No)
					{
						continue;
					}
					if (AskResult == EAppReturnType::YesAll)
					{
						bKeepWarning = false;
					}
				}
				MidiFile->SaveStdMidiFile(OutFilePath);
			}
		}
	}
}

EAppReturnType::Type UAssetDefinition_MidiFile::AskOverwrite(FString& OutPath)
{
	FText OutPathText = FText::FromString(*OutPath);
	return FMessageDialog::Open(EAppMsgCategory::Warning, EAppMsgType::YesNoYesAllNoAll, 
		FText::Format(LOCTEXT("MidiFile_ExportMid_OverwriteMessage", "{0} exists.\n\nWould you like to overwrite it?"), OutPathText));
}

#undef LOCTEXT_NAMESPACE
