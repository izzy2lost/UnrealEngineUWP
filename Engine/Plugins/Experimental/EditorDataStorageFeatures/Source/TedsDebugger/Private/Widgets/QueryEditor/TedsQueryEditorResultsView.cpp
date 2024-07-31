// Copyright Epic Games, Inc. All Rights Reserved.

#include "TedsQueryEditorResultsView.h"

#include "SceneOutlinerPublicTypes.h"
#include "SWarningOrErrorBox.h"
#include "TedsOutlinerModule.h"
#include "TypedElementOutlinerColumnIntegration.h"
#include "TypedElementOutlinerMode.h"
#include "Components/VerticalBox.h"
#include "Elements/Columns/TypedElementLabelColumns.h"
#include "Elements/Interfaces/TypedElementDataStorageInterface.h"
#include "Elements/Framework/TypedElementQueryBuilder.h"
#include "Modules/ModuleManager.h"
#include "QueryEditor/TedsQueryEditorModel.h"
#include "QueryStack/FQueryStackNode_RowView.h"
#include "Widgets/STedsTableViewer.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "TedsDebuggerModule"

using namespace UE::EditorDataStorage::Debug::QueryEditor;

SResultsView::~SResultsView()
{
	Model->GetModelChangedDelegate().Remove(ModelChangedDelegateHandle);
}

void SResultsView::Construct(const FArguments& InArgs, FTedsQueryEditorModel& InModel)
{
	Model = &InModel;
	ModelChangedDelegateHandle = Model->GetModelChangedDelegate().AddRaw(this, &SResultsView::OnModelChanged);
	// RowQueryHandle = TypedElementDataStorage::InvalidQueryHandle;

	RowQueryStack = MakeShared<UE::EditorDataStorage::FQueryStackNode_RowView>(&TableViewerRows);

	ChildSlot
	[
		SNew(SVerticalBox)
		+SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SWarningOrErrorBox)
			.MessageStyle(EMessageStyle::Warning)
			.Message(LOCTEXT("QueryEditor_TableViewUnreliable", "Integration of table view is WIP. Currently Unreliable"))
		]
		+SVerticalBox::Slot()
		[
			SAssignNew(TableViewer, UE::EditorDataStorage::STedsTableViewer)
			.QueryStack(RowQueryStack)
		]
		+SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(STextBlock)
			.Text_Lambda([this]()
			{
				auto& TedsInterface = Model->GetTedsInterface();
				TypedElementDataStorage::FQueryResult QueryResult = TedsInterface.RunQuery(CountQueryHandle);
				if (QueryResult.Completed == TypedElementDataStorage::FQueryResult::ECompletion::Fully)
				{
					FString String = FString::Printf(TEXT("Element Count: %u"), QueryResult.Count);
					return FText::FromString(String);
				}
				else
				{
					return FText::FromString(TEXT("Invalid query"));
				}
			})
		]
	];
}

void SResultsView::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	using namespace TypedElementQueryBuilder;

	ITypedElementDataStorageInterface& TedsInterface = Model->GetTedsInterface();

	if(bModelDirty)
	{
		{
			TypedElementDataStorage::FQueryDescription CountQueryDescription = Model->GenerateNoSelectQueryDescription();
		
			if (CountQueryHandle != TypedElementDataStorage::InvalidQueryHandle)
			{
				TedsInterface.UnregisterQuery(CountQueryHandle);
				CountQueryHandle = TypedElementDataStorage::InvalidQueryHandle;
			}
			CountQueryHandle = TedsInterface.RegisterQuery(MoveTemp(CountQueryDescription));
		}

		{
			TypedElementDataStorage::FQueryDescription TableViewerQueryDescription = Model->GenerateQueryDescription();

			// Update the columns in the table viewer using the selection types from the query description
			TableViewer->SetColumns(TArray<TWeakObjectPtr<const UScriptStruct>>(TableViewerQueryDescription.SelectionTypes));
			
			if (TableViewerQueryHandle != TypedElementDataStorage::InvalidQueryHandle)
			{
				TedsInterface.UnregisterQuery(TableViewerQueryHandle);
				TableViewerQueryHandle = TypedElementDataStorage::InvalidQueryHandle;
			}

			// Mass doesn't like empty queries, so we only set it if there are actual conditions
			if(TableViewerQueryDescription.ConditionTypes.Num())
			{
				TableViewerQueryHandle = TedsInterface.RegisterQuery(MoveTemp(TableViewerQueryDescription));
			}
		}
		
		bModelDirty = false;
	}

	// Every frame we re-run the query to update the rows the table viewer is showing
	if(TableViewerQueryHandle != TypedElementDataStorage::InvalidQueryHandle)
	{
		TSet<TypedElementDataStorage::RowHandle> NewTableViewerRows_Set;
		NewTableViewerRows_Set.Reserve(TableViewerRows_Set.Num());

		TypedElementDataStorage::FQueryResult QueryResult = Model->GetTedsInterface().RunQuery(TableViewerQueryHandle,
			CreateDirectQueryCallbackBinding([&NewTableViewerRows_Set](const ITypedElementDataStorageInterface::IDirectQueryContext& Context, const TypedElementDataStorage::RowHandle*)
		{
			NewTableViewerRows_Set.Append(Context.GetRowHandles());
		}));

		// Check if the two sets are equal, i.e not changes and no need to update the table viewer
		const bool bSetsEqual = (TableViewerRows_Set.Num() == NewTableViewerRows_Set.Num()) && TableViewerRows_Set.Includes(NewTableViewerRows_Set);

		if(!bSetsEqual)
		{
			Swap(TableViewerRows_Set, NewTableViewerRows_Set);
			TableViewerRows = TableViewerRows_Set.Array();
			RowQueryStack->MarkDirty();
		}
	}
}

void SResultsView::OnModelChanged()
{
	bModelDirty = true;
	Invalidate(EInvalidateWidgetReason::Layout);
}

#undef LOCTEXT_NAMESPACE