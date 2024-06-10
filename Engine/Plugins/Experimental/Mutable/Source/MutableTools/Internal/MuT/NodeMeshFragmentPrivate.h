// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuT/NodeMeshFragment.h"
#include "MuT/NodeScalar.h"
#include "MuT/NodeMeshPrivate.h"


namespace mu
{


	class NodeMeshFragment::Private : public NodeMesh::Private
	{
	public:

		Private()
		{
            LayoutOrGroup = -1;
		}

		static FNodeType s_type;

		Ptr<NodeMesh> m_pMesh;
        int32 LayoutOrGroup;
		TArray<uint64> Blocks;

        FRAGMENT_TYPE m_fragmentType;
	};


}
