// Copyright Epic Games, Inc. All Rights Reserved.

#include "PPMChainGraphActor.h"
#include "PPMChainGraphComponent.h"

APPMChainGraphActor::APPMChainGraphActor(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	PPMChainGraphExecutorComponent = CreateDefaultSubobject<UPPMChainGraphExecutorComponent>(TEXT("PPMChainGraphExecutorComponent0"));
}
