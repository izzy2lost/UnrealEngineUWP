// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuT/NodeScalarSwitch.h"
#include "MuT/NodeScalarPrivate.h"
#include "MuT/NodeScalar.h"

namespace mu
{


    class NodeScalarSwitch::Private : public NodeScalar::Private
	{
	public:

		static FNodeType s_type;

		NodeScalarPtr m_pParameter;
        TArray<NodeScalarPtr> m_options;
	};


}
