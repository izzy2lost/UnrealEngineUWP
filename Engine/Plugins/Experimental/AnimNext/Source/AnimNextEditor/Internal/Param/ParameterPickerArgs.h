// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/NameTypes.h"
#include "Delegates/DelegateCombinations.h"
#include "AssetRegistry/AssetData.h"
#include "Param/ParamTypeHandle.h"

namespace UE::AnimNext::Editor
{

// A parameter asset, optionally bound in a block
struct FParameterBindingReference
{
	FParameterBindingReference() = default;

	FParameterBindingReference(const FName& InParameter, const FAssetData& InLibrary, const FAssetData& InBlock = FAssetData())
		: Parameter(InParameter)
		, Library(InLibrary)
		, Block(InBlock)
	{
	}

	// Parameter name
	FName Parameter;

	// Library asset
	FAssetData Library;

	// Optional block asset that the parameter is bound in
	FAssetData Block;
};

// Delegate called when a parameter has been picked. Block argument is invalid when an unbound parameter is chosen.
DECLARE_DELEGATE_OneParam(FOnGetParameterBindings, TArray<FParameterBindingReference>& /*OutParameterBindings*/);

// Delegate called when a parameter has been picked. Block argument is invalid when an unbound parameter is chosen.
DECLARE_DELEGATE_OneParam(FOnParameterPicked, const FParameterBindingReference& /*OutParameterBinding*/);

// Result of a filter operation via FOnFilterParameter
enum class EFilterParameterResult : int32
{
	Include,
	Exclude
};

// Delegate called to filter parameters for display to the user
DECLARE_DELEGATE_RetVal_OneParam(EFilterParameterResult, FOnFilterParameter, const FParameterBindingReference& /*InParameterBinding*/);

// Delegate called to filter parameters by type for display to the user
DECLARE_DELEGATE_RetVal_OneParam(EFilterParameterResult, FOnFilterParameterType, const FAnimNextParamType& /*InParameterType*/);

struct FParameterPickerArgs
{
	FParameterPickerArgs() = default;

	// Ptr to existing called delegate to which the picker will register a function which returns the selected parameter
	// bindings
	FOnGetParameterBindings* OnGetParameterBindings = nullptr;

	// Delegate used to signal whether selection has changed
	FSimpleDelegate OnSelectionChanged;

	// Delegate called when a single parameter has been picked
	FOnParameterPicked OnParameterPicked;

	// Delegate called to filter parameters for display to the user
	FOnFilterParameter OnFilterParameter;

	// Delegate called to filter parameters by type for display to the user
	FOnFilterParameterType OnFilterParameterType;

	// Whether we allow selecting multiple parameters or just one
	bool bMultiSelect = true;

	// Whether we should show parameters that are bound in a parameter block
	bool bShowBoundParameters = true;

	// Whether we should show parameters that are not bound in a parameter block (if bShowBoundParameters is false this will show all parameters)
	bool bShowUnboundParameters = true;	

	// Whether we should show the library alongside parameters
	bool bShowLibraries = true;

	// Whether we should show the block alongside bound parameters
	bool bShowBlocks = true;
};

}
