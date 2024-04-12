// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuT/NodePrivate.h"
#include "MuT/NodeColourVariation.h"


namespace mu
{

    //---------------------------------------------------------------------------------------------
    //!
    //---------------------------------------------------------------------------------------------
    class NodeColourVariation::Private : public Node::Private
    {
    public:
        Private() {}

        static FNodeType s_type;

        NodeColourPtr m_defaultColour;

        struct FVariation
        {
            NodeColourPtr m_colour;
			FString m_tag;
        };

        TArray<FVariation> m_variations;
    };

} // namespace mu

