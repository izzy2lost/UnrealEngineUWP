// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Param/ParamType.h"

struct FAnimNextParamInstanceIdentifier;
template<typename T> struct TInstancedStruct;

namespace UE::AnimNext::UncookedOnly
{
	class FModule;
	struct FUtils;
}

namespace UE::AnimNext::Editor
{
	class SParameterPicker;
}


namespace UE::AnimNext::UncookedOnly
{

// Info about parameters gleaned from FindParameterInfo and ForEachParameter
struct FParameterSourceInfo
{
	// The parameter's type
	FAnimNextParamType Type;
	// Display name for editor
	FText DisplayName;
	// Tooltip to display in editor
	FText Tooltip;
	// Function used to access this parameter, if any
	const UFunction* Function = nullptr;
	// Property for this parameter, if any
	const FProperty* Property = nullptr;
	// Whether this parameter is safe to be accessed on worker threads
	bool bThreadSafe = false;
};

// Interface used in editor/uncooked situations to determine the characteristics of a parameter source.
// @see IParameterSource, IParameterSourceFactory
class IParameterSourceType
{
public:
	virtual ~IParameterSourceType() = default;

private:
	// Get the struct type that this FAnimNextParamInstanceIdentifier resolves to
	virtual const UStruct* GetStruct(const TInstancedStruct<FAnimNextParamInstanceIdentifier>& InInstanceId) const = 0;

	// Get the display text for the specified instance ID
	virtual FText GetDisplayText(const TInstancedStruct<FAnimNextParamInstanceIdentifier>& InInstanceId) const = 0;

	// Get the tooltip text for the specified instance ID
	virtual FText GetTooltipText(const TInstancedStruct<FAnimNextParamInstanceIdentifier>& InInstanceId) const = 0;

	// Given a parameter name and class, find associated info for that parameter
	// @param   InInstanceId           Instance ID used to find the parameter
	// @param   InParameterNames       The parameter names to find. Size must match OutInfo.
	// @param   OutInfo                The parameter info found. Size must match InParameterNames.
	// @return true if any info was found, false if no info was found
	virtual bool FindParameterInfo(const TInstancedStruct<FAnimNextParamInstanceIdentifier>& InInstanceId, TConstArrayView<FName> InParameterNames, TArrayView<FParameterSourceInfo> OutInfo) const = 0;

	// Iterate over all the known parameters for a particular class
	// @param   InInstanceId           Instance ID used to find the parameters
	// @param   InFunction             Function called for each known parameter
	virtual void ForEachParameter(const TInstancedStruct<FAnimNextParamInstanceIdentifier>& InInstanceId, TFunctionRef<void(FName, const FParameterSourceInfo&)> InFunction) const = 0;

	friend class FModule;
	friend struct FUtils;
	friend class Editor::SParameterPicker;
};

}