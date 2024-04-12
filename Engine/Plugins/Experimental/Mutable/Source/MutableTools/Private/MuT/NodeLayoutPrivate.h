// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuT/NodePrivate.h"
#include "MuT/NodeLayout.h"
#include "MuT/NodeImage.h"


namespace mu
{


	class NodeLayout::Private : public Node::Private
	{
	public:

		virtual Layout* GetLayout() = 0;

	};


	class NodeLayoutBlocks::Private : public NodeLayout::Private
	{
	public:

		Private()
		{
			m_pLayout = new Layout();
		}

		static FNodeType s_type;

		LayoutPtr m_pLayout;

        // NodeLayout::Private interface
        Layout* GetLayout() override
        {
            return m_pLayout.get();
        }
    };

}
