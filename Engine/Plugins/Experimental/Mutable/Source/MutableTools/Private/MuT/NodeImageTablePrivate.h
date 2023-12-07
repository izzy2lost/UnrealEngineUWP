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

		FString m_parameterName;
		TablePtr m_pTable;
		FString m_columnName;
		uint16 MaxTextureSize = 0;
		FImageDesc ReferenceImageDesc;

		//!
		void Serialise( OutputArchive& arch ) const
		{
            uint32_t ver = 4;
			arch << ver;

			arch << m_parameterName;
			arch << m_pTable;
			arch << m_columnName;
			arch << MaxTextureSize;
			
			// FImageDesc
			{
				arch << ReferenceImageDesc.m_size;
				arch << uint8(ReferenceImageDesc.m_format);
				arch << ReferenceImageDesc.m_lods;
			}
		}

		//!
		void Unserialise( InputArchive& arch )
		{
            uint32_t ver;
			arch >> ver;
			check(ver>=1 && ver<= 4);

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

			if (ver >= 3)
			{
				arch >> MaxTextureSize;
			}

			if (ver >= 4)
			{
				arch >> ReferenceImageDesc.m_size;
				
				uint8 AuxFromat = 0;
				arch >> AuxFromat;
				ReferenceImageDesc.m_format = EImageFormat(AuxFromat);
				
				arch >> ReferenceImageDesc.m_lods;
			}
		}
	};

}
