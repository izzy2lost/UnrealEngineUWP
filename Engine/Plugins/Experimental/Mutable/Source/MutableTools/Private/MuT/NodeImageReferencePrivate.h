// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuT/NodeImagePrivate.h"
#include "MuT/NodeImageReference.h"
#include "MuT/StreamsPrivate.h"


namespace mu
{

	class NodeImageReference::Private : public NodeImage::Private
	{
	public:

		MUTABLE_DEFINE_CONST_VISITABLE();

	public:

		static NODE_TYPE s_type;

		uint32 ImageReferenceID = 0;

		FImageDesc ImageDesc;
		
		bool bForceLoad = false;

		void Serialise(OutputArchive& arch) const
		{
			uint32 Ver = 1;
			arch << Ver;

			arch << ImageReferenceID;
			arch << ImageDesc.m_size;
			arch << ImageDesc.m_lods;
			arch << ImageDesc.m_format;
			arch << bForceLoad;
		}

		void Unserialise(InputArchive& arch)
		{
			uint32 Ver;
			arch >> Ver;
			check(Ver >= 0 && Ver <= 1);

			arch >> ImageReferenceID;
			
			if (Ver >= 1)
			{
				arch >> ImageDesc.m_size;
				arch >> ImageDesc.m_lods;
				arch >> ImageDesc.m_format;
				arch >> bForceLoad;
			}
		}
	};

}
