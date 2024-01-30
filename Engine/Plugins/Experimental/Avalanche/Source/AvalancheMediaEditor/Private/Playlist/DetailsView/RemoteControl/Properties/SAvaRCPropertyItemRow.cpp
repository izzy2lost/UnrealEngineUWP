// Copyright Epic Games, Inc. All Rights Reserved.

#include "SAvaRCPropertyItemRow.h"
#include "AvaPageRemoteControlWidgetUtils.h"
#include "AvaRCPropertyItem.h"
#include "IPropertyRowGenerator.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "Widgets/Layout/SScissorRectBox.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SAvaRCPropertyItemRow"

void SAvaRCPropertyItemRow::Construct(const FArguments& InArgs, TSharedRef<SAvaPageRemoteControlProps> InPropertyPanel,
	const TSharedRef<STableViewBase>& InOwnerTableView, const TSharedPtr<const FAvaRCPropertyItem>& InRowItem)
{
	ItemPtrWeak = InRowItem;
	PropertyPanelWeak = InPropertyPanel;
	Generator = nullptr;
	ValueContainer = nullptr;
	ValueWidget = nullptr;

	SMultiColumnTableRow<FAvaRCPropertyItemPtr>::Construct(FSuperRowType::FArguments(), InOwnerTableView);
}

TSharedRef<SWidget> SAvaRCPropertyItemRow::GenerateWidgetForColumn(const FName& InColumnName)
{
	TSharedPtr<const FAvaRCPropertyItem> ItemPtr = ItemPtrWeak.Pin();

	if (ItemPtr.IsValid())
	{
		if (InColumnName == SAvaPageRemoteControlProps::PropertyColumnName)
		{
			return SNew(SScissorRectBox)
				[
					SNew(SBox)
					.VAlign(EVerticalAlignment::VAlign_Center)
					.Padding(3.f, 2.f, 3.f, 2.f)
					[
						SNew(STextBlock)
						.Text(GetFieldLabel())
					]
				];
		}
		else if (InColumnName == SAvaPageRemoteControlProps::ValueColumnName)
		{
			return SAssignNew(ValueContainer, SBox)
				[
					CreateValue()
				];
		}
		else
		{
			TSharedPtr<SAvaPageRemoteControlProps> PropertyPanel = PropertyPanelWeak.Pin();

			if (PropertyPanel.IsValid())
			{
				TSharedPtr<SWidget> Cell = nullptr;
				const TArray<FAvaRCPropertyTableRowExtensionDelegate>& TableRowExtensionDelegates = PropertyPanel->GetTableRowExtensionDelegates(InColumnName);

				for (const FAvaRCPropertyTableRowExtensionDelegate& TableRowExtensionDelegate : TableRowExtensionDelegates)
				{
					TableRowExtensionDelegate.ExecuteIfBound(PropertyPanel.ToSharedRef(), ItemPtr.ToSharedRef(), Cell);
				}

				if (Cell.IsValid())
				{
					return Cell.ToSharedRef();
				}
			}
		}
	}

	return SNullWidget::NullWidget;
}

void SAvaRCPropertyItemRow::UpdateValue()
{
	if (ValueContainer.IsValid())
	{
		ValueContainer->SetContent(CreateValue());
	}
}

FText SAvaRCPropertyItemRow::GetFieldLabel() const
{
	if (TSharedPtr<const FAvaRCPropertyItem> ItemPtr = ItemPtrWeak.Pin())
	{
		if (TSharedPtr<FRemoteControlEntity> EntityPtr = ItemPtr->GetEntity())
		{
			TSharedPtr<FRemoteControlField> FieldPtr = StaticCastSharedPtr<FRemoteControlField>(EntityPtr);
			return FText::FromName(FieldPtr->FieldName);
		}
	}

	return FText::GetEmpty();
}

TSharedRef<SWidget> SAvaRCPropertyItemRow::CreateValue()
{
	if (TSharedPtr<const FAvaRCPropertyItem> ItemPtr = ItemPtrWeak.Pin())
	{
		if (TSharedPtr<FRemoteControlEntity> EntityPtr = ItemPtr->GetEntity())
		{
			TSharedPtr<FRemoteControlField> FieldPtr = StaticCastSharedPtr<FRemoteControlField>(EntityPtr);

			// For the moment, just use the first object.
			TArray<UObject*> Objects = FieldPtr->GetBoundObjects();

			if ((FieldPtr->FieldType == EExposedFieldType::Property) && (Objects.Num() > 0))
			{
				FPropertyRowGeneratorArgs Args;
				Generator = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor").CreatePropertyRowGenerator(Args);
				Generator->SetObjects({Objects[0]});

				if (TSharedPtr<IDetailTreeNode> Node = FAvaPageWidgetUtils::FindNode(Generator->GetRootTreeNodes(), FieldPtr->FieldPathInfo.ToPathPropertyString(), FAvaPageWidgetUtils::EFindNodeMethod::Path))
				{
					const FNodeWidgets NodeWidgets = Node->CreateNodeWidgets();
					ValueWidget = NodeWidgets.WholeRowWidget.IsValid() ? NodeWidgets.WholeRowWidget : NodeWidgets.ValueWidget;

					if (ItemPtr->IsEntityControlled())
					{
						ValueWidget = SNew(SHorizontalBox)
							+ SHorizontalBox::Slot()
							.VAlign(EVerticalAlignment::VAlign_Center)
							[
								ValueWidget.ToSharedRef()
							]
						+ SHorizontalBox::Slot()
							.VAlign(EVerticalAlignment::VAlign_Center)
							.Padding(3.f, 0.f, 0.f, 0.f)
							[
								SNew(STextBlock)
								.Text(LOCTEXT("Controlled", "(Controlled)"))
							];

						ValueWidget->SetEnabled(false);
					}

					return ValueWidget.ToSharedRef();
				}
			}
		}
	}

	return SNullWidget::NullWidget;
}

#undef LOCTEXT_NAMESPACE
