// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PCGContext.h"
#include "PCGSettings.h"
#include "Async/PCGAsyncLoadingContext.h"

#include "UObject/SoftObjectPtr.h"

#include "PCGPointFromMeshElement.generated.h"

class UStaticMesh;

// PointFromMesh creates a single point at the origin with an attribute containing a SoftObjectPath to the selected UStaticMesh
UCLASS(BlueprintType, ClassGroup = (Procedural))
class PCG_API UPCGPointFromMeshSettings : public UPCGSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings interface
#if WITH_EDITOR
	virtual FName GetDefaultNodeName() const override { return FName(TEXT("PointFromMesh")); }
	virtual FText GetDefaultNodeTitle() const override;
	virtual FText GetNodeTooltipText() const override;
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Spatial; }
#endif

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override { return TArray<FPCGPinProperties>(); }
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override { return Super::DefaultPointOutputPinProperties(); }
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings interface

public:
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	TSoftObjectPtr<UStaticMesh> StaticMesh;

	/** Name of the string attribute to be created and hold a SoftObjectPath to the StaticMesh */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	FName MeshPathAttributeName = NAME_None;

	/** By default, mesh loading is asynchronous, can force it synchronous if needed. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Debug")
	bool bSynchronousLoad = false;
};

struct FPCGPointFromMeshContext : public FPCGContext, public IPCGAsyncLoadingContext {};

class FPCGPointFromMeshElement : public IPCGElementWithCustomContext<FPCGPointFromMeshContext>
{
protected:
	virtual bool PrepareDataInternal(FPCGContext* Context) const override;
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};
