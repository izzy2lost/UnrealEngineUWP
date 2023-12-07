// Copyright Epic Games, Inc. All Rights Reserved.


#include "MuT/NodeImageTable.h"

#include "Misc/AssertionMacros.h"
#include "MuT/NodeImageTablePrivate.h"
#include "MuT/NodePrivate.h"
#include "MuT/Table.h"


namespace mu
{


	//---------------------------------------------------------------------------------------------
	// Static initialisation
	//---------------------------------------------------------------------------------------------
	NODE_TYPE NodeImageTable::Private::s_type =
			NODE_TYPE( "TableImage", NodeImage::GetStaticType() );


	//---------------------------------------------------------------------------------------------
	//!
	//---------------------------------------------------------------------------------------------

	MUTABLE_IMPLEMENT_NODE( NodeImageTable, EType::Table, Node, Node::EType::Image);


	//---------------------------------------------------------------------------------------------
	// Own Interface
	//---------------------------------------------------------------------------------------------
	void NodeImageTable::SetColumn( const FString& strName )
	{
		m_pD->m_columnName = strName;
	}


	//---------------------------------------------------------------------------------------------
	void NodeImageTable::SetParameterName( const FString& strName )
	{
		m_pD->m_parameterName = strName;
	}


	//---------------------------------------------------------------------------------------------
	void NodeImageTable::SetTable( TablePtr pTable )
	{
		m_pD->m_pTable = pTable;
	}


	//---------------------------------------------------------------------------------------------
	TablePtr NodeImageTable::GetTable() const
	{
		return m_pD->m_pTable;
	}


	//---------------------------------------------------------------------------------------------
	void NodeImageTable::SetMaxTextureSize(uint16 MaxTextureSize)
	{
		m_pD->MaxTextureSize = MaxTextureSize;
	}


	//---------------------------------------------------------------------------------------------
	uint16 NodeImageTable::GetMaxTextureSize()
	{
		return m_pD->MaxTextureSize;
	}


	//---------------------------------------------------------------------------------------------
	void NodeImageTable::SetReferenceImageDescriptor(const FImageDesc& ImageDesc)
	{
		m_pD->ReferenceImageDesc = ImageDesc;
	}

}


