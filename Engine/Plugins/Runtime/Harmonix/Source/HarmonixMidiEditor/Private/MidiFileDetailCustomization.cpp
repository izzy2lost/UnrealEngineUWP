// Copyright Epic Games, Inc. All Rights Reserved.

#include "MidiFileDetailCustomization.h"
#include "HarmonixMidi/MidiFile.h"

void FMidiFileDetailCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailLayout)
{
	//Details Panel for the UROPERTY AssetImportData, no actual customization, just reordering its position 
	IDetailCategoryBuilder& MidiFileImportSettingsCategory = DetailLayout.EditCategory(TEXT("Import Settings"));
	TSharedPtr<IPropertyHandle> AssetImportHandle = DetailLayout.GetProperty(GET_MEMBER_NAME_CHECKED(UMidiFile, AssetImportData));
	MidiFileImportSettingsCategory.AddProperty(AssetImportHandle);

	//Details Panel for the UROPERTY Start Bar, no actual customization, just reordering its position 
	IDetailCategoryBuilder& MidiFileStartBarCategory = DetailLayout.EditCategory(TEXT("Midi File Start Bar"));
	TSharedPtr<IPropertyHandle> StartBarHandle = DetailLayout.GetProperty(GET_MEMBER_NAME_CHECKED(UMidiFile, StartBar));
	MidiFileStartBarCategory.AddProperty(StartBarHandle);

	//Detail Customization for UProperty FileLengthBars
	IDetailCategoryBuilder& MidiFileLengthCategory = DetailLayout.EditCategory(TEXT("Midi File Length"));

	//Get the Midi File that is being edited
	TArray<TWeakObjectPtr<UObject>> Objects;
	DetailLayout.GetObjectsBeingCustomized(Objects);
	if (Objects.Num() != 1)
	{
		return;
	}
	MidiFile = Objects.Last();
	TWeakObjectPtr<UMidiFile> MidiFileBeingEdited = Cast<UMidiFile>(MidiFile);
	
	//determine if file length is already conformed during import:
	//if it's already conformed just display the conformed length otherwise the original fractional length
	int32 MidiFileLengthConformed = MidiFileBeingEdited->GetSongMaps()->GetSongLengthData().LengthBars;
	float MidiFileFractionalLength = MidiFileBeingEdited->GetSongMaps()->GetBarIncludingCountInAtTick(MidiFileBeingEdited->GetLastEventTick());
	if (MidiFileBeingEdited->bLengthRoundedUp)
	{
		//if the file length is 1 bar, it CANNOT be rounded down to 0 bar
		//otherwise still allowing it to be conformed by rounding down by showing a message on a  tooptip 
		if (MidiFileLengthConformed != 1)
		{
			RoundedLengthToolTipText = FText::FromString(FString::Printf(TEXT("*rounded up from file length of %.3f bars, can still round down"), MidiFileFractionalLength));
			FileLengthText = FText::FromString(FString::Printf(TEXT("%d Bar(s)*"), MidiFileLengthConformed));
		}
		else {
			RoundedLengthToolTipText = FText::FromString(TEXT(""));
			FileLengthText = FText::FromString(FString::Printf(TEXT("%d Bar(s)"), MidiFileLengthConformed));
		}

	}
	else if (MidiFileBeingEdited->bLengthRoundedDown)
	{
		RoundedLengthToolTipText = FText::FromString(TEXT(""));
		FileLengthText = FText::FromString(FString::Printf(TEXT("%d Bar(s)"), MidiFileLengthConformed));
	}
	else {
		RoundedLengthToolTipText = FText::FromString(TEXT(""));
		//if the file doesn't need to be conformed (and hence never conformed), display the integer bar length, otherwise display fractional bar length
		if (!MidiFileBeingEdited->ShouldConformMidiFileLength(EMidiFileLengthConformOption::RoundDown) && !MidiFileBeingEdited->ShouldConformMidiFileLength(EMidiFileLengthConformOption::RoundUp))
		{
			FileLengthText = FText::FromString(FString::Printf(TEXT("%d Bar(s)"), (int32)MidiFileFractionalLength));
		}
		else {
			FileLengthText = FText::FromString(FString::Printf(TEXT("%.3f Bar(s)"), MidiFileFractionalLength));
		}
	}

	//determine whether the file length should be conformed, disable buttons if not
	auto ShouldConformRoundDown = [MidiFileBeingEdited]
	{
		return MidiFileBeingEdited->ShouldConformMidiFileLength(EMidiFileLengthConformOption::RoundDown);
	};

	auto ShouldConformRoundUp = [this,MidiFileBeingEdited,&MidiFileLengthCategory]
	{
		return MidiFileBeingEdited->ShouldConformMidiFileLength(EMidiFileLengthConformOption::RoundUp);
	};

	auto ShouldConformRoundToNearest = [MidiFileBeingEdited]
	{
		return MidiFileBeingEdited->ShouldConformMidiFileLength(EMidiFileLengthConformOption::Nearest);
	};


	//conform buttons OnClick callback function
	auto OnLengthConformedRoundDown = [this, MidiFileBeingEdited, &MidiFileLengthCategory,MidiFileFractionalLength]
	{
		if (MidiFileBeingEdited.IsValid())
			MidiFileBeingEdited->ConformMidiFileLength(EMidiFileLengthConformOption::RoundDown);

		//update display text for file length
		FileLengthText = FText::FromString(FString::Printf(TEXT("%d Bar(s)"), MidiFileBeingEdited->GetSongMaps()->GetSongLengthData().LengthBars));
		FileLengthTextBlock->SetText(FileLengthText);
		RoundedLengthToolTipText = FText::FromString(TEXT(""));
		FileLengthTextBlock->SetToolTipText(RoundedLengthToolTipText);

		return FReply::Handled();
	};

	auto OnLengthConformedRoundUp = [this, MidiFileBeingEdited, &MidiFileLengthCategory,MidiFileFractionalLength]
	{
		if (MidiFileBeingEdited.IsValid())
			MidiFileBeingEdited->ConformMidiFileLength(EMidiFileLengthConformOption::RoundUp);
		
		//update display text for file length
		FileLengthText = FText::FromString(FString::Printf(TEXT("%d Bar(s)*"), MidiFileBeingEdited->GetSongMaps()->GetSongLengthData().LengthBars));
		FileLengthTextBlock->SetText(FileLengthText);
		RoundedLengthToolTipText = FText::FromString(FString::Printf(TEXT("*rounded up from file length of %.3f bars, can still round down"), MidiFileFractionalLength));
		FileLengthTextBlock->SetToolTipText(RoundedLengthToolTipText);

		return FReply::Handled();
	};

	auto OnLengthConformedRoundToNearest = [this, MidiFileBeingEdited, &MidiFileLengthCategory, MidiFileFractionalLength]
	{
		if (MidiFileBeingEdited.IsValid())
			MidiFileBeingEdited->ConformMidiFileLength(EMidiFileLengthConformOption::Nearest);

	
		if (MidiFileBeingEdited->bLengthRoundedDown && MidiFileBeingEdited->bLengthRoundedToNearest)
		{
			FileLengthText = FText::FromString(FString::Printf(TEXT("%d Bar(s)"), MidiFileBeingEdited->GetSongMaps()->GetSongLengthData().LengthBars));
			RoundedLengthToolTipText = FText::FromString(TEXT(""));
		}

		if (MidiFileBeingEdited->bLengthRoundedUp && MidiFileBeingEdited->bLengthRoundedToNearest)
		{
			FileLengthText = FText::FromString(FString::Printf(TEXT("%d Bar(s)*"), MidiFileBeingEdited->GetSongMaps()->GetSongLengthData().LengthBars));
			RoundedLengthToolTipText = FText::FromString(FString::Printf(TEXT("*rounded up from file length of %.3f bars, can still round down"), MidiFileFractionalLength));
		}
		 
		FileLengthTextBlock->SetText(FileLengthText);
		FileLengthTextBlock->SetToolTipText(RoundedLengthToolTipText);

		return FReply::Handled();
	};

	//Detail Customization for the File Length Row
	MidiFileLengthCategory.AddCustomRow(FText::FromString("Midi File Length"))
	.NameContent()
	[
		SNew(STextBlock)
		.Text(FText::FromString("File Length"))
		.Font(IDetailLayoutBuilder::GetDetailFont())
	]
	.ValueContent()
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.HAlign(HAlign_Left)
		.VAlign(VAlign_Center)
		.FillWidth(0.9f)
		[
			SAssignNew(FileLengthTextBlock,STextBlock)
			.Text(FileLengthText)
			.Font(IDetailLayoutBuilder::GetDetailFont())
			.ToolTipText(RoundedLengthToolTipText)
		]
		+ SHorizontalBox::Slot()
			.HAlign(HAlign_Right)
			.VAlign(VAlign_Center)
			.FillWidth(0.1f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.HAlign(HAlign_Right)
			.VAlign(VAlign_Center)
			.AutoWidth()
			[
				SNew(STextBlock)
				.Font(IDetailLayoutBuilder::GetDetailFont())
				.Text(FText::FromString("Conform file length to a whole bar by: "))
			]
			+ SHorizontalBox::Slot()
			.HAlign(HAlign_Left)
			.VAlign(VAlign_Center)
			.AutoWidth()
			[
				SNew(SButton)
				.VAlign(VAlign_Center)
				.Text(FText::FromString("Round to Nearest"))
				.OnClicked_Lambda(OnLengthConformedRoundToNearest)
				.IsEnabled_Lambda(ShouldConformRoundToNearest)
			]
			+ SHorizontalBox::Slot()
			.HAlign(HAlign_Left)
			.VAlign(VAlign_Center)
			.AutoWidth()
			.Padding(10, 0)
			[
				SNew(SButton)
				.VAlign(VAlign_Center)
				.Text(FText::FromString("Round Down"))
				.OnClicked_Lambda(OnLengthConformedRoundDown)
				.IsEnabled_Lambda(ShouldConformRoundDown)
			]
			+ SHorizontalBox::Slot()
				.HAlign(HAlign_Right)
				.VAlign(VAlign_Center)
				.AutoWidth()
			[
				SNew(SButton)
				.VAlign(VAlign_Center)
				.Text(FText::FromString("Round Up"))
				.OnClicked_Lambda(OnLengthConformedRoundUp)
				.IsEnabled_Lambda(ShouldConformRoundUp)
			]

		]
	];
}