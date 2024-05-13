// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "RigVMDefines.h"
#include "RigVMCore/RigVMStruct.h"

#include "RigVMDecorator.generated.h"

#if WITH_EDITOR

class URigVMController;
class URigVMNode;

#endif

/**
 * The base class for all RigVM decorators.
 */
USTRUCT()
struct RIGVM_API FRigVMDecorator : public FRigVMStruct
{
	GENERATED_BODY()

public:

	FRigVMDecorator()
	{}

	virtual ~FRigVMDecorator() {}

	// returns the name of the decorator (the instance of it on the node)
	FString GetName() const { return Name; }

	// returns the display name of the decorator
	virtual FString GetDisplayName() const
	{
		return FString();
	}

#if WITH_EDITOR

	// returns true if this decorator can be added to a given node
	virtual bool CanBeAddedToNode(URigVMNode* InNode, FString* OutFailureReason) const { return true; }

	// allows the decorator to react when added to a node
	virtual void OnDecoratorAdded(URigVMController* InController, URigVMNode* InNode) {}

	// allows the decorator to return dynamic pins (parent pin index must be INDEX_NONE or point to a valid index of the parent pin in the OutPinArray)
	virtual void GetProgrammaticPins(URigVMController* InController, int32 InParentPinIndex, const FString& InDefaultValue, struct FRigVMPinInfoArray& OutPinArray) const {}

	virtual UScriptStruct* GetDecoratorSharedDataStruct() const { return nullptr; }

	virtual bool ShouldCreatePinForProperty(const FProperty* InProperty) const override
	{
		if(!Super::ShouldCreatePinForProperty(InProperty))
		{
			return false;
		}
		return InProperty->GetFName() != GET_MEMBER_NAME_CHECKED(FRigVMDecorator, Name);
	}

#endif

private:

	// The name of the decorator on the node
	UPROPERTY()
	FString Name;
	
	friend class URigVMNode;
	friend class URigVMController;
	friend class FRigVMDecoratorScope;
};
