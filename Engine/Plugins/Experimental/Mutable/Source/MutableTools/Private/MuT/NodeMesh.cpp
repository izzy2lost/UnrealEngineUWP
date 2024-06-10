// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuT/NodeMesh.h"

#include "Misc/AssertionMacros.h"


namespace mu
{

	//---------------------------------------------------------------------------------------------
	// Static initialisation
	//---------------------------------------------------------------------------------------------
	static FNodeType s_nodeMeshType = FNodeType(Node::EType::Mesh , Node::GetStaticType());


	//---------------------------------------------------------------------------------------------
	const FNodeType* NodeMesh::GetType() const
	{
		return GetStaticType();
	}


	//---------------------------------------------------------------------------------------------
	const FNodeType* NodeMesh::GetStaticType()
	{
		return &s_nodeMeshType;
	}


}


