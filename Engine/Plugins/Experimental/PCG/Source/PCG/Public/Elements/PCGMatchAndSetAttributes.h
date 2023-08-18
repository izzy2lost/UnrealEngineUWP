// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PCGSettings.h"

#include "PCGMatchAndSetAttributes.generated.h"

/** This class creates a PCG node that can match, select by weight or match & select by weight 
* a 'matching' entry in a provided Attribute Set with multiple entries.
* E.g. for a given point, if the point has the same specified attribute as the matching attribute in the attribute set,
* then we will copy all the other non-selection attributes to the point.
*/
UCLASS(BlueprintType, ClassGroup = (Procedural))
class PCG_API UPCGMatchAndSetAttributesSettings : public UPCGSettings
{
	GENERATED_BODY()

public:
	UPCGMatchAndSetAttributesSettings();

	// ~Begin UPCGSettings interface
#if WITH_EDITOR
	virtual FName GetDefaultNodeName() const override;
	virtual FText GetDefaultNodeTitle() const override;
	virtual FText GetNodeTooltipText() const override;
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Metadata; }
#endif // WITH_EDITOR

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;
	virtual FPCGElementPtr CreateElement() const override;
	// ~End UPCGSettings interface

public:
	/** Controls whether selection of the attribute set values to copy will be done by matching point-to-attribute set (true) or done randomly (false) */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	bool bMatchAttributes = false;

	/** Attribute from the point data to select & maetch */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (EditCondition = "bMatchAttributes"))
	FPCGAttributePropertyInputSelector InputAttribute;

	/** Attribute from the attribute set to match against */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (EditCondition = "bMatchAttributes"))
	FName MatchAttribute = NAME_None;

	/** Controls whether points that have no valid match in the attribute set are kept as is (default values) or removed from the output */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (EditCondition = "bMatchAttributes", PCG_Overridable))
	bool bKeepUnmatched = true;

	/** Controls whether we will consider the weights, as determined by the Weight Attribute values on the attribute set */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (InlineEditConditionToggle, PCG_Overridable))
	bool bUseWeightAttribute = false;

	/** Attribute to weight more or less some entries from the attribute set */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (EditCondition = "bUseWeightAttribute", PCG_Overridable))
	FName WeightAttribute = NAME_None;
};

class FPCGMatchAndSetAttributesElement : public FSimplePCGElement
{
public:
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};