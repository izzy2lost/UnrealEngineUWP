// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuR/Ptr.h"
#include "MuR/RefCounted.h"
#include "MuT/Node.h"
#include "MuT/NodeSurface.h"
#include "MuT/NodeMesh.h"
#include "MuT/NodePatchMesh.h"
#include "MuT/NodeImage.h"
#include "MuT/NodeScalar.h"
#include "MuT/NodePatchImage.h"


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
		Ptr<NodePatchMesh> Mesh;
		Ptr<NodeMesh> Morph;
		Ptr<NodeScalar> MorphFactor;

		struct FTexture
		{
			Ptr<NodeImage> Extend;
			Ptr<NodePatchImage> Patch;
		};

		TArray<FTexture> Textures;

		//! Tags in this surface edit
		TArray<FString> Tags;

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

