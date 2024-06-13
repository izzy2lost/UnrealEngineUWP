// Copyright Epic Games, Inc. All Rights Reserved.


#include "MuT/NodeColourTable.h"

#include "Misc/AssertionMacros.h"
#include "MuT/NodePrivate.h"
#include "MuT/Table.h"


namespace mu
{


	//---------------------------------------------------------------------------------------------
	// Static initialisation
	//---------------------------------------------------------------------------------------------
	FNodeType NodeColourTable::StaticType = FNodeType(Node::EType::ColorTable, NodeColour::GetStaticType() );


	//---------------------------------------------------------------------------------------------
	// Own Interface
	//---------------------------------------------------------------------------------------------
	void NodeColourTable::SetColumn( const FString& strName )
	{
		ColumnName = strName;
	}


	//---------------------------------------------------------------------------------------------
	void NodeColourTable::SetParameterName( const FString& strName )
	{
		ParameterName = strName;
	}


	//---------------------------------------------------------------------------------------------
	void NodeColourTable::SetNoneOption(bool bAddNoneOption)
	{
		bNoneOption = bAddNoneOption;
	}

	
	//---------------------------------------------------------------------------------------------
	void NodeColourTable::SetDefaultRowName(const FString RowName)
	{
		DefaultRowName = RowName;
	}

}


