// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IAnimNextModuleInterface.h"

struct FAnimNextGraphInstancePtr;
struct FAnimNextParamInstanceIdentifier;

namespace UE::AnimNext
{
	class IParameterSource;
	class IParameterSourceFactory;
	struct FParameterSourceContext;
}

// Enable console commands only in development builds when logging is enabled
#define WITH_ANIMNEXT_CONSOLE_COMMANDS (!UE_BUILD_SHIPPING && !NO_LOGGING)

namespace UE::AnimNext
{

class FAnimNextModuleImpl : public IAnimNextModuleInterface
{
public:
	// IModuleInterface interface
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	// IAnimNextModuleInterface interface
	virtual void RegisterAnimNextAnimGraph(const IAnimNextAnimGraph& InAnimGraphImpl) override;
	virtual void UnregisterAnimNextAnimGraph() override;
	virtual void UpdateGraph(FAnimNextGraphInstancePtr& GraphInstance, float DeltaTime, FTraitEventList& InputEventList, FTraitEventList& OutputEventList) override;
	virtual void EvaluateGraph(FAnimNextGraphInstancePtr& GraphInstance, const FReferencePose& RefPose, int32 GraphLODLevel, FLODPoseHeap& OutputPose) const override;

	// Factory method used to create a parameter source to access the specified instance ID, with a set of parameters that are initially required
	// @param   InContext             Context used to set up the parameter source
	// @param   InInstanceId          The instance identifier associated with the parameters that are required
	// @param   InRequiredParameters  Any required parameters that the source should initially supply, can be empty, in which ase all parameters are created
	// @return a new parameter source, or nullptr if the instance ID could not be handled
	ANIMNEXT_API TUniquePtr<IParameterSource> CreateParameterSource(const FParameterSourceContext& InContext, const TInstancedStruct<FAnimNextParamInstanceIdentifier>& InInstanceId, TConstArrayView<FName> InRequiredParameters = TConstArrayView<FName>()) const;

	// Register a factory that can be used to generate parameter sources
	// @param   InName                Identifier for the factory
	// @param   InFactory             The factory to register
	ANIMNEXT_API void RegisterParameterSourceFactory(FName InName, TSharedRef<IParameterSourceFactory> InFactory);

	// Unregister a factory previously passed to RegisterParameterSourceFactory
	// @param   InName                Identifier for the factory
	ANIMNEXT_API void UnregisterParameterSourceFactory(FName InName);

	// Find a factory previously passed to RegisterParameterSourceFactory
	// @param   InName                Identifier for the factory
	// @return a shared ptr to the factory, if found, otherwise an invalid shared ptr
	ANIMNEXT_API TSharedPtr<IParameterSourceFactory> FindParameterSourceFactory(FName InName);

#if WITH_ANIMNEXT_CONSOLE_COMMANDS
	void ListNodeTemplates(const TArray<FString>& Args);
	void ListModules(const TArray<FString>& Args);

	TArray<IConsoleObject*> ConsoleCommands;
#endif

	// All known factories
	TMap<FName, TSharedRef<IParameterSourceFactory>> ParameterSourceFactories;
};

}
