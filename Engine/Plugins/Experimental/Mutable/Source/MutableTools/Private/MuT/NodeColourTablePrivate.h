// Copyright Epic Games, Inc. All Rights Reserved.


#pragma once

#include "MuT/NodePrivate.h"
#include "MuT/NodeColourTable.h"
#include "MuT/TablePrivate.h"


namespace mu
{


	class NodeColourTable::Private : public Node::Private
	{
	public:

		static FNodeType s_type;

		FString ParameterName;
		TablePtr Table;
		FString ColumnName;
		bool bNoneOption = false;
		FString DefaultRowName;

	};

}

