// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CustomizableObjectNodeDetails.h"
#include "IDetailCustomization.h"
#include "Types/SlateEnums.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Views/ITableRow.h"

namespace ESelectInfo { enum Type : int; }

class FString;
class IDetailLayoutBuilder;
class IPropertyHandle;
class UCustomizableObjectNodeObject;

class FCustomizableObjectNodeModifierMorphMeshSectionDetails : public FCustomizableObjectNodeDetails
{
public:
	// Makes a new instance of this detail layout class for a specific detail view requesting it 
	static TSharedRef<IDetailCustomization> MakeInstance();

	// ILayoutDetails interface
	void CustomizeDetails( IDetailLayoutBuilder& DetailBuilder ) override;

private:
	class UCustomizableObjectNodeModifierMorphMeshSection* Node = nullptr;

	/** */
	TSharedPtr<SComboButton> MorphCombo;
	/** The search field used for the combox box's contents */
	TSharedPtr<SEditableTextBox> SearchField;
	/** The ListView that we pop up; visualized the available options. */
	TSharedPtr< SListView< TSharedPtr<FString> > > ComboListView;
	/** */
	TArray< TSharedPtr<FString> > OptionsSource;
	/** Updated whenever search text is changed */
	FText SearchText;
	/** Filtered list that is actually displayed */
	TArray< TSharedPtr<FString> > FilteredOptionsSource;

	void OnMorphTargetComboBoxSelectionChanged(const FText& NewText);
	void OnSearchTextChanged(const FText& ChangedText);
	void OnSearchTextCommitted(const FText& InText, ETextCommit::Type InCommitType);
	TSharedRef<ITableRow> GenerateMenuItemRow(TSharedPtr<FString> InItem, const TSharedRef<STableViewBase>& OwnerTable);
	void RefreshOptions();
	void OnSelectionChanged(TSharedPtr<FString> ProposedSelection, ESelectInfo::Type SelectInfo);
	FReply OnKeyDownHandler(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent);

};
