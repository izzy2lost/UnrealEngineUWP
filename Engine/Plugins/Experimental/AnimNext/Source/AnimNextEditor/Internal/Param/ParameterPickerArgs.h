// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UniversalObjectLocator.h"
#include "UObject/NameTypes.h"
#include "Delegates/DelegateCombinations.h"
#include "AssetRegistry/AssetData.h"
#include "Param/AnimNextParam.h"
#include "Param/ParamTypeHandle.h"

struct FAnimNextParamInstanceIdentifier;
class UAnimNextRigVMAsset;

namespace UE::AnimNext::Editor
{

struct FParameterToAdd
{
	FParameterToAdd() = default;

	FParameterToAdd(const FAnimNextParamType& InType, FName InName)
		: Type(InType)
		, Name(InName)
	{}

	bool IsValid() const
	{
		return Name != NAME_None && Type.IsValid(); 
	}

	bool IsValid(FText& OutReason) const;

	// Type
	FAnimNextParamType Type;

	// Name for parameter
	FName Name;
};

// A parameter asset, optionally bound in a graph
struct FParameterBindingReference
{
	FParameterBindingReference() = default;

	FParameterBindingReference(FName InParameter, const FAnimNextParamType& InType, const TInstancedStruct<FAnimNextParamInstanceIdentifier>& InInstanceId,  const FAssetData& InGraph = FAssetData())
		: Parameter(InParameter)
		, Type(InType)
		, InstanceId(InInstanceId)
		, Graph(InGraph)
	{
	}

	// Parameter name
	FName Parameter;

	// Parameter type
	FAnimNextParamType Type;

	// Instance Id used to disambiguate the parameter
	TInstancedStruct<FAnimNextParamInstanceIdentifier> InstanceId;

	// Optional graph asset that the parameter is bound in
	FAssetData Graph;
};

// Delegate called when a parameter has been picked. Graph argument is invalid when an unbound parameter is chosen.
using FOnGetParameterBindings = TDelegate<void(TArray<FParameterBindingReference>& /*OutParameterBindings*/)>;

// Delegate called when a parameter has been picked. Graph argument is invalid when an unbound parameter is chosen.
using FOnParameterPicked = TDelegate<void(const FParameterBindingReference& /*InParameterBinding*/)>;

// Delegate called when a parameter is due to be added.
using FOnAddParameter = TDelegate<void(const FParameterToAdd& /*InParameterToAdd*/)>;

// Result of a filter operation via FOnFilterParameter
enum class EFilterParameterResult : int32
{
	Include,
	Exclude
};

// Delegate called to filter parameters for display to the user
using FOnFilterParameter = TDelegate<EFilterParameterResult(const FParameterBindingReference& /*InParameterBinding*/)>;

// Delegate called to filter parameters by type for display to the user
using FOnFilterParameterType = TDelegate<EFilterParameterResult(const FAnimNextParamType& /*InParameterType*/)>;

// Delegate called to filter parameters by type for display to the user
using FOnFilterParameterType = TDelegate<EFilterParameterResult(const FAnimNextParamType& /*InParameterType*/)>;

// Delegate called when the selected instance ID changes 
using FOnInstanceIdChanged = TDelegate<void(const TInstancedStruct<FAnimNextParamInstanceIdentifier>& /*InInstanceId*/)>;

struct FParameterPickerArgs
{
	FParameterPickerArgs() = default;

	// Ptr to existing called delegate to which the picker will register a function which returns the selected parameter bindings
	FOnGetParameterBindings* OnGetParameterBindings = nullptr;

	// Delegate used to signal whether selection has changed
	FSimpleDelegate OnSelectionChanged;

	// Delegate called when a single parameter has been picked
	FOnParameterPicked OnParameterPicked;

	// Delegate called when a parameter, or set of parameters is added
	FOnAddParameter OnAddParameter;

	// Delegate called to filter parameters for display to the user
	FOnFilterParameter OnFilterParameter;

	// Delegate called to filter parameters by type for display to the user
	FOnFilterParameterType OnFilterParameterType;

	// Type to use for any new parameters generated through the picker
	FAnimNextParamType NewParameterType;

	// Ptr to existing called delegate to which the picker will register a function which returns the selected instance ID
	FOnInstanceIdChanged* OnSetInstanceId = nullptr;

	// Delegate called when the selected instance ID changes in the picker
	FOnInstanceIdChanged OnInstanceIdChanged;

	// The initial instance ID to use
	TInstancedStruct<FAnimNextParamInstanceIdentifier> InstanceId;

	// The context to use when resolving instance IDs
	const UObject* Context = nullptr;

	// Whether we allow selecting multiple parameters or just one
	bool bMultiSelect = true;

	// Whether the search box should be focussed on widget creation
	bool bFocusSearchWidget = true;

	// Whether the instance ID chooser is displayed
	bool bShowInstanceId = true;
};

}
