// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/Platform.h"
#include "MuR/Layout.h"
#include "MuR/Mesh.h"
#include "MuR/Ptr.h"
#include "MuR/RefCounted.h"
#include "MuT/Node.h"


namespace mu
{

	/** This node is used to define the texture layout for a texture coordinates channel of a mesh. */
	class MUTABLETOOLS_API NodeLayout : public Node
	{
	public:

		Ptr<Layout> Layout;

		int32 FirstLODToIgnoreWarnings = 0;

	public:

		NodeLayout();

		// Node Interface
		virtual const FNodeType* GetType() const override { return GetStaticType(); }
		static const FNodeType* GetStaticType() { return &StaticType; }

		// Own interface

		//! Generate the blocks of a layout using the UV of the meshes
		static Ptr<NodeLayout> GenerateLayoutBlocks(const Ptr<Mesh> pMesh, int32 layoutIndex, int32 gridSizeX, int32 gridSizeY);

	protected:

		//! Forbidden. Manage with the Ptr<> template.
		~NodeLayout() {}

	private:

		static FNodeType StaticType;

	};


}
