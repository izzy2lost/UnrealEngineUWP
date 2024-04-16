// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"
#include "LODPose.h"
#include "ReferencePose.h"

struct FAnimNextGraphInstancePtr;
struct FAnimNextParamInstanceIdentifier;

namespace UE::AnimNext
{
	class IParameterSource;
	class IParameterSourceFactory;
	struct FParameterSourceContext;
}

namespace UE::AnimNext
{

class IAnimNextAnimGraph
{
public:
	virtual ~IAnimNextAnimGraph() = default;

	virtual void UpdateGraph(FAnimNextGraphInstancePtr& GraphInstance, float DeltaTime) const = 0;
	virtual void EvaluateGraph(FAnimNextGraphInstancePtr& GraphInstance, const UE::AnimNext::FReferencePose& RefPose, int32 GraphLODLevel, FLODPoseHeap& OutputPose) const = 0;
};

class IAnimNextModule : public IModuleInterface
{
public:
	// Factory method used to create a parameter source to access the specified scope locator, with a set of parameters that are initially required
	// @param   InContext             Context used to set up the parameter source
	// @param   InInstanceId          The instance identifier associated with the parameters that are required
	// @param   InRequiredParameters  Any required parameters that the source should initially supply, can be empty
	// @return a new parameter source, or nullptr if the scope was not found or could not be handled
	virtual TUniquePtr<IParameterSource> CreateParameterSource(const FParameterSourceContext& InContext, const TInstancedStruct<FAnimNextParamInstanceIdentifier>& InInstanceId, TConstArrayView<FName> InRequiredParameters) const = 0;

	// Register a factory that can be used to generate parameter sources
	// @param   InName                Identifier for the factory
	// @param   InFactory             The factory to register
	virtual void RegisterParameterSourceFactory(FName InName, TSharedRef<IParameterSourceFactory> InFactory) = 0;

	// Unregister a factory previously passed to RegisterParameterSourceFactory
	// @param   InName                Identifier for the factory
	virtual void UnregisterParameterSourceFactory(FName InName) = 0;

	// Find a factory previously passed to RegisterParameterSourceFactory
	// @param   InName                Identifier for the factory
	// @return a shared ptr to the factory, if found, otherwise an invalid shared ptr
	virtual TSharedPtr<IParameterSourceFactory> FindParameterSourceFactory(FName InName) = 0;

	// WARNING
	// These functions are here because the schedule currently cannot have tasks defined in
	// external plugins. And as such, we must be able to update and evaluate graphs from
	// AnimNextCore even though they are implementation details of the AnimNextAnimGraph.
	// We thus introduce this interface.
	// When AnimNextAnimGraph's module will load, it will register callbacks that
	// AnimNextCore will call when we need to update/evaluate graphs.
	static ANIMNEXT_API IAnimNextModule& Get();
	virtual void RegisterAnimNextAnimGraph(const IAnimNextAnimGraph& InAnimGraphImpl) = 0;
	virtual void UnregisterAnimNextAnimGraph() = 0;
	virtual void UpdateGraph(FAnimNextGraphInstancePtr& GraphInstance, float DeltaTime) = 0;
	virtual void EvaluateGraph(FAnimNextGraphInstancePtr& GraphInstance, const FReferencePose& RefPose, int32 GraphLODLevel, FLODPoseHeap& OutputPose) const = 0;
};

}
