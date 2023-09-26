// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuT/NodeMeshPrivate.h"
#include "MuT/NodeMeshTable.h"
#include "MuT/NodeLayout.h"
#include "MuT/Table.h"
#include "MuT/AST.h"


namespace mu
{


	class NodeMeshTable::Private : public NodeMesh::Private
	{
	public:

		MUTABLE_DEFINE_CONST_VISITABLE()

	public:

		static NODE_TYPE s_type;

		FString ParameterName;
		TablePtr Table;
		FString ColumnName;
		bool bNoneOption = false;

		TArray<NodeLayoutPtr> Layouts;

		//!
		void Serialise( OutputArchive& arch ) const
		{
            uint32 ver = 3;
			arch << ver;

			arch << ParameterName;
			arch << Table;
			arch << ColumnName;
			arch << Layouts;
			arch << bNoneOption;
		}

		//!
		void Unserialise( InputArchive& arch )
		{
            uint32 ver;
			arch >> ver;
            check(ver>=1 && ver<=3);

			if (ver == 1)
			{
				std::string Temp;
				arch >> Temp;
				ParameterName = Temp.c_str();
			}
			else
			{
				arch >> ParameterName;
			}

			arch >> Table;

			if (ver == 1)
			{
				std::string Temp;
				arch >> Temp;
				ColumnName = Temp.c_str();
			}
			else
			{
				arch >> ColumnName;
			}
			
			arch >> Layouts;

			if(ver >= 3)
			{
				arch >> bNoneOption;
			}
		}

		// NodeMesh::Private interface
        NodeLayoutPtr GetLayout( int index ) const override;

	};

}
