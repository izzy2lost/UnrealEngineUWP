// Copyright Epic Games, Inc. All Rights Reserved.


#include "MuT/NodeColourParameter.h"

#include "Misc/AssertionMacros.h"
#include "MuR/MutableMath.h"
#include "MuT/NodeColourParameterPrivate.h"
#include "MuT/NodePrivate.h"
#include "MuT/NodeRange.h"


namespace mu
{

	//---------------------------------------------------------------------------------------------
	// Static initialisation
	//---------------------------------------------------------------------------------------------
	NODE_TYPE NodeColourParameter::Private::s_type =
			NODE_TYPE( "ColourParameter", NodeColour::GetStaticType() );


	//---------------------------------------------------------------------------------------------
	//!
	//---------------------------------------------------------------------------------------------

	MUTABLE_IMPLEMENT_NODE( NodeColourParameter, EType::Parameter, Node, Node::EType::Colour)


	//---------------------------------------------------------------------------------------------
	// Node Interface
	//---------------------------------------------------------------------------------------------
	int NodeColourParameter::GetInputCount() const
	{
        return  m_pD->m_ranges.Num();
    }


	//---------------------------------------------------------------------------------------------
    Node* NodeColourParameter::GetInputNode( int i ) const
	{
        check( i<GetInputCount() );
        if (i<GetInputCount())
        {
            return m_pD->m_ranges[i].get();
        }
        return nullptr;
    }


	//---------------------------------------------------------------------------------------------
    void NodeColourParameter::SetInputNode( int i, NodePtr n )
	{
        check( i<GetInputCount() );
        if (i<GetInputCount())
        {
            m_pD->m_ranges[i] = dynamic_cast<NodeRange*>(n.get());
        }
    }


	//---------------------------------------------------------------------------------------------
	// Own Interface
	//---------------------------------------------------------------------------------------------
	void NodeColourParameter::SetName( const FString& strName )
	{
		m_pD->m_name = strName;
	}


	//---------------------------------------------------------------------------------------------
	void NodeColourParameter::SetUid( const FString& strUid )
	{
		m_pD->m_uid = strUid;
	}


	//---------------------------------------------------------------------------------------------
	void NodeColourParameter::SetDefaultValue( float r, float g, float b )
	{
		m_pD->m_defaultValue = vec3<float>( r, g, b );
	}


    //---------------------------------------------------------------------------------------------
    void NodeColourParameter::SetRangeCount( int i )
    {
        check(i>=0);
        m_pD->m_ranges.SetNum(i);
    }


    //---------------------------------------------------------------------------------------------
    void NodeColourParameter::SetRange( int i, NodeRangePtr pRange )
    {
        check( i>=0 && i<int(m_pD->m_ranges.Num()) );
        if ( i>=0 && i<int(m_pD->m_ranges.Num()) )
        {
            m_pD->m_ranges[i] = pRange;
        }
    }

}


