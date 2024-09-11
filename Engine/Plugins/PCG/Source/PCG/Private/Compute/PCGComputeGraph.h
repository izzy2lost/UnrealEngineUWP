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

	/** Some attributes in table may be missing type information that we could not infer statically. Fill in missing types from the execution-time data. */
	void FillInMissingAttributeTableTypes(const FPCGDataCollection& InComputeGraphElementInputData);

	/** Get the global attribute indices. */
	const TMap<FName, FPCGKernelAttributeIDAndType>& GetAttributeLookupTable() const { return GlobalAttributeLookupTable; }

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
	TMap<FName /* Attribute name */, FPCGKernelAttributeIDAndType> GlobalAttributeLookupTable;

	friend class FPCGGraphCompilerGPU;
};
