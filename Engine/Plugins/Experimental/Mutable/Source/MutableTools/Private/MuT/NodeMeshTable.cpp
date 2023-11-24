// Copyright Epic Games, Inc. All Rights Reserved.


#include "MuT/NodeMeshTable.h"

#include "Misc/AssertionMacros.h"
#include "MuT/NodeLayout.h"
#include "MuT/NodeMeshTablePrivate.h"
#include "MuT/NodePrivate.h"
#include "MuT/Table.h"


namespace mu
{


	//---------------------------------------------------------------------------------------------
	// Static initialisation
	//---------------------------------------------------------------------------------------------
	NODE_TYPE NodeMeshTable::Private::s_type =
			NODE_TYPE( "TableMesh", NodeMesh::GetStaticType() );


	//---------------------------------------------------------------------------------------------
	//!
	//---------------------------------------------------------------------------------------------

	MUTABLE_IMPLEMENT_NODE( NodeMeshTable, EType::Table, Node, Node::EType::Mesh);


	//---------------------------------------------------------------------------------------------
	// Own Interface
	//---------------------------------------------------------------------------------------------
	void NodeMeshTable::SetColumn( const FString& strName )
	{
		m_pD->m_columnName = strName;
	}


	//---------------------------------------------------------------------------------------------
	void NodeMeshTable::SetParameterName( const FString& strName )
	{
		m_pD->m_parameterName = strName;
	}


	//---------------------------------------------------------------------------------------------
	void NodeMeshTable::SetTable( TablePtr pTable )
	{
		m_pD->m_pTable = pTable;
	}


	//---------------------------------------------------------------------------------------------
	TablePtr NodeMeshTable::GetTable() const
	{
		return m_pD->m_pTable;
	}


	//---------------------------------------------------------------------------------------------
	int NodeMeshTable::GetLayoutCount() const
	{
		return m_pD->m_layouts.Num();
	}


	//---------------------------------------------------------------------------------------------
	void NodeMeshTable::SetLayoutCount( int i )
	{
		m_pD->m_layouts.SetNum( i );
	}


	//---------------------------------------------------------------------------------------------
	NodeLayoutPtr NodeMeshTable::GetLayout( int i ) const
	{
		check( i>=0 && i<GetLayoutCount() );
		return m_pD->GetLayout( i );
	}


	//---------------------------------------------------------------------------------------------
	void NodeMeshTable::SetLayout( int i, NodeLayoutPtr pLayout )
	{
		check( i>=0 && i<GetLayoutCount() );
		m_pD->m_layouts[i] = pLayout;
	}


	//---------------------------------------------------------------------------------------------
	NodeLayoutPtr NodeMeshTable::Private::GetLayout( int i ) const
	{
		NodeLayoutPtr pResult;

		if ( i>=0 && i<m_layouts.Num() )
		{
			pResult = m_layouts[i];
		}

		return pResult;
	}

}


