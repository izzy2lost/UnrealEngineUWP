// Copyright Epic Games, Inc. All Rights Reserved.


#include "MuT/NodeMeshFragment.h"

#include "Misc/AssertionMacros.h"
#include "MuT/NodeLayout.h"
#include "MuT/NodeMeshFragmentPrivate.h"
#include "MuT/NodeMeshPrivate.h"
#include "MuT/NodePrivate.h"


namespace mu
{

	//---------------------------------------------------------------------------------------------
	// Static initialisation
	//---------------------------------------------------------------------------------------------
	FNodeType NodeMeshFragment::Private::s_type = FNodeType(Node::EType::MeshFragment, NodeMesh::GetStaticType() );


	//---------------------------------------------------------------------------------------------
	//!
	//---------------------------------------------------------------------------------------------

	MUTABLE_IMPLEMENT_NODE( NodeMeshFragment )


	//---------------------------------------------------------------------------------------------
	// Own Interface
	//---------------------------------------------------------------------------------------------
    NodeMeshFragment::FRAGMENT_TYPE NodeMeshFragment::GetFragmentType() const
    {
        return m_pD->m_fragmentType;
    }


    //---------------------------------------------------------------------------------------------
    void NodeMeshFragment::SetFragmentType( FRAGMENT_TYPE type )
    {
        m_pD->m_fragmentType = type;
    }


    //---------------------------------------------------------------------------------------------
    Ptr<NodeMesh> NodeMeshFragment::GetMesh() const
	{
		return m_pD->m_pMesh.get();
	}


	//---------------------------------------------------------------------------------------------
	void NodeMeshFragment::SetMesh(Ptr<NodeMesh> pNode )
	{
		m_pD->m_pMesh = pNode;
	}


	//---------------------------------------------------------------------------------------------
    int32 NodeMeshFragment::GetLayoutOrGroup() const
	{
        return m_pD->LayoutOrGroup;
	}


	//---------------------------------------------------------------------------------------------
    void NodeMeshFragment::SetLayoutOrGroup( int32 l )
	{
        m_pD->LayoutOrGroup = l;
	}


	//---------------------------------------------------------------------------------------------
	void NodeMeshFragment::SetBlockCount( int32 t )
	{
		m_pD->Blocks.SetNum(t);
	}


	//---------------------------------------------------------------------------------------------
	int32 NodeMeshFragment::GetBlock( int32 t ) const
	{
		check( t>=0 && t<m_pD->Blocks.Num() );
		return m_pD->Blocks[t];
	}


	//---------------------------------------------------------------------------------------------
	void NodeMeshFragment::SetBlock( int32 t, int32 b )
	{
		check( t>=0 && t<m_pD->Blocks.Num() );
		m_pD->Blocks[t] = b;
	}

}


