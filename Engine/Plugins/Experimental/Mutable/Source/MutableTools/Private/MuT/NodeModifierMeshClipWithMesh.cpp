// Copyright Epic Games, Inc. All Rights Reserved.


#include "MuT/NodeModifierMeshClipWithMesh.h"

#include "Misc/AssertionMacros.h"
#include "MuT/NodeMesh.h"
#include "MuT/NodeModifierMeshClipWithMeshPrivate.h"
#include "MuT/NodePrivate.h"


namespace mu
{

	//---------------------------------------------------------------------------------------------
	// Static initialisation
	//---------------------------------------------------------------------------------------------
    FNodeType NodeModifierMeshClipWithMesh::Private::s_type = FNodeType(Node::EType::ModifierMeshClipWithMesh, NodeModifier::GetStaticType() );


	//---------------------------------------------------------------------------------------------
	//!
	//---------------------------------------------------------------------------------------------

    MUTABLE_IMPLEMENT_NODE( NodeModifierMeshClipWithMesh )


	//---------------------------------------------------------------------------------------------
	// Own Interface
	//---------------------------------------------------------------------------------------------
	void NodeModifierMeshClipWithMesh::SetClipMesh(NodeMesh* clipMesh)
	{
		m_pD->ClipMesh = clipMesh;
	}

}
