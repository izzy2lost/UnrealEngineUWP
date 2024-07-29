// Copyright Epic Games, Inc. All Rights Reserved.

#include "QueryEditor/TedsQueryEditor.h"

#include "QueryEditor/TedsQueryEditorModel.h"
#include "Components/VerticalBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SEditableComboBox.h"
#include "Widgets/QueryEditor/TedsConditionSelectionComboWidget.h"
#include "Widgets/QueryEditor/TedsConditionCollectionViewWidget.h"
#include "Widgets/QueryEditor/TedsQueryEditorResultsView.h"

#define LOCTEXT_NAMESPACE "TedsQueryEditor"

namespace UE::EditorDataStorage::Debug::QueryEditor
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
		using namespace UE::EditorDataStorage::Debug;
	
		ComboItems.Reset();
		Model = &QueryEditorModel;
	
		ChildSlot
		[
			SNew(SBorder)
			.BorderImage(FAppStyle::GetBrush("Brushes.Background"))
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
						SNew(SConditionComboWidget, *Model, QueryEditor::EOperatorType::Select)
					]
				]
				+SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SHorizontalBox)
					+SHorizontalBox::Slot()
					[
						SNew(SConditionCollectionViewWidget, *Model, QueryEditor::EOperatorType::All)
					]
					+SHorizontalBox::Slot()
					.AutoWidth()
					[
						SNew(SConditionComboWidget, *Model, QueryEditor::EOperatorType::All)
					]
				]
				+SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SHorizontalBox)
					+SHorizontalBox::Slot()
					[
					SNew(SConditionCollectionViewWidget, *Model, QueryEditor::EOperatorType::Any)
					]
					+SHorizontalBox::Slot()
					.AutoWidth()
					[
					SNew(SConditionComboWidget, *Model, QueryEditor::EOperatorType::Any)
					]
				]
				+SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SHorizontalBox)
					+SHorizontalBox::Slot()
					[
					SNew(SConditionCollectionViewWidget, *Model, QueryEditor::EOperatorType::None)
					]
					+SHorizontalBox::Slot()
					.AutoWidth()
					[
					SNew(SConditionComboWidget, *Model, QueryEditor::EOperatorType::None)
					]
				]
				+SVerticalBox::Slot()
				[
					SNew(SResultsView, *Model)
				]
			]
			
		];
	}
}

#undef LOCTEXT_NAMESPACE