// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuT/NodeExtensionData.h"


namespace mu
{

	static FNodeType s_type = FNodeType("NodeExtensionData", Node::GetStaticType());


	//---------------------------------------------------------------------------------------------
	const FNodeType* NodeExtensionData::GetType() const
	{
		return GetStaticType();
	}


	//---------------------------------------------------------------------------------------------
	const FNodeType* NodeExtensionData::GetStaticType()
	{
		return &s_type;
	}
}


