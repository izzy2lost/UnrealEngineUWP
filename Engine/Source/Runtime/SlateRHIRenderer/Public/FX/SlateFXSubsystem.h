// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Subsystems/EngineSubsystem.h"

#include "Engine/World.h"
#include "SlateRHIRendererSettings.h"

#include "SlateFXSubsystem.generated.h"

UCLASS(DisplayName = "Slate FX Subsystem")
class SLATERHIRENDERER_API USlateFXSubsystem : public UEngineSubsystem
{
	GENERATED_BODY()

public:

	//~Begin UGameInstanceSubsystem Insterface
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	//~End UGameInstanceSubsystem Insterface

public:

	/** Get post processor for a particular post buffer index, if it exists */
	UFUNCTION(BlueprintCallable, Category = "SlateFX")
	USlateRHIPostBufferProcessor* GetSlatePostProcessor(ESlatePostRT InPostBufferBit);

private:

	/** Map of post RT buffer index to buffer processors, if they exist */
	UPROPERTY(Transient)
	TMap<ESlatePostRT, TObjectPtr<USlateRHIPostBufferProcessor>> SlatePostBufferProcessors;

private:

	/** Callback to create processors on world init */
	void OnPreWorldInitialization(UWorld* World, const UWorld::InitializationValues IVS);

	/** Callback to remove processors on world cleanup */
	void OnPostWorldCleanup(UWorld* World, bool SessionEnded, bool bCleanupResources);
};
