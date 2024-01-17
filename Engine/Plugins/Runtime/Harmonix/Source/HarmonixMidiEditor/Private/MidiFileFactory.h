// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/ObjectMacros.h"
#include "Factories/Factory.h"
#include "EditorReimportHandler.h"
#include "HarmonixMidi/MidiFile.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SWindow.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Styling/SlateTypes.h"
#include "Widgets/SBoxPanel.h"
#include "Layout/Visibility.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SBox.h"
#include "Templates/SharedPointer.h"
#include "Framework/Application/SlateApplication.h"

#include "MidiFileFactory.generated.h"

// Imports a standard midi file
UCLASS()
class UMidiFileFactory : public UFactory, public FReimportHandler
{
	GENERATED_UCLASS_BODY()

	//~ BEGIN UFactory interface
	virtual FText GetToolTip() const override;
	virtual bool FactoryCanImport(const FString& Filename) override;
	virtual UObject* FactoryCreateFile(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, const FString& Filename, const TCHAR* Parms, FFeedbackContext* Warn, bool& bOutOperationCanceled) override;
	virtual void CleanUp() override;
	//~ END of UFactory interface

	//~ BEGIN FReimportHandler interface --
	virtual bool CanReimport(UObject* Obj, TArray<FString>& OutFilenames) override;
	virtual void SetReimportPaths(UObject* Obj, const TArray<FString>& NewReimportPaths) override;
	virtual EReimportResult::Type Reimport(UObject* Obj) override;
	//~ END FReimportHandler interface --

	//Show a custom dialog upon importing midi files, prompt the user the option to conform midi file length
	void ShowConformMidiFileLengthDialog(int32 MidiFileAssetIndex);
	
	//keep track of files that are imported for the pop-up dialog (multi batch)
	TArray<UMidiFile*> ImportedFiles;
	bool bShouldApplyToAll = true;
	EMidiFileLengthConformOption ApplyToAllOption;
};

/* 
* A Custom Widget Class for displaying a pop-up window upon importing an midi file, 
* providing the option to conform midi file length to a new length 
* by rounding up, rounding down, or rounding to the nearest integer bar depending on conform option selection)
*/

class SConformMidiFileLengthDialog : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SConformMidiFileLengthDialog)
		: _ConformOption(EMidiFileLengthConformOption::RoundUp)
		, _ApplyToAll(true) 
	{}
	
	SLATE_ARGUMENT(EMidiFileLengthConformOption,ConformOption)
	SLATE_ARGUMENT(bool,ApplyToAll)
	
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	//custom widgets 
	const FTextBlockStyle MidiFileInformationStyle = FTextBlockStyle().SetFont(FCoreStyle::GetDefaultFontStyle("Bold", 10)).SetColorAndOpacity(FLinearColor::White);
	EMidiFileLengthConformOption ConformOption = EMidiFileLengthConformOption::RoundUp;
	bool ApplyToAll = true;
	TSharedPtr<STextBlock> AskConformFileLengthText;
	TSharedPtr<SCheckBox> RoundToNearestCheckBox;
	TSharedPtr<SCheckBox> RoundDownCheckBox;
	TSharedPtr<SCheckBox> RoundUpCheckBox;
	TSharedPtr<SCheckBox> ApplyToAllCheckBox;
	TSharedPtr<SButton> OkButton;

private:
	void HandleConformOptionCheckboxChanged(ECheckBoxState NewState, EMidiFileLengthConformOption Option);
	void HandleApplyToAllCheckboxChanged(ECheckBoxState NewState);

};