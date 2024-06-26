// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Compute/DataInterfaces/PCGDataCollectionDataInterface.h"

#include "ComputeFramework/ComputeDataInterface.h"
#include "ComputeFramework/ComputeDataProvider.h"

#include "PCGDataCollectionReadbackDataInterface.generated.h"

class UPCGDataBinding;

/** Compute Framework Data Interface for reading back data from PCG data collection. */
UCLASS(ClassGroup = (Procedural))
class UPCGDataCollectionReadbackDataInterface : public UPCGDataCollectionDataInterface
{
	GENERATED_BODY()

public:
	//~ Begin UComputeDataInterface Interface
	virtual bool GetRequiresReadback() const override { return true; }
	UComputeDataProvider* CreateDataProvider(TObjectPtr<UObject> InBinding, uint64 InInputMask, uint64 InOutputMask) const override;
	//~ End UComputeDataInterface Interface
};

/** Compute Framework Data Provider for writing a PCG Data Collection. */
UCLASS()
class UPCGDataProviderDataCollectionReadback : public UPCGDataCollectionDataProvider
{
	GENERATED_BODY()

public:
	//~ Begin UComputeDataProvider Interface
	FComputeDataProviderRenderProxy* GetRenderProxy() override;
	//~ End UComputeDataProvider Interface

	/** Processed read back data. Should be called after readback complete. Called from game thread. Returns true if data processed successfully. */
	bool ProcessReadBackData();

	std::atomic<bool> bReadbackComplete = false;
	TArray<uint8> RawReadbackData;

	FName OutputPinLabelAlias;

	DECLARE_EVENT(UPCGDataProviderDataCollectionReadback, FOnReadbackComplete);
	FOnReadbackComplete& OnReadbackComplete_RenderThread() { return OnReadbackComplete; }

private:
	FOnReadbackComplete OnReadbackComplete;
};

class FPCGDataProviderDataCollectionReadbackProxy : public FPCGDataCollectionDataProviderProxy
{
public:
	FPCGDataProviderDataCollectionReadbackProxy(TWeakObjectPtr<UPCGDataBinding> InBinding, const FPCGDataCollectionDesc& InPinDesc, int InSizeBytes, FReadbackCallback InAsyncReadbackCallback_RenderThread);

	//~ Begin FComputeDataProviderRenderProxy Interface
	bool IsValid(FValidationData const& InValidationData) const override;
	void AllocateResources(FRDGBuilder& GraphBuilder, FAllocationData const& InAllocationData) override;
	void GetReadbackData(TArray<FReadbackData>& OutReadbackData) const override;
	//~ End FComputeDataProviderRenderProxy Interface

private:
	FReadbackCallback AsyncReadbackCallback_RenderThread;
};
