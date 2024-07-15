// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Compute/PCGDataForGPU.h"
#include "Compute/DataInterfaces/PCGComputeDataInterface.h"

#include "ComputeFramework/ComputeDataProvider.h"

#include "PCGDataCollectionDataInterface.generated.h"

class FPCGDataCollectionDataInterfaceParameters;
class FRDGBuffer;
class FRDGBufferSRV;
class UPCGDataBinding;

/** Compute Framework Data Interface for reading PCG data. */
UCLASS(ClassGroup = (Procedural))
class UPCGDataCollectionDataInterface : public UPCGComputeDataInterface
{
	GENERATED_BODY()

public:
	//~ Begin UComputeDataInterface Interface
	TCHAR const* GetClassName() const override { return TEXT("PCGDataCollection"); }
	/** Return true if the associated UComputeDataProvider holds data that can be combined into a single dispatch invocation. */
	bool CanSupportUnifiedDispatch() const override { return false; } // I think this means compute shader can produce multiple buffers simultaneously?
	// TODO don't allow writing to an input!
	void GetSupportedInputs(TArray<FShaderFunctionDefinition>& OutFunctions) const override;
	// TODO could differentiate later for SRV vs UAV.
	void GetSupportedOutputs(TArray<FShaderFunctionDefinition>& OutFunctions) const override { GetSupportedInputs(OutFunctions); }
	void GetShaderParameters(TCHAR const* UID, FShaderParametersMetadataBuilder& InOutBuilder, FShaderParametersMetadataAllocations& InOutAllocations) const override;
	TCHAR const* GetShaderVirtualPath() const override; // TODO probably easier to just inline rather than external source?
	void GetShaderHash(FString& InOutKey) const override;
	void GetHLSL(FString& OutHLSL, FString const& InDataInterfaceName) const override;
	UComputeDataProvider* CreateDataProvider(TObjectPtr<UObject> InBinding, uint64 InInputMask, uint64 InOutputMask) const override;
	//~ End UComputeDataInterface Interface

	/** Settings of node that produces this data, normally the upstream node. */
	UPROPERTY()
	const UPCGSettings* ProducerSettings;

private:
	static TCHAR const* TemplateFilePath;
};

/** Compute Framework Data Provider for reading a PCG Data Collection. */
UCLASS()
class UPCGDataCollectionDataProvider : public UComputeDataProvider
{
	GENERATED_BODY()

public:
	//~ Begin UComputeDataProvider Interface
	FComputeDataProviderRenderProxy* GetRenderProxy() override;
	//~ End UComputeDataProvider Interface

	TWeakObjectPtr<UPCGDataBinding> Binding;
	FPCGDataCollectionDesc PinDesc;
};

class FPCGDataCollectionDataProviderProxy : public FComputeDataProviderRenderProxy
{
public:
	FPCGDataCollectionDataProviderProxy(TWeakObjectPtr<UPCGDataBinding> InBinding, const FPCGDataCollectionDesc& InPinDesc)
		: Binding(InBinding)
		, PinDesc(InPinDesc)
	{}

	//~ Begin FComputeDataProviderRenderProxy Interface
	bool IsValid(FValidationData const& InValidationData) const override;
	void GatherDispatchData(FDispatchData const& InDispatchData) override;
	void AllocateResources(FRDGBuilder& GraphBuilder, FAllocationData const& InAllocationData) override;
	//~ End FComputeDataProviderRenderProxy Interface

protected:
	using FParameters = FPCGDataCollectionDataInterfaceParameters;

	TWeakObjectPtr<UPCGDataBinding> Binding;
	FPCGDataCollectionDesc PinDesc;

	FRDGBufferRef Buffer = nullptr;
	FRDGBufferUAVRef BufferUAV = nullptr;
};
