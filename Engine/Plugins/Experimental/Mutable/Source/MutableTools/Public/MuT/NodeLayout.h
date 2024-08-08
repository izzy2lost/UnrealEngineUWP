// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuT/Node.h"
#include "MuR/Layout.h"
#include "MuR/Image.h"
#include "MuR/Ptr.h"
#include "MuR/RefCounted.h"
#include "HAL/Platform.h"


namespace mu
{
	class Mesh;

	/** Data for a layout block before it is compiled. */
	struct FSourceLayoutBlock
	{
		/** Optional mask image that selects the vertices to include in the block. */
		Ptr<Image> Mask;

		UE::Math::TIntVector2<uint16> Min = { 0, 0 };
		UE::Math::TIntVector2<uint16> Size = { 0, 0 };

		//! Priority value to control the shrink texture layout strategy
		int32 Priority;

		//! Value to control the method to reduce the block
		uint32 bReduceBothAxes : 1;

		//! Value to control if a block has to be reduced by two in an unitary reduction strategy
		uint32 bReduceByTwo : 1;
	};


	/** This node is used to define the texture layout for a texture coordinates channel of a mesh. */
	class MUTABLETOOLS_API NodeLayout : public Node
	{
	public:

		//!
		UE::Math::TIntVector2<uint16> Size = UE::Math::TIntVector2<uint16>(0, 0);

		/** Maximum size in layout blocks that this layout can grow to. From there on, blocks will shrink to fit.
		* If 0,0 then no maximum size applies.
		*/
		UE::Math::TIntVector2<uint16> MaxSize = UE::Math::TIntVector2<uint16>(0, 0);

		//!
		TArray<FSourceLayoutBlock> Blocks;

		//! Packing strategy
		EPackStrategy Strategy = EPackStrategy::Resizeable;
		EReductionMethod ReductionMethod = EReductionMethod::Halve;

		/** When compiling, ignore generated warnings from this LOD on.
		* -1 means all warnings are generated.
		*/
		int32 FirstLODToIgnoreWarnings = 0;

	public:

		// Node Interface
		virtual const FNodeType* GetType() const override { return GetStaticType(); }
		static const FNodeType* GetStaticType() { return &StaticType; }

		// Own interface

		//! Generate the blocks of a layout using the UV of the meshes
		static Ptr<NodeLayout> GenerateLayoutBlocks(const Ptr<Mesh>& pMesh, int32 layoutIndex, int32 gridSizeX, int32 gridSizeY);

	protected:

		//! Forbidden. Manage with the Ptr<> template.
		~NodeLayout() {}

	private:

		static FNodeType StaticType;

	};


}
