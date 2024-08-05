// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuCOE/Nodes/CustomizableObjectNodeEditLayoutBlocks.h"
#include "MuCOE/Nodes/CustomizableObjectNodeUseMaterial.h"
#include "MuCOE/Nodes/CustomizableObjectNodeMaterial.h"

#include "CustomizableObjectNodeEditMaterial.generated.h"

namespace ENodeTitleType { enum Type : int; }
struct FCustomizableObjectNodeEditMaterialImage;
struct FNodeMaterialParameterId;

class FArchive;
class FCustomizableObjectNodeParentedMaterial;
class UCustomizableObject;
class UCustomizableObjectNodeRemapPins;
class UObject;


/** Additional data for the Parent Texture Parameter to edit pin. */
UCLASS()
class CUSTOMIZABLEOBJECTEDITOR_API UCustomizableObjectNodeEditMaterialPinEditImageData : public UCustomizableObjectNodePinData
{
	GENERATED_BODY()

public:

	/** Parameter FGuid + Layer Index */
	UPROPERTY()
	FNodeMaterialParameterId ImageParamId;

	UPROPERTY();
	FEdGraphPinReference PinMask;

	UPROPERTY()
	FGuid ImageId_DEPRECATED;
};


UCLASS()
class CUSTOMIZABLEOBJECTEDITOR_API UCustomizableObjectNodeEditMaterial :
	public UCustomizableObjectNodeEditLayoutBlocks,
	public FCustomizableObjectNodeUseMaterial
{
public:
	GENERATED_BODY()

	// UObject interface
	virtual void Serialize(FArchive& Ar) override;
	
	// EdGraphNode interface
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual FLinearColor GetNodeTitleColor() const override;
	virtual FText GetTooltipText() const override;
	virtual void PinConnectionListChanged(UEdGraphPin* Pin) override;

	// UCustomizableObjectNode interface
	virtual void BackwardsCompatibleFixup() override;
	virtual void PostBackwardsCompatibleFixup() override;
	virtual void AllocateDefaultPins(UCustomizableObjectNodeRemapPins* RemapPins) override;
	virtual bool IsNodeOutDatedAndNeedsRefresh() override;
	virtual FString GetRefreshMessage() const override;
	virtual bool IsSingleOutputNode() const override;
	virtual bool CustomRemovePin(UEdGraphPin& Pin) override;
	virtual bool HasPinViewer() const override;

	// UCustomizableObjectNodeEditMaterialBase interface
	virtual UEdGraphPin* OutputPin() const override;
	virtual void SetLayoutIndex(const int32 LayoutIndex) override;
	
	// ICustomizableObjectNodeParentedMaterial interface
	virtual void SetParentNode(UCustomizableObject* Object, FGuid NodeId) override;

	// FCustomizableObjectNodeUseMaterial interface
	virtual UCustomizableObjectNode& GetNode() override;
	virtual FCustomizableObjectNodeParentedMaterial& GetNodeParentedMaterial() override;
	virtual TMap<FNodeMaterialParameterId, FEdGraphPinReference>& GetPinsParameter() override;
	
	
	/** Returns the Image mask pin of the given Image that will be edited.
	 *
	 * @returns Always returns a valid pin if EditsImage(const FGuid&) returns true. */
	const UEdGraphPin* GetUsedImageMaskPin(const FNodeMaterialParameterId& ImageId) const;

private:

	/** Relates a Parameter id (and layer if is a layered material) to a Pin. Only used to improve performance. */
	UPROPERTY()
	TMap<FNodeMaterialParameterId, FEdGraphPinReference> PinsParameterMap;

	// Deprecated properties
	UPROPERTY()
	TMap<FGuid, FEdGraphPinReference> PinsParameter_DEPRECATED;
	
	UPROPERTY()
	TArray<FCustomizableObjectNodeEditMaterialImage> Images_DEPRECATED;
	
	// Old layout blocks to patch. Now in parent class.
	UPROPERTY()
	TArray<int32> Blocks_DEPRECATED;
};
