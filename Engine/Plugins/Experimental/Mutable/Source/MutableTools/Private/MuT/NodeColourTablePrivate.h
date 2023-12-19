// Copyright Epic Games, Inc. All Rights Reserved.


#pragma once

#include "MuT/NodePrivate.h"
#include "MuT/NodeColourTable.h"
#include "MuT/Table.h"
#include "MuT/AST.h"


namespace mu
{


	class NodeColourTable::Private : public Node::Private
	{
	public:

		static NODE_TYPE s_type;

		FString m_parameterName;
		TablePtr m_pTable;
		FString m_columnName;

		//!
		void Serialise( OutputArchive& arch ) const
		{
            uint32 ver = 1;
			arch << ver;

			arch << m_parameterName;
			arch << m_pTable;
			arch << m_columnName;
		}

		//!
		void Unserialise( InputArchive& arch )
		{
            uint32 ver;
			arch >> ver;
			check(ver <= 1);

			if (ver == 0)
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

			if (ver == 0)
			{
				std::string Temp;
				arch >> Temp;
				m_columnName = Temp.c_str();
			}
			else
			{
				arch >> m_columnName;
			}
		}

	};

}

