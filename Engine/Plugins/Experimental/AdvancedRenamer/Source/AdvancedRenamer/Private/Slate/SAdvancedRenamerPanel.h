// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Styling/SlateTypes.h"
#include "UObject/StrongObjectPtr.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Views/STableRow.h"
#include "Widgets/Views/STableViewBase.h"

class FUICommandList;
class IAdvancedRenamer;
class SBox;
class SButton;
class SCanvas;
class SCheckBox;
class SEditableTextBox;
class SHeaderRow;
class SMultiLineEditableTextBox; 
class UObject;
struct FAdvancedRenamerPreview;
template<typename NumericType> class SSpinBox;

class SAdvancedRenamerPanel : public SCompoundWidget
{
	friend class SAdvancedRenamerPreviewListRow;

public:
	SLATE_BEGIN_ARGS(SAdvancedRenamerPanel) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const TSharedRef<IAdvancedRenamer>& InRenamer);

protected:
	static constexpr double MinUpdateFrequency = 0.1;

	TSharedPtr<IAdvancedRenamer> Renamer;
	TSharedPtr<FUICommandList> CommandList;

	double ListLastUpdateTime = 0;
	float MinDesiredOriginalNameWidth = 0.f;
	float MinDesiredNewNameWidth = 0.f;

	bool bRemovePrefixSeparator;
	bool bRemovePrefixNumChars;
	bool bRemoveSuffixSeparator;
	bool bRemoveSuffixNumChars;

	TSharedPtr<SEditableTextBox> BaseNameTextBox;
	TSharedPtr<SEditableTextBox> PrefixTextBox;
	TSharedPtr<SCheckBox> PrefixRemoveCheckBox;
	TSharedPtr<SEditableTextBox> PrefixSeparatorTextBox;
	TSharedPtr<SCheckBox> PrefixRemoveCharactersCheckBox;
	TSharedPtr<SSpinBox<uint8>> PrefixRemoveCharactersSpinBox;
	TSharedPtr<SEditableTextBox> SuffixTextBox;
	TSharedPtr<SCheckBox> SuffixRemoveCheckBox;
	TSharedPtr<SEditableTextBox> SuffixSeparatorTextBox;
	TSharedPtr<SCheckBox> SuffixRemoveCharactersCheckBox;
	TSharedPtr<SSpinBox<uint8>> SuffixRemoveCharactersSpinBox;
	TSharedPtr<SCheckBox> SuffixRemoveNumberCheckBox;
	TSharedPtr<SCheckBox> SuffixNumberCheckBox;
	TSharedPtr<SSpinBox<int32>> SuffixNumberStartSpinBox;
	TSharedPtr<SSpinBox<int32>> SuffixNumberStepSpinBox;
	TSharedPtr<SCheckBox> SearchReplacePlainTextCheckbox;
	TSharedPtr<SCheckBox> SearchReplaceRegexCheckbox;
	TSharedPtr<SCheckBox> SearchReplaceIgnoreCaseCheckBox;
	TSharedPtr<SMultiLineEditableTextBox> SearchReplaceSearchTextBox;
	TSharedPtr<SMultiLineEditableTextBox> SearchReplaceReplaceTextBox;
	TSharedPtr<SBox> RenamePreviewListBox;
	TSharedPtr<SHeaderRow> RenamePreviewListHeaderRow;
	TSharedPtr<SListView<TSharedPtr<FAdvancedRenamerPreview>>> RenamePreviewList;
	TSharedPtr<SButton> ApplyButton;

	void CreateLeftPane(const TSharedRef<SCanvas>& InCanvas);
	TSharedRef<SWidget> CreateBaseName();
	TSharedRef<SWidget> CreatePrefix();
	TSharedRef<SWidget> CreateSuffix();
	TSharedRef<SWidget> CreateSearchAndReplace();

	void CreateRightPane(const TSharedRef<SCanvas>& InCanvas);
	TSharedRef<SWidget> CreateRenamePreview();

	bool CloseWindow();

	void RefreshListView(const double InCurrentTime);
	void UpdateRequiredListWidth();

	void RemoveSelectedObjects();

	virtual void Tick(const FGeometry& InAllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

	void OnBaseNameChanged(const FText& InNewText);

	void OnPrefixChanged(const FText& InNewText);

	ECheckBoxState IsPrefixRemoveChecked() const;
	void OnPrefixRemoveCheckBoxChanged(ECheckBoxState InNewState);

	bool IsPrefixRemoveSeparatorEnabled() const;
	bool OnPrefixSeparatorVerifyTextChanged(const FText& InText, FText& OutErrorText) const;
	void OnPrefixSeparatorChanged(const FText& InNewText);

	ECheckBoxState IsPrefixRemoveCharactersChecked() const;
	void OnPrefixRemoveCharactersCheckBoxChanged(ECheckBoxState InNewState);

	bool IsPrefixRemoveNumCharsEnabled() const;
	void OnPrefixRemoveCharactersChanged(uint8 InNewValue);

	void OnSuffixChanged(const FText& InNewText);

	ECheckBoxState IsSuffixRemoveChecked() const;
	void OnSuffixRemoveCheckBoxChanged(ECheckBoxState InNewState);

	bool IsSuffixRemoveSeparatorEnabled() const;
	bool OnSuffixSeparatorVerifyTextChanged(const FText& InText, FText& OutErrorText) const;
	void OnSuffixSeparatorChanged(const FText& InNewText);

	ECheckBoxState IsSuffixRemoveCharactersChecked() const;
	void OnSuffixRemoveCharactersCheckBoxChanged(ECheckBoxState InNewState);

	bool IsSuffixRemoveNumCharsEnabled() const;
	void OnSuffixRemoveCharactersChanged(uint8 InNewValue);

	ECheckBoxState IsSuffixRemoveNumberChecked() const;
	void OnSuffixRemoveNumberCheckBoxChanged(ECheckBoxState InNewState);

	ECheckBoxState IsSuffixNumberChecked() const;
	bool IsSuffixRemoveNumberCheckBoxEnabled() const;
	void OnSuffixNumberCheckBoxChanged(ECheckBoxState InNewState);

	void OnSuffixNumberStartChanged(int32 InNewValue);

	void OnSuffixNumberStepChanged(int32 InNewValue);

	ECheckBoxState IsSearchReplacePlainTextChecked() const;
	void OnSearchReplacePlainTextCheckBoxChanged(ECheckBoxState InNewState);

	ECheckBoxState IsSearchReplaceRegexChecked() const;
	void OnSearchReplaceRegexCheckBoxChanged(ECheckBoxState InNewState);

	ECheckBoxState IsSearchReplaceIgnoreCaseChecked() const;
	void OnSearchReplaceIgnoreCaseCheckBoxChanged(ECheckBoxState InNewState);

	void OnSearchReplaceSearchTextChanged(const FText& InNewText);

	void OnSearchReplaceReplaceTextChanged(const FText& InNewText);

	TSharedRef<ITableRow> OnGenerateRowForList(TSharedPtr<FAdvancedRenamerPreview> InItem, const TSharedRef<STableViewBase>& InOwnerTable);

	FReply OnListViewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent);

	TSharedPtr<SWidget> GenerateListViewContextMenu();

	bool IsApplyButtonEnabled() const;
	FReply OnApplyButtonClicked();

	FVector2D GetRightPaneSize() const;

	FVector2D GetListViewsize() const;

	FVector2D GetApplyButtonSize() const;
};
