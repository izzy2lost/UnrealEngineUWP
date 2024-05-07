// Copyright Epic Games, Inc. All Rights Reserved.

#include "TypedElementRowReferenceWidget.h"

#include "Columns/TedsOutlinerColumns.h"
#include "Elements/Columns/TypedElementHiearchyColumns.h"
#include "Elements/Columns/TypedElementLabelColumns.h"
#include "Elements/Columns/TypedElementMiscColumns.h"
#include "ISceneOutliner.h"
#include "Modules/ModuleManager.h"
#include "TedsDebuggerModule.h"
#include "Elements/Columns/TypedElementSlateWidgetColumns.h"
#include "Elements/Framework/TypedElementRegistry.h"
#include "Widgets/Input/SHyperlink.h"
#include "Widgets/Layout/SBox.h"

#define LOCTEXT_NAMESPACE "RowReferenceWidget"

namespace UE::TypedElementRowReferenceWidget::Local
{
	void OnNavigateHyperlink(const ITypedElementDataStorageInterface* DataStorage, TypedElementDataStorage::RowHandle TargetRowHandle, TypedElementDataStorage::RowHandle UiRowHandle)
	{
		const FTableViewerColumn* TableViewerColumn = DataStorage->GetColumn<FTableViewerColumn>(UiRowHandle);

		if(!TableViewerColumn)
		{
			return;
		}
		
		TSharedPtr<ISceneOutliner> OwningTableViewer = TableViewerColumn->Outliner.Pin();
		
		if(!OwningTableViewer)
		{
			return;
		}

		// If the item was found in this table viewer, select it and navigate to it
		if(FSceneOutlinerTreeItemPtr TreeItem = OwningTableViewer->GetTreeItem(TargetRowHandle))
		{
			OwningTableViewer->SetSelection([TreeItem](ISceneOutlinerTreeItem& Item)
			{
				return Item.GetID() == TreeItem->GetID();
			});
			
			OwningTableViewer->FrameSelectedItems();

			return;
		}

		// If it wasn't found in the table viewer owning this widget, navigate to it in the global TEDS debugger
		FTedsDebuggerModule& TedsDebuggerModule = FModuleManager::GetModuleChecked<FTedsDebuggerModule>("TedsDebugger");
		TedsDebuggerModule.NavigateToRow(TargetRowHandle);
	}
	
	void CreateInternalWidget(const TWeakPtr<SWidget>& InWidget, TypedElementDataStorage::RowHandle UiRow, TypedElementDataStorage::RowHandle TargetRow)
	{
		const TSharedPtr<SWidget> Widget = InWidget.Pin();

		if(!Widget)
		{
			return;
		}
		
		checkf(Widget->GetType() == SBox::StaticWidgetClass().GetWidgetType(),
			TEXT("Stored widget with FTypedElementLabelWidgetConstructor doesn't match type %s, but was a %s."),
			*(SBox::StaticWidgetClass().GetWidgetType().ToString()),
			*(Widget->GetTypeAsString()));

		SBox* WidgetInstance = static_cast<SBox*>(Widget.Get());
		WidgetInstance->SetContent(SNullWidget::NullWidget);

		const ITypedElementDataStorageInterface* DataStorage = UTypedElementRegistry::GetInstance()->GetDataStorage();

		// We only navigate to row references that have a label column
		if (const FTypedElementLabelColumn* LabelColumn = DataStorage->GetColumn<FTypedElementLabelColumn>(TargetRow))
		{
			FNumberFormattingOptions NumberFormattingOptions;
			NumberFormattingOptions.SetUseGrouping(false);

			const FText Text = FText::AsNumber(TargetRow, &NumberFormattingOptions);
			const FText TooltipText = FText::FromString(LabelColumn->Label);

			TSharedRef<SWidget> HyperlinkWidget =
				SNew(SHyperlink)
					.Text(Text)
					.Style(FAppStyle::Get(), "Common.GotoBlueprintHyperlink")
					.ToolTipText(TooltipText)
					.OnNavigate(FSimpleDelegate::CreateStatic(&UE::TypedElementRowReferenceWidget::Local::OnNavigateHyperlink, DataStorage, TargetRow, UiRow));
			
			WidgetInstance->SetContent(HyperlinkWidget);
		}
	}


}

UTypedElementRowReferenceWidgetFactory::~UTypedElementRowReferenceWidgetFactory()
{
}

void UTypedElementRowReferenceWidgetFactory::RegisterWidgetConstructors(ITypedElementDataStorageInterface& DataStorage, ITypedElementDataStorageUiInterface& DataStorageUi) const
{
	// TEDS UI TODO: We can re-use this widget for FTypedElementParentColumn
	DataStorageUi.RegisterWidgetFactory<FTypedElementRowReferenceWidgetConstructor>(FName(TEXT("SceneOutliner.Cell")),
	TypedElementDataStorage::FColumn<FTypedElementRowReferenceColumn>());
}

void UTypedElementRowReferenceWidgetFactory::RegisterQueries(ITypedElementDataStorageInterface& DataStorage)
{
	using namespace TypedElementQueryBuilder;
	using DSI = ITypedElementDataStorageInterface;

	const TypedElementQueryHandle UpdateRowReferenceWidget = DataStorage.RegisterQuery(
		Select()
			.ReadOnly<FTypedElementRowReferenceColumn>()
		.Where()
			.Any<FTypedElementSyncFromWorldTag, FTypedElementSyncBackToWorldTag>()
		.Compile());

	DataStorage.RegisterQuery(
		Select(
			TEXT("Sync row reference to widget"),
			FProcessor(DSI::EQueryTickPhase::FrameEnd, DataStorage.GetQueryTickGroupName(DSI::EQueryTickGroups::SyncWidgets))
				.ForceToGameThread(true),
			[](
				DSI::IQueryContext& Context,
				TypedElementDataStorage::RowHandle UiRowHandle,
				FTypedElementSlateWidgetReferenceColumn& Widget,
				const FTypedElementRowReferenceColumn& Target)
			{
				Context.RunSubquery(0, Target.Row, CreateSubqueryCallbackBinding(
					[&Widget, UiRowHandle](const FTypedElementRowReferenceColumn& Target)
					{
						UE::TypedElementRowReferenceWidget::Local::CreateInternalWidget(Widget.Widget, UiRowHandle, Target.Row);
					}));
			})
		.DependsOn()
			.SubQuery( UpdateRowReferenceWidget )
		.Compile()
	);
}

FTypedElementRowReferenceWidgetConstructor::FTypedElementRowReferenceWidgetConstructor()
	: Super(FTypedElementRowReferenceWidgetConstructor::StaticStruct())
{
}

TSharedPtr<SWidget> FTypedElementRowReferenceWidgetConstructor::CreateWidget(const TypedElementDataStorage::FMetaDataView& Arguments)
{
	return SNew(SBox)
			.HAlign(HAlign_Left)
			.VAlign(VAlign_Center);
}

bool FTypedElementRowReferenceWidgetConstructor::FinalizeWidget(ITypedElementDataStorageInterface* DataStorage, ITypedElementDataStorageUiInterface* DataStorageUi, TypedElementDataStorage::RowHandle Row, const TSharedPtr<SWidget>& Widget)
{
	checkf(Widget, TEXT("Referenced widget is not valid. A constructed widget may not have been cleaned up. This can "
		"also happen if this processor is running in the same phase as the processors responsible for cleaning up old "
		"references."));

	// The actual row we want to view in the widget
	TypedElementRowHandle TargetRowReference = TypedElementInvalidRowHandle;

	// The target row for which this widget was created
	const TypedElementRowHandle TargetRow = DataStorage->GetColumn<FTypedElementRowReferenceColumn>(Row)->Row;

	// Check if the target row has a row reference column, if so we want to view the row in there.
	if(const FTypedElementRowReferenceColumn* RowReferenceColumn = DataStorage->GetColumn<FTypedElementRowReferenceColumn>(TargetRow))
	{
		TargetRowReference = RowReferenceColumn->Row;
	}
	
	UE::TypedElementRowReferenceWidget::Local::CreateInternalWidget(Widget, Row, TargetRowReference);

	return true;
}

#undef LOCTEXT_NAMESPACE