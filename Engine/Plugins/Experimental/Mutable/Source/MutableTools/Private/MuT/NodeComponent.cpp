// Copyright Epic Games, Inc. All Rights Reserved.


#include "MuT/NodeComponent.h"

#include "Misc/AssertionMacros.h"


namespace mu
{

	static FNodeType s_nodeComponentType = FNodeType( "NodeComponent", Node::GetStaticType() );


	//---------------------------------------------------------------------------------------------
	//---------------------------------------------------------------------------------------------
	//---------------------------------------------------------------------------------------------
	const FNodeType* NodeComponent::GetType() const
	{
		return GetStaticType();
	}


	//---------------------------------------------------------------------------------------------
	const FNodeType* NodeComponent::GetStaticType()
	{
		return &s_nodeComponentType;
	}

}


