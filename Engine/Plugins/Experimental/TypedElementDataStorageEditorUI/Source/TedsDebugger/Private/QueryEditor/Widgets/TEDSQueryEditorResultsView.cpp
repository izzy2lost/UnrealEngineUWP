// Copyright Epic Games, Inc. All Rights Reserved.

#include "TEDSQueryEditorResultsView.h"

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
#include "QueryEditor/TEDSQueryEditorModel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "TedsOutlinerModule"

using namespace UE::Teds::Debug::QueryEditor;

SResultsView::~SResultsView()
{
	Model->GetModelChangedDelegate().Remove(ModelChangedDelegateHandle);
}

void SResultsView::Construct(const FArguments& InArgs, FTedsQueryEditorModel& InModel)
{
	Model = &InModel;
	ModelChangedDelegateHandle = Model->GetModelChangedDelegate().AddRaw(this, &SResultsView::OnModelChanged);
	// RowQueryHandle = TypedElementDataStorage::InvalidQueryHandle;

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
			SAssignNew(TableViewHolder, SHorizontalBox)
			+SHorizontalBox::Slot()
			[
				// Placeholder for Table View
				SNullWidget::NullWidget
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
}

void SResultsView::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	using namespace TypedElementQueryBuilder;

	ITypedElementDataStorageInterface& TedsInterface = Model->GetTedsInterface();

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
		// TODO: For now, destroy and re-create the table view
		// This could be all that is required at some point
		//
		// FTypedElementSceneOutlinerQueryBinder::GetInstance().AssignQuery(SelectQueryHandle, TableView);
		// TableView->FullRefresh();
		
		TableViewHolder->ClearChildren();

		FSceneOutlinerInitializationOptions InitOptions;
		InitOptions.bShowHeaderRow = true;
		InitOptions.bShowSearchBox = false;
		InitOptions.FilterBarOptions.bHasFilterBar = false;
		InitOptions.OutlinerIdentifier = "QueryEditorResultsView";
		
		// if (RowQueryHandle != TypedElementDataStorage::InvalidQueryHandle)
		// {
		// 	TedsInterface.UnregisterQuery(RowQueryHandle);
		// 	RowQueryHandle = TypedElementDataStorage::InvalidQueryHandle;
		// } 
		RowQueryDescription = Model->GenerateNoSelectQueryDescription();
		if (!RowQueryDescription.ConditionTypes.IsEmpty())
		{
			// Work around since query registration takes by RRef but later the
			// FTypedElementOutlinerModeParams.QueryDescription needs a reference to it.
			// We are just going to copy it for now...
			// TypedElementDataStorage::FQueryDescription Copy = SelectQueryDescription;
			RowQueryDescription.Action = TypedElementDataStorage::FQueryDescription::EActionType::Select;
			//RowQueryHandle = TedsInterface.RegisterQuery(MoveTemp(RowQueryDescription));
		}
	
		FTypedElementOutlinerModeParams Params(nullptr);
		Params.QueryDescription = TAttribute<TypedElementDataStorage::FQueryDescription>::CreateLambda([WeakResultsView = AsWeak()]()
		{
			if (TSharedPtr<SWidget> Shared = WeakResultsView.Pin())
			{
				SResultsView* This = static_cast<SResultsView*>(Shared.Get());
				return This->RowQueryDescription;
			}
			return TypedElementDataStorage::FQueryDescription();
		});
		Params.HierarchyData = TOptional<FTypedElementOutlinerHierarchyData>(); // We don't want to show hierarchies
		Params.CellWidgetPurposes = TArray<FName>{TEXT("General.Cell")};

		TypedElementDataStorage::FQueryDescription ColumnQueryDescription = Model->GenerateQueryDescription();
		if (ColumnQueryHandle != TypedElementDataStorage::InvalidQueryHandle)
		{
			TedsInterface.UnregisterQuery(ColumnQueryHandle);
			ColumnQueryHandle = TypedElementDataStorage::InvalidQueryHandle;
		}

		if (!ColumnQueryDescription.ConditionTypes.IsEmpty() || !ColumnQueryDescription.SelectionTypes.IsEmpty())
		{
			// Work around since query registration takes by RRef but later the
			// FTypedElementOutlinerModeParams.QueryDescription needs a reference to it.
			// We are just going to copy it for now...
			// TypedElementDataStorage::FQueryDescription Copy = SelectQueryDescription;
			ColumnQueryDescription.Action = TypedElementDataStorage::FQueryDescription::EActionType::Select;
			ColumnQueryHandle = TedsInterface.RegisterQuery(MoveTemp(ColumnQueryDescription));
		}

		FTedsOutlinerModule& TedsOutlinerModule = FModuleManager::GetModuleChecked<FTedsOutlinerModule>("TedsOutliner");
		if (ColumnQueryHandle != TypedElementDataStorage::InvalidQueryHandle)
		{
			TSharedPtr<ISceneOutliner> TableView = TedsOutlinerModule.CreateTedsOutliner(InitOptions, Params, ColumnQueryHandle);

			TableViewHolder->AddSlot()
			[
				TableView.ToSharedRef()
			];
		}
		else
		{
			TableViewHolder->AddSlot()
			[
				// Placeholder for table view.  Mass doesn't like invalid/empty queries
				SNullWidget::NullWidget
			];
		}
	}

	SetCanTick(false);
}

void SResultsView::OnModelChanged()
{
	SetCanTick(true);
	Invalidate(EInvalidateWidgetReason::Layout);
	
}

#undef LOCTEXT_NAMESPACE