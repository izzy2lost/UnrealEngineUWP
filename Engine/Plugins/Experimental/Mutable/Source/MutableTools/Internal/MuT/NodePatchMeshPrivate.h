// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuT/NodeMeshPrivate.h"
#include "MuT/NodePatchMesh.h"


namespace mu
{


	class NodePatchMesh::Private : public Node::Private
	{
	public:

		static FNodeType s_type;

		NodeMeshPtr m_pRemove;
		NodeMeshPtr m_pAdd;

	};


}
