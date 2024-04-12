// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuT/NodeExtensionDataVariation.h"
#include "MuT/NodePrivate.h"

namespace mu
{
	class NodeExtensionDataVariation::Private : public Node::Private
	{
	public:

		static FNodeType s_type;

		NodeExtensionDataPtr DefaultValue;

		struct FVariation
		{
			NodeExtensionDataPtr Value;
			FString Tag;
		};

		TArray<FVariation> Variations;
	};
}