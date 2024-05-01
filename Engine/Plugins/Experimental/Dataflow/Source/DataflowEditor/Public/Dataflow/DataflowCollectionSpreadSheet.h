// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/SListView.h"
#include "Dataflow/DataflowEdNode.h"
#include "Dataflow/DataflowCollectionSpreadSheetWidget.h"
#include "Dataflow/DataflowSelection.h"
#include "Dataflow/DataflowView.h"


/**
*
* Class to handle the SelectionView widget
*
*/
class FDataflowCollectionSpreadSheet : public FDataflowNodeView
{
public:
	~FDataflowCollectionSpreadSheet();

	virtual void SetSupportedOutputTypes() override;
	virtual void UpdateViewData() override;
	virtual void ConstructionViewSelectionChanged(const TArray<UPrimitiveComponent*>& InSelectedComponents) override {};

	void SetCollectionSpreadSheet(TSharedPtr<SCollectionSpreadSheetWidget>& InCollectionSpreadSheet);

private:
	TSharedPtr<SCollectionSpreadSheetWidget> CollectionSpreadSheet;

	FDelegateHandle OnPinnedDownChangedDelegateHandle;
	FDelegateHandle OnRefreshLockedChangedDelegateHandle;
};

