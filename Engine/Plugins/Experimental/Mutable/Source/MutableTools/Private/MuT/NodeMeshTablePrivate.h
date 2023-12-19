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

		static NODE_TYPE s_type;

		FString m_parameterName;
		TablePtr m_pTable;
		FString m_columnName;

		TArray<NodeLayoutPtr> m_layouts;

		//!
		void Serialise( OutputArchive& arch ) const
		{
            uint32 ver = 2;
			arch << ver;

			arch << m_parameterName;
			arch << m_pTable;
			arch << m_columnName;
			arch << m_layouts;
		}

		//!
		void Unserialise( InputArchive& arch )
		{
            uint32 ver;
			arch >> ver;
            check(ver>=1 && ver<=2);

			if (ver == 1)
			{
				std::string Temp;
				arch >> Temp;
				m_parameterName = Temp.c_str();
			}
			else
			{
				arch >> m_parameterName;
			}

			arch >> m_pTable;

			if (ver == 1)
			{
				std::string Temp;
				arch >> Temp;
				m_columnName = Temp.c_str();
			}
			else
			{
				arch >> m_columnName;
			}
			arch >> m_layouts;
		}

		// NodeMesh::Private interface
        NodeLayoutPtr GetLayout( int index ) const override;

	};

}
