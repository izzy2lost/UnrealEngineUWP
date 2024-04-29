// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IWorkspaceOutlinerItemDetails.h"

namespace UE::AnimNext::Editor
{
class FAnimNextGraphItemDetails : public UE::Workspace::IWorkspaceOutlinerItemDetails
{
public:
	FAnimNextGraphItemDetails() = default;
	virtual ~FAnimNextGraphItemDetails() override = default;
	virtual void HandleDoubleClick(const FToolMenuContext& ToolMenuContext) const override;	
	virtual UPackage* GetPackage(const FWorkspaceOutlinerItemExport& Export) const override;
	virtual const FSlateBrush* GetItemIcon() const override;

	static void RegisterToolMenuExtensions();
	static void UnregisterToolMenuExtensions();
};
}
