// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuT/NodeMeshPrivate.h"
#include "MuT/NodeMeshTable.h"
#include "MuT/NodeLayout.h"
#include "MuT/TablePrivate.h"


namespace mu
{


	class NodeMeshTable::Private : public NodeMesh::Private
	{
	public:

		static FNodeType s_type;

		FString ParameterName;
		TablePtr Table;
		FString ColumnName;
		bool bNoneOption = false;
		FString DefaultRowName;

		TArray<NodeLayoutPtr> Layouts;
	};

}
