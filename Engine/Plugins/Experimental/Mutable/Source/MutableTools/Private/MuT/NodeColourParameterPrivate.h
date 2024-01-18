// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuT/NodeColourParameter.h"
#include "MuT/NodeColourPrivate.h"

#include "MuT/NodeRange.h"
#include "MuR/MutableMath.h"


namespace mu
{

	class NodeColourParameter::Private : public NodeColour::Private
	{
	public:

		static FNodeType s_type;

		vec3<float> m_defaultValue;
		FString m_name;
		FString m_uid;

        TArray<Ptr<NodeRange>> m_ranges;

		//!
		void Serialise( OutputArchive& arch ) const
		{
            uint32_t ver = 3;
			arch << ver;

			arch << m_defaultValue;
			arch << m_name;
            arch << m_uid;
            arch << m_ranges;
        }

		//!
		void Unserialise( InputArchive& arch )
		{
            uint32_t ver;
			arch >> ver;
            check(ver>=2&&ver<=3);

			arch >> m_defaultValue;
			
			if (ver <= 2)
			{
				std::string Temp;
				arch >> Temp;
				m_name = Temp.c_str();
				arch >> Temp;
				m_uid = Temp.c_str();
			}
			else
			{
				arch >> m_name;
				arch >> m_uid;
			}
			
			arch >> m_ranges;
        }
	};

}
