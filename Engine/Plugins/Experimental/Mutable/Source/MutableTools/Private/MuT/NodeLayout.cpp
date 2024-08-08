// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuT/NodeLayout.h"

#include "Math/IntPoint.h"
#include "Misc/AssertionMacros.h"
#include "MuR/ConvertData.h"
#include "MuR/MeshBufferSet.h"
#include "MuR/MeshPrivate.h"
#include "MuR/MutableMath.h"


namespace mu
{

	FNodeType NodeLayout::StaticType = FNodeType(Node::EType::Layout, Node::GetStaticType() );


	Ptr<NodeLayout> NodeLayout::GenerateLayoutBlocks(const Ptr<Mesh>& pMesh, int32 layoutIndex, int32 gridSizeX, int32 gridSizeY)
	{
		Ptr<NodeLayout> LayoutNode = nullptr;

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

			LayoutNode = new NodeLayout;
			LayoutNode->Size = { uint16(gridSizeX), uint16(gridSizeY) };
			LayoutNode->MaxSize = { uint16(gridSizeX), uint16(gridSizeY) };
			LayoutNode->Strategy = EPackStrategy::Resizeable;
			
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
			
			// Check if the blocks intersect with each other
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
			
			int32 NumBlocks = blocks.Num();
			
			//Generating layout blocks
			if (NumBlocks > 0)
			{
				LayoutNode->Blocks.SetNum(NumBlocks);
			
				for (int32 BlockIndex = 0; BlockIndex < NumBlocks; ++BlockIndex)
				{
					LayoutNode->Blocks[BlockIndex].Min = UE::Math::TIntVector2<uint16>(blocks[BlockIndex].min);
					LayoutNode->Blocks[BlockIndex].Size = UE::Math::TIntVector2<uint16>(blocks[BlockIndex].size);
				}
			}
		}

		return LayoutNode;
	}

}


