// Copyright Epic Games, Inc. All Rights Reserved.


#include "MuT/NodeScalarVariation.h"

#include "Misc/AssertionMacros.h"
#include "MuT/NodePrivate.h"
#include "MuT/NodeScalarVariationPrivate.h"


namespace mu
{

    //---------------------------------------------------------------------------------------------
    // Static initialisation
    //---------------------------------------------------------------------------------------------
    NODE_TYPE NodeScalarVariation::Private::s_type =
        NODE_TYPE( "ScalarVariation", NodeScalar::GetStaticType() );


    //---------------------------------------------------------------------------------------------
    //---------------------------------------------------------------------------------------------
    //---------------------------------------------------------------------------------------------

    MUTABLE_IMPLEMENT_NODE( NodeScalarVariation, EType::Variation, Node, Node::EType::Scalar)


    //---------------------------------------------------------------------------------------------
    int NodeScalarVariation::GetInputCount() const { return 1 + int( m_pD->m_variations.Num() ); }


    //---------------------------------------------------------------------------------------------
    Node* NodeScalarVariation::GetInputNode( int i ) const
    {
        check( i >= 0 && i < GetInputCount() );

        if ( i == 0 )
        {
            return m_pD->m_defaultScalar.get();
        }
        i -= 1;

        if ( i < int( m_pD->m_variations.Num() ) )
        {
            return m_pD->m_variations[i].m_scalar.get();
        }
        i -= int( m_pD->m_variations.Num() );

        return nullptr;
    }


    //---------------------------------------------------------------------------------------------
    void NodeScalarVariation::SetInputNode( int i, NodePtr pNode )
    {
        check( i >= 0 && i < GetInputCount() );

        if ( i == 0 )
        {
            m_pD->m_defaultScalar = dynamic_cast<NodeScalar*>( pNode.get() );
            return;
        }

        i -= 1;
        if ( i < int( m_pD->m_variations.Num() ) )
        {

            m_pD->m_variations[i].m_scalar = dynamic_cast<NodeScalar*>( pNode.get() );
            return;
        }
        i -= (int)m_pD->m_variations.Num();
    }


    //---------------------------------------------------------------------------------------------
    // Own Interface
    //---------------------------------------------------------------------------------------------
    void NodeScalarVariation::SetDefaultScalar( NodeScalar* p ) 
	{ 
		m_pD->m_defaultScalar = p; 
	}


	void NodeScalarVariation::SetVariationCount( int32 num )
    {
        check( num >= 0 );
        m_pD->m_variations.SetNum( num );
    }


	void NodeScalarVariation::SetVariationTag( int32 index, const FString& Tag )
    {
        check( index >= 0 && index < m_pD->m_variations.Num() );

		m_pD->m_variations[index].m_tag = Tag;
    }


    void NodeScalarVariation::SetVariationScalar( int32 index, NodeScalar* pNode )
    {
        check( index >= 0 && index < m_pD->m_variations.Num() );

        m_pD->m_variations[index].m_scalar = pNode;
    }


} // namespace mu
