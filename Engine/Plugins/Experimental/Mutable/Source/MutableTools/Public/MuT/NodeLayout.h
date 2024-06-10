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

	//
	class NodeLayout;
	typedef Ptr<NodeLayout> NodeLayoutPtr;
	typedef Ptr<const NodeLayout> NodeLayoutPtrConst;

	//
	class NodeLayoutBlocks;
	typedef Ptr<NodeLayoutBlocks> NodeLayoutBlocksPtr;
	typedef Ptr<const NodeLayoutBlocks> NodeLayoutBlocksPtrConst;


	//! This node is used to define the texture layout for a texture coordinates channel of a
	//! constant mesh.
	class MUTABLETOOLS_API NodeLayout : public Node
	{
	public:

		//-----------------------------------------------------------------------------------------
		// Node Interface
		//-----------------------------------------------------------------------------------------

		const FNodeType* GetType() const override;
		static const FNodeType* GetStaticType();


		//-----------------------------------------------------------------------------------------
		// Interface pattern
		//-----------------------------------------------------------------------------------------
		class Private;

	protected:

		//! Forbidden. Manage with the Ptr<> template.
		~NodeLayout() {}

	};


	//! This node is used to define the texture layout for a texture coordinates channel of a
	//! constant mesh.
	//! The blocks defined here will be set to the channel of the mesh were this node is connected.
	class MUTABLETOOLS_API NodeLayoutBlocks : public NodeLayout
	{
	public:

		NodeLayoutBlocks();

		//-----------------------------------------------------------------------------------------
		// Node Interface
		//-----------------------------------------------------------------------------------------

		const FNodeType* GetType() const override;
		static const FNodeType* GetStaticType();

		//-----------------------------------------------------------------------------------------
		// Own Interface
		//-----------------------------------------------------------------------------------------

		//! Set the size of the grid where the blocks will be defined
		void SetGridSize( int32 x, int32 y );

		//! Set the maximum size of the grid where the blocks can be defined
		void SetMaxGridSize(int32 x, int32 y);

		//! Get the size of the grid where the blocks will be defined
		void GetGridSize( int32* pX, int32* pY ) const;

		//! Get the maximum size of the grid where the blocks can be defined
		void GetMaxGridSize(int32* pX, int32* pY) const;

		//! Set the number of blocks in the layout.
		//! It keeps the current data as much as possible, and the new data is undefined.
		void SetBlockCount( int32 );

		//! Get the number of blocks in the layout
		int32 GetBlockCount();

		/** */
		Ptr<const Layout> GetLayout() const;

		//! Set a block of the layout.
		//! minx and miny refer to the lowest left corner of the block.
        void SetBlock( int32 index, int32 minx, int32 miny, int32 sizex, int32 sizey );

		//! Set reduction block options like priority or if the block has to be reduced symmetrically.
		void SetBlockOptions(int32 index, int32 priority, bool bReduceBothAxes, bool bReduceByTwo);

		//! Set the texture layout packing strategy 
		void SetLayoutPackingStrategy(EPackStrategy strategy);

		//! Generate the blocks of a layout using the UV of the meshes
		static NodeLayoutBlocksPtr GenerateLayoutBlocks(const MeshPtr pMesh, int32 layoutIndex, int32 gridSizeX, int32 gridSizeY);

		//! Set at which LOD the unassigned vertices warnings will star to be ignored
		void SetIgnoreWarningsLOD(int32 LOD);

		//! Get the LOD where the unassigned vertices warnings starts to be ignored
		int32 GetIgnoreWarningsLOD();

		//! Set the block reduction method a the Fixed_Layout strategy
		void SetBlockReductionMethod(EReductionMethod strategy);

		//! Returns the block reduction method
		EReductionMethod GetBlockReductionMethod();


		//-----------------------------------------------------------------------------------------
		// Interface pattern
		//-----------------------------------------------------------------------------------------
		class Private;
		Private* GetPrivate() const;

	protected:

		//! Forbidden. Manage with the Ptr<> template.
		~NodeLayoutBlocks();

	private:

		Private* m_pD;

	};



}
