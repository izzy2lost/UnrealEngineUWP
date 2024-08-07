// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Elements/Interfaces/TypedElementDataStorageInterface.h"
#include "Widgets/SCompoundWidget.h"

namespace UE::EditorDataStorage
{
	class SRowDetails;
}

class ISceneOutliner;
class SSceneOutliner;
class SHorizontalBox;
struct FTypedElementWidgetConstructor;

namespace UE::EditorDataStorage
{
	class STedsTableViewer;
	class FQueryStackNode_RowView;
	class FTedsTableViewerColumn;
}

namespace UE::EditorDataStorage::Debug::QueryEditor
{
	class FTedsQueryEditorModel;

	class SResultsView : public SCompoundWidget
	{
	public:
		SLATE_BEGIN_ARGS( SResultsView ){}
		SLATE_END_ARGS()

		~SResultsView() override;
		void Construct(const FArguments& InArgs, FTedsQueryEditorModel& InModel);
		void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

	private:

		void OnModelChanged();
		void CreateRowHandleColumn();
		
		FTedsQueryEditorModel* Model = nullptr;
		FDelegateHandle ModelChangedDelegateHandle;
		bool bModelDirty = true;


		TypedElementQueryHandle CountQueryHandle = TypedElementDataStorage::InvalidQueryHandle;
		TypedElementQueryHandle TableViewerQueryHandle = TypedElementDataStorage::InvalidQueryHandle;

		TArray<TypedElementDataStorage::RowHandle> TableViewerRows;
		// We have to keep a TSet copy because queries return duplicate rows sometimes and to have some form of sorted order for the rows for now
		TSet<TypedElementDataStorage::RowHandle> TableViewerRows_Set;
		TSharedPtr<UE::EditorDataStorage::STedsTableViewer> TableViewer;
		TSharedPtr<UE::EditorDataStorage::FQueryStackNode_RowView> RowQueryStack;

		// Custom column for the table viewer to display row handles
		TSharedPtr<UE::EditorDataStorage::FTedsTableViewerColumn> RowHandleColumn;

		// Widget that displays details of a row
		TSharedPtr<UE::EditorDataStorage::SRowDetails> RowDetailsWidget;

	};

	
}
