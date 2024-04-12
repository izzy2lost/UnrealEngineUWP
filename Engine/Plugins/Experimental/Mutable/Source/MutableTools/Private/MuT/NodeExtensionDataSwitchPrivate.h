// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuR/ExtensionData.h"
#include "MuT/NodeExtensionDataSwitch.h"
#include "MuT/NodePrivate.h"
#include "MuT/NodeScalar.h"

namespace mu
{
	class NodeExtensionDataSwitch::Private : public Node::Private
	{
	public:

		static FNodeType s_type;

		NodeScalarPtr Parameter;
		TArray<NodeExtensionDataPtr> Options;
	};
}