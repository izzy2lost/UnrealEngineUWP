// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IWorkspaceEditorModule.h"
#include "UObject/TopLevelAssetPath.h"

struct FWorkspaceAssetRegistryExports;

namespace UE::Workspace
{
	struct FAssetDocumentSummoner;
	class FWorkspaceEditor;
}

namespace UE::Workspace
{

class FWorkspaceEditorModule : public IWorkspaceEditorModule
{
private:
	// IWorkspaceModule interface
	virtual void RegisterObjectDocumentType(const FTopLevelAssetPath& InClassPath, const FObjectDocumentArgs& InParams) override;
	virtual void UnregisterObjectDocumentType(const FTopLevelAssetPath& InClassPath) override;
	virtual FObjectDocumentArgs CreateGraphDocumentArgs(const FGraphDocumentWidgetArgs& InArgs) override;
	virtual void OpenWorkspaceForObject(UObject* InObject, EOpenWorkspaceMethod InOpenMethod, const TSubclassOf<UWorkspaceFactory> WorkSpaceFactoryClass) override;
	virtual FOnRegisterDetailCustomizations& OnRegisterWorkspaceDetailsCustomization() override;

	// Find an existing registered object document type
	const FObjectDocumentArgs* FindObjectDocumentType(const FTopLevelAssetPath& InClassPath) const;

	// Find the set of allowed object types for the specified spawn location
	TArray<FTopLevelAssetPath> GetAllowedObjectTypesForArea(FName InSpawnLocation) const;

	// Get the exported set of assets in asset registry tags for a workspace's FAssetData
	static bool GetExportedAssetsForWorkspace(const FAssetData& InWorkspaceAsset, FWorkspaceAssetRegistryExports& OutExports);

	// Applies any previously registered details-view customizations
	void ApplyWorkspaceDetailsCustomization(TSharedPtr<IDetailsView>& DetailsView) const;

	TMap<FTopLevelAssetPath, FObjectDocumentArgs> ObjectDocumentArgs;

	TMap<FName, TSet<FTopLevelAssetPath>> DocumentAreaMap;

	/** Event called to allow external clients to register details customizations */
	FOnRegisterDetailCustomizations OnRegisterDetailCustomizations;

	friend struct FAssetDocumentSummoner;
	friend class FWorkspaceEditor;
};

}
