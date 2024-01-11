// Copyright Epic Games, Inc. All Rights Reserved.


#include "MuT/NodeScalarTable.h"

#include "Misc/AssertionMacros.h"
#include "MuT/NodePrivate.h"
#include "MuT/NodeScalarTablePrivate.h"
#include "MuT/Table.h"


namespace mu
{


	//---------------------------------------------------------------------------------------------
	// Static initialisation
	//---------------------------------------------------------------------------------------------
	NODE_TYPE NodeScalarTable::Private::s_type =
			NODE_TYPE( "TableScalar", NodeScalar::GetStaticType() );


	//---------------------------------------------------------------------------------------------
	//!
	//---------------------------------------------------------------------------------------------

	MUTABLE_IMPLEMENT_NODE(NodeScalarTable, EType::Table, Node, Node::EType::Scalar)


	//---------------------------------------------------------------------------------------------
	// Own Interface
	//---------------------------------------------------------------------------------------------
	void NodeScalarTable::SetColumn( const FString& strName )
	{
		m_pD->ColumnName = strName;
	}


	//---------------------------------------------------------------------------------------------
	void NodeScalarTable::SetParameterName( const FString& strName )
	{
		m_pD->ParameterName = strName;
	}


	//---------------------------------------------------------------------------------------------
	void NodeScalarTable::SetTable( TablePtr pTable )
	{
		m_pD->Table = pTable;
	}


	//---------------------------------------------------------------------------------------------
	TablePtr NodeScalarTable::GetTable() const
	{
		return m_pD->Table;
	}


	//---------------------------------------------------------------------------------------------
	void NodeScalarTable::SetNoneOption(bool bAddNoneOption)
	{
		m_pD->bNoneOption = bAddNoneOption;
	}

}


