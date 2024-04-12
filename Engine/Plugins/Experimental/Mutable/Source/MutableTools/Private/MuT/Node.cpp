// Copyright Epic Games, Inc. All Rights Reserved.


#include "MuT/Node.h"
#include "MuT/NodePrivate.h"
#include "Misc/AssertionMacros.h"


namespace mu
{

	// Static initialisation
	static FNodeType s_nodeType = FNodeType( "Node", 0 );

	FNodeType::FNodeType()
	{
		m_strName = "";
		m_pParent = nullptr;
	}


	FNodeType::FNodeType( const char* strName, const FNodeType* pParent )
	{
        m_strName = strName;
		m_pParent = pParent;
	}

	const FNodeType* Node::GetType() const
	{
		return GetStaticType();
	}


	const FNodeType* Node::GetStaticType()
	{
		return &s_nodeType;
	}


	void Node::SetMessageContext( const void* context )
	{
		GetBasePrivate()->m_errorContext = context;
	}

}


