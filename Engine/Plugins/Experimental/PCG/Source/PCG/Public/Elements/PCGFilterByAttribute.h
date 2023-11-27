// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Elements/PCGFilterDataBase.h"

#include "PCGFilterByAttribute.generated.h"

/** Separates data on whether they have a specific metadata attribute (not by value) */
UCLASS(BlueprintType, ClassGroup = (Procedural))
class PCG_API UPCGFilterByAttributeSettings : public UPCGFilterDataBaseSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings interface
#if WITH_EDITOR
	virtual FName GetDefaultNodeName() const override { return FName(TEXT("FilterDataByAttribute")); }
	virtual FText GetDefaultNodeTitle() const override;
	virtual FText GetNodeTooltipText() const override { return NSLOCTEXT("PCGFilterByAttributeElement", "NodeTooltip", "Separates input data by whether they have the specified attribute or not."); }
#endif

	virtual FName AdditionalTaskName() const override;

protected:
#if WITH_EDITOR
	virtual EPCGChangeType GetChangeTypeForProperty(const FName& InPropertyName) const override { return Super::GetChangeTypeForProperty(InPropertyName) | EPCGChangeType::Cosmetic; }
#endif
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings interface

public:
	/** Attribute to look for */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FName Attribute;
};

class FPCGFilterByAttributeElement : public IPCGElement
{
protected:
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};
