// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widgets/SCompoundWidget.h"

enum class ECheckBoxState : uint8;
template <typename OptionType> class SComboBox;
class SEditableTextBox;
class UDMXControlConsoleEditorGlobalLayoutBase;


/** A widget to pick layouts for Control Console */
class SDMXControlConsoleEditorLayoutPicker
	: public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SDMXControlConsoleEditorLayoutPicker)
	{}

	SLATE_END_ARGS()

	/** Constructs the widget */
	void Construct(const FArguments& InArgs);

private:
	/** Generates a widget for selecting layouts */
	TSharedRef<SWidget> GenerateLayoutCheckBoxWidget();

	/** Generates a widget for each element in UserLayoutsComboBox */
	TSharedRef<SWidget> GenerateLayoutComboBoxWidget(const TWeakObjectPtr<UDMXControlConsoleEditorGlobalLayoutBase> InLayout);

	/** True if the active layout is the default layout */
	bool IsDefaultLayoutActive() const;

	/** Called when Default Layout checkbox state has changed */
	void OnDefaultLayoutChecked(ECheckBoxState CheckBoxState);

	/** Called when UserLayout checkbox state has changed */
	void OnUserLayoutChecked(ECheckBoxState CheckBoxState);

	/** Updates ComboBoxSource array according to the current DMX Library */
	void UpdateComboBoxSource();

	/** Called when an FixturePatchesComboBox element is selected */
	void OnComboBoxSelectionChanged(const TWeakObjectPtr<UDMXControlConsoleEditorGlobalLayoutBase> InLayout, ESelectInfo::Type SelectInfo);

	/** Called when LayoutNameEditableBox text changes */
	void OnLayoutNameTextChanged(const FText& NewText);

	/** Called when a new text on LayoutNameEditableBox editable text box is committed */
	void OnLayoutNameTextCommitted(const FText& NewText, ETextCommit::Type CommitInfo);

	/** Called to rename the active layout */
	void OnRenameLayout(const FString& NewName);

	/** Called when add layout button is clicked */
	FReply OnAddLayoutClicked();

	/** Called when rename layout button is clicked */
	FReply OnRenameLayoutClicked();

	/** Called when delete layout button is clicked */
	FReply OnDeleteLayoutClicked();

	/** Gets visibility for the layout ComboBox widget */
	EVisibility GetComboBoxVisibility() const;

	/** Reference to UserLayoutsComboBox last selected item */
	TWeakObjectPtr<UDMXControlConsoleEditorGlobalLayoutBase> LastSelectedItem;

	/** Source items for UserLayoutsComboBox */
	TArray<TWeakObjectPtr<UDMXControlConsoleEditorGlobalLayoutBase>> ComboBoxSource;

	/** A ComboBox for showing all User Layouts in the current Control Console */
	TSharedPtr<SComboBox<TWeakObjectPtr<UDMXControlConsoleEditorGlobalLayoutBase>>> UserLayoutsComboBox;

	/** Reference to text box for editing layout name */
	TSharedPtr<SEditableTextBox> LayoutNameEditableBox;

	/** Text for editing layout name */
	FText LayoutNameText;
};
