// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IAnimNextRigVMGraphInterface.h"
#include "AssetRegistry/AssetData.h"
#include "Param/AnimNextParamInstanceIdentifier.h"
#include "Param/ParamTypeHandle.h"
#include "RigVMCore/RigVMTemplate.h"
#include "InstancedStruct.h"
#include "UncookedOnlyUtils.generated.h"

struct FAnimNextEditorParam;
struct FAnimNextParam;
class UAnimNextSchedule;
class UAnimNextGraph;
class UAnimNextGraph_EditorData;
class UAnimNextGraph_EdGraph;
class URigVMController;
class URigVMGraph;
class UAnimNextGraph_EdGraph;
struct FEdGraphPinType;
class UAnimNextRigVMAsset;
class UAnimNextRigVMAssetEditorData;
class UAnimNextRigVMAssetEntry;

namespace UE
{
	namespace AnimNext
	{
		static const FName ExportsAnimNextAssetRegistryTag = TEXT("AnimNextExports");
	}
}

UENUM()
enum class EAnimNextParameterFlags : uint32
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
	
	FAnimNextParameterAssetRegistryExportEntry(FName InName, const TInstancedStruct<FAnimNextParamInstanceIdentifier>& InInstanceId, const FAnimNextParamType& InType, EAnimNextParameterFlags InFlags = EAnimNextParameterFlags::NoFlags)
		: Name(InName)
		, InstanceId(InInstanceId)
		, Type(InType)
		, Flags((uint32)InFlags) 
	{}

	bool operator==(const FAnimNextParameterAssetRegistryExportEntry& Other) const
	{
		return Name == Other.Name && InstanceId == Other.InstanceId;
	}

	friend uint32 GetTypeHash(const FAnimNextParameterAssetRegistryExportEntry& Entry)
	{
		const FName InstanceIdName = Entry.InstanceId.IsValid() ? Entry.InstanceId.Get().ToName() : NAME_None;
		return HashCombineFast(GetTypeHash(Entry.Name), GetTypeHash(InstanceIdName));
	}

	EAnimNextParameterFlags GetFlags() const
	{
		return (EAnimNextParameterFlags)Flags;
	}
	
	UPROPERTY()
	FName Name;

	UPROPERTY()
	TInstancedStruct<FAnimNextParamInstanceIdentifier> InstanceId;

	UPROPERTY()
	FAnimNextParamType Type;

	UPROPERTY()
	uint32 Flags = (int32)EAnimNextParameterFlags::NoFlags;
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

	static void CompileVM(UAnimNextGraph* InGraph);

	static void CompileStruct(UAnimNextGraph* InGraph);

	static UAnimNextGraph_EditorData* GetEditorData(const UAnimNextGraph* InAnimNextGraph);

	static UAnimNextGraph* GetGraph(const UAnimNextGraph_EditorData* InEditorData);

	static FInstancedPropertyBag* GetPropertyBag(UAnimNextGraph* InAnimNextGraph);

	static void RecreateVM(UAnimNextGraph* InGraph);

	/**
	 * Get an AnimNext parameter type handle from an FEdGraphPinType.
	 * Note that the returned handle may not be valid, so should be checked using IsValid() before use.
	 **/
	static FParamTypeHandle GetParameterHandleFromPin(const FEdGraphPinType& InPinType);

	static UAnimNextRigVMAsset* GetAsset(UAnimNextRigVMAssetEditorData* InEditorData);

	static UAnimNextRigVMAssetEditorData* GetEditorData(UAnimNextRigVMAsset* InAsset);

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
	static void SetupAnimGraph(UAnimNextRigVMAssetEntry* InEntry, URigVMController* InController);
	
	/** Set up a simple event graph */
	static void SetupEventGraph(URigVMController* InController);

	/** Get a parameter name (e.g. MyParameter) from a fully-qualified parameter name (e.g. /Game/MyAsset.MyAsset:MyParameter) */
	static FName GetParameterNameFromQualifiedName(FName InName);

	/** Get a fully-qualified parameter name (e.g. /Game/MyAsset.MyAsset:MyParameter) from its containing asset and base name (e.g. MyParameter) */
	static FName GetQualifiedName(UAnimNextRigVMAsset* InAsset, FName InBaseName);

	/** Gets a name to display for a parameter in the editor, including scope if external */
	static FText GetParameterDisplayNameText(FName InParameterName, const TInstancedStruct<FAnimNextParamInstanceIdentifier>& InInstanceId);

	/** Gets a name to display for a parameter's tooltip in the editor, including scope if external */
	static FText GetParameterTooltipText(FName InParameterName, const TInstancedStruct<FAnimNextParamInstanceIdentifier>& InInstanceId);

	// Gets the parameters that are exported to the asset registry for an asset
	static bool GetExportedParametersForAsset(const FAssetData& InAsset, FAnimNextParameterProviderAssetRegistryExports& OutExports);

	// Gets all the parameters that are exported to the asset registry
	static bool GetExportedParametersFromAssetRegistry(TMap<FAssetData, FAnimNextParameterProviderAssetRegistryExports>& OutExports);

	// Gets the exported parameters that are used by a RigVM asset
	static void GetAssetParameters(const UAnimNextRigVMAssetEditorData* EditorData, FAnimNextParameterProviderAssetRegistryExports& OutExports);
	static void GetAssetParameters(const UAnimNextRigVMAssetEditorData* EditorData, TSet<FAnimNextParameterAssetRegistryExportEntry>& OutExports);

	// Gets the exported parameters that are used by a RigVM graph
	static void GetGraphParameters(const URigVMGraph* Graph, FAnimNextParameterProviderAssetRegistryExports& OutExports);
	static void GetGraphParameters(const URigVMGraph* Graph, TSet<FAnimNextParameterAssetRegistryExportEntry>& OutExports);

	// Gets the parameters that are exported to the asset registry by a schedule
	static void GetScheduleParameters(const UAnimNextSchedule* InSchedule, FAnimNextParameterProviderAssetRegistryExports& OutExports);
	static void GetScheduleParameters(const UAnimNextSchedule* InSchedule, TSet<FAnimNextParameterAssetRegistryExportEntry>& OutExports);

	// Gets the parameters that are exported to the asset registry by a blueprint
	static void GetBlueprintParameters(const UBlueprint* InBlueprint, FAnimNextParameterProviderAssetRegistryExports& OutExports);
	static void GetBlueprintParameters(const UBlueprint* InBlueprint, TSet<FAnimNextParameterAssetRegistryExportEntry>& OutExports);

	// Attempts to determine the type from a parameter name
	// If the name cannot be found, the returned type will be invalid
	// Note that this is expensive and can query the asset registry
	static FAnimNextParamType GetParameterTypeFromName(FName InName);

	// Compiles a schedule
	static void CompileSchedule(UAnimNextSchedule* InSchedule);

	// Sorts the incoming array of parameters, then generates a hash and returns it.
	static uint64 SortAndHashParameters(TArray<FAnimNextParam>& InParameters);
};

}