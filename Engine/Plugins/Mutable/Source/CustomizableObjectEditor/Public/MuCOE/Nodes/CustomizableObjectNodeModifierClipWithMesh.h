// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuCOE/Nodes/CustomizableObjectNodeModifierBase.h"
#include "MuT/NodeModifier.h"

#include "CustomizableObjectNodeModifierClipWithMesh.generated.h"

namespace ENodeTitleType { enum Type : int; }

class UCustomizableObject;
class UCustomizableObjectNodeRemapPins;
class UEdGraphPin;
class UObject;
struct FGuid;

UCLASS()
class CUSTOMIZABLEOBJECTEDITOR_API UCustomizableObjectNodeModifierClipWithMesh : public UCustomizableObjectNodeModifierBase
{
public:
	GENERATED_BODY()

	UCustomizableObjectNodeModifierClipWithMesh();

	UPROPERTY()
	TArray<FString> Tags_DEPRECATED;

	//!< If assigned, then a material inside this CO will be clipped by this node.
    //!< If several materials with the same name, all are considered (to cover all LOD levels)
    UPROPERTY(EditAnywhere, Category = CustomizableObjectToClip)
	TObjectPtr<UCustomizableObject> CustomizableObjectToClipWith;

    //!< Array with the Guids of the nodes with the same material inside the CustomizableObjectToClipWith CO (if any is assigned)
    UPROPERTY(EditAnywhere, Category = CustomizableObjectToClip)
    TArray<FGuid> ArrayMaterialNodeToClipWithID;

	/** Mesh Transform*/
	UPROPERTY(EditAnywhere, Category = CustomizableObject)
	FTransform Transform;

	// Details view variables
	// The clipping node uses tags
	bool bUseTags;

	// The clipping node uses a material name
	bool bUseMaterials;

	// EdGraphNode interface
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual FText GetTooltipText() const override;

	// UCustomizableObjectNode interface
	virtual void AllocateDefaultPins(UCustomizableObjectNodeRemapPins* RemapPins) override;
	virtual void ReconstructNode(UCustomizableObjectNodeRemapPins* RemapPinsMode) override;
	virtual void PinConnectionListChanged(UEdGraphPin* Pin) override;
	virtual void UpdateReferencedNodeId(const FGuid& NewGuid) override;
	virtual void BeginPostDuplicate(bool bDuplicateForPIE) override;
	virtual void BackwardsCompatibleFixup(int32 CustomizableObjectCustomVersion) override;

	// Own interface
	UEdGraphPin* OutputPin() const;

	UEdGraphPin* ClipMeshPin() const;
};

