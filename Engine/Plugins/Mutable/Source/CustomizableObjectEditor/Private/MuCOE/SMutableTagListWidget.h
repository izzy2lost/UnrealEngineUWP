// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widgets/SCompoundWidget.h"
#include "MuCOE/SMutableSearchComboBox.h"
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
class UCustomizableObjectNode;
class UCustomizableObjectNodeObject;

/** */
class SMutableTagListWidget : public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS(SMutableTagListWidget)
		: _Node(nullptr)
		, _TagArray(nullptr)
		{}

		SLATE_ARGUMENT(UCustomizableObjectNode*, Node)
		SLATE_ARGUMENT(TArray<FString>*, TagArray)
		SLATE_ARGUMENT(FText, EmptyListText)

		SLATE_EVENT(FSimpleDelegate, OnTagListChanged)

	SLATE_END_ARGS()

	/** */
	void Construct(const FArguments& InArgs);

	/** */
	FSimpleDelegate OnTagListChangedDelegate;

	/** */
	void RefreshOptions();

private:

	class UCustomizableObjectNode* Node = nullptr;
	TArray<FString>* TagArray = nullptr;
	FText EmptyListText;

	TSharedPtr<SMutableSearchComboBox> TagCombo;
	TArray< TSharedPtr<FString> > TagComboOptionsSource;
	void OnTagComboBoxSelectionChanged(const FText& NewText);

	struct FTagUIData
	{
		FString Tag;
		FString DisplayName;
	};
	TSharedPtr<SListView<TSharedPtr<FTagUIData>>> TagListWidget;
	TArray< TSharedPtr<FTagUIData> > CurrentTagsSource;

	/** */
	TSharedRef<ITableRow> GenerateTagMenuItemRow(TSharedPtr<FTagUIData> InItem, const TSharedRef<STableViewBase>& OwnerTable);

};

