// Copyright Epic Games, Inc. All Rights Reserved.


#pragma once

#include "MuT/NodePrivate.h"
#include "MuT/NodePatchImage.h"
#include "MuT/NodeImage.h"


namespace mu
{

    class NodePatchImage::Private : public Node::Private
    {
    public:

        static FNodeType s_type;

        NodeImagePtr m_pImage;
        NodeImagePtr m_pMask;

		// These are the indices of the blocks in the layout being patched (not the Ids)
        TArray<int32> BlockIndices;

        EBlendType m_blendType = EBlendType::BT_BLEND;

        // Patch alpha channel as well?
        bool m_applyToAlpha = false;
    };

}

