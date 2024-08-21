// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuR/Ptr.h"
#include "MuR/RefCounted.h"
#include "MuT/Node.h"
#include "MuT/NodeModifier.h"
#include "MuT/NodeImage.h"


namespace mu
{

	/** */
	class MUTABLETOOLS_API NodeModifierMeshClipWithUVMask : public NodeModifier
	{
	public:

		/** Image with the regions to remove. It will be interpreted as a bitmap. */
		Ptr<NodeImage> ClipMask;

		/** Layout index of the UVs to use inthe source mesh to ben clipped with the mask. */
		uint8 LayoutIndex = 0;

	public:

		// Node interface
		virtual const FNodeType* GetType() const override { return &StaticType; }
		static const FNodeType* GetStaticType() { return &StaticType; }

	protected:

		//! Forbidden. Manage with the Ptr<> template.
		inline ~NodeModifierMeshClipWithUVMask() {}

	private:

		static FNodeType StaticType;

	};


}
