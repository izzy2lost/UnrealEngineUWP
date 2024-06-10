// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuT/NodeSurface.h"

#include "Misc/AssertionMacros.h"

namespace mu
{


	//---------------------------------------------------------------------------------------------
	// Static initialisation
	//---------------------------------------------------------------------------------------------
    static FNodeType s_nodeSurfaceType = FNodeType(Node::EType::Surface, Node::GetStaticType() );


	//---------------------------------------------------------------------------------------------
	//---------------------------------------------------------------------------------------------
	//---------------------------------------------------------------------------------------------
    const FNodeType* NodeSurface::GetType() const
	{
		return GetStaticType();
	}


	//---------------------------------------------------------------------------------------------
    const FNodeType* NodeSurface::GetStaticType()
	{
        return &s_nodeSurfaceType;
	}

}


