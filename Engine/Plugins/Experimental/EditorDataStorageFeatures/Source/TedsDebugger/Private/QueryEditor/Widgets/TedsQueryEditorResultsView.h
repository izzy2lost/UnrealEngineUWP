// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Elements/Interfaces/TypedElementDataStorageInterface.h"
#include "Widgets/SCompoundWidget.h"

class ISceneOutliner;
class SSceneOutliner;
class SHorizontalBox;

namespace UE::Teds::Debug::QueryEditor
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
		
		FTedsQueryEditorModel* Model = nullptr;
		FDelegateHandle ModelChangedDelegateHandle;

		TypedElementDataStorage::FQueryDescription RowQueryDescription;
		TypedElementQueryHandle CountQueryHandle = TypedElementDataStorage::InvalidQueryHandle;
		TypedElementQueryHandle ColumnQueryHandle = TypedElementDataStorage::InvalidQueryHandle;

		TSharedPtr<SHorizontalBox> TableViewHolder;

		
	};

	
}
