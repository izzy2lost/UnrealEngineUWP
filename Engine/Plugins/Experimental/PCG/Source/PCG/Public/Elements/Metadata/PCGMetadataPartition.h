// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PCGSettings.h"
#include "Metadata/PCGAttributePropertySelector.h"

#include "PCGMetadataPartition.generated.h"

UCLASS(BlueprintType, ClassGroup = (Procedural))
class PCG_API UPCGMetadataPartitionSettings : public UPCGSettings
{
	GENERATED_BODY()

public:
	//~Begin UObject interface
	virtual void PostLoad() override;
	//~End UObject interface

	//~Begin UPCGSettings interface
#if WITH_EDITOR
	virtual FName GetDefaultNodeName() const override { return FName(TEXT("AttributePartition")); }
	virtual FText GetDefaultNodeTitle() const override { return NSLOCTEXT("PCGMetadataPartitionSettings", "NodeTitle", "Attribute Partition"); }
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Metadata; }
	virtual bool HasDynamicPins() const override { return true; }
#endif
	virtual bool IsInputPinRequiredByExecution(const UPCGPin* InPin) const override { return !InPin->Properties.bAdvancedPin; }
	virtual FString GetAdditionalTitleInformation() const override;

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings interface

public:
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, PCG_OverrideAliases = "PartitionAttribute"))
	FPCGAttributePropertyInputSelector PartitionAttributeSource;

#if WITH_EDITORONLY_DATA
	UPROPERTY()
	FName PartitionAttribute_DEPRECATED = NAME_None;
#endif // WITH_EDITORONLY_DATA
};

class FPCGMetadataPartitionElement : public IPCGElement
{
protected:
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};
