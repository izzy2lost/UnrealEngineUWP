// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuCOE/Nodes/CustomizableObjectNodeModifierBase.h"
#include "MuT/NodeModifier.h"

#include "CustomizableObjectNodeModifierClipWithUVMask.generated.h"

namespace ENodeTitleType { enum Type : int; }

class UCustomizableObject;
class UCustomizableObjectNodeRemapPins;
class UEdGraphPin;

UCLASS()
class CUSTOMIZABLEOBJECTEDITOR_API UCustomizableObjectNodeModifierClipWithUVMask : public UCustomizableObjectNodeModifierBase
{
	GENERATED_BODY()

public:

	/** Materials in all other objects that activate this tags will be clipped with this UV mask. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = MeshToClip)
	TArray<FString> Tags;

	/** Policy to use tags in case more than one is added. */
	UPROPERTY(EditAnywhere, Category = MeshToClip)
	EMutableMultipleTagPolicy MultipleTagPolicy = EMutableMultipleTagPolicy::OnlyOneRequired;

	/** UV channel index that will be used to get the UVs to apply the clipping mask to. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = MeshToClip)
	int32 UVChannelForMask = 0;

public:

	// EdGraphNode interface
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual FText GetTooltipText() const override;

	// UCustomizableObjectNode interface
	virtual void AllocateDefaultPins(UCustomizableObjectNodeRemapPins* RemapPins) override;
	virtual void PinConnectionListChanged(UEdGraphPin* Pin) override;

	// UCustomizableObjectNodeModifierBase interface
	virtual const TArray<FString>* GetRequiredTags() const override { return &Tags; }
	virtual EMutableMultipleTagPolicy GetMultipleTagsPolicy() const override { return MultipleTagPolicy; }

	// Own interface

	/** Access to input pins. */
	UEdGraphPin* ClipMaskPin() const;
};

