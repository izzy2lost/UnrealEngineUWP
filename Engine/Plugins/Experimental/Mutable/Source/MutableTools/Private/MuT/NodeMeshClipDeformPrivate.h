// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuT/NodeMeshClipDeform.h"
#include "MuT/NodeScalar.h"
#include "MuT/NodeMeshPrivate.h"
#include "MuT/NodeImagePrivate.h"

namespace mu
{

	class NodeMeshClipDeform::Private : public NodeMesh::Private
	{
	public:

		static FNodeType s_type;

		Ptr<NodeMesh> m_pBaseMesh;
		Ptr<NodeMesh> m_pClipShape;
		Ptr<NodeImage> m_pShapeWeights;

	};

}
