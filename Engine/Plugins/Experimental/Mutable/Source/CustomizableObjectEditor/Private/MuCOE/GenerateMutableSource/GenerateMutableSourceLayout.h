// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuT/NodeLayout.h"
#include "MuCOE/CustomizableObjectLayout.h"

class UEdGraphPin;
struct FMutableGraphGenerationContext;


mu::Ptr<mu::NodeLayout> GenerateMutableSourceLayout(const UEdGraphPin* Pin, FMutableGraphGenerationContext& GenerationContext, bool bLinkedToExtendMaterial = false);


/** */
inline mu::Layout::FBlock ToMutable(const FCustomizableObjectLayoutBlock& UnrealBlock)
{
	mu::Layout::FBlock MutableBlock;

	MutableBlock.Min = { uint16(UnrealBlock.Min.X), uint16(UnrealBlock.Min.Y) };
	FIntPoint Size = UnrealBlock.Max - UnrealBlock.Min;
	MutableBlock.Size = { uint16(Size.X), uint16(Size.Y) };

	MutableBlock.Priority = UnrealBlock.Priority;
	MutableBlock.bReduceBothAxes = UnrealBlock.bReduceBothAxes;
	MutableBlock.bReduceByTwo = UnrealBlock.bReduceByTwo;

	return MutableBlock;
}
