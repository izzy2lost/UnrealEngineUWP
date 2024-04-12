// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuT/NodeColourSwitch.h"
#include "MuT/NodeColourPrivate.h"
#include "MuT/NodeScalar.h"


namespace mu
{


    class NodeColourSwitch::Private : public NodeColour::Private
	{
	public:

		static FNodeType s_type;

		NodeScalarPtr m_pParameter;
        TArray<NodeColourPtr> m_options;

	};


}

