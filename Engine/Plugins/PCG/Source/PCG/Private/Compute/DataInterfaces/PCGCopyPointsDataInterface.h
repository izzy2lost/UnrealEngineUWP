// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Compute/DataInterfaces/PCGComputeDataInterface.h"
#include "ComputeFramework/ComputeDataProvider.h"

#include "PCGCopyPointsDataInterface.generated.h"

class FPCGCopyPointsDataInterfaceParameters;
class UPCGCopyPointsSettings;
class UPCGSettings;

/** Data Interface to marshal Copy Points settings to the GPU. */
UCLASS(ClassGroup = (Procedural))
class UPCGCopyPointsDataInterface : public UPCGComputeDataInterface
{
	GENERATED_BODY()

public:
	//~ Begin UComputeDataInterface Interface
	TCHAR const* GetClassName() const override { return TEXT("PCGCopyPoints"); }
	void GetSupportedInputs(TArray<FShaderFunctionDefinition>& OutFunctions) const override;
	void GetShaderParameters(TCHAR const* UID, FShaderParametersMetadataBuilder& InOutBuilder, FShaderParametersMetadataAllocations& InOutAllocations) const override;
	void GetHLSL(FString& OutHLSL, FString const& InDataInterfaceName) const override;
	UComputeDataProvider* CreateDataProvider(TObjectPtr<UObject> InBinding, uint64 InInputMask, uint64 InOutputMask) const override;
	//~ End UComputeDataInterface Interface

	UPROPERTY()
	TObjectPtr<const UPCGSettings> Settings = nullptr;
};

UCLASS()
class UPCGCopyPointsDataProvider : public UComputeDataProvider
{
	GENERATED_BODY()

public:
	//~ Begin UComputeDataProvider Interface
	FComputeDataProviderRenderProxy* GetRenderProxy() override;
	//~ End UComputeDataProvider Interface

	const UPCGCopyPointsSettings* Settings;
};

class FPCGCopyPointsDataProviderProxy : public FComputeDataProviderRenderProxy
{
public:
	FPCGCopyPointsDataProviderProxy(const UPCGCopyPointsSettings* InSettings)
		: Settings(InSettings)
	{}

	//~ Begin FComputeDataProviderRenderProxy Interface
	bool IsValid(FValidationData const& InValidationData) const override;
	void GatherDispatchData(FDispatchData const& InDispatchData) override;
	//~ End FComputeDataProviderRenderProxy Interface

protected:
	using FParameters = FPCGCopyPointsDataInterfaceParameters;

	const UPCGCopyPointsSettings* Settings;
};
