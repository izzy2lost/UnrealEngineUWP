// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuT/NodePrivate.h"
#include "MuT/NodeSurfaceVariation.h"
#include "MuT/NodeSurfaceNewPrivate.h"
#include "MuT/NodeModifier.h"


namespace mu
{

	//---------------------------------------------------------------------------------------------
	//!
	//---------------------------------------------------------------------------------------------
    class NodeSurfaceVariation::Private : public NodeSurface::Private
	{
	public:

		static FNodeType s_type;

        TArray<NodeSurfacePtr> m_defaultSurfaces;
		TArray<NodeModifierPtr> m_defaultModifiers;

		struct FVariation
		{
			TArray<NodeSurfacePtr> m_surfaces;
			TArray<NodeModifierPtr> m_modifiers;
            FString m_tag;
		};

        NodeSurfaceVariation::VariationType m_type = NodeSurfaceVariation::VariationType::Tag;

		TArray<FVariation> m_variations;
	};

}
