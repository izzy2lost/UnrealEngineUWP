// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Param/AnimNextParameterBlock_EditorData.h"
#include "Graph/AnimNextGraph_EditorData.h"
#include "Param/ParamTypeHandle.h"
#include "RigVMCore/RigVMTemplate.h"

#include "UncookedOnlyUtils.generated.h"

class UAnimNextSchedule;
class UAnimNextGraph;
class UAnimNextGraph_EditorData;
class UAnimNextGraph_EdGraph;
class URigVMController;
class URigVMGraph;
class UAnimNextParameterBlock;
class UAnimNextParameterBlock_EditorData;
class UAnimNextGraph_EdGraph;
struct FEdGraphPinType;

namespace UE
{
	namespace AnimNext
	{
		static const FName ExportsAnimNextAssetRegistryTag = TEXT("AnimNextExports");
	}
}

UENUM()
enum class EAnimNextParameterFlags
{
	NoFlags = 0x0,
	Private = 0x1,
	Read = 0x02,
	Write = 0x04,
	Bound = 0x08,
	Max
};

ENUM_CLASS_FLAGS(EAnimNextParameterFlags)

USTRUCT()
struct FAnimNextParameterAssetRegistryExportEntry
{
	GENERATED_BODY()

	FAnimNextParameterAssetRegistryExportEntry() = default;
	
	FAnimNextParameterAssetRegistryExportEntry(FName InName, const FAnimNextParamType& InType, EAnimNextParameterFlags InFlags = EAnimNextParameterFlags::NoFlags)
		: Name(InName)
		, Type(InType)
		, Flags(InFlags) 
	{}
	
	UPROPERTY()
	FName Name;

	UPROPERTY()
	FAnimNextParamType Type;

	// Asset, found first in asset-registry, that references this parameter entry
	FAssetData ReferencingAsset;

	UPROPERTY()
	EAnimNextParameterFlags Flags;
};

USTRUCT()
struct FAnimNextParameterProviderAssetRegistryExports
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FAnimNextParameterAssetRegistryExportEntry> Parameters;
};

namespace UE::AnimNext::UncookedOnly
{

struct ANIMNEXTUNCOOKEDONLY_API FUtils
{
	static void Compile(UAnimNextGraph* InGraph);
	
	static UAnimNextGraph_EditorData* GetEditorData(const UAnimNextGraph* InAnimNextGraph);
	
	static UAnimNextGraph* GetGraph(const UAnimNextGraph_EditorData* InEditorData);
	
	static void RecreateVM(UAnimNextGraph* InGraph);

	/**
	 * Get an AnimNext parameter type handle from an FEdGraphPinType.
	 * Note that the returned handle may not be valid, so should be checked using IsValid() before use.
	 **/
	static FParamTypeHandle GetParameterHandleFromPin(const FEdGraphPinType& InPinType);

	static void Compile(UAnimNextParameterBlock* InParameterBlock);

	static void CompileVM(UAnimNextParameterBlock* InParameterBlock);

	static void CompileStruct(UAnimNextParameterBlock* InParameterBlock);
	
	static UAnimNextParameterBlock_EditorData* GetEditorData(const UAnimNextParameterBlock* InParameterBlock);

	static UAnimNextParameterBlock* GetBlock(const UAnimNextParameterBlock_EditorData* InEditorData);

	static FInstancedPropertyBag* GetPropertyBag(UAnimNextParameterBlock* ReferencedBlock);

	static void RecreateVM(UAnimNextParameterBlock* InParameterBlock);

	/**
	 * Get an AnimNext parameter type from an FEdGraphPinType.
	 * Note that the returned handle may not be valid, so should be checked using IsValid() before use.
	 **/
	static FParamTypeHandle GetParamTypeHandleFromPinType(const FEdGraphPinType& InPinType);
	static FAnimNextParamType GetParamTypeFromPinType(const FEdGraphPinType& InPinType);

	/**
	 * Get an FEdGraphPinType from an AnimNext parameter type/handle.
	 * Note that the returned pin type may not be valid.
	 **/
	static FEdGraphPinType GetPinTypeFromParamTypeHandle(const FParamTypeHandle& InParamTypeHandle);
	static FEdGraphPinType GetPinTypeFromParamType(const FAnimNextParamType& InParamType);

	/**
	 * Get an FRigVMTemplateArgumentType from an AnimNext parameter type/handle.
	 * Note that the returned pin type may not be valid.
	 **/
	static FRigVMTemplateArgumentType GetRigVMArgTypeFromParamTypeHandle(const FParamTypeHandle& InParamTypeHandle);
	static FRigVMTemplateArgumentType GetRigVMArgTypeFromParamType(const FAnimNextParamType& InParamType);

	/** Set up a simple animation graph */
	static void SetupAnimGraph(URigVMController* InController);
	
	/** Set up a simple parameter graph */
	static void SetupParameterGraph(URigVMController* InController);
	
	/** Set up a binding graph given the type to set */
	static void SetupBindingGraph(URigVMController* InController, FName InParameterName, const FAnimNextParamType& InParamType);

	/** Set up a binding graph for a simple literal value given the type to set */
	static void SetupBindingGraphForLiteral(URigVMController* InController, FName InParameterName, const FAnimNextParamType& InParamType);

	/** Converts the Verse-tag-like snake_case_parameter_name to a period-separated display name similar to a gameplay tag */
	static FText GetParameterDisplayNameText(FName InParameterName);

	/** Returns all nodes in all graphs of the specified class */
	template<class T>
	static void GetAllNodesOfClass(const UAnimNextParameterBlock_EditorData* InEditorData, TArray<T*>& OutNodes)
	{
		for(const UAnimNextParameterBlock_EdGraph* Graph : InEditorData->Graphs)
		{
			check(Graph);
			TArray<T*> GraphNodes;
			Graph->GetNodesOfClass<T>(GraphNodes);
			OutNodes.Append(GraphNodes);
		}
	}

	/** Returns all nodes in all graphs of the specified class */
	template<class T>
	static void GetAllNodesOfClass(const UAnimNextGraph_EditorData* InEditorData, TArray<T*>& OutNodes)
	{
		for(const UAnimNextGraph_EdGraph* Graph : InEditorData->Graphs)
		{
			check(Graph);
			TArray<T*> GraphNodes;
			Graph->GetNodesOfClass<T>(GraphNodes);
			OutNodes.Append(GraphNodes);
		}
	}
	
	static bool GetExportedParametersForAsset(const FAssetData& InAsset, FAnimNextParameterProviderAssetRegistryExports& OutExports);
	static bool GetExportedParametersFromAssetRegistry(FAnimNextParameterProviderAssetRegistryExports& OutExports);

	static void GetGraphParameters(const URigVMGraph* Graph,FAnimNextParameterProviderAssetRegistryExports& OutExports);
	
	// Attempts to determine the type from a parameter name
	// If the name cannot be found, the returned type will be invalid
	// Note that this is expensive and can query the asset registry
	static FAnimNextParamType GetParameterTypeFromName(FName InName);

	// Compiles a schedule
	static void CompileSchedule(UAnimNextSchedule* InSchedule);
};

}