// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Param/ParamType.h"
#include "UObject/Interface.h"
#include "IAnimNextParameterBlockParameterInterface.generated.h"

class UAnimNextParameterLibrary;
class UAnimNextParameterBlock_EditorData;

namespace UE::AnimNext
{
	struct FParamDefinition;
}

namespace UE::AnimNext::Editor
{
	class SParameterBlockView;
	class SParameterBlockViewRow;
	struct FUtils;
}

namespace UE::AnimNext::UncookedOnly
{
	struct FUtils;
}

UINTERFACE(meta=(CannotImplementInterfaceInBlueprint))
class ANIMNEXTUNCOOKEDONLY_API UAnimNextParameterBlockParameterInterface : public UInterface
{
	GENERATED_BODY()
};

class ANIMNEXTUNCOOKEDONLY_API IAnimNextParameterBlockParameterInterface
{
	GENERATED_BODY()

	friend class UAnimNextParameterBlock_EditorData;
	friend class UAnimNextParameterBlockBindingReference;
	friend class UE::AnimNext::Editor::SParameterBlockView;
	friend class UE::AnimNext::Editor::SParameterBlockViewRow;
	friend struct UE::AnimNext::UncookedOnly::FUtils;
	friend struct UE::AnimNext::Editor::FUtils;

	// Get the parameter type
	virtual FAnimNextParamType GetParamType() const = 0;

	// Set the parameter type
	virtual bool SetParamType(const FAnimNextParamType& InType, bool bSetupUndoRedo = true) = 0;

	// Get the parameter name
	virtual FName GetParameterName() const = 0;

	// Set the parameter name
	virtual void SetParameterName(FName InName, bool bSetupUndoRedo = true) = 0;
};