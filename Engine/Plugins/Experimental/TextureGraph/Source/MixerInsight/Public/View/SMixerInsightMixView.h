// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

//#include "SlateBasics.h"

#include "Model/MixerInsightSession.h"
#include "Widgets/Views/STreeView.h"
#include "Widgets/SCompoundWidget.h"
class SMixerInsightMixListViewRow; /// Declare the concrete type of widget used for the raws of the view. Defined in the cpp file
class STableViewBase;
class ITableRow;

class MIXERINSIGHT_API SMixerInsightMixListView : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SMixerInsightMixListView) {}
	SLATE_END_ARGS()

		void Construct(const FArguments& Args);

	// TreeView Item Types
	class FItemData;
	using FItem = TSharedPtr<FItemData>;
	using FItemArray = TArray< FItem >;
	class FItemData
	{
	public:
		FItemData(RecordID rid) : _recordID(rid) {}
		TSharedPtr < SMixerInsightMixListViewRow > _widget;
		RecordID _recordID;
		FItemArray	_children;
	};
	//using SItemTableView = SListView< FItem >;
	using SItemTableView = STreeView< FItem >;


	// Standard delegates for the view
	TSharedRef<ITableRow>	OnGenerateRowForView(FItem item, const TSharedRef<STableViewBase>& OwnerTable);
	FORCEINLINE void		OnGetChildrenForView(FItem item, FItemArray& children) { children = item->_children; };
	void					OnClickItemForView(FItem item);
	void					OnDoubleClickItemForView(FItem item);

	/// The list of root items
	FItemArray _rootItems;

	// The TreeView widget
	TSharedPtr<SItemTableView> _tableView;

	void RefreshRootItems();

	void OnMixNew(RecordID rid);
	void OnMixUpdate(RecordID rid);
	void OnEngineReset(int id);

	void UpdateItemData(FItemData& item);
};