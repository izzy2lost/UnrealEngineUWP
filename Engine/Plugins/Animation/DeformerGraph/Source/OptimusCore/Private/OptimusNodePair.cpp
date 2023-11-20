// Copyright Epic Games, Inc. All Rights Reserved.

#include "OptimusNodePair.h"
#include "OptimusNode.h"
#include "IOptimusNodePairProvider.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(OptimusNodePair)

void UOptimusNodePair::PostDuplicate(bool bDuplicateForPIE)
{
	Super::PostDuplicate(bDuplicateForPIE);

	IOptimusNodePairProvider* FirstProvider = CastChecked<IOptimusNodePairProvider>(First);
	IOptimusNodePairProvider* SecondProvider = CastChecked<IOptimusNodePairProvider>(Second);

	FirstProvider->PairToCounterpartNode(SecondProvider);
	SecondProvider->PairToCounterpartNode(FirstProvider);
}

bool UOptimusNodePair::Contains(const UOptimusNode* InFirst, const UOptimusNode* InSecond) const
{
	return (First == InFirst && Second == InSecond) || (First == InSecond && Second == InFirst);	
}

bool UOptimusNodePair::Contains(const UOptimusNode* InNode) const
{
	return First == InNode || Second == InNode;	
}

UOptimusNode* UOptimusNodePair::GetNodeCounterpart(const UOptimusNode* InNode) const
{
	if (First == InNode)
	{
		return Second;
	}

	if (Second == InNode)
	{
		return First;
	}

	return nullptr;
}
