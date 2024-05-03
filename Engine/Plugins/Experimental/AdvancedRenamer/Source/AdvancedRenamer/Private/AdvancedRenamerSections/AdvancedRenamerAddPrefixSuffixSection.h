// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AdvancedRenamerSectionBase.h"
#include "Styling/SlateTypes.h"

class FString;
class SCheckBox;
class SEditableTextBox;
template <typename InNumericType>
class SSpinBox;
class SWidget;

class FAdvancedRenamerAddPrefixSuffixSection : public FAdvancedRenamerSectionBase
{
public:
	FAdvancedRenamerAddPrefixSuffixSection();

	virtual ~FAdvancedRenamerAddPrefixSuffixSection() {}

	virtual void Init(TSharedRef<IAdvancedRenamer> InRenamer) override;

	virtual TSharedRef<SWidget> GetWidget() override;

	virtual void ResetToDefault() override;

private:
	TSharedRef<SWidget> CreateAddPrefix();

	TSharedRef<SWidget> CreateAddSuffix();

	TSharedRef<SWidget> CreateAddNumber();

	FText GetPrefixText() const;

	FText GetSuffixText() const;

	int32 GetSuffixNumberStart() const;

	int32 GetSuffixNumberStep() const;

	ECheckBoxState IsSuffixNumberChecked() const;

	bool IsSuffixNumberEnabled() const;

	void OnPrefixChanged(const FText& InNewText);

	void OnSuffixChanged(const FText& InNewText);

	void OnSuffixNumberCheckBoxChanged(ECheckBoxState InNewState);

	void OnSuffixNumberStartChanged(int32 InNewValue);

	void OnSuffixNumberStepChanged(int32 InNewValue);

	bool CanApplyAddPrefixOperation();

	bool CanApplyAddSuffixOperation();

	bool CanApplyAddSuffixNumberOperation();

	void ApplyAddPrefixOperation(FString& OutOriginalName);

	void ApplyAddSuffixOperation(FString& OutOriginalName);

	void ApplyAddSuffixNumberOperation(FString& OutOriginalName);

	void ResetCurrentSuffixNumber();

	void ApplyAddPrefixSuffixNumberOperation(FString& OutOriginalName);

private:
	TSharedPtr<SEditableTextBox> PrefixTextBox;
	TSharedPtr<SEditableTextBox> SuffixTextBox;
	TSharedPtr<SCheckBox> SuffixNumberCheckBox;
	TSharedPtr<SSpinBox<int32>> SuffixNumberStartSpinBox;
	TSharedPtr<SSpinBox<int32>> SuffixNumberStepSpinBox;
	FText PrefixText;
	FText SuffixText;
	bool bAddSuffixNumbers;
	int32 SuffixNumberStartValue;
	int32 SuffixNumberStepValue;
	int32 CurrentSuffixNumber;
};
