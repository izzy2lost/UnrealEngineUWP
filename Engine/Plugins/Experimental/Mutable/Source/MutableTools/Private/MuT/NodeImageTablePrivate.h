// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuT/NodePrivate.h"
#include "MuT/NodeImageTable.h"
#include "MuT/TablePrivate.h"
#include "MuT/AST.h"


namespace mu
{


	class NodeImageTable::Private : public Node::Private
	{
	public:

		static NODE_TYPE s_type;

		FString ParameterName;
		TablePtr Table;
		FString ColumnName;
		uint16 MaxTextureSize = 0;
		FImageDesc ReferenceImageDesc;
		bool bNoneOption = false;

		//!
		void Serialise( OutputArchive& arch ) const
		{
            uint32_t ver = 5;
			arch << ver;

			arch << ParameterName;
			arch << Table;
			arch << ColumnName;
			arch << MaxTextureSize;

			// FImageDesc
			{
				arch << ReferenceImageDesc.m_size;
				arch << uint8(ReferenceImageDesc.m_format);
				arch << ReferenceImageDesc.m_lods;
			}

			arch << bNoneOption;
		}

		//!
		void Unserialise( InputArchive& arch )
		{
            uint32_t ver;
			arch >> ver;
			check(ver>=1 && ver<= 5);

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
				arch >> MaxTextureSize;
			}
			else
			{
				bNoneOption = Table->GetPrivate()->bNoneOption_DEPRECATED;
			}

			if (ver >= 4)
			{
				arch >> ReferenceImageDesc.m_size;
				
				uint8 AuxFromat = 0;
				arch >> AuxFromat;
				ReferenceImageDesc.m_format = EImageFormat(AuxFromat);
				
				arch >> ReferenceImageDesc.m_lods;
			}
			
			if (ver >= 5)
			{
				arch >> bNoneOption;
			}
		}
	};

}
