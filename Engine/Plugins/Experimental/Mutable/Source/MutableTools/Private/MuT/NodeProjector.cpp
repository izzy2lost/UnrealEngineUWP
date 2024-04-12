// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuT/NodeProjector.h"

#include "Misc/AssertionMacros.h"


namespace mu
{

	static FNodeType s_nodeProjectorType = FNodeType( "NodeProjector", Node::GetStaticType() );


	//---------------------------------------------------------------------------------------------
	const FNodeType* NodeProjector::GetType() const
	{
		return GetStaticType();
	}


	//---------------------------------------------------------------------------------------------
	const FNodeType* NodeProjector::GetStaticType()
	{
		return &s_nodeProjectorType;
	}


}


