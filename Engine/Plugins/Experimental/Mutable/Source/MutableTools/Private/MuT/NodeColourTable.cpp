// Copyright Epic Games, Inc. All Rights Reserved.


#include "MuT/NodeColourTable.h"

#include "Misc/AssertionMacros.h"
#include "MuT/NodeColourTablePrivate.h"
#include "MuT/NodePrivate.h"
#include "MuT/Table.h"



namespace mu
{


	//---------------------------------------------------------------------------------------------
	// Static initialisation
	//---------------------------------------------------------------------------------------------
	NODE_TYPE NodeColourTable::Private::s_type =
			NODE_TYPE( "TableColour", NodeColour::GetStaticType() );


	//---------------------------------------------------------------------------------------------
	//!
	//---------------------------------------------------------------------------------------------

	MUTABLE_IMPLEMENT_NODE( NodeColourTable, EType::Table, Node, Node::EType::Colour)


	//---------------------------------------------------------------------------------------------
	// Own Interface
	//---------------------------------------------------------------------------------------------
	void NodeColourTable::SetColumn( const FString& strName )
	{
		m_pD->m_columnName = strName;
	}


	//---------------------------------------------------------------------------------------------
	void NodeColourTable::SetParameterName( const FString& strName )
	{
		m_pD->m_parameterName = strName;
	}


	//---------------------------------------------------------------------------------------------
	void NodeColourTable::SetTable( TablePtr pTable )
	{
		m_pD->m_pTable = pTable;
	}


	//---------------------------------------------------------------------------------------------
	TablePtr NodeColourTable::GetTable() const
	{
		return m_pD->m_pTable;
	}


}


