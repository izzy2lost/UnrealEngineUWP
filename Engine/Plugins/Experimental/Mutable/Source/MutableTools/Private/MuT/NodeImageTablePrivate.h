// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuT/NodePrivate.h"
#include "MuT/NodeImageTable.h"
#include "MuT/TablePrivate.h"


namespace mu
{


	class NodeImageTable::Private : public Node::Private
	{
	public:

		static FNodeType s_type;

		FString ParameterName;
		TablePtr Table;
		FString ColumnName;
		uint16 MaxTextureSize = 0;
		FImageDesc ReferenceImageDesc;
		bool bNoneOption = false;
		FString DefaultRowName;

	};

}
