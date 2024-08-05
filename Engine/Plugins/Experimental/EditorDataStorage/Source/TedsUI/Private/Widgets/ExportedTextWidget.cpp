// Copyright Epic Games, Inc. All Rights Reserved.

#include "Widgets/ExportedTextWidget.h"

#include "Elements/Columns/TypedElementMiscColumns.h"
#include "Elements/Columns/TypedElementSlateWidgetColumns.h"
#include "Elements/Columns/TypedElementTypeInfoColumns.h"
#include "Elements/Framework/TypedElementQueryBuilder.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "TedsUI_ExportedTextWidget"

//
// UExportedTextWidgetFactory
//

static void UpdateExportedTextWidget(FText Text, FTypedElementSlateWidgetReferenceColumn& Widget)
{
	TSharedPtr<SWidget> WidgetPointer = Widget.Widget.Pin();
	checkf(WidgetPointer, TEXT("Referenced widget is not valid. A constructed widget may not have been cleaned up. This can "
		"also happen if this processor is running in the same phase as the processors responsible for cleaning up old "
		"references."));
	checkf(WidgetPointer->GetType() == STextBlock::StaticWidgetClass().GetWidgetType(),
		TEXT("Stored widget with FExportedTextWidgetTag doesn't match type %s, but was a %s."),
		*(STextBlock::StaticWidgetClass().GetWidgetType().ToString()),
		*(WidgetPointer->GetTypeAsString()));

	STextBlock* TextWidget = static_cast<STextBlock*>(WidgetPointer.Get());
	TextWidget->SetToolTipText(Text);
	TextWidget->SetText(MoveTemp(Text));
}

static void UpdateExportedTextWidget(const void* Data, FTypedElementSlateWidgetReferenceColumn& Widget, 
	const UScriptStruct* StructType)
{
	FString Label;
	StructType->ExportText(Label, Data, Data, nullptr, PPF_None, nullptr);
	UpdateExportedTextWidget(FText::FromString(MoveTemp(Label)), Widget);
}

static void UpdateExportedTextWidget(ITypedElementDataStorageInterface& DataStorage, FTypedElementSlateWidgetReferenceColumn& Widget,
	const FTypedElementScriptStructTypeInfoColumn& TypeInfo, const FTypedElementRowReferenceColumn& ReferencedRow)
{
	const UScriptStruct* StructType = TypeInfo.TypeInfo.Get();
	if (void* Data = DataStorage.GetColumnData(ReferencedRow.Row, StructType))
	{
		UpdateExportedTextWidget(Data, Widget, StructType);
	}
}

static TypedElementQueryHandle RegisterUpdateCallback(ITypedElementDataStorageInterface& DataStorage, const UScriptStruct* Target)
{
	using namespace TypedElementQueryBuilder;
	using DSI = ITypedElementDataStorageInterface;
	namespace DS = TypedElementDataStorage;
	
	TypedElementQueryHandle TypeDataQuery = DataStorage.RegisterQuery(
		Select()
			.ReadOnly(Target)
		.Where()
			.Any<FTypedElementSyncFromWorldTag, FTypedElementSyncBackToWorldTag>()
		.Compile());

	FString Name = TEXT("Sync exported text widgets (");
	Target->AppendName(Name);
	Name += ')';

	FName ProcessorName(Name);

	return DataStorage.RegisterQuery(
		Select(ProcessorName,
			FProcessor(DSI::EQueryTickPhase::FrameEnd, DataStorage.GetQueryTickGroupName(DSI::EQueryTickGroups::SyncWidgets))
				.ForceToGameThread(true),
			[Target, ProcessorName](
				DS::IQueryContext& Context, 
				FTypedElementSlateWidgetReferenceColumn& Widget,
				const FTypedElementScriptStructTypeInfoColumn& TypeInfo,
				const FTypedElementRowReferenceColumn& ReferencedRow)
			{
				const UScriptStruct* StructType = TypeInfo.TypeInfo.Get();
				if (!ensureMsgf(StructType != nullptr, TEXT("WeakPtr TypeInfo is null in query '%s'"), *ProcessorName.ToString()))
				{
					return;
				}

				/* This query will grab all ExportedText widgets that were created for rows with the "Target" column, but we want to make sure we are
				 * only updating the widgets that are actually displaying the "Target" column.
				 *
				 * E.g a row could have ColumnA and ColumnB which are both using the exported text widget to display - but there is no way for the
				 * two widgets to be differentiated from a TEDS query. So if the widget for ColumnB wants to update: Both the widgets for ColumnA
				 * and ColumnB would match the query condition (WidgetRow has FExportedTextWidgetTag && TargetRow has ColumnB), but this
				 * query (+ subquery) only have access to ColumnB and only want to update the widget that's displaying ColumnB.
				 *
				 * To work around this we check to make sure the widget this query is trying to update is for the column this query is targeting.
				 */
				if(StructType == Target)
				{
					Context.RunSubquery(0, ReferencedRow.Row, 
					[&Widget, StructType](const DS::FQueryDescription&, DS::ISubqueryContext& SubqueryContext)
					{
						const void* ColumnData = SubqueryContext.GetColumn(StructType);
						UpdateExportedTextWidget(ColumnData, Widget, StructType);
					});
				}
			})
		.Where()
			.All<FExportedTextWidgetTag>()
		.DependsOn()
			.SubQuery(TypeDataQuery)
		.Compile());
}

void UExportedTextWidgetFactory::RegisterWidgetConstructors(ITypedElementDataStorageInterface& DataStorage,
	ITypedElementDataStorageUiInterface& DataStorageUi) const
{
	DataStorageUi.RegisterWidgetFactory(FName(TEXT("General.Cell.Default")), FExportedTextWidgetConstructor::StaticStruct());
}

//
// FExportedTextWidgetConstructor
//

FExportedTextWidgetConstructor::FExportedTextWidgetConstructor()
	: Super(FExportedTextWidgetConstructor::StaticStruct())
{
}

TConstArrayView<const UScriptStruct*> FExportedTextWidgetConstructor::GetAdditionalColumnsList() const
{
	static TTypedElementColumnTypeList<
		FTypedElementRowReferenceColumn,
		FTypedElementScriptStructTypeInfoColumn,
		FExportedTextWidgetTag> Columns;
	return Columns;
}

const TypedElementDataStorage::FQueryConditions* FExportedTextWidgetConstructor::GetQueryConditions() const
{
	// For the exported text widget, the query condition we are matched against is the column we are exporting text for
	return &MatchedColumn;
}

FString FExportedTextWidgetConstructor::CreateWidgetDisplayName(ITypedElementDataStorageInterface* DataStorage,
	TypedElementDataStorage::RowHandle Row) const
{
	if (FTypedElementScriptStructTypeInfoColumn* TypeInfoColumn = DataStorage->GetColumn<FTypedElementScriptStructTypeInfoColumn>(Row))
	{
		return DescribeColumnType(TypeInfoColumn->TypeInfo.Get());
	}
	else
	{
		return DescribeColumnType(nullptr);
	}
}

TSharedPtr<SWidget> FExportedTextWidgetConstructor::CreateWidget(const TypedElementDataStorage::FMetaDataView& Arguments)
{
	return SNew(STextBlock);
}

bool FExportedTextWidgetConstructor::FinalizeWidget(
	ITypedElementDataStorageInterface* DataStorage,
	ITypedElementDataStorageUiInterface* DataStorageUi,
	TypedElementRowHandle Row,
	const TSharedPtr<SWidget>& Widget)
{
	FTypedElementScriptStructTypeInfoColumn& TypeInfoColumn = *DataStorage->GetColumn<FTypedElementScriptStructTypeInfoColumn>(Row);

	// NOTE: We are currently assuming that an instance of FExportedTextWidgetConstructor will only be used to show the same type info
	// which isn't ideal but it's better than nothing since we need some sort of matched conditions for column based virtualization to work.
	// TEDS UI TODO: We should work around it by refactoring this into an STedsWidget in the future so it can store the column conditions per instance
	MatchedColumn = TypedElementDataStorage::FQueryConditions(TypedElementDataStorage::FColumn(TypeInfoColumn.TypeInfo));

	if (TypeInfoColumn.TypeInfo->IsChildOf(FTypedElementDataStorageTag::StaticStruct()))
	{
		UpdateExportedTextWidget(
			LOCTEXT("ExportedTextWidgetTag", "<Tag>"),
			*DataStorage->GetColumn<FTypedElementSlateWidgetReferenceColumn>(Row));
	}
	else
	{
		UpdateExportedTextWidget(
			*DataStorage,
			*DataStorage->GetColumn<FTypedElementSlateWidgetReferenceColumn>(Row),
			TypeInfoColumn,
			*DataStorage->GetColumn<FTypedElementRowReferenceColumn>(Row));

		UExportedTextWidgetFactory* Factory =
			UExportedTextWidgetFactory::StaticClass()->GetDefaultObject<UExportedTextWidgetFactory>();
		if (Factory && !Factory->RegisteredTypes.Contains(TypeInfoColumn.TypeInfo))
		{
			RegisterUpdateCallback(*DataStorage, TypeInfoColumn.TypeInfo.Get());
			Factory->RegisteredTypes.Add(TypeInfoColumn.TypeInfo);
		}
	}
	
	return true;
}

#undef LOCTEXT_NAMESPACE
