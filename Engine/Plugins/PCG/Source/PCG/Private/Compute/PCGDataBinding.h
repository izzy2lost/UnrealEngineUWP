// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PCGData.h"
#include "Compute/PCGDataForGPU.h"

#include "ComputeFramework/ComputeGraphInstance.h"

#include "PCGDataBinding.generated.h"

class UPCGComponent;
class UPCGComputeGraph;

UCLASS(Transient, Category = PCG)
class UPCGDataBinding : public UObject
{
	GENERATED_BODY()

public:
	TWeakObjectPtr<UPCGComponent> SourceComponent;

	FPCGDataCollection OutputDataCollection;

	const UPCGComputeGraph* Graph = nullptr;

	// Data collection pointer + set of pins that get data from the collection (cross barrier)
	FPCGDataForGPU DataForGPU;
};
