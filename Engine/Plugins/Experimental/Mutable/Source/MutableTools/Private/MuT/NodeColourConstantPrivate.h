// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuT/NodeColourConstant.h"
#include "MuT/NodeColourPrivate.h"
#include "MuR/MutableMath.h"

namespace mu
{

	class NodeColourConstant::Private : public NodeColour::Private
	{
	public:

		static FNodeType s_type;

		FVector4f m_value;
	};

}
