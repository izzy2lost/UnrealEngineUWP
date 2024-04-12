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

        TArray<int> m_blocks;

        EBlendType m_blendType = EBlendType::BT_BLEND;

        // Patch alpha channel as well?
        bool m_applyToAlpha = false;
    };

}

