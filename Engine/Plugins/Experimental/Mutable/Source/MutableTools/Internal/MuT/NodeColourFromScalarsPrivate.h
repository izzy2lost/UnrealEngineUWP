// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuT/NodeColourPrivate.h"
#include "MuT/NodeColourFromScalars.h"
#include "MuT/NodeScalar.h"
#include "MuT/NodeImage.h"


namespace mu
{


	class NodeColourFromScalars::Private : public NodeColour::Private
	{
	public:

		static FNodeType s_type;

		NodeScalarPtr m_pX;
		NodeScalarPtr m_pY;
		NodeScalarPtr m_pZ;
		NodeScalarPtr m_pW;

	};


}
