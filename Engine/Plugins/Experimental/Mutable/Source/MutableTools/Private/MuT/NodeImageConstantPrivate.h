// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuT/NodeImagePrivate.h"
#include "MuT/NodeImageConstant.h"


namespace mu
{
	class NodeImageConstant::Private : public NodeImage::Private
	{
	public:

		static FNodeType s_type;

        Ptr<ResourceProxy<Image>> m_pProxy;

	};

}
