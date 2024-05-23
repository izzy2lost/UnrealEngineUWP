// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Elements/Interfaces/TypedElementDataStorageInterface.h"
#include "Compatibility/TedsCompatibilityUtils.h"
#include "TypedElementOutlinerMode.h"

class FBaseTEDSOutlinerMode;

/*
 * A generic item in the TEDS driven Outliner, that uses a TypedElementRowHandle to uniquely identify the object it is
 * looking at. Functionality should be added through TEDS queries instead of having a different TreeItem type for each
 * type of object you are looking at (Actor vs Entity vs Folder), See CreateGenericTEDSOutliner() in
 * EntityEditorModule.cpp for example usage
 * Inherits from ISceneOutlinerItem - which determines what type of item you are looking at. E.G FActorTreeItem for actors
 */
struct FTypedElementOutlinerTreeItem : ISceneOutlinerTreeItem
{
public:
	
	DECLARE_DELEGATE_RetVal_OneParam(bool, FFilterPredicate, const TypedElementDataStorage::RowHandle);

	bool Filter(FFilterPredicate Pred) const
	{
		return Pred.Execute(RowHandle);
	}

	FTypedElementOutlinerTreeItem(const TypedElementRowHandle& InRowHandle, const TSharedRef<const FTedsOutlinerImpl>& InTedsOutlinerImpl);

	/* Begin ISceneOutlinerTreeItem Implementation */
	virtual bool IsValid() const override;
	virtual FSceneOutlinerTreeItemID GetID() const override;
	virtual FString GetDisplayString() const override;
	virtual bool CanInteract() const override;
	virtual TSharedRef<SWidget> GenerateLabelWidget(ISceneOutliner& Outliner, const STableRow<FSceneOutlinerTreeItemPtr>& InRow) override;
	/** Generate a context menu for this item. Only called if *only* this item is selected. */
	virtual void GenerateContextMenu(UToolMenu* Menu, SSceneOutliner& Outliner);
	/* End ISceneOutlinerTreeItem Implementation */

	TEDSOUTLINER_API static const FSceneOutlinerTreeItemType Type;

	TEDSOUTLINER_API TypedElementRowHandle GetRowHandle() const;

private:
	const TypedElementRowHandle RowHandle;
	const TSharedRef<const FTedsOutlinerImpl> TedsOutlinerImpl;
};