// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuR/Ptr.h"

class UEdGraphPin;
struct FMutableGraphGenerationContext;

namespace mu
{
	class NodeComponent;
}


mu::Ptr<mu::NodeComponent> GenerateMutableSourceComponent(const UEdGraphPin* Pin, FMutableGraphGenerationContext& GenerationContext);

