// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuT/NodeSurfaceSwitch.h"

#include "Misc/AssertionMacros.h"
#include "MuT/NodeLayout.h"
#include "MuT/NodeSurfaceSwitchPrivate.h"
#include "MuT/NodePrivate.h"
#include "MuT/NodeScalar.h"


namespace mu
{

	//---------------------------------------------------------------------------------------------
	// Static initialisation
	//---------------------------------------------------------------------------------------------
	NODE_TYPE NodeSurfaceSwitch::Private::s_type =
			NODE_TYPE( "SurfaceSwitch", NodeSurface::GetStaticType() );


	//---------------------------------------------------------------------------------------------
	//!
	//---------------------------------------------------------------------------------------------

	MUTABLE_IMPLEMENT_NODE( NodeSurfaceSwitch, EType::Switch, Node, Node::EType::Surface)


	//---------------------------------------------------------------------------------------------
	// Node Interface
	//---------------------------------------------------------------------------------------------
	int NodeSurfaceSwitch::GetInputCount() const
	{
		return 1 + m_pD->Options.Num();
	}


	//---------------------------------------------------------------------------------------------
	Node* NodeSurfaceSwitch::GetInputNode( int i ) const
	{
		check( i>=0 && i<GetInputCount() );

		Node* pResult = 0;

		switch (i)
		{
		case 0:
			pResult = m_pD->Parameter.get();
			break;

		default:
			pResult = m_pD->Options[i-1].get();
			break;
		}

		return pResult;
	}


	//---------------------------------------------------------------------------------------------
	void NodeSurfaceSwitch::SetInputNode( int i, NodePtr pNode )
	{
		check( i>=0 && i<GetInputCount() );

		switch (i)
		{
		case 0:
			m_pD->Parameter = dynamic_cast<NodeScalar*>(pNode.get());
			break;

		default:
			m_pD->Options[i-1] = dynamic_cast<NodeSurface*>(pNode.get());
			break;
		}
	}


	//---------------------------------------------------------------------------------------------
	// Own Interface
	//---------------------------------------------------------------------------------------------
	NodeScalarPtr NodeSurfaceSwitch::GetParameter() const
	{
		return m_pD->Parameter.get();
	}


	//---------------------------------------------------------------------------------------------
	void NodeSurfaceSwitch::SetParameter( NodeScalarPtr pNode )
	{
		m_pD->Parameter = pNode;
	}


	//---------------------------------------------------------------------------------------------
	void NodeSurfaceSwitch::SetOptionCount( int32 t )
	{
		m_pD->Options.SetNum(t);
	}


	//---------------------------------------------------------------------------------------------
	NodeSurfacePtr NodeSurfaceSwitch::GetOption( int32 t ) const
	{
		check( t>=0 && t<m_pD->Options.Num() );
		return m_pD->Options[t].get();
	}


	//---------------------------------------------------------------------------------------------
	void NodeSurfaceSwitch::SetOption( int32 t, NodeSurfacePtr pNode )
	{
		check( t>=0 && t<m_pD->Options.Num() );
		m_pD->Options[t] = pNode;
	}

}


