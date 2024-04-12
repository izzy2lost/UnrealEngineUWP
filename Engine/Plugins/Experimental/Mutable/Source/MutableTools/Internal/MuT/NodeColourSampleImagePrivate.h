// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuT/NodeColourPrivate.h"
#include "MuT/NodeColourSampleImage.h"
#include "MuT/NodeScalar.h"
#include "MuT/NodeImage.h"


namespace mu
{


	class NodeColourSampleImage::Private : public NodeColour::Private
	{
	public:

		static FNodeType s_type;

		NodeImagePtr m_pImage;
		NodeScalarPtr m_pX;
		NodeScalarPtr m_pY;
	};


}
