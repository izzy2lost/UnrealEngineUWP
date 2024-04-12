// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuT/NodeMeshPrivate.h"

#include "MuT/NodeMeshConstant.h"
#include "MuT/NodeLayout.h"


namespace mu
{

	class NodeMeshConstant::Private : public NodeMesh::Private
	{
	public:

		static FNodeType s_type;

		MeshPtr m_pValue;

		TArray<NodeLayoutPtr> m_layouts;

		// NodeMesh::Private interface
        NodeLayoutPtr GetLayout( int index ) const override;

	};

}
