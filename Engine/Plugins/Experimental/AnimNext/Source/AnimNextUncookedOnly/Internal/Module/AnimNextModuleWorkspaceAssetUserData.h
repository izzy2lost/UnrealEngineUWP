// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "WorkspaceAssetRegistryInfo.h"
#include "Param/ParamType.h"
#include "IAnimNextRigVMGraphInterface.h"
#include "RigVMModel/Nodes/RigVMFunctionReferenceNode.h"

#include "AnimNextModuleWorkspaceAssetUserData.generated.h"

USTRUCT()
struct FAnimNextGraphAssetOutlinerData : public FWorkspaceOutlinerItemData
{
	GENERATED_BODY()
	
	FAnimNextGraphAssetOutlinerData() = default;
};

USTRUCT()
struct FAnimNextParameterOutlinerData : public FWorkspaceOutlinerItemData
{
	GENERATED_BODY()
	
	FAnimNextParameterOutlinerData() = default;

	UPROPERTY(VisibleAnywhere, Category=AnimNext)
	FAnimNextParamType Type;
};

USTRUCT()
struct FAnimNextGraphOutlinerData : public FWorkspaceOutlinerItemData
{
	GENERATED_BODY()
	
	FAnimNextGraphOutlinerData() = default;

	UPROPERTY(VisibleAnywhere, Category=AnimNext)
	TScriptInterface<IAnimNextRigVMGraphInterface> GraphInterface;
};

USTRUCT()
struct FAnimNextCollapseGraphOutlinerData : public FWorkspaceOutlinerItemData
{
	GENERATED_BODY()
	
	FAnimNextCollapseGraphOutlinerData() = default;

	UPROPERTY(VisibleAnywhere, Category=AnimNext)
	TWeakObjectPtr<URigVMEdGraph> EditorObject;
};

USTRUCT()
struct FAnimNextGraphFunctionOutlinerData : public FWorkspaceOutlinerItemData
{
	GENERATED_BODY()
	
	FAnimNextGraphFunctionOutlinerData() = default;

	UPROPERTY(VisibleAnywhere, Category=AnimNext)
	TWeakObjectPtr<URigVMEdGraph> EditorObject;
};

USTRUCT()
struct FAnimNextSchedulerData : public FAnimNextGraphAssetOutlinerData
{
	GENERATED_BODY()
	
	FAnimNextSchedulerData() = default;
};

UCLASS()
class UAnimNextModuleWorkspaceAssetUserData : public UAssetUserData
{	
	GENERATED_BODY()

	virtual void GetAssetRegistryTags(FAssetRegistryTagsContext Context) const override;
};
