// Copyright Epic Games, Inc. All Rights Reserved.

#include "EnumColumnEditor.h"
#include "EnumColumn.h"
#include "OutputEnumColumn.h"
#include "SPropertyAccessChainWidget.h"
#include "ObjectChooserWidgetFactories.h"
#include "ChooserTableEditor.h"
#include "ChooserColumnHeader.h"
#include "GraphEditorSettings.h"
#include "SEnumCombo.h"
#include "TransactionCommon.h"
#include "Widgets/Input/SButton.h"
#include "ScopedTransaction.h"

#define LOCTEXT_NAMESPACE "EnumColumnEditor"

namespace UE::ChooserEditor
{

TSharedRef<SWidget> CreateEnumColumnWidget(UChooserTable* Chooser, FChooserColumnBase* Column, int Row)
{
	FEnumColumn* EnumColumn = static_cast<FEnumColumn*>(Column);
	
	if (Row == ColumnWidget_SpecialIndex_Fallback)
	{
		return SNullWidget::NullWidget;
	}
	if (Row == ColumnWidget_SpecialIndex_Header)
	{
		const FSlateBrush* ColumnIcon = FCoreStyle::Get().GetBrush("Icons.Filter");
		const FText ColumnTooltip = LOCTEXT("Enum Value Tooltip", "Enum Value: cells pass if the cell value is equal to the column input value");
		const FText ColumnName = LOCTEXT("Enum Value","Enum Value");
		
		TSharedPtr<SWidget> DebugWidget = nullptr;
		if (Chooser->GetEnableDebugTesting())
		{
			DebugWidget = SNew(SEnumCell<FEnumColumn>).TransactionObject(Chooser).EnumColumn(EnumColumn)
								.OnValueSet_Lambda([EnumColumn](int Value) { EnumColumn->TestValue = Value; })
								.EnumValue_Lambda([EnumColumn]() { return EnumColumn->TestValue; })
								.IsEnabled_Lambda([Chooser] { return !Chooser->HasDebugTarget(); });
		}

		return MakeColumnHeaderWidget(Chooser, Column, ColumnName, ColumnTooltip, ColumnIcon, DebugWidget);
	}

	// create cell widget
	
	return SNew(SHorizontalBox)
    		+ SHorizontalBox::Slot().AutoWidth()
    		[
    			SNew(SBox).WidthOverride(Row < 0 ? 0 : 55)
    			[
    				SNew(SButton).ButtonStyle(FAppStyle::Get(),"FlatButton").TextStyle(FAppStyle::Get(),"RichTextBlock.Bold").HAlign(HAlign_Center)
    				.Visibility(Row < 0 ? EVisibility::Hidden : EVisibility::Visible)
					.Text_Lambda([EnumColumn, Row]()
					{
						switch (EnumColumn->RowValues[Row].Comparison)
						{
						case EEnumColumnCellValueComparison::MatchEqual:
							return LOCTEXT("CompEqual", "=");

						case EEnumColumnCellValueComparison::MatchNotEqual:
							return LOCTEXT("CompNotEqual", "Not");

						case EEnumColumnCellValueComparison::MatchAny:
							return LOCTEXT("CompAny", "Any");
						}
						return FText::GetEmpty();
					})
					.OnClicked_Lambda([EnumColumn, Chooser, Row]()
					{
						if (EnumColumn->RowValues.IsValidIndex(Row))
						{
							const FScopedTransaction Transaction(LOCTEXT("Edit Comparison", "Edit Comparison Operation"));
							Chooser->Modify(true);
							// cycle through comparison options
							EEnumColumnCellValueComparison& Comparison = EnumColumn->RowValues[Row].Comparison;
							const int32 NextComparison = (static_cast<int32>(Comparison) + 1) % static_cast<int32>(EEnumColumnCellValueComparison::Modulus);
							Comparison = static_cast<EEnumColumnCellValueComparison>(NextComparison);
						}
						return FReply::Handled();
					})
				]
			]
			+ SHorizontalBox::Slot().FillWidth(1)
			[
				SNew(SEnumCell<FEnumColumn>).TransactionObject(Chooser).EnumColumn(EnumColumn)
					.OnValueSet_Lambda([EnumColumn, Row](int Value)
					{
						if (EnumColumn->RowValues.IsValidIndex(Row))
						{
							EnumColumn->RowValues[Row].Value = static_cast<uint8>(Value);
						}
					})
					.EnumValue_Lambda([EnumColumn, Row]()
					{
						return EnumColumn->RowValues.IsValidIndex(Row) ? static_cast<int32>(EnumColumn->RowValues[Row].Value) : 0;
					})
				.Visibility_Lambda([EnumColumn,Column, Row]()
					{
						return (EnumColumn->RowValues.IsValidIndex(Row) &&
								EnumColumn->RowValues[Row].Comparison == EEnumColumnCellValueComparison::MatchAny)
								   ? EVisibility::Collapsed
								   : EVisibility::Visible;
					})
			];
}

TSharedRef<SWidget> CreateOutputEnumColumnWidget(UChooserTable* Chooser, FChooserColumnBase* Column, int Row)
{
	FOutputEnumColumn* EnumColumn = static_cast<FOutputEnumColumn*>(Column);
	
	if (Row == ColumnWidget_SpecialIndex_Header)
	{
		const FSlateBrush* ColumnIcon = FCoreStyle::Get().GetBrush("Icons.ArrowRight");
		const FText ColumnTooltip = LOCTEXT("Output Enum Tooltip", "Output Enum:  writes the value from cell in the result row to the bound variable");
		const FText ColumnName = LOCTEXT("Output Enum","Output Enum");
		
		TSharedPtr<SWidget> DebugWidget = nullptr;
		if (Chooser->GetEnableDebugTesting())
		{
			DebugWidget = SNew(SEnumCell<FOutputEnumColumn>).TransactionObject(Chooser).EnumColumn(EnumColumn).IsEnabled(false)
					.EnumValue_Lambda([EnumColumn]() { return static_cast<int32>(EnumColumn->TestValue); });
		}

		return MakeColumnHeaderWidget(Chooser, Column, ColumnName, ColumnTooltip, ColumnIcon, DebugWidget);
	}
	else if (Row == ColumnWidget_SpecialIndex_Fallback)
	{
		return 	SNew(SEnumCell<FOutputEnumColumn>).TransactionObject(Chooser).EnumColumn(EnumColumn)
        			.OnValueSet_Lambda([EnumColumn](int Value) { EnumColumn->FallbackValue.Value = Value; })
        			.EnumValue_Lambda([EnumColumn]() { return EnumColumn->FallbackValue.Value; });
	}

	// create cell widget
	
	return SNew(SEnumCell<FOutputEnumColumn>).TransactionObject(Chooser).EnumColumn(EnumColumn)
		.OnValueSet_Lambda([EnumColumn, Row](int Value)
		{
			if (EnumColumn->RowValues.IsValidIndex(Row))
			{
				EnumColumn->RowValues[Row].Value = static_cast<uint8>(Value);
			}
		})
		.EnumValue_Lambda([EnumColumn, Row]()
		{
			return EnumColumn->RowValues.IsValidIndex(Row) ? static_cast<int32>(EnumColumn->RowValues[Row].Value) : 0;
		});
}

TSharedRef<SWidget> CreateEnumPropertyWidget(bool bReadOnly, UObject* TransactionObject, void* Value, UClass* ResultBaseClass, FChooserWidgetValueChanged ValueChanged)
{
	IHasContextClass* HasContextClass = Cast<IHasContextClass>(TransactionObject);

	FEnumContextProperty* ContextProperty = reinterpret_cast<FEnumContextProperty*>(Value);

	return SNew(SPropertyAccessChainWidget).ContextClassOwner(HasContextClass).AllowFunctions(false).BindingColor("BytePinTypeColor").TypeFilter("enum")
	.PropertyBindingValue(&ContextProperty->Binding)
	.OnValueChanged(ValueChanged);
}
	
void RegisterEnumWidgets()
{
	FObjectChooserWidgetFactories::RegisterWidgetCreator(FEnumContextProperty::StaticStruct(), CreateEnumPropertyWidget);
	FObjectChooserWidgetFactories::RegisterColumnWidgetCreator(FEnumColumn::StaticStruct(), CreateEnumColumnWidget);
	FObjectChooserWidgetFactories::RegisterColumnWidgetCreator(FOutputEnumColumn::StaticStruct(), CreateOutputEnumColumnWidget);
}
	
}

#undef LOCTEXT_NAMESPACE
