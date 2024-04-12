// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuT/NodePrivate.h"
#include "MuT/NodeScalarCurve.h"
#include "MuT/NodeScalar.h"


namespace mu
{
	//---------------------------------------------------------------------------------------------
	//!
	//---------------------------------------------------------------------------------------------
    class NodeScalarCurve::Private : public Node::Private
	{
	public:

		static FNodeType s_type;

		NodeScalarPtr m_input_scalar;
		Curve m_curve;
	};


}
