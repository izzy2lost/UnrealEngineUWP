// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuT/NodePrivate.h"
#include "MuT/NodeLOD.h"
#include "MuT/NodeComponentPrivate.h"
#include "MuT/NodeModifierPrivate.h"

namespace mu
{

	class NodeLOD::Private : public Node::Private
	{
	public:

		static FNodeType s_type;

		TArray<NodeComponentPtr> m_components;
		TArray<NodeModifierPtr> m_modifiers;
	};

}
