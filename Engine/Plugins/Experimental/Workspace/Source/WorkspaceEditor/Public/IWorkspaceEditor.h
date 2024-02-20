// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "WorkflowOrientedApp/WorkflowCentricApplication.h"

namespace UE::Workspace
{

class IWorkspaceEditor : public FWorkflowCentricApplication
{
public:
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
};

}