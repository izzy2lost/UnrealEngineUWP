// Copyright Epic Games, Inc. All Rights Reserved.

#include "TedsQueryEditor.h"

#include "TedsQueryEditorModel.h"
#include "Components/VerticalBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SEditableComboBox.h"
#include "QueryEditor/Widgets/TEDSConditionSelectionComboWidget.h"
#include "Widgets/TEDSConditionCollectionViewWidget.h"
#include "Widgets/TedsQueryEditorResultsView.h"

#define LOCTEXT_NAMESPACE "TedsQueryEditor"

namespace UE::Teds::Debug::QueryEditor
{
	struct SQueryEditorWidget::ColumnComboItem
	{
		const FConditionEntry* Entry = nullptr;

		bool operator==(const ColumnComboItem& Rhs) const
		{
			return Entry == Rhs.Entry;
		}
	};

	void SQueryEditorWidget::Construct(const FArguments& InArgs, FTedsQueryEditorModel& QueryEditorModel)
	{
		using namespace UE::Teds::Debug;
	
		ComboItems.Reset();
		Model = &QueryEditorModel;
	
		ChildSlot
		[
			SNew(SVerticalBox)
			+SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				[
					SNew(SConditionCollectionViewWidget, *Model, QueryEditor::EOperatorType::Select)
				]
				+SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(QueryEditor::SConditionComboWidget, *Model, QueryEditor::EOperatorType::Select)
				]
			]
			+SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				[
					SNew(UE::Teds::Debug::QueryEditor::SConditionCollectionViewWidget, *Model, QueryEditor::EOperatorType::All)
				]
				+SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(QueryEditor::SConditionComboWidget, *Model, QueryEditor::EOperatorType::All)
				]
			]
			+SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				[
				SNew(UE::Teds::Debug::QueryEditor::SConditionCollectionViewWidget, *Model, QueryEditor::EOperatorType::Any)
				]
				+SHorizontalBox::Slot()
				.AutoWidth()
				[
				SNew(QueryEditor::SConditionComboWidget, *Model, QueryEditor::EOperatorType::Any)
				]
			]
			+SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				[
				SNew(UE::Teds::Debug::QueryEditor::SConditionCollectionViewWidget, *Model, QueryEditor::EOperatorType::None)
				]
				+SHorizontalBox::Slot()
				.AutoWidth()
				[
				SNew(QueryEditor::SConditionComboWidget, *Model, QueryEditor::EOperatorType::None)
				]
			]
			+SVerticalBox::Slot()
			[
				SNew(QueryEditor::SResultsView, *Model)
			]
		];
	}
}

#undef LOCTEXT_NAMESPACE