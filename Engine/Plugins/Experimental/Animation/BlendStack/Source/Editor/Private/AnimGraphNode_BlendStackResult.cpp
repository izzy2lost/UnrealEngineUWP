// Copyright Epic Games, Inc. All Rights Reserved.

#include "AnimGraphNode_BlendStackResult.h"
#include "AnimGraphNode_BlendStackInput.h"

bool UAnimGraphNode_BlendStackResult::IsNodeRootSet() const
{
	if (!Pins.IsEmpty() && !Pins[0]->LinkedTo.IsEmpty())
	{
		if (Pins[0]->LinkedTo[0]->GetOuter()->IsA<UAnimGraphNode_BlendStackInput>())
		{
			// If output is connected to input directly, graph is a no-op. Consider it not set for pruning purposes.
			return false;
		}
	}

	return true;
}
