// Copyright Epic Games, Inc. All Rights Reserved.


#include "Misc/AssertionMacros.h"
#include "MuR/MutableMath.h"
#include "MuR/Parameters.h"
#include "MuR/Ptr.h"
#include "MuR/RefCounted.h"
#include "MuT/Node.h"
#include "MuT/NodePrivate.h"
#include "MuT/NodeProjector.h"
#include "MuT/NodeProjectorPrivate.h"
#include "MuT/NodeRange.h"


namespace mu
{

	//---------------------------------------------------------------------------------------------
	// Static initialisation
	//---------------------------------------------------------------------------------------------
	NODE_TYPE NodeProjectorParameter::Private::s_type =
			NODE_TYPE( "ProjectorParameter", NodeProjector::GetStaticType() );


	//---------------------------------------------------------------------------------------------
	//!
	//---------------------------------------------------------------------------------------------

	MUTABLE_IMPLEMENT_NODE( NodeProjectorParameter, EType::Parameter, Node, Node::EType::Projector)


	//---------------------------------------------------------------------------------------------
	// Own Interface
	//---------------------------------------------------------------------------------------------
	void NodeProjectorParameter::SetName( const FString& strName )
	{
		m_pD->m_name = strName;
	}


	const FString& NodeProjectorParameter::GetUid() const
	{
		return m_pD->m_uid;
	}


	void NodeProjectorParameter::SetUid( const FString& strUid )
	{
		m_pD->m_uid = strUid;
	}


	//---------------------------------------------------------------------------------------------
    void NodeProjectorParameter::SetDefaultValue( PROJECTOR_TYPE type,
                                                  float posX, float posY, float posZ,
												  float dirX, float dirY, float dirZ,
												  float upX, float upY, float upZ,
                                                  float scaleU, float scaleV, float scaleW,
                                                  float projectionAngle )
	{
        m_pD->m_type = type;
        m_pD->m_position = vec3<float>( posX, posY, posZ );
		m_pD->m_direction = vec3<float>( dirX, dirY, dirZ );
		m_pD->m_up = vec3<float>( upX, upY, upZ );
        m_pD->m_scale = vec3<float>( scaleU, scaleV, scaleW );
        m_pD->m_projectionAngle = projectionAngle;
    }


    //---------------------------------------------------------------------------------------------
    void NodeProjectorParameter::SetRangeCount( int i )
    {
        check(i>=0);
        m_pD->m_ranges.SetNum(i);
    }


    //---------------------------------------------------------------------------------------------
    void NodeProjectorParameter::SetRange( int i, NodeRangePtr pRange )
    {
        check( i>=0 && i<m_pD->m_ranges.Num() );
        if ( i>=0 && i<m_pD->m_ranges.Num() )
        {
            m_pD->m_ranges[i] = pRange;
        }
    }

}


