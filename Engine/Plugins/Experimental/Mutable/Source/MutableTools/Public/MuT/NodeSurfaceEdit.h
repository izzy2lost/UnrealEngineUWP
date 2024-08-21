// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuR/Ptr.h"
#include "MuR/RefCounted.h"
#include "MuR/ImageTypes.h"
#include "MuT/Node.h"
#include "MuT/NodeSurface.h"
#include "MuT/NodeMesh.h"
#include "MuT/NodeImage.h"
#include "MuT/NodeScalar.h"


namespace mu
{

	/** This node modifies a node of the parent object of the object that this node belongs to.
    * It allows to extend, cut and morph the parent Surface's meshes.
    * It also allows to patch the parent Surface's textures.
	*/
    class MUTABLETOOLS_API NodeSurfaceEdit : public NodeSurface
	{
	public:

		Ptr<NodeSurface> Parent;

		/** Mesh to remove from the modified surface. */
		Ptr<NodeMesh> MeshRemove;

		/** Mesh to add to the modified surface. */
		Ptr<NodeMesh> MeshAdd;

		/** Morph to apply to the modified surface. */
		Ptr<NodeMesh> MeshMorph;

		/** Factor of the morph to apply. */
		Ptr<NodeScalar> MorphFactor;

		/** Data for every modified texture. */
		struct FTexture
		{
			/** Image to add if extgending. */
			Ptr<NodeImage> Extend;

			/** Image to blend if patching. */
			Ptr<NodeImage> PatchImage;

			/** Optional mask controlling the blending area. */
			Ptr<NodeImage> PatchMask;

			/** Rects in the parent layout homogeneous UV space to patch. */
			TArray<FBox2f> PatchBlocks;

			/** */
			EBlendType PatchBlendType = EBlendType::BT_BLEND;

			/** Patch alpha channel as well? */
			bool bPatchApplyToAlpha = false;

		};

		/** Textures to modify. */
		TArray<FTexture> Textures;

		/** Tags enabled by this surface edit. */
		TArray<FString> EnableTags;

	public:

		// Node interface
		virtual const FNodeType* GetType() const override { return &StaticType; }
		static const FNodeType* GetStaticType() { return &StaticType; }

	protected:

		//! Forbidden. Manage with the Ptr<> template.
		~NodeSurfaceEdit() {}

	private:

		static FNodeType StaticType;

	};



}

