// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuT/NodePrivate.h"
#include "MuT/NodeImageTable.h"
#include "MuT/Table.h"
#include "MuT/AST.h"


namespace mu
{


	class NodeImageTable::Private : public Node::Private
	{
	public:

		MUTABLE_DEFINE_CONST_VISITABLE()

	public:

		static NODE_TYPE s_type;

		FString ParameterName;
		TablePtr Table;
		FString ColumnName;
		bool bNoneOption = false;

		//!
		void Serialise( OutputArchive& arch ) const
		{
            uint32_t ver = 3;
			arch << ver;

			arch << ParameterName;
			arch << Table;
			arch << ColumnName;
			arch << bNoneOption;
		}

		//!
		void Unserialise( InputArchive& arch )
		{
            uint32_t ver;
			arch >> ver;
			check(ver>=1 && ver<= 3);

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
			
			if (ver >= 3)
			{
				arch >> bNoneOption;
			}
		}

	};

}
