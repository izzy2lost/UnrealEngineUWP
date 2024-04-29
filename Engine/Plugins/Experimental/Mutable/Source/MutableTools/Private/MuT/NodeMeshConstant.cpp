// Copyright Epic Games, Inc. All Rights Reserved.


#include "MuT/NodeMeshConstant.h"

#include "Misc/AssertionMacros.h"
#include "MuT/NodeLayout.h"
#include "MuT/NodeMeshConstantPrivate.h"
#include "MuT/NodePrivate.h"


namespace mu
{
	FNodeType NodeMeshConstant::Private::s_type = FNodeType( "MeshConstant", NodeMesh::GetStaticType() );


	MUTABLE_IMPLEMENT_NODE( NodeMeshConstant, EType::Constant, Node, Node::EType::Mesh);


	Ptr<Mesh> NodeMeshConstant::GetValue() const
	{
		return m_pD->Value;
	}


	void NodeMeshConstant::SetValue( Ptr<Mesh> pValue )
	{
		m_pD->Value = pValue;

        if (m_pD->Value)
        {
            // Ensure mesh is well formed
            m_pD->Value->EnsureSurfaceData();
        }
    }


	int32 NodeMeshConstant::GetLayoutCount() const
	{
		return m_pD->Layouts.Num();
	}


	void NodeMeshConstant::SetLayoutCount( int32 num )
	{
		check( num >=0 );
		m_pD->Layouts.SetNum( num );
	}


	Ptr<NodeLayout> NodeMeshConstant::GetLayout( int32 index ) const
	{
		check( index >=0 && index < m_pD->Layouts.Num() );

		return m_pD->GetLayout( index );
	}


	Ptr<NodeLayout> NodeMeshConstant::Private::GetLayout( int32 index ) const
	{
		NodeLayoutPtr pResult;

		if ( index >=0 && index < Layouts.Num() )
		{
			pResult = Layouts[ index ];
		}

		return pResult;
	}


	void NodeMeshConstant::SetLayout( int32 index, Ptr<NodeLayout> pLayout )
	{
		check( index >=0 && index < m_pD->Layouts.Num() );

		m_pD->Layouts[ index ] = pLayout;
	}


}


