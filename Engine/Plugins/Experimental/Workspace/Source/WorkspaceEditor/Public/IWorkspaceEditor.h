// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Tools/BaseAssetToolkit.h"

class UWorkspaceAssetEditor;
class UWorkspaceSchema;

namespace UE::Workspace
{

typedef TWeakPtr<SWidget> FGlobalSelectionId;
using FOnClearGlobalSelection = FSimpleDelegate;

class IWorkspaceEditor : public FBaseAssetToolkit
{
public:
	IWorkspaceEditor(UAssetEditor* InOwningAssetEditor) : FBaseAssetToolkit(InOwningAssetEditor) {}

	// Open the supplied assets for editing within the workspace editor
	virtual void OpenAssets(TConstArrayView<FAssetData> InAssets) = 0;

	// Open the supplied objects for editing within the workspace editor
	virtual void OpenObjects(TConstArrayView<UObject*> InObjects) = 0;

	// Close the supplied objects if they are open for editing within the workspace editor
	virtual void CloseObjects(TConstArrayView<UObject*> InObjects) = 0;

	// Show the supplied objects in the workspace editor details panel
	virtual void SetDetailsObjects(const TArray<UObject*>& InObjects) = 0;

	// Refresh the workspace editor details panel
	virtual void RefreshDetails() = 0;

	// Exposes the editor WorkspaceSchema
	virtual UWorkspaceSchema* GetSchema() const = 0;

	// Set the _current_ global selection (last SWidget with selection set) with delegate to clear it selection on next SetGlobalSelection()
	virtual void SetGlobalSelection(FGlobalSelectionId SelectionId, FOnClearGlobalSelection OnClearSelectionDelegate) = 0;

	virtual void SetFocussedAsset(const TObjectPtr<UObject> InAsset) = 0;
	virtual const TObjectPtr<UObject> GetFocussedAssetOfClass(const TObjectPtr<UClass> InClass ) const = 0;
	
	template<typename AssetClass>
	TObjectPtr<AssetClass> GetFocussedAsset() const { return GetFocussedAssetOfClass(AssetClass::StaticClass()); }
	TObjectPtr<UObject> GetFocussedAsset() const { return GetFocussedAssetOfClass(UObject::StaticClass()); }
};

}