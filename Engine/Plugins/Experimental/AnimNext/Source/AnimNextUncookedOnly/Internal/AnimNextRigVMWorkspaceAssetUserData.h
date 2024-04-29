// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "WorkspaceAssetRegistryInfo.h"
#include "Param/ParamType.h"
#include "IAnimNextRigVMGraphInterface.h"

#include "AnimNextRigVMWorkspaceAssetUserData.generated.h"

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
struct FAnimNextSchedulerData : public FAnimNextGraphAssetOutlinerData
{
	GENERATED_BODY()
	
	FAnimNextSchedulerData() = default;
};

UCLASS()
class UAnimNextGraphWorkspaceAssetUserData : public UAssetUserData
{	
	GENERATED_BODY()

	virtual void GetAssetRegistryTags(FAssetRegistryTagsContext Context) const override;
};