// Copyright Epic Games, Inc. All Rights Reserved.
#include "PPMChainGraphWorldSubsystem.h"
#include "PPMChainGraphSceneViewExtension.h"
#include "PPMChainGraphComponent.h"

#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "EngineUtils.h"
#include "SceneViewExtension.h"


#if WITH_EDITOR
#include "Editor.h"
#endif

#define LOCTEXT_NAMESPACE "PPMChainGraph"


namespace
{
	bool IsComponentValid(UPPMChainGraphExecutorComponent* InPPMChainComponent, UWorld* CurrentWorld)
	{
		return InPPMChainComponent && InPPMChainComponent->GetWorld() == CurrentWorld;
	}

}

void UPPMChainGraphWorldSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	SceneViewExtension = FSceneViewExtensions::NewExtension<FPPMChainGraphSceneViewExtension>(this);
	Super::Initialize(Collection);
}

void UPPMChainGraphWorldSubsystem::Deinitialize()
{
	// This has to be done on render thread as otherwise there could be a race condition when new level is loaded. 
	ENQUEUE_RENDER_COMMAND(ReleaseSVE)([this](FRHICommandListImmediate& RHICmdList)
	{
		// Prevent this SVE from being gathered, in case it is kept alive by a strong reference somewhere else.
		{
			SceneViewExtension->IsActiveThisFrameFunctions.Empty();

			FSceneViewExtensionIsActiveFunctor IsActiveFunctor;

			IsActiveFunctor.IsActiveFunction = [](const ISceneViewExtension* InSceneViewExtension, const FSceneViewExtensionContext& InContext)
			{
				return TOptional<bool>(false);
			};

			SceneViewExtension->IsActiveThisFrameFunctions.Add(IsActiveFunctor);
		}

		SceneViewExtension.Reset();
	});

	// Finish all rendering commands before cleaning up actors.
	FlushRenderingCommands();
	{
		FScopeLock ScopeLock(&ComponentAccessCriticalSection);
		PPMChainGraphComponents.Reset();
	}
	Super::Deinitialize();

}

void UPPMChainGraphWorldSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

void UPPMChainGraphWorldSubsystem::AddPPMChainGraphComponent(TWeakObjectPtr<UPPMChainGraphExecutorComponent> InComponent)
{
	if (!PPMChainGraphComponents.Contains(InComponent))
	{
		FScopeLock ScopeLock(&ComponentAccessCriticalSection);
		PPMChainGraphComponents.Add(InComponent);
	}
}

void UPPMChainGraphWorldSubsystem::RemovePPMChainGraphComponent(TWeakObjectPtr<UPPMChainGraphExecutorComponent> InComponent)
{
	if (PPMChainGraphComponents.Contains(InComponent))
	{
		FScopeLock ScopeLock(&ComponentAccessCriticalSection);
		PPMChainGraphComponents.Remove(InComponent);
	}
}

#undef LOCTEXT_NAMESPACE