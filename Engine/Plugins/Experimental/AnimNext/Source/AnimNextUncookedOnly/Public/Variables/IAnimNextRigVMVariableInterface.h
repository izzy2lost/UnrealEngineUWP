// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/Interface.h"
#include "StructUtils/PropertyBag.h"
#include "IAnimNextRigVMVariableInterface.generated.h"

struct FAnimNextParamType;
struct FAnimNextVariableBinding;

UINTERFACE(meta=(CannotImplementInterfaceInBlueprint))
class ANIMNEXTUNCOOKEDONLY_API UAnimNextRigVMVariableInterface : public UInterface
{
	GENERATED_BODY()
};

class ANIMNEXTUNCOOKEDONLY_API IAnimNextRigVMVariableInterface
{
	GENERATED_BODY()

public:
	// Get the variable type
	virtual FAnimNextParamType GetType() const = 0;

	// Set the variable type
	virtual bool SetType(const FAnimNextParamType& InType, bool bSetupUndoRedo = true) = 0;

	// Get the variable name
	virtual FName GetVariableName() const = 0;

	// Set the variable name
	virtual void SetVariableName(FName InName, bool bSetupUndoRedo = true) = 0;

	// Set the default value from a string
	virtual bool SetDefaultValue(const FString& InDefaultValue, bool bSetupUndoRedo = true) = 0;

	// Access the backing storage property bag for the parameter
	virtual const FInstancedPropertyBag& GetPropertyBag() const = 0;

	// Access the mutable backing storage property bag for the parameter
	FInstancedPropertyBag& GetMutablePropertyBag()
	{
		return const_cast<FInstancedPropertyBag&>(GetPropertyBag());
	}

	// Get the binding for this variable, if any
	virtual const FAnimNextVariableBinding& GetBinding() const = 0;

	// Access the memory for the internal value
	const uint8* GetValuePtr() const
	{
		const FInstancedPropertyBag& PropertyBag = GetPropertyBag();
		const UPropertyBag* PropertyBagStruct = PropertyBag.GetPropertyBagStruct();
		const uint8* Memory = PropertyBag.GetValue().GetMemory();
		return PropertyBagStruct && Memory ? PropertyBagStruct->GetPropertyDescs()[0].CachedProperty->ContainerPtrToValuePtr<uint8>(Memory) : nullptr;
	}

	// The name of the value property in the internal property bag
	static const FLazyName ValueName;
};