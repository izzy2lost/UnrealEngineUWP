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
            m_layoutOrGroup = -1;
		}

		static FNodeType s_type;

		NodeMeshPtr m_pMesh;
        int m_layoutOrGroup;
		TArray<int> m_blocks;

        FRAGMENT_TYPE m_fragmentType;


		// NodeMesh::Private interface
        NodeLayoutPtr GetLayout( int index ) const override;
	};


}
