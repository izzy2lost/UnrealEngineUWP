// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuT/NodeColourPrivate.h"
#include "MuT/NodeColourArithmeticOperation.h"

namespace mu
{

	class NodeColourArithmeticOperation::Private : public NodeColour::Private
	{
	public:

		static FNodeType s_type;

		NodeColourArithmeticOperation::OPERATION m_operation;
		NodeColourPtr m_pA;
		NodeColourPtr m_pB;
	};


}
