// Copyright Epic Games, Inc. All Rights Reserved.

#include "MidiFileFactory.h"
#include "HarmonixMidi/MidiConstants.h"
#include "HarmonixMidiEditorModule.h"
#include "EditorFramework/AssetImportData.h"
#include "Misc/MessageDialog.h"
#include "Misc/Paths.h"

UMidiFileFactory::UMidiFileFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bCreateNew = false;
	SupportedClass = UMidiFile::StaticClass();

	bEditorImport = true;
	bText = false;

	Formats.Add(TEXT("mid;Standard MIDI File"));
}

FText UMidiFileFactory::GetDisplayName() const
{
	return NSLOCTEXT("MIDI", "MIDIImporterFactoryName", "Standard MIDI File");
}

FText UMidiFileFactory::GetToolTip() const
{
	return NSLOCTEXT("MIDI", "MIDIImporterFactoryDescription", "Standard MIDI Files exported from Digital Audio Workstations");
}

bool UMidiFileFactory::FactoryCanImport(const FString& Filename)
{
	return true;
}

UObject* UMidiFileFactory::FactoryCreateFile(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, const FString& Filename, const TCHAR* Parms, FFeedbackContext* Warn, bool& bOutOperationCanceled)
{
	UMidiFile* MidiFileAsset = FindObject<UMidiFile>(InParent, *InName.ToString());
	if (!MidiFileAsset)
	{
		MidiFileAsset = NewObject<UMidiFile>(InParent, InClass, InName, Flags);
	}
	MidiFileAsset->LoadStdMidiFile(Filename, Harmonix::Midi::Constants::GTicksPerQuarterNoteInt);
	
	//add the current file to the file array (for handling multi-batch import)
	ImportedFiles.Add(MidiFileAsset);
	
	return MidiFileAsset;
}

bool UMidiFileFactory::CanReimport(UObject* Obj, TArray<FString>& OutFilenames)
{
	if (UMidiFile* AsMidiFile = Cast<UMidiFile>(Obj))
	{
		FString FilePath = AsMidiFile->GetImportedSrcFilePath();
		if (FilePath.IsEmpty())
		{
			UE_LOG(LogHarmonixMidiEditor, Warning, TEXT("Couldn't find source path for %s!"), *AsMidiFile->GetPathName());
			return false;
		}
		OutFilenames.Push(FilePath);
		return true;
	}
	return false;
}

void UMidiFileFactory::SetReimportPaths(UObject* Obj, const TArray<FString>& NewReimportPaths)
{
	if (UMidiFile* AsMidiFile = Cast<UMidiFile>(Obj))
	{
		AsMidiFile->AssetImportData->UpdateFilenameOnly(FPaths::ConvertRelativePathToFull(NewReimportPaths[0]));
	}
}

void UMidiFileFactory::CleanUp()
{ 
	for (int FileIndex = 0; FileIndex < ImportedFiles.Num(); FileIndex++)
	{
		UMidiFile* MidiFileAsset = ImportedFiles[FileIndex];
		//only prompt this dialog if imported file(s) can be conformed by at least one conform option (round up or round down)
		if (MidiFileAsset->ShouldConformMidiFileLength(EMidiFileLengthConformOption::RoundDown) || MidiFileAsset->ShouldConformMidiFileLength(EMidiFileLengthConformOption::RoundUp))
		{
			if (bShouldApplyToAll && FileIndex > 0)
			{
				MidiFileAsset->ConformMidiFileLength(ApplyToAllOption);
				continue;
			}
			ShowConformMidiFileLengthDialog(FileIndex);
		}

	}
	ImportedFiles.Empty();
	bShouldApplyToAll = true;
	ApplyToAllOption = EMidiFileLengthConformOption::RoundUp;
}

void UMidiFileFactory::ShowConformMidiFileLengthDialog(int32 MidiFileAssetIndex)
{
	UMidiFile* MidiFileAsset = ImportedFiles[MidiFileAssetIndex];
	
	//show the dialog widgets on a window
	TSharedPtr<SWindow> ParentWindow;
	SAssignNew(ParentWindow, SWindow)
		.Title(FText::FromString(TEXT("Conform MIDI File Length")))
		.ClientSize(FVector2D(490, 250));
	
	//the custom pop up dialog that asks for conforming midi file length
	TSharedRef<SConformMidiFileLengthDialog> ConformMidiFileLengthDialog = SNew(SConformMidiFileLengthDialog);
	ParentWindow->SetContent(ConformMidiFileLengthDialog);

	// enable the Apply To All checkbox if importing more than 1 file 
	(ImportedFiles.Num() > 1 && MidiFileAssetIndex != ImportedFiles.Num() - 1) ? ConformMidiFileLengthDialog->ApplyToAllCheckBox->SetVisibility(EVisibility::Visible) : ConformMidiFileLengthDialog->ApplyToAllCheckBox->SetVisibility(EVisibility::Collapsed);

	// we'll need this for a few options down below...	
	float MidiFileFractionalLength = MidiFileAsset->GetSongMaps()->GetBarIncludingCountInAtTick(MidiFileAsset->GetLastEventTick() + 1);

	//if the file has length less than 1 bar (e.g. 0.25 bars),it cannot be rounded down to 0 bar, this is checked in UMidiFile::ShouldConformFileLength(EMidiFileLengthConformOption Option)
	//disable the Round Down checkbox and the Round To Nearest checkbox, the only available option should be Round Up
	if (!MidiFileAsset->ShouldConformMidiFileLength(EMidiFileLengthConformOption::RoundDown) && MidiFileAsset->ShouldConformMidiFileLength(EMidiFileLengthConformOption::RoundUp))
	{
		ConformMidiFileLengthDialog->RoundDownCheckBox->SetEnabled(false);
		ConformMidiFileLengthDialog->RoundDownCheckBox->SetIsChecked(ECheckBoxState::Unchecked);

		ConformMidiFileLengthDialog->RoundToNearestCheckBox->SetEnabled(false);
		ConformMidiFileLengthDialog->RoundToNearestCheckBox->SetIsChecked(ECheckBoxState::Unchecked);

		ConformMidiFileLengthDialog->RoundUpCheckBox->SetIsChecked(ECheckBoxState::Checked);
		ConformMidiFileLengthDialog->ConformOption = EMidiFileLengthConformOption::RoundUp;
	}
	else
	{
		ConformMidiFileLengthDialog->RoundDownCheckBox->SetIsChecked(ECheckBoxState::Unchecked);
		ConformMidiFileLengthDialog->RoundToNearestCheckBox->SetIsChecked(ECheckBoxState::Unchecked);
		ConformMidiFileLengthDialog->RoundUpCheckBox->SetIsChecked(ECheckBoxState::Unchecked);
		if (FMath::Frac(MidiFileFractionalLength) < 0.1f || FMath::Frac(MidiFileFractionalLength) > 0.9f)
		{
			ConformMidiFileLengthDialog->RoundToNearestCheckBox->SetIsChecked(ECheckBoxState::Checked);
			ConformMidiFileLengthDialog->ConformOption = EMidiFileLengthConformOption::Nearest;
		}
		else if (FMath::Frac(MidiFileFractionalLength) < 0.2f)
		{
			ConformMidiFileLengthDialog->RoundDownCheckBox->SetIsChecked(ECheckBoxState::Checked);
			ConformMidiFileLengthDialog->ConformOption = EMidiFileLengthConformOption::RoundDown;
		}
		else
		{
			ConformMidiFileLengthDialog->RoundUpCheckBox->SetIsChecked(ECheckBoxState::Checked);
			ConformMidiFileLengthDialog->ConformOption = EMidiFileLengthConformOption::RoundUp;
		}
	}

	//set text for text block that asks user whether or not to conform midi file length
	FString MidiFileName = MidiFileAsset->GetName();
	ConformMidiFileLengthDialog->AskConformFileLengthText->SetText(FText::FromString(FString::Printf(
		TEXT("MIDI File Name: %s.mid, Length: %.5f bars"),
		*MidiFileName,
		MidiFileFractionalLength)));

	//Set callback function for Ok button
	ConformMidiFileLengthDialog->OkButton->SetOnClicked(FOnClicked::CreateLambda([this, ParentWindow, ConformMidiFileLengthDialog, &MidiFileAsset]()-> FReply {
		MidiFileAsset->ConformMidiFileLength(ConformMidiFileLengthDialog->ConformOption);
		ParentWindow->RequestDestroyWindow();
		return FReply::Handled();
		}));

	//show the dialog
	FSlateApplication::Get().AddModalWindow(ParentWindow.ToSharedRef(), nullptr);
		
	//remember the current conform option and mark that this option should be apply to all other files imported
	if (ConformMidiFileLengthDialog->ApplyToAll)
	{
		ApplyToAllOption = ConformMidiFileLengthDialog->ConformOption;
		bShouldApplyToAll = true;
	}
	else {
		bShouldApplyToAll = false;
	}
}

EReimportResult::Type UMidiFileFactory::Reimport(UObject* Obj)
{
	UMidiFile* AsMidiFile = Cast<UMidiFile>(Obj);
	if (!AsMidiFile)
		return EReimportResult::Failed;

	FString ReimportPath = AsMidiFile->GetImportedSrcFilePath();
	if (ReimportPath.IsEmpty())
	{
		UE_LOG(LogHarmonixMidiEditor, Warning, TEXT("Failed to reimport MIDI file: %s"), *AsMidiFile->GetFullName());
		return EReimportResult::Failed;
	}
	
	AsMidiFile->LoadStdMidiFile(ReimportPath, Harmonix::Midi::Constants::GTicksPerQuarterNoteInt);

	//Allow file length to be reconformed after reimport 
	AsMidiFile->bLengthRoundedDown = false;
	AsMidiFile->bLengthRoundedUp = false;
	AsMidiFile->bLengthRoundedToNearest = false;

	return EReimportResult::Succeeded;
	
}

void SConformMidiFileLengthDialog::Construct(const FArguments& InArgs)
{
	ChildSlot
	[
		SNew(SBorder)
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		.BorderImage(FCoreStyle::Get().GetBrush("ToolPanel.GroupBorder"))
		[
			SNew(SVerticalBox)
			// Text for the conform option
			+ SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			.Padding(10)
			[
				SAssignNew(AskConformFileLengthText,STextBlock)
				.TextStyle(&MidiFileInformationStyle)
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			.Padding(5)
			[
				SNew(STextBlock)
				.Text(FText::FromString(TEXT(
					"Currently, all MIDI asset lengths must be conformed to some full number of bars."
					"\n\n      Some or all of the MIDI files you are importing have fractional bar counts."
					"\n\n                                        How would you like them conformed?\n")))
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(5)
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Center)
			[
				// Round Up checkbox
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.Padding(5, 5)
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				.AutoWidth()
				[
					SAssignNew(RoundUpCheckBox, SCheckBox)
					.ToolTipText(FText::FromString(TEXT("this does NOT change/add events in file because file length is automatically rounded up to the next integer bar after import")))
					.OnCheckStateChanged(this, &SConformMidiFileLengthDialog::HandleConformOptionCheckboxChanged, EMidiFileLengthConformOption::RoundUp)
					.IsChecked_Lambda([this]() { return ConformOption == EMidiFileLengthConformOption::RoundUp ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
					.Content()
					[
						SNew(STextBlock)
						.Text(FText::FromString(TEXT("Round Up")))
					]
				]
				// Round Down checkbox
				+ SHorizontalBox::Slot()
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				.AutoWidth()
				.Padding(5, 5)
				[
					SAssignNew(RoundDownCheckBox, SCheckBox)
					.OnCheckStateChanged(this, &SConformMidiFileLengthDialog::HandleConformOptionCheckboxChanged, EMidiFileLengthConformOption::RoundDown)
					.IsChecked_Lambda([this]() { return ConformOption == EMidiFileLengthConformOption::RoundDown ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
					.Content()
					[
						SNew(STextBlock)
						.ToolTipText(FText::FromString(TEXT("move MIDI events in file that exceed the last integer bar to the last tick of that bar and remove excessive events on that tick")))
						.Text(FText::FromString(TEXT("Round Down")))
					]
				]
				//Round To Nearest checkbox
				+ SHorizontalBox::Slot()
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				.AutoWidth()
				.Padding(5, 5)
				[
					SAssignNew(RoundToNearestCheckBox, SCheckBox)
					.OnCheckStateChanged(this, &SConformMidiFileLengthDialog::HandleConformOptionCheckboxChanged, EMidiFileLengthConformOption::Nearest)
					.IsChecked_Lambda([this]() { return ConformOption == EMidiFileLengthConformOption::Nearest ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
					.Content()
					[
						SNew(STextBlock)
						.ToolTipText(FText::FromString(TEXT("round MIDI File length to the nearest integer bar (either rounding up or rounding down)")))
						.Text(FText::FromString(TEXT("Round to Nearest")))
					]
				]
			]
			// "Apply to All" checkbox (visible only if ApplyToAll is true)
			+ SVerticalBox::Slot()
			.AutoHeight()
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Center)
			.Padding(5)
			[
				SNew(SBox)
				[
					SAssignNew(ApplyToAllCheckBox, SCheckBox)
					.OnCheckStateChanged(this, &SConformMidiFileLengthDialog::HandleApplyToAllCheckboxChanged)
					.IsChecked_Lambda([this]() { return ApplyToAll ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
					.Content()
					[
						SNew(STextBlock)
						.Text(FText::FromString(TEXT("Apply to All")))
					]
				]
			]
			// "Ok" button
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(15)
			.VAlign(VAlign_Bottom)
			.HAlign(HAlign_Center)
			[
				SAssignNew(OkButton, SButton)
				.Text(FText::FromString(TEXT("Ok")))
			]
		]	
	];
}

void SConformMidiFileLengthDialog::HandleConformOptionCheckboxChanged(ECheckBoxState NewState, EMidiFileLengthConformOption Option)
{
	if (NewState == ECheckBoxState::Checked)
	{
		ConformOption = Option;
	}
}

void SConformMidiFileLengthDialog::HandleApplyToAllCheckboxChanged(ECheckBoxState NewState)
{
	ApplyToAll = (NewState == ECheckBoxState::Checked);
}
