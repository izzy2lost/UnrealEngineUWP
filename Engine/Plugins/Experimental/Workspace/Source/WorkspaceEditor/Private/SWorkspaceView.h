// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ContentBrowserDelegates.h"
#include "Widgets/SCompoundWidget.h"

class UWorkspace;
class UWorkSpaceAssetUserData;
class SSceneOutliner;

namespace UE::Workspace
{
class IWorkspaceEditor;

class SWorkspaceView : public SCompoundWidget
{
	SLATE_BEGIN_ARGS(SWorkspaceView) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, UWorkspace* InWorkspace, TSharedRef<UE::Workspace::IWorkspaceEditor> InWorkspaceEditor);
private:
	UWorkspace* Workspace = nullptr;
	TSharedPtr<SSceneOutliner> SceneWorkspaceOutliner;
};

};