// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuT/NodeSurfaceSwitch.h"

#include "MuT/NodePrivate.h"
#include "MuT/NodeScalar.h"


namespace mu
{

	class NodeSurfaceSwitch::Private : public NodeSurface::Private
	{
	public:

		static FNodeType s_type;

		Ptr<NodeScalar> Parameter;
		TArray<Ptr<NodeSurface>> Options;
	};


}
