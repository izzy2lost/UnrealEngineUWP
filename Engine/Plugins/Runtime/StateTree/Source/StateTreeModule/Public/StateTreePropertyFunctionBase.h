// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "StateTreeNodeBase.h"
#include "StateTreePropertyFunctionBase.generated.h"

struct FStateTreeExecutionContext;

/**
 * Base struct for all property functions.
 * PropertyFunction is a node which is executed just before evaluating owner's bindings.
 * Only Output properties are allowed to be bound to.
 */
USTRUCT(meta = (Hidden))
struct STATETREEMODULE_API FStateTreePropertyFunctionBase : public FStateTreeNodeBase
{
	GENERATED_BODY()

	/**
	 * Called right before evaluating bindings for the owning node.
	 * @param Context Reference to current execution context.
	 */
	virtual void Execute(FStateTreeExecutionContext& Context) const {}

#if WITH_EDITOR
	virtual FName GetIconName() const override
	{
		return FName("StateTreeEditorStyle|Node.Function");
	}

	virtual FColor GetIconColor() const override;
#endif
};

USTRUCT(meta = (Hidden))
struct STATETREEMODULE_API FStateTreePropertyFunctionCommonBase : public FStateTreePropertyFunctionBase
{
	GENERATED_BODY()
};