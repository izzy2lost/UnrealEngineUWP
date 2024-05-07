// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Param/ParamStackLayerHandle.h"

struct FAnimNextScheduleExternalParamTask;
struct FAnimNextScheduleParamScopeEntryTask;
struct FAnimNextScheduleGraphTask;
struct FAnimNextGraphInstancePtr;
struct FAnimNextGraphInstance;

namespace UE::AnimNext
{
	struct FParamContext;
	struct FScheduleInstanceData;
	struct FScheduleBeginTickFunction;
	struct FSubGraphHostTrait;
}

namespace UE::AnimNext
{

/** An interface used to abstract AnimNext parameter sources */
class IParameterSource
{
public:
	virtual ~IParameterSource() = default;

private:
	/**
	 * Get the instance ID of this parameter source. Instance IDs are used to differentiate sources from one another. They do not have to be unique,
	 * but when applying sources to scopes, the instance ID is used to determine if the source should be replaced
	 */
	virtual FName GetInstanceId() const = 0;
	
	/** Update this parameter source */
	virtual void Update(float DeltaTime) = 0;

	/** Get a layer handle to allow this source to be used in the parameter stack */
	virtual const FParamStackLayerHandle& GetLayerHandle() const = 0; 

	/** Allow GC subscription */
	virtual void AddReferencedObjects(FReferenceCollector& Collector) {}

	friend struct FScheduleInstanceData;
	friend struct FScheduleBeginTickFunction;
	friend struct FSubGraphHostTrait;
	friend struct ::FAnimNextScheduleExternalParamTask;
	friend struct ::FAnimNextScheduleParamScopeEntryTask;
	friend struct ::FAnimNextScheduleGraphTask;
	friend struct ::FAnimNextGraphInstancePtr;
	friend struct ::FAnimNextGraphInstance;
};

}