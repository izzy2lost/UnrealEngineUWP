// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Param/ParamType.h"
#include "EdGraphSchema_K2.h"

class UAnimNextGraph;
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

	static FName GetNewParameterName(const TCHAR* InBaseName, TArrayView<FName> InAdditionalExistingNames);

	static bool IsValidEntryNameString(FStringView InStringView, FText& OutErrorText);

	static bool IsValidEntryName(const FName InName, FText& OutErrorText);

	static bool DoesParameterNameExist(const FName InName);
	
	static bool DoesParameterNameExistInAsset(const FName InName, const FAssetData& InAsset);
};

}
