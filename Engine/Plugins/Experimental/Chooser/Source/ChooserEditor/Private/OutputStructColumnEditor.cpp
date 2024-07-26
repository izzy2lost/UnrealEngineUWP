// Copyright Epic Games, Inc. All Rights Reserved.

#include "OutputStructColumnEditor.h"
#include "OutputStructColumn.h"
#include "SPropertyAccessChainWidget.h"
#include "ObjectChooserWidgetFactories.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/Text/STextBlock.h"
#include "GraphEditorSettings.h"

#define LOCTEXT_NAMESPACE "StructOutputColumnEditor"

namespace UE::ChooserEditor
{
	
TSharedRef<SWidget> CreateOutputStructColumnWidget(UChooserTable* Chooser, FChooserColumnBase* Column, int Row)
{
	if (Row == ColumnWidget_SpecialIndex_Header)
	{
		// create column header widget
		TSharedPtr<SWidget> InputValueWidget = nullptr;
		if (FChooserParameterBase* InputValue = Column->GetInputValue())
		{
			InputValueWidget = FObjectChooserWidgetFactories::CreateWidget(false, Chooser, InputValue, Column->GetInputType(), Chooser->OutputObjectType,
				FChooserWidgetValueChanged::CreateLambda([Column]()
				{
					FOutputStructColumn* StructColumn = static_cast<FOutputStructColumn*>(Column);
					StructColumn->StructTypeChanged();
				})
				);
		}
		
		const FSlateBrush* ColumnIcon = FCoreStyle::Get().GetBrush("Icons.ArrowRight");
		
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
	
		return ColumnHeaderWidget;
	}
	
	FOutputStructColumn* StructColumn = static_cast<FOutputStructColumn*>(Column);

	TAttribute<FText> StructValueAttribute = MakeAttributeLambda([StructColumn, Row]()
		{
			const FInstancedStruct* RowValue = nullptr;
			if (Row == ColumnWidget_SpecialIndex_Fallback)
			{
				RowValue = &StructColumn->FallbackValue;
			}
			else
			{
				RowValue = &StructColumn->RowValues[Row];
			}


			FString Value;
			if (const UScriptStruct* ScriptStruct = RowValue->GetScriptStruct())
			{
				void* DefaultStructMemory = FMemory_Alloca_Aligned(ScriptStruct->GetStructureSize(), ScriptStruct->GetMinAlignment());
				ScriptStruct->InitializeStruct(DefaultStructMemory);
				ScriptStruct->ExportText(Value, RowValue->GetMemory(), DefaultStructMemory, nullptr, PPF_ExternalEditor, nullptr);
				ScriptStruct->DestroyStruct(DefaultStructMemory);
			}
			else
			{
				Value = TEXT("()");
			}
			return FText::FromString(Value);
		});

	TSharedRef<STextBlock> TextBlock = SNew(STextBlock)
		.Text(StructValueAttribute);
	TextBlock->SetToolTipText(StructValueAttribute);

	return SNew(SBox)
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		[
			TextBlock
		];
}

TSharedRef<SWidget> CreateStructPropertyWidget(bool bReadOnly, UObject* TransactionObject, void* Value, UClass* ResultBaseClass, FChooserWidgetValueChanged ValueChanged)
{
	IHasContextClass* HasContextClass = Cast<IHasContextClass>(TransactionObject);

	FStructContextProperty* ContextProperty = reinterpret_cast<FStructContextProperty*>(Value);

	return SNew(SPropertyAccessChainWidget).ContextClassOwner(HasContextClass).BindingColor("StructPinTypeColor").TypeFilter("struct")
		.PropertyBindingValue(&ContextProperty->Binding)
		.OnValueChanged(ValueChanged);
}
	
void RegisterStructWidgets()
{
	FObjectChooserWidgetFactories::RegisterWidgetCreator(FStructContextProperty::StaticStruct(), CreateStructPropertyWidget);
	FObjectChooserWidgetFactories::RegisterColumnWidgetCreator(FOutputStructColumn::StaticStruct(), CreateOutputStructColumnWidget);
}
	
}

#undef LOCTEXT_NAMESPACE
