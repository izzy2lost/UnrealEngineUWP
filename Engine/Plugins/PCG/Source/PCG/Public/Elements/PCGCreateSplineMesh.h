// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PCGCommon.h"
#include "PCGContext.h"
#include "PCGElement.h"
#include "PCGSettings.h"
#include "PCGSplineMeshParams.h"
#include "Async/PCGAsyncLoadingContext.h"
#include "Metadata/PCGObjectPropertyOverride.h"

#include "Engine/SplineMeshComponentDescriptor.h"

#include "PCGCreateSplineMesh.generated.h"

struct FPCGContext;

/** Create a USplineMeshComponent for each segment along a given spline. */
UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural))
class UPCGCreateSplineMeshSettings : public UPCGSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings interface
#if WITH_EDITOR
	virtual FName GetDefaultNodeName() const override { return FName(TEXT("CreateSplineMesh")); }
	virtual FText GetDefaultNodeTitle() const override { return NSLOCTEXT("PCGCreateSplineMeshElement", "NodeTitle", "Create Spline Mesh"); }
	virtual FText GetNodeTooltipText() const override;
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Spatial; }
#endif

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override { return TArray<FPCGPinProperties>(); }
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings interface

public:
	UPROPERTY(EditAnywhere, Category = Settings)
	FSoftSplineMeshComponentDescriptor SplineMeshDescriptor;

	UPROPERTY(EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FPCGSplineMeshParams SplineMeshParams;

	UPROPERTY(meta = (PCG_Overridable))
	TSoftObjectPtr<AActor> TargetActor;

	/** Specify a list of functions to be called on the target actor after spline mesh creation. Functions need to be parameter-less and with "CallInEditor" flag enabled. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	TArray<FName> PostProcessFunctionNames;

	/** Force meshes/materials to load synchronously. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Debug")
	bool bSynchronousLoad = false;

	/** Overrides for spline mesh descriptor. For now it is only support data wide attributes. Does not work for per-control point overrides for now. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Settings)
	TArray<FPCGObjectPropertyOverrideDescription> SplineMeshOverrideDescriptions;
};

struct FPCGCreateSplineMeshContext : public FPCGContext, public IPCGAsyncLoadingContext {};

class FPCGCreateSplineMeshElement : public IPCGElement
{
public:
	virtual bool CanExecuteOnlyOnMainThread(FPCGContext* Context) const override { return true; }
	virtual bool IsCacheable(const UPCGSettings* InSettings) const override { return false; }

protected:
	virtual FPCGContext* CreateContext() override;
	virtual bool PrepareDataInternal(FPCGContext* InContext) const override;
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};
