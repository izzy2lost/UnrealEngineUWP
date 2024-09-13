// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Elements/PCGDynamicMeshBaseElement.h"

#include "GeometryScript/MeshBooleanFunctions.h"

#include "PCGBooleanOperation.generated.h"

UENUM(Blueprintable)
enum class EPCGBooleanOperationTagInheritanceMode : uint8
{
	Both,
	A,
	B,
};

/**
* Do a boolean operation between 2 dynamic meshes.
*/
UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural))
class UPCGBooleanOperationSettings : public UPCGDynamicMeshBaseSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings interface
#if WITH_EDITOR
	virtual FName GetDefaultNodeName() const override;
	virtual FText GetDefaultNodeTitle() const override;
	virtual FText GetNodeTooltipText() const override;
#endif

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings interface
	
public:
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	EGeometryScriptBooleanOperation BooleanOperation = EGeometryScriptBooleanOperation::Intersection;
	
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FGeometryScriptMeshBooleanOptions BooleanOperationOptions;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	EPCGBooleanOperationTagInheritanceMode TagInheritanceMode;
	
	/** Each dynamic mesh in input A will be boolean'd with every dyn mesh in input B (cartesian product), producing N * M dyn meshes. Otherwise, will do a N:N (or N:1 or 1:N) operation, producing N dynamic meshes. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, DisplayName = "Bool Each A With Every B"))
	bool bBoolEachAWithEveryB = false;
};

class FPCGBooleanOperationElement : public IPCGDynamicMeshBaseElement
{
protected:
	virtual bool ExecuteInternal(FPCGContext* InContext) const override;
};

