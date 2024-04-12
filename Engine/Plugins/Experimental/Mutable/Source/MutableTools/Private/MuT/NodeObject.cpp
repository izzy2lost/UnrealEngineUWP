// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuT/NodeObject.h"

#include "Misc/AssertionMacros.h"


namespace mu
{

	FNodeType s_nodeObjectType = FNodeType( "NodeObject", Node::GetStaticType() );


	//---------------------------------------------------------------------------------------------
	const FNodeType* NodeObject::GetType() const
	{
		return GetStaticType();
	}


	//---------------------------------------------------------------------------------------------
	const FNodeType* NodeObject::GetStaticType()
	{
		return &s_nodeObjectType;
	}


}


