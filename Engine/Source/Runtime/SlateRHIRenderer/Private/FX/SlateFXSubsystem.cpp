// Copyright Epic Games, Inc. All Rights Reserved.

#include "FX/SlateFXSubsystem.h"

#include "Engine/Engine.h"
#include "Engine/GameInstance.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(SlateFXSubsystem)

bool USlateFXSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	TArray<UClass*> ChildClasses;
	GetDerivedClasses(GetClass(), ChildClasses, false);

	// Only create an instance if there is no override implementation defined elsewhere
	return ChildClasses.Num() == 0;
}

void USlateFXSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	FWorldDelegates::OnPreWorldInitialization.AddUObject(this, &USlateFXSubsystem::OnPreWorldInitialization);
	FWorldDelegates::OnPostWorldCleanup.AddUObject(this, &USlateFXSubsystem::OnPostWorldCleanup);
}

void USlateFXSubsystem::Deinitialize()
{
	SlatePostBufferProcessors.Empty();

	FWorldDelegates::OnPreWorldInitialization.RemoveAll(this);
	FWorldDelegates::OnPostWorldCleanup.RemoveAll(this);

	Super::Deinitialize();
}

USlateRHIPostBufferProcessor* USlateFXSubsystem::GetSlatePostProcessor(ESlatePostRT InPostBufferBit)
{
	if (TObjectPtr<USlateRHIPostBufferProcessor>* Processor = SlatePostBufferProcessors.Find(InPostBufferBit))
	{
		return *Processor;
	}

	return nullptr;
}

void USlateFXSubsystem::OnPreWorldInitialization(UWorld* World, const UWorld::InitializationValues IVS)
{
	SlatePostBufferProcessors.Empty();

	if (const USlateRHIRendererSettings* SlateRendererSettings = USlateRHIRendererSettings::Get())
	{
		for (ESlatePostRT SlatePostBufferBit : TEnumRange<ESlatePostRT>())
		{
			const FSlatePostSettings& PostSetting = SlateRendererSettings->GetSlatePostSetting(SlatePostBufferBit);
			if (PostSetting.bEnabled && PostSetting.PostProcessorClass)
			{
				SlatePostBufferProcessors.Add(SlatePostBufferBit, NewObject<USlateRHIPostBufferProcessor>(this, PostSetting.PostProcessorClass));
			}
		}
	}
}

void USlateFXSubsystem::OnPostWorldCleanup(UWorld* World, bool SessionEnded, bool bCleanupResources)
{
	SlatePostBufferProcessors.Empty();
}
