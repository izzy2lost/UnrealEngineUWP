// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuCOE/Nodes/CustomizableObjectNodeMaterialBase.h"

#include "CustomizableObjectNodeModifierBase.generated.h"

class UObject;


UCLASS(abstract)
class CUSTOMIZABLEOBJECTEDITOR_API UCustomizableObjectNodeModifierBase : public UCustomizableObjectNode
{
public:
	GENERATED_BODY()

	// EdGraphNode interface
	virtual FLinearColor GetNodeTitleColor() const override;

	// UCustomizableObjectNode interface
	virtual void BackwardsCompatibleFixup(int32 CustomizableObjectCustomVersion) override;
	virtual void PostBackwardsCompatibleFixup() override;
};

