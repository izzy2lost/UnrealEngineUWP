// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Compute/PCGDataForGPU.h"

#include "ComputeFramework/ComputeGraph.h"
#include "ComputeFramework/ComputeKernelCompileResult.h"

#include "UObject/ObjectKey.h"

#include "PCGComputeGraph.generated.h"

class UPCGNode;
class UPCGPin;

UCLASS()
class UPCGComputeGraph : public UComputeGraph
{
	GENERATED_BODY()

public:
	//~Begin UComputeGraph interface
	void OnKernelCompilationComplete(int32 InKernelIndex, FComputeKernelCompileResults const& InCompileResults) override;
	//~End UComputeGraph interface

	/** Get the global attribute indices. */
	const TMap<FPCGKernelAttributeKey, int32>& GetAttributeLookupTable() const { return GlobalAttributeLookupTable; }

public:
	TMap<TObjectKey<const UPCGNode>, TArray<FComputeKernelCompileMessage>> KernelToCompileMessages;
	
	/** Set of input pins at the CPU -> GPU border. */
	UPROPERTY()
	TArray<TWeakObjectPtr<const UPCGPin>> PinsReceivingDataFromCPU;

	/** Pin label aliases, used for selecting data items corresponding to an input pin from the input data collection. */
	UPROPERTY()
	TMap<TObjectPtr<const UPCGPin>, FName> InputPinLabelAliases;

	/** Mapping from upstream output pin to downstream pin alias, used to select data items originating from upstream pin from the input data collection. */
	UPROPERTY()
	TMap<TObjectPtr<const UPCGPin>, FName> OutputCPUPinToInputGPUPinAlias;

	// Node corresponding to each kernel, useful for compilation feedback.
	UPROPERTY()
	TArray<TWeakObjectPtr<const UPCGNode>> KernelToNode;

protected:
	UPROPERTY()
	TMap<FPCGKernelAttributeKey, int32 /* Attribute Index */> GlobalAttributeLookupTable;

	friend class FPCGGraphCompilerGPU;
};
