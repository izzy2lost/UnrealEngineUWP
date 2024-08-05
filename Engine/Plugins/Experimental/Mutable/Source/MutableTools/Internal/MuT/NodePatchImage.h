// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuR/Image.h"
#include "MuR/Ptr.h"
#include "MuR/RefCounted.h"
#include "MuT/Node.h"
#include "MuT/NodeImage.h"


namespace mu
{

    // Forward definitions
    class NodeImage;
 

    /** Node that allows to modify an image from an object by blending other images on specific layout blocks. */
	class MUTABLETOOLS_API NodePatchImage : public Node
    {
	public:

		/** Image to blend. */
		Ptr<NodeImage> Image;

		/** Optional mask controlling the blending area. */
		Ptr<NodeImage> Mask;

		/** Rects in the parent layout homogeneous UV space to patch. */
		TArray<FBox2f> Blocks;

		/** */
		EBlendType BlendType = EBlendType::BT_BLEND;

		/** Patch alpha channel as well? */
		bool bApplyToAlpha = false;

    public:

		// Node Interface
		virtual const FNodeType* GetType() const override { return GetStaticType(); }
		static const FNodeType* GetStaticType() { return &StaticType; }

    protected:

        //! Forbidden. Manage with the Ptr<> template.
		~NodePatchImage() {}

	private:

		static FNodeType StaticType;

    };



}
