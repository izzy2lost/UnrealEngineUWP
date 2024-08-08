// Copyright Epic Games, Inc. All Rights Reserved.

#include "HoldoutCompositeSubsystem.h"
#include "HoldoutCompositeModule.h"
#include "HoldoutCompositeSceneViewExtension.h"

#include "Engine/RendererSettings.h"

UHoldoutCompositeSubsystem::UHoldoutCompositeSubsystem()
{
}

void UHoldoutCompositeSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	UWorld* World = GetWorld();
	check(IsValid(World));

	HoldoutCompositeViewExtension = FSceneViewExtensions::NewExtension<FHoldoutCompositeSceneViewExtension>(World);
}

void UHoldoutCompositeSubsystem::Deinitialize()
{
	HoldoutCompositeViewExtension.Reset();
	
	Super::Deinitialize();
}

void UHoldoutCompositeSubsystem::RegisterPrimitive(TSoftObjectPtr<UPrimitiveComponent> InPrimitiveComponent, bool bInHoldoutState)
{
	TArray<TSoftObjectPtr<UPrimitiveComponent>> PrimitiveComponents = { InPrimitiveComponent };

	RegisterPrimitives(PrimitiveComponents, bInHoldoutState);
}

void UHoldoutCompositeSubsystem::RegisterPrimitives(TArrayView<TSoftObjectPtr<UPrimitiveComponent>> InPrimitiveComponents, bool bInHoldoutState)
{
	// The compositing relies on alpha preserved through the "tonemapper" post-processing step.
	static IConsoleVariable* CVarPropagateAlpha = IConsoleManager::Get().FindConsoleVariable(TEXT("r.PostProcessing.PropagateAlpha"));
	if (CVarPropagateAlpha && CVarPropagateAlpha->GetInt() != static_cast<int32>(EAlphaChannelMode::AllowThroughTonemapper))
	{
		UE_CALL_ONCE([]()
			{
				UE_LOG(LogHoldoutComposite, Warning, TEXT("Holdout composite is disabled until alpha is enabled through post-processing."));
			}
		);
		return;
	}

	if (HoldoutCompositeViewExtension.IsValid())
	{
		HoldoutCompositeViewExtension->RegisterPrimitives(MoveTemp(InPrimitiveComponents), bInHoldoutState);
	}
}

void UHoldoutCompositeSubsystem::UnregisterPrimitive(TSoftObjectPtr<UPrimitiveComponent> InPrimitiveComponent, bool bInHoldoutState)
{
	TArray<TSoftObjectPtr<UPrimitiveComponent>> PrimitiveComponents = { InPrimitiveComponent };
	
	UnregisterPrimitives(PrimitiveComponents, bInHoldoutState);
}

void UHoldoutCompositeSubsystem::UnregisterPrimitives(TArrayView<TSoftObjectPtr<UPrimitiveComponent>> InPrimitiveComponents, bool bInHoldoutState)
{
	if (HoldoutCompositeViewExtension.IsValid())
	{
		HoldoutCompositeViewExtension->UnregisterPrimitives(MoveTemp(InPrimitiveComponents), bInHoldoutState);
	}
}

