// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Param/ParamType.h"
#include "EdGraphSchema_K2.h"

class UAnimNextModule;
class UAnimNextRigVMAssetEditorData;
struct FAnimNextParamType;
class URigVMController;
struct FAnimNextWorkspaceAssetRegistryExports;

struct FAnimNextParameterProviderAssetRegistryExports;

namespace UE::AnimNext::Editor
{

struct FUtils
{
	static FName ValidateName(const UObject* InObject, const FString& InName);

	static void GetAllEntryNames(const UAnimNextRigVMAssetEditorData* InEditorData, TSet<FName>& OutNames);

	static FAnimNextParamType GetParameterTypeFromMetaData(const FStringView& InStringView);

	static void GetFilteredVariableTypeTree(TArray<TSharedPtr<UEdGraphSchema_K2::FPinTypeTreeInfo>>& TypeTree, ETypeTreeFilter TypeTreeFilter);

	static FName GetNewParameterName(FName InBaseName, const FAssetData& InAssetData, TArrayView<FName> InExistingNames);

	static bool IsValidParameterNameString(FStringView InStringView, FText& OutErrorText);

	static bool IsValidParameterName(const FName InName, FText& OutErrorText);

	static bool DoesParameterNameExistInAsset(const FName InName, const FAssetData& InAsset);
};

}
