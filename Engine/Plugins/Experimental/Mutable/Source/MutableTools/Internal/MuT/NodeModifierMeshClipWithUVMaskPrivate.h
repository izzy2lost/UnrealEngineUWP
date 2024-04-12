// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuT/NodeModifierPrivate.h"
#include "MuT/NodeModifierMeshClipWithUVMask.h"
#include "MuT/NodeImage.h"

#include "MuR/MutableMath.h"


namespace mu
{

    class NodeModifierMeshClipWithUVMask::Private : public NodeModifier::Private
	{
	public:

		static FNodeType s_type;

		/** Image with the regions to remove. It will be interpreted as a bitmap. */
		Ptr<NodeImage> ClipMask;

		/** Layout index of the UVs to use inthe source mesh to ben clipped with the mask. */
		uint8 LayoutIndex = 0;

	};


}
