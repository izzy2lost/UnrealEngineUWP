// Copyright Epic Games, Inc. All Rights Reserved.


#include "MuT/NodeModifierMeshClipWithUVMask.h"

#include "Misc/AssertionMacros.h"
#include "MuT/NodeImage.h"
#include "MuT/NodeModifierMeshClipWithUVMaskPrivate.h"
#include "MuT/NodePrivate.h"


namespace mu
{

	//---------------------------------------------------------------------------------------------
	// Static initialisation
	//---------------------------------------------------------------------------------------------
    NODE_TYPE NodeModifierMeshClipWithUVMask::Private::s_type =
            NODE_TYPE( "NodeModifierMeshClipWithUVMask", NodeModifier::GetStaticType() );


	//---------------------------------------------------------------------------------------------
	//!
	//---------------------------------------------------------------------------------------------

    MUTABLE_IMPLEMENT_NODE( NodeModifierMeshClipWithUVMask, EType::MeshClipWithMesh, Node, Node::EType::Modifier)


	//---------------------------------------------------------------------------------------------
	// Node Interface
	//---------------------------------------------------------------------------------------------
    int NodeModifierMeshClipWithUVMask::GetInputCount() const
	{
		return 1;
	}


	//---------------------------------------------------------------------------------------------
    Node* NodeModifierMeshClipWithUVMask::GetInputNode( int i ) const
	{
		check( i>=0 && i< GetInputCount());
        (void)i;
        return m_pD->ClipMask.get();
	}


	//---------------------------------------------------------------------------------------------
    void NodeModifierMeshClipWithUVMask::SetInputNode( int i, Ptr<Node> Input )
	{
		check( i>=0 && i< GetInputCount());
        (void)i;
		m_pD->ClipMask = dynamic_cast<NodeImage*>(Input.get());
    }


	//---------------------------------------------------------------------------------------------
	// Own Interface
	//---------------------------------------------------------------------------------------------
	void NodeModifierMeshClipWithUVMask::SetClipMask(NodeImage* InClipMask)
	{
		m_pD->ClipMask = InClipMask;
	}

	void NodeModifierMeshClipWithUVMask::SetLayoutIndex(uint8 LayoutIndex)
	{
		m_pD->LayoutIndex = LayoutIndex;
	}

	

}
