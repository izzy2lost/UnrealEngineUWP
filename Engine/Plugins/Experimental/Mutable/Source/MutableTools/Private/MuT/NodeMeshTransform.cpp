// Copyright Epic Games, Inc. All Rights Reserved.


#include "MuT/NodeMeshTransform.h"

#include "HAL/PlatformString.h"
#include "Misc/AssertionMacros.h"
#include "MuR/MutableMath.h"
#include "MuT/NodeLayout.h"
#include "MuT/NodeMeshPrivate.h"
#include "MuT/NodeMeshTransformPrivate.h"
#include "MuT/NodePrivate.h"


namespace mu
{

	//---------------------------------------------------------------------------------------------
	// Static initialisation
	//---------------------------------------------------------------------------------------------
    NODE_TYPE NodeMeshTransform::Private::s_type =
            NODE_TYPE( "MeshTransform", NodeMesh::GetStaticType() );


	//---------------------------------------------------------------------------------------------
	//!
	//---------------------------------------------------------------------------------------------

    MUTABLE_IMPLEMENT_NODE( NodeMeshTransform, EType::Transform, Node, Node::EType::Mesh)


	//---------------------------------------------------------------------------------------------
	// Own Interface
	//---------------------------------------------------------------------------------------------
    NodeMeshPtr NodeMeshTransform::GetSource() const
	{
		return m_pD->m_pSource;
	}


    //---------------------------------------------------------------------------------------------
    void NodeMeshTransform::SetSource( NodeMesh* p )
    {
        m_pD->m_pSource = p;
    }


    //---------------------------------------------------------------------------------------------
    void NodeMeshTransform::SetTransform( const float* mat )
    {
        memcpy( &m_pD->m_transform[0][0], mat, 16*sizeof(float) );
    }


    //---------------------------------------------------------------------------------------------
    void NodeMeshTransform::GetTransform( float* mat ) const
    {
        memcpy( mat, &m_pD->m_transform[0][0], 16*sizeof(float) );
    }


	//---------------------------------------------------------------------------------------------
    NodeLayoutPtr NodeMeshTransform::Private::GetLayout( int index ) const
	{
		NodeLayoutPtr pResult;

		if ( m_pSource )
		{
			NodeMesh::Private* pPrivate =
					dynamic_cast<NodeMesh::Private*>( m_pSource->GetBasePrivate() );

			pResult = pPrivate->GetLayout( index );
		}

		return pResult;
	}


}
