// Copyright Epic Games, Inc. All Rights Reserved.

#include "QueryStack/FQueryStackNode_RowView.h"

namespace UE::EditorDataStorage
{
	TConstArrayView<TypedElementDataStorage::RowHandle> FQueryStackNode_RowView::GetOrderedRowList()
	{
		return *Rows;
	}

	uint32 FQueryStackNode_RowView::GetRevisionId() const
	{
		return RevisionId;
	}

	FQueryStackNode_RowView::FQueryStackNode_RowView(TArray<TypedElementDataStorage::RowHandle>* InRows)
		: Rows(InRows)
	{
		
	}

	void FQueryStackNode_RowView::MarkDirty()
	{
		++RevisionId;
	}
}


