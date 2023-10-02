// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Param/AnimNextObjectAdapterConfig.h"
#include "AnimNextConfig.generated.h"

namespace UE::AnimNext
{
	struct FParamId;
}

UCLASS(Config=AnimNext)
class ANIMNEXT_API UAnimNextConfig : public UObject
{
	GENERATED_BODY()

private:
	// UObject interface
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

private:
	friend struct UE::AnimNext::FParamId;

	/** The classes that are exposed to AnimNext systems */
	UPROPERTY(Config, EditAnywhere, Category = "Exposed Classes")
	TArray<FAnimNextObjectAdapterConfig> ExposedClasses;
};