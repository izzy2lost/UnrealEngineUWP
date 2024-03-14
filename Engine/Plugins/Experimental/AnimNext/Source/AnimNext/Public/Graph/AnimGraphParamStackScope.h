// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Templates/SharedPointer.h"
#include "UObject/WeakObjectPtr.h"

struct FAnimationBaseContext;
struct FAnimNodeExposedValueHandler_AnimNextParameters;

namespace UE::AnimNext
{
	struct FParamStack;
}

namespace UE::AnimNext
{

// Utility for managing parameter stack access in the anim graph
// Pulls from stacks that are marked as 'pending' against specific objects (in this case the component).
// Instantiating this will attach the param stack to the current thread within its scope
struct ANIMNEXT_API FAnimGraphParamStackScope
{
	UE_NODISCARD_CTOR explicit FAnimGraphParamStackScope(const FAnimationBaseContext& InContext);
	~FAnimGraphParamStackScope();

private:
	friend struct ::FAnimNodeExposedValueHandler_AnimNextParameters;

	TWeakObjectPtr<const UObject> ComponentObject;
	TSharedPtr<FParamStack> OwnedParamStack;

	// Whether we attached to a pending object
	bool bAttachedPending = false;
};

}