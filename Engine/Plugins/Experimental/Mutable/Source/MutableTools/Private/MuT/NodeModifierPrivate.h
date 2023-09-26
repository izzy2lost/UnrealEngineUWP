// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuT/NodePrivate.h"
#include "MuT/NodeModifier.h"


namespace mu
{

	class NodeModifier::Private : public Node::Private
	{
	public:

		// Tags that target surface need to have enabled to receive this modifier.
		TArray<FString> m_tags;

		// Wether the modifier has to be applied after the normal node operations or before
		bool m_applyBeforeNormalOperations = true;

		//!
		void Serialise(OutputArchive& arch) const
		{
            uint32_t ver = 3;
			arch << ver;

			arch << m_tags;
			arch << m_applyBeforeNormalOperations;
		}

		//!
		void Unserialise(InputArchive& arch)
		{
            uint32_t ver;
			arch >> ver;
			check(ver>=2 && ver<=3);

			if (ver <= 2)
			{
				TArray<std::string> Temp;
				arch >> Temp;
				m_tags.SetNum(Temp.Num());
				for( int32 i=0; i<Temp.Num(); ++i)
				{
					m_tags[i] = Temp[i].c_str();
				}
			}
			else
			{
				arch >> m_tags;
			}
			arch >> m_applyBeforeNormalOperations;
		}

	};

}
