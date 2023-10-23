// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Param/AnimNextParameterBlockEntry.h"
#include "Param/ParamType.h"
#include "EdGraphSchema_K2.h"

class UAnimNextGraph;
class UAnimNextGraph_EditorData;
struct FAnimNextParamType;
class UAnimNextParameterBlock;
class UAnimNextParameterBlockBinding;
class UAnimNextParameterBlock_EditorData;
class UAnimNextParameter;
class URigVMController;
struct FAnimNextParameterLibraryAssetRegistryExports;
struct FAnimNextParameterBlockAssetRegistryExports;
struct FAnimNextWorkspaceAssetRegistryExports;

namespace UE::AnimNext::Editor
{

struct FUtils
{
	static FName ValidateName(const UAnimNextGraph_EditorData* InEditorData, const FString& InName);

	static void GetAllGraphNames(const UAnimNextGraph_EditorData* InEditorData, TSet<FName>& OutNames);

	static FAnimNextParamType GetParameterTypeFromMetaData(const FStringView& InStringView);

	static FName ValidateName(const UAnimNextParameterBlock_EditorData* InEditorData, const FString& InName);

	static void GetAllEntryNames(const UAnimNextParameterBlock_EditorData* InEditorData, TSet<FName>& OutNames);

	static void GetFilteredVariableTypeTree(TArray<TSharedPtr<UEdGraphSchema_K2::FPinTypeTreeInfo>>& TypeTree, ETypeTreeFilter TypeTreeFilter);

	static FName GetNewParameterNameInLibrary(const FAssetData& InLibraryAsset, const TCHAR* InBaseName, TArrayView<FName> InAdditionalExistingNames);

	static bool DoesParameterExistInLibrary(const FAssetData& InLibraryAsset, const FName InParameterName);

	static FAnimNextParamType GetParameterTypeFromLibraryExports(FName InName, const FAnimNextParameterLibraryAssetRegistryExports& InExports);

	static bool GetExportedBindingsForBlock(const FAssetData& InBlockAsset, FAnimNextParameterBlockAssetRegistryExports& OutExports);


	static bool GetExportedAssetsForWorkspace(const FAssetData& InWorkspaceAsset, FAnimNextWorkspaceAssetRegistryExports& OutExports);
};

}
