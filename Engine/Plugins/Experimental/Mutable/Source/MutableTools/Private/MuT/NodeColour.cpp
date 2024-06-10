// Copyright Epic Games, Inc. All Rights Reserved.


#include "MuT/NodeColour.h"

#include "Misc/AssertionMacros.h"


namespace mu
{

	static FNodeType s_nodeColourType = FNodeType( Node::EType::Color, Node::GetStaticType() );


	//---------------------------------------------------------------------------------------------
	const FNodeType* NodeColour::GetType() const
	{
		return GetStaticType();
	}


	//---------------------------------------------------------------------------------------------
	const FNodeType* NodeColour::GetStaticType()
	{
		return &s_nodeColourType;
	}


}


