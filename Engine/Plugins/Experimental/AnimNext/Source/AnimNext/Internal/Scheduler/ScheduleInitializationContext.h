// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

struct FAnimNextSchedulerEntry;
struct FPropertyBagPropertyDesc;
template<typename StructType> struct TInstancedStruct;
struct FAnimNextParamInstanceIdentifier;

namespace UE::AnimNext
{
struct FParameterSourceContext;
struct FScheduleContext;
}

namespace UE::AnimNext
{

enum class EParameterScopeOrdering : int32
{
	// Value will be pushed before the scope, allowing the static scope to potentially override the value
	Before,

	// Value will be pushed after the scope, potentially overriding the static scope
	After,
};

// Context passed to schedule initialization callbacks
struct FScheduleInitializationContext
{
public:
	// Apply the supplied parameter source to the specified scope, evicting any source that was there previously
	ANIMNEXT_API void ApplyParametersToScope(FName InScope, EParameterScopeOrdering InOrdering, const TInstancedStruct<FAnimNextParamInstanceIdentifier>& InInstanceId, const FParameterSourceContext& InContext, TConstArrayView<FName> InRequiredParameters = TConstArrayView<FName>()) const;
	ANIMNEXT_API void ApplyParametersToScope(FName InScope, EParameterScopeOrdering InOrdering, FName InId, TConstArrayView<FPropertyBagPropertyDesc> InPropertyDescs, TConstArrayView<TConstArrayView<uint8>> InValues) const;

private:
	FScheduleInitializationContext(const FScheduleContext& InContext);

	// The context we wrap
	const FScheduleContext& Context;

	friend struct ::FAnimNextSchedulerEntry;
};

}