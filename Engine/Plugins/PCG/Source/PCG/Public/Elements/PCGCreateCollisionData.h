// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PCGContext.h"
#include "PCGSettings.h"
#include "Async/PCGAsyncLoadingContext.h"
#include "Data/PCGCollisionWrapperData.h"

#include "PCGCreateCollisionData.generated.h"

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural))
class UPCGCreateCollisionDataSettings : public UPCGSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings interface
#if WITH_EDITOR
	virtual FName GetDefaultNodeName() const override { return FName(TEXT("CreateCollisionData")); }
	virtual FText GetDefaultNodeTitle() const override { return NSLOCTEXT("PCGCreateCollisionDataSettings", "NodeTitle", "Create Collision Data"); }
	virtual FText GetNodeTooltipText() const override;
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Spatial; }
#endif // WITH_EDITOR

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings interface

public:
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, PCG_DiscardPropertySelection, PCG_DiscardExtraSelection))
	FPCGAttributePropertyInputSelector CollisionAttribute;

	/** Queries against complex collision if enabled, performance warning */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	bool bUseComplexCollision = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Advanced")
	bool bWarnIfAttributeCouldNotBeUsed = true;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Debug")
	bool bSynchronousLoad = false;
};

struct FPCGCreateCollisionContext : public FPCGContext, public IPCGAsyncLoadingContext
{
	~FPCGCreateCollisionContext();

	struct InputMeshData
	{
		int InputIndex = INDEX_NONE;
		TArray<FSoftObjectPath> MeshPaths;
		UPCGCollisionWrapperData* Data = nullptr;
	};

	TArray<InputMeshData> PerInputData;
};

class FPCGCreateCollisionDataElement : public IPCGElementWithCustomContext<FPCGCreateCollisionContext>
{
public:
	// Loading needs to be done on the main thread and accessing objects outside of PCG might not be thread safe, so taking the safe approach
	virtual bool CanExecuteOnlyOnMainThread(FPCGContext* Context) const override { return true; }

protected:
	virtual bool PrepareDataInternal(FPCGContext* Context) const override;
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};