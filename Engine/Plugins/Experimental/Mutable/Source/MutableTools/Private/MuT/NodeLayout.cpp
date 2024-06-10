// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuT/NodeLayout.h"

#include "Math/IntPoint.h"
#include "Math/UnrealMathSSE.h"
#include "Misc/AssertionMacros.h"
#include "MuR/ConvertData.h"
#include "MuR/MeshBufferSet.h"
#include "MuR/MeshPrivate.h"
#include "MuR/MutableMath.h"
#include "MuT/NodeLayoutPrivate.h"


namespace mu
{

	static FNodeType s_nodeLayoutType = FNodeType(Node::EType::Layout, Node::GetStaticType() );


	//---------------------------------------------------------------------------------------------
	const FNodeType* NodeLayout::GetType() const
	{
		return GetStaticType();
	}


	//---------------------------------------------------------------------------------------------
	const FNodeType* NodeLayout::GetStaticType()
	{
		return &s_nodeLayoutType;
	}


	//---------------------------------------------------------------------------------------------
	FNodeType NodeLayoutBlocks::Private::s_type = FNodeType(Node::EType::LayoutBlocks, NodeLayout::GetStaticType() );


	//---------------------------------------------------------------------------------------------
	//!
	//---------------------------------------------------------------------------------------------

	MUTABLE_IMPLEMENT_NODE( NodeLayoutBlocks )


	//---------------------------------------------------------------------------------------------
	// Own Interface
	//---------------------------------------------------------------------------------------------
	void NodeLayoutBlocks::SetGridSize( int32 x, int32 y )
	{
		m_pD->m_pLayout->SetGridSize( x, y );
	}


	//---------------------------------------------------------------------------------------------
	void NodeLayoutBlocks::SetMaxGridSize(int32 x, int32 y)
	{
		m_pD->m_pLayout->SetMaxGridSize(x, y);
	}


	//---------------------------------------------------------------------------------------------
	void NodeLayoutBlocks::GetGridSize( int32* pX, int32* pY ) const
	{
		FIntPoint grid = m_pD->m_pLayout->GetGridSize();
		if (pX) *pX = grid[0];
		if (pY) *pY = grid[1];
	}


	//---------------------------------------------------------------------------------------------
	void NodeLayoutBlocks::GetMaxGridSize(int32* pX, int32* pY) const
	{
		m_pD->m_pLayout->GetMaxGridSize(pX, pY);
	}


	//---------------------------------------------------------------------------------------------
	void NodeLayoutBlocks::SetBlockCount( int32 n )
	{
		m_pD->m_pLayout->SetBlockCount( n );
	}


	//---------------------------------------------------------------------------------------------
	int32 NodeLayoutBlocks::GetBlockCount()
	{
		return m_pD->m_pLayout->GetBlockCount();
	}


	//---------------------------------------------------------------------------------------------
	Ptr<const Layout> NodeLayoutBlocks::GetLayout() const
	{
		return m_pD->m_pLayout;
	}


	//---------------------------------------------------------------------------------------------
    void NodeLayoutBlocks::SetBlock( int32 index, int32 minx, int32 miny, int32 sizex, int32 sizey )
	{
		m_pD->m_pLayout->Blocks[index].Min = FImageSize( minx, miny );
		m_pD->m_pLayout->Blocks[index].Size = FImageSize( sizex, sizey );
	}


	//---------------------------------------------------------------------------------------------
	void NodeLayoutBlocks::SetBlockOptions(int32 index, int32 priority, bool bReduceBothAxes, bool bReduceByTwo)
	{
		m_pD->m_pLayout->Blocks[index].Priority = priority;
		m_pD->m_pLayout->Blocks[index].bReduceBothAxes = bReduceBothAxes;
		m_pD->m_pLayout->Blocks[index].bReduceByTwo = bReduceByTwo;
	}


	//---------------------------------------------------------------------------------------------
	void NodeLayoutBlocks::SetLayoutPackingStrategy(mu::EPackStrategy strategy)
	{
		m_pD->m_pLayout->SetLayoutPackingStrategy(strategy);
	}


	//---------------------------------------------------------------------------------------------
	NodeLayoutBlocksPtr NodeLayoutBlocks::GenerateLayoutBlocks(const MeshPtr pMesh, int32 layoutIndex, int32 gridSizeX, int32 gridSizeY)
	{
		NodeLayoutBlocksPtr layout = nullptr;

		if (pMesh && layoutIndex >=0 && gridSizeX+gridSizeY>0)
		{
			int32 indexCount = pMesh->GetIndexCount();
			TArray< FVector2f > UVs;
			UVs.SetNumUninitialized(indexCount * 2);

			UntypedMeshBufferIteratorConst indexIt(pMesh->GetIndexBuffers(), MBS_VERTEXINDEX, 0);
			UntypedMeshBufferIteratorConst texIt(pMesh->GetVertexBuffers(), MBS_TEXCOORDS, layoutIndex);
			
			//Getting UVs face by face
			for (int32 v = 0; v < indexCount/3; ++v)
			{
				uint32_t i_1 = indexIt.GetAsUINT32(); 
				indexIt++;
				uint32_t i_2 = indexIt.GetAsUINT32();
				indexIt++;
				uint32_t i_3 = indexIt.GetAsUINT32();
				indexIt++;

				float uv_1[2] = { 0.0f,0.0f };
				ConvertData(0, uv_1, MBF_FLOAT32, (texIt + i_1).ptr(), texIt.GetFormat());
				ConvertData(1, uv_1, MBF_FLOAT32, (texIt + i_1).ptr(), texIt.GetFormat());
				
				float uv_2[2] = { 0.0f,0.0f };
				ConvertData(0, uv_2, MBF_FLOAT32, (texIt + i_2).ptr(), texIt.GetFormat());
				ConvertData(1, uv_2, MBF_FLOAT32, (texIt + i_2).ptr(), texIt.GetFormat());

				float uv_3[2] = { 0.0f,0.0f };
				ConvertData(0, uv_3, MBF_FLOAT32, (texIt + i_3).ptr(), texIt.GetFormat());
				ConvertData(1, uv_3, MBF_FLOAT32, (texIt + i_3).ptr(), texIt.GetFormat());

				
				UVs[v * 6 + 0][0] = uv_1[0];
				UVs[v * 6 + 0][1] = uv_1[1];
				UVs[v * 6 + 1][0] = uv_2[0];
				UVs[v * 6 + 1][1] = uv_2[1];
						
				UVs[v * 6 + 2][0] = uv_2[0];
				UVs[v * 6 + 2][1] = uv_2[1];
				UVs[v * 6 + 3][0] = uv_3[0];
				UVs[v * 6 + 3][1] = uv_3[1];
						
				UVs[v * 6 + 4][0] = uv_3[0]; 
				UVs[v * 6 + 4][1] = uv_3[1]; 
				UVs[v * 6 + 5][0] = uv_1[0];
				UVs[v * 6 + 5][1] = uv_1[1];
			}

			layout = new NodeLayoutBlocks;
			layout->SetGridSize(gridSizeX, gridSizeY);
			layout->SetMaxGridSize(gridSizeX, gridSizeY);
			layout->SetLayoutPackingStrategy(EPackStrategy::RESIZABLE_LAYOUT);
			
			TArray<box<FIntVector2>> blocks;
			
			//Generating blocks
			for (int32 i = 0; i < indexCount; ++i)
			{
				FIntVector2 a, b;
			
				a[0] = (int32)floor(UVs[i * 2][0] * gridSizeX);
				a[1] = (int32)floor(UVs[i * 2][1] * gridSizeY);
									  
				b[0] = (int32)floor(UVs[i * 2 +1][0] * gridSizeX);
				b[1] = (int32)floor(UVs[i * 2 +1][1] * gridSizeY);

				//floor of UV = 1*gridSize is gridSize which is not a valid range
				if (a[0] == gridSizeX){ a[0] = gridSizeX-1; }
				if (a[1] == gridSizeY){	a[1] = gridSizeY-1;	}
				if (b[0] == gridSizeX){	b[0] = gridSizeX-1;	}
				if (b[1] == gridSizeY){ b[1] = gridSizeY-1;	}
			
				//a and b are in the same block
				if (a == b)
				{
					bool contains = false;
			
					for (int32 it = 0; it < blocks.Num(); ++it)
					{
						if (blocks[it].Contains(a) || blocks[it].Contains(b))
						{
							contains = true;
						}
					}
					
					//There is no block that contains them 
					if (!contains)
					{
						box<FIntVector2> currBlock;
						currBlock.min = a;
						currBlock.size = FIntVector2(1, 1);
			
						blocks.Add(currBlock);
					}
				}
				else //they are in different blocks
				{
					int32 idxA = -1;
					int32 idxB = -1;
					
					//Getting the blocks that contain them
					for (int32 it = 0; it < blocks.Num(); ++it)
					{
						if (blocks[it].Contains(a))
						{
							idxA = (int32)it;
						}
						if (blocks[it].Contains(b))
						{
							idxB = (int32)it;
						}
					}
					
					//The blocks are not the same
					if (idxA != idxB)
					{
						box<FIntVector2> currBlock;
						
						//One of the blocks doesn't exist
						if (idxA != -1 && idxB == -1)
						{
							currBlock.min = b;
							currBlock.size = FIntVector2(1, 1);
							blocks[idxA].Bound(currBlock);
						}
						else if (idxB != -1 && idxA == -1)
						{
							currBlock.min = a;
							currBlock.size = FIntVector2(1, 1);
							blocks[idxB].Bound(currBlock);
						}
						else //Both exist
						{
							blocks[idxA].Bound(blocks[idxB]);
							blocks.RemoveAt(idxB);
						}
					}
					else //the blocks doesn't exist
					{
						if (idxA == -1)
						{
							box<FIntVector2> currBlockA;
							box<FIntVector2> currBlockB;
			
							currBlockA.min = a;
							currBlockB.min = b;
							currBlockA.size = FIntVector2(1, 1);
							currBlockB.size = FIntVector2(1, 1);
			
							currBlockA.Bound(currBlockB);
							blocks.Add(currBlockA);
						}
					}
				}
			}
			
			bool intersections = true;
			
			//Cheking if the blocks intersect with each other
			while (intersections)
			{
				intersections = false;
			
				for (int32 i = 0; !intersections && i < blocks.Num(); ++i)
				{
					for (int32 j = 0; j < blocks.Num(); ++j)
					{
						if (i != j && blocks[i].IntersectsExclusive(blocks[j]))
						{
							blocks[i].Bound(blocks[j]);
							blocks.RemoveAt(j);
							intersections = true;
							break;
						}
					}
				}
			}
			
			int32 numBlocks = blocks.Num();
			
			//Generating layout blocks
			if (numBlocks > 0)
			{
				layout->SetBlockCount(numBlocks);
			
				for (int32 i = 0; i < numBlocks; ++i)
				{
					int32 blockIndex = i;
					int32 minX = blocks[i].min[0];
					int32 minY = blocks[i].min[1];
					int32 sizeX = blocks[i].size[0];
					int32 sizeY = blocks[i].size[1];
			
					layout->SetBlock(blockIndex, minX, minY, sizeX, sizeY);
				}
			}
		}

		return layout;
	}

	//---------------------------------------------------------------------------------------------
	void NodeLayoutBlocks::SetIgnoreWarningsLOD(int32 LOD)
	{
		m_pD->m_pLayout->SetIgnoreLODWarnings(LOD);
	}


	//---------------------------------------------------------------------------------------------
	int32 NodeLayoutBlocks::GetIgnoreWarningsLOD()
	{
		return m_pD->m_pLayout->GetIgnoreLODWarnings();
	}


	//---------------------------------------------------------------------------------------------
	void NodeLayoutBlocks::SetBlockReductionMethod(EReductionMethod Method)
	{
		m_pD->m_pLayout->SetBlockReductionMethod(Method);
	}


	//---------------------------------------------------------------------------------------------
	EReductionMethod NodeLayoutBlocks::GetBlockReductionMethod()
	{
		return m_pD->m_pLayout->GetBlockReductionMethod();
	}
}


