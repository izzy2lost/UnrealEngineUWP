// Copyright Epic Games, Inc. All Rights Reserved.

#include "TedsQueryEditorResultsView.h"

#include "SWarningOrErrorBox.h"
#include "TedsOutlinerModule.h"
#include "TedsTableViewerColumn.h"
#include "Elements/Interfaces/TypedElementDataStorageInterface.h"
#include "Elements/Framework/TypedElementQueryBuilder.h"
#include "Elements/Framework/TypedElementRegistry.h"
#include "Modules/ModuleManager.h"
#include "QueryEditor/TedsQueryEditorModel.h"
#include "QueryStack/FQueryStackNode_RowView.h"
#include "Widgets/STedsTableViewer.h"
#include "Widgets/SRowDetails.h"
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

	// Create a custom column for the table viewer to display row handles
	CreateRowHandleColumn();
	
	RowQueryStack = MakeShared<FQueryStackNode_RowView>(&TableViewerRows);

	ChildSlot
	[
		SNew(SVerticalBox)
		+SVerticalBox::Slot()
		[
			SNew(SSplitter)
			+SSplitter::Slot()
			.Value(0.5f)
			[
				SAssignNew(TableViewer, UE::EditorDataStorage::STedsTableViewer)
				.QueryStack(RowQueryStack)
				.OnSelectionChanged(STedsTableViewer::FOnSelectionChanged::CreateLambda(
					[this](TypedElementDataStorage::RowHandle RowHandle)
						{
							if(RowDetailsWidget)
							{
								if(RowHandle != TypedElementDataStorage::InvalidRowHandle)
								{
									RowDetailsWidget->SetRow(RowHandle);
								}
								else
								{
									RowDetailsWidget->ClearRow();
								}
								
							}
						}))
			]
			+SSplitter::Slot()
			.Value(0.5f)
			[
				SAssignNew(RowDetailsWidget, UE::EditorDataStorage::SRowDetails)
			]
			
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

	if(RowHandleColumn)
	{
		TableViewer->AddCustomColumn(RowHandleColumn.ToSharedRef());
	}
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

			// Since setting the columns clears all columns, we are going to re-add the custom column back
			if(RowHandleColumn)
			{
				TableViewer->AddCustomColumn(RowHandleColumn.ToSharedRef());
			}
			
			if (TableViewerQueryHandle != TypedElementDataStorage::InvalidQueryHandle)
			{
				TedsInterface.UnregisterQuery(TableViewerQueryHandle);
				TableViewerQueryHandle = TypedElementDataStorage::InvalidQueryHandle;
			}

			// Mass doesn't like empty queries, so we only set it if there are actual conditions
			if(TableViewerQueryDescription.ConditionTypes.Num() || TableViewerQueryDescription.SelectionTypes.Num())
			{
				TableViewerQueryHandle = TedsInterface.RegisterQuery(MoveTemp(TableViewerQueryDescription));
			}
		}
		
		bModelDirty = false;
	}

	TSet<TypedElementDataStorage::RowHandle> NewTableViewerRows_Set;
	
	// Every frame we re-run the query to update the rows the table viewer is showing
	if(TableViewerQueryHandle != TypedElementDataStorage::InvalidQueryHandle)
	{
		NewTableViewerRows_Set.Reserve(TableViewerRows_Set.Num());

		TypedElementDataStorage::FQueryResult QueryResult = Model->GetTedsInterface().RunQuery(TableViewerQueryHandle,
			CreateDirectQueryCallbackBinding([&NewTableViewerRows_Set](const ITypedElementDataStorageInterface::IDirectQueryContext& Context, const TypedElementDataStorage::RowHandle*)
		{
			NewTableViewerRows_Set.Append(Context.GetRowHandles());
		}));
	}
	
	// Check if the two sets are equal, i.e not changes and no need to update the table viewer
	const bool bSetsEqual = (TableViewerRows_Set.Num() == NewTableViewerRows_Set.Num()) && TableViewerRows_Set.Includes(NewTableViewerRows_Set);

	if(!bSetsEqual)
	{
		Swap(TableViewerRows_Set, NewTableViewerRows_Set);
		TableViewerRows = TableViewerRows_Set.Array();
		RowQueryStack->MarkDirty();
	}
}

void SResultsView::OnModelChanged()
{
	bModelDirty = true;
	Invalidate(EInvalidateWidgetReason::Layout);
}

void SResultsView::CreateRowHandleColumn()
{
	auto AssignWidgetToColumn = [this](TUniquePtr<FTypedElementWidgetConstructor> Constructor, TConstArrayView<TWeakObjectPtr<const UScriptStruct>>)
	{
		TSharedPtr<FTypedElementWidgetConstructor> WidgetConstructor(Constructor.Release());
		RowHandleColumn = MakeShared<FTedsTableViewerColumn>(TEXT("Row Handle"), WidgetConstructor);
		return false;
	};
	
	UTypedElementRegistry* Registry = UTypedElementRegistry::GetInstance();
	checkf(Registry, TEXT("SResultsView created before UTypedElementRegistry is available."));
	ITypedElementDataStorageUiInterface* StorageUi = Registry->GetMutableDataStorageUi();
	checkf(StorageUi, TEXT("SResultsView created before data storage interfaces were initialized."))

	StorageUi->CreateWidgetConstructors(TEXT("General.Cell.RowHandle"), TypedElementDataStorage::FMetaDataView(), AssignWidgetToColumn);
}

#undef LOCTEXT_NAMESPACE
