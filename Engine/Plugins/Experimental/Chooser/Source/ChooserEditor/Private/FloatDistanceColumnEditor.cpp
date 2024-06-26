// Copyright Epic Games, Inc. All Rights Reserved.

#include "FloatDistanceColumnEditor.h"
#include "FloatDistanceColumn.h"
#include "ObjectChooserWidgetFactories.h"
#include "ChooserTableEditor.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SNumericEntryBox.h"
#include "Widgets/Images/SImage.h"
#include "GraphEditorSettings.h"
#include "ScopedTransaction.h"

#define LOCTEXT_NAMESPACE "FloatDistanceColumnEditor"

namespace UE::ChooserEditor
{

TSharedRef<SWidget> CreateFloatDistanceColumnWidget(UChooserTable* Chooser, FChooserColumnBase* Column, int Row)
{
	FFloatDistanceColumn* FloatDistanceColumn = static_cast<FFloatDistanceColumn*>(Column);

	if (Row == ColumnWidget_SpecialIndex_Fallback)
	{
		return SNullWidget::NullWidget;
	}
	else if (Row == ColumnWidget_SpecialIndex_Header)
	{
		// create column header widget
		TSharedPtr<SWidget> InputValueWidget = nullptr;
		if (FChooserParameterBase* InputValue = Column->GetInputValue())
		{
			InputValueWidget = FObjectChooserWidgetFactories::CreateWidget(false, Chooser, InputValue, Column->GetInputType(), Chooser->OutputObjectType);
		}
		
		const FSlateBrush* ColumnIcon = FCoreStyle::Get().GetBrush("Icons.Filter");
		
		TSharedRef<SWidget> ColumnHeaderWidget = SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth()
			[
				SNew(SBorder)
				.BorderBackgroundColor(FLinearColor(0,0,0,0))
				.Content()
				[
					SNew(SImage).Image(ColumnIcon)
				]
			]
			+ SHorizontalBox::Slot()
			[
				InputValueWidget ? InputValueWidget.ToSharedRef() : SNullWidget::NullWidget
			];
	
		if (Chooser->GetEnableDebugTesting())
		{
			ColumnHeaderWidget = SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			[
				ColumnHeaderWidget
			]
			+ SVerticalBox::Slot()
			[
				SNew(SNumericEntryBox<float>).IsEnabled_Lambda([Chooser](){ return !Chooser->HasDebugTarget(); })
				.Value_Lambda([FloatDistanceColumn]() { return FloatDistanceColumn->TestValue; })
				.OnValueCommitted_Lambda([Chooser, FloatDistanceColumn](float NewValue, ETextCommit::Type CommitType) { FloatDistanceColumn->TestValue = NewValue; })
			];
		}

		return ColumnHeaderWidget;
	}

	// create cell widget
	return SNew(SNumericEntryBox<float>)
	.Value_Lambda([FloatDistanceColumn, Row]()
	{
		return (Row < FloatDistanceColumn->RowValues.Num()) ? FloatDistanceColumn->RowValues[Row].Value : 0;
	})
	.OnValueCommitted_Lambda([Chooser, FloatDistanceColumn, Row](float NewValue, ETextCommit::Type CommitType)
	{
		if (Row < FloatDistanceColumn->RowValues.Num())
		{
			const FScopedTransaction Transaction(LOCTEXT("Edit Float Distance Value", "Edit Float Distance Value"));
			Chooser->Modify(true);
			FloatDistanceColumn->RowValues[Row].Value = NewValue;
		}
	});
}

	
void RegisterFloatDistanceWidgets()
{
	FObjectChooserWidgetFactories::RegisterColumnWidgetCreator(FFloatDistanceColumn::StaticStruct(), CreateFloatDistanceColumnWidget);
}
	
}

#undef LOCTEXT_NAMESPACE
