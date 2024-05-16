// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "TraitCore/TraitEvent.h"

struct FInstancedPropertyBag;

namespace UE::AnimNext
{
	struct FScheduleContext;
	enum class EParameterScopeOrdering : int32;
	struct FScheduleBeginTickFunction;
	struct FScheduleTickFunction;
}

namespace UE::AnimNext
{

// Context passed to schedule task callbacks
struct FScheduleTaskContext
{
public:
	// Apply the supplied parameter source to the specified scope, evicting any source that was there previously
	ANIMNEXT_API void ApplyParametersToScope(FName InScope, EParameterScopeOrdering InOrdering, FName InInstanceId, FInstancedPropertyBag&& InPropertyBag) const;

	// Queues an input trait event
	// Input events will be processed in the next graph update after they are queued
	ANIMNEXT_API void QueueInputTraitEvent(FAnimNextTraitEventPtr Event) const;

private:
	FScheduleTaskContext(const FScheduleContext& InContext);

	// The context we wrap
	const FScheduleContext& Context;

	friend struct FScheduleBeginTickFunction;
	friend struct FScheduleTickFunction;
};

}