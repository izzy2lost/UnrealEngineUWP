// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "WorkspaceAssetRegistryInfo.h"

class UPackage;
struct FToolMenuContext;
struct FSlateBrush;

namespace UE::Workspace
{

typedef FName FOutlinerItemDetailsId;
static FOutlinerItemDetailsId MakeOutlinerDetailsId(const FWorkspaceOutlinerItemExport& InExport)
{
    return InExport.GetData().IsValid() ? InExport.GetData().GetScriptStruct()->GetFName() : NAME_None;
}

class IWorkspaceOutlinerItemDetails : public TSharedFromThis<IWorkspaceOutlinerItemDetails>
{
public:
    virtual ~IWorkspaceOutlinerItemDetails() = default;
    virtual const FSlateBrush* GetItemIcon() const { return nullptr; }
    virtual void HandleDoubleClick(const FToolMenuContext& ToolMenuContext) const {}
    virtual UPackage* GetPackage(const FWorkspaceOutlinerItemExport& Export) const { return nullptr; }
};

}
