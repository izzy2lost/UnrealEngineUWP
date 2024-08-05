// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCOE/Nodes/CustomizableObjectNodeEditLayoutBlocks.h"


#define LOCTEXT_NAMESPACE "CustomizableObjectEditor"


UCustomizableObjectNodeEditLayoutBlocks::UCustomizableObjectNodeEditLayoutBlocks()
	: Super()
{
	Layout = CreateDefaultSubobject<UCustomizableObjectLayout>(FName("CustomizableObjectLayout"));
	Layout->Blocks.Empty();
}

#undef LOCTEXT_NAMESPACE

