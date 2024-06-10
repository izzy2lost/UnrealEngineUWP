// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuT/NodeMeshClipDeform.h"

#include "Misc/AssertionMacros.h"
#include "MuR/RefCounted.h"
#include "MuT/NodeLayout.h"
#include "MuT/NodeMeshClipDeformPrivate.h"
#include "MuT/NodeMeshPrivate.h"
#include "MuT/NodePrivate.h"


namespace mu
{

	//---------------------------------------------------------------------------------------------
	// Static initialisation
	//---------------------------------------------------------------------------------------------
	FNodeType NodeMeshClipDeform::Private::s_type = FNodeType(Node::EType::MeshClipDeform, NodeMesh::GetStaticType() );


	//---------------------------------------------------------------------------------------------
	//!
	//---------------------------------------------------------------------------------------------

	MUTABLE_IMPLEMENT_NODE( NodeMeshClipDeform )


	//---------------------------------------------------------------------------------------------
	// Own Interface
	//---------------------------------------------------------------------------------------------
	const Ptr<NodeMesh>& NodeMeshClipDeform::GetBaseMesh() const
	{
		return m_pD->m_pBaseMesh;
	}


	//---------------------------------------------------------------------------------------------
	void NodeMeshClipDeform::SetBaseMesh(const Ptr<NodeMesh>& pNode)
	{
		m_pD->m_pBaseMesh = pNode;
	}


	//---------------------------------------------------------------------------------------------
	const Ptr<NodeMesh>& NodeMeshClipDeform::GetClipShape() const
	{
		return m_pD->m_pClipShape;
	}


	//---------------------------------------------------------------------------------------------
	void NodeMeshClipDeform::SetClipShape(const Ptr<NodeMesh>& pNode)
	{
		m_pD->m_pClipShape = pNode;
	}


}


