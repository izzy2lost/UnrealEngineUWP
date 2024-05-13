// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuR/MeshPrivate.h"
#include "MuR/Platform.h"


namespace mu
{


    //---------------------------------------------------------------------------------------------
    inline void MeshExtractFromVertices( const Mesh* Source,
                                         Mesh* Result,
                                         const TArray<int32>& oldToNew,
                                         const TArray<int32>& newToOld )
    {
        int32 ResultVertices = newToOld.Num();

        // Assemble the new vertex buffer
        Result->GetVertexBuffers().SetElementCount(ResultVertices);
        for ( int b=0; b<Result->GetVertexBuffers().GetBufferCount(); ++b )
        {
            const uint8* pSourceData = Source->GetVertexBuffers().GetBufferData(b);
            uint8* pDest = Result->GetVertexBuffers().GetBufferData(b);
            int32 size = Result->GetVertexBuffers().GetElementSize(b);
            for ( int32 NewIndex =0; NewIndex < ResultVertices; ++NewIndex)
            {
                int32 OldIndex = newToOld[NewIndex];
                FMemory::Memcpy( pDest, pSourceData + size*OldIndex, size );
                pDest+=size;
            }
        }

		// If the vertices are explicit or relative, the above operation will already handle them correctly
		// Otherwise, create a relative vertex ID buffer if necessary
		Result->MeshIDPrefix = Source->MeshIDPrefix;
		if (Source->AreVertexIdsImplicit() 
			&& 
			// If we extract everything, we can keep the ids implicit.
			ResultVertices!=Source->GetVertexCount())
		{
			// Add a new buffer
			int32 NewBuffer = Result->VertexBuffers.GetBufferCount();
			Result->VertexBuffers.SetBufferCount(NewBuffer + 1);
			EMeshBufferSemantic Semantic = MBS_VERTEXINDEX;
			int32 SemanticIndex = 0;
			EMeshBufferFormat Format = MBF_UINT32;
			int32 Components = 1;
			int32 Offset = 0;
			Result->VertexBuffers.SetBuffer(NewBuffer, sizeof(uint32), 1, &Semantic, &SemanticIndex, &Format, &Components, &Offset);
			uint32* pIdData = reinterpret_cast<uint32*>(Result->VertexBuffers.GetBufferData(NewBuffer));

			for (int32 NewIndex = 0; NewIndex < ResultVertices; ++NewIndex)
			{
				uint32 OldIndex = newToOld[NewIndex];
				(*pIdData++) = OldIndex;
			}
		}

        // Assemble the new index buffers
		TArray<bool> usedSourceFaces;
		usedSourceFaces.SetNumZeroed(Source->GetFaceCount());
        UntypedMeshBufferIteratorConst itIndex( Source->GetIndexBuffers(), MBS_VERTEXINDEX );
        UntypedMeshBufferIterator itResultIndex( Result->GetIndexBuffers(), MBS_VERTEXINDEX );
        int indexCount = 0;
        if ( itIndex.GetFormat()==MBF_UINT32 )
        {
            const uint32* pIndices = reinterpret_cast<const uint32_t*>( itIndex.ptr() );
            uint32* pDestIndices = reinterpret_cast<uint32_t*>( itResultIndex.ptr() );
            for ( int32 i=0; i<Source->GetIndexCount()/3; ++i )
            {
                if ( oldToNew[ pIndices[i*3+0] ]>=0
                     &&
                     oldToNew[ pIndices[i*3+1] ]>=0
                     &&
                     oldToNew[ pIndices[i*3+2] ]>=0 )
                {
                    usedSourceFaces[i] = true;

                    // Clamp in case triangles go across blocks
                    pDestIndices[ indexCount++ ] = FMath::Max( 0, oldToNew[ pIndices[i*3+0] ] );
                    pDestIndices[ indexCount++ ] = FMath::Max( 0, oldToNew[ pIndices[i*3+1] ] );
                    pDestIndices[ indexCount++ ] = FMath::Max( 0, oldToNew[ pIndices[i*3+2] ] );
                }
            }
        }
        else if ( itIndex.GetFormat()==MBF_UINT16 )
        {
            const uint16* pIndices = reinterpret_cast<const uint16*>( itIndex.ptr() );
            uint16* pDestIndices = reinterpret_cast<uint16*>( itResultIndex.ptr() );
            for ( int32 i=0; i<Source->GetIndexCount()/3; ++i )
            {
                if ( oldToNew[ pIndices[i*3+0] ]>=0
                     &&
                     oldToNew[ pIndices[i*3+1] ]>=0
                     &&
                     oldToNew[ pIndices[i*3+2] ]>=0 )
                {
                    usedSourceFaces[i] = true;

                    // Clamp in case triangles go across blocks
                    pDestIndices[ indexCount++ ] = (uint16)FMath::Max( 0, oldToNew[ pIndices[i*3+0] ] );
                    pDestIndices[ indexCount++ ] = (uint16)FMath::Max( 0, oldToNew[ pIndices[i*3+1] ] );
                    pDestIndices[ indexCount++ ] = (uint16)FMath::Max( 0, oldToNew[ pIndices[i*3+2] ] );
                }
            }
        }
        else
        {
            check( false );
        }
        Result->GetIndexBuffers().SetElementCount( indexCount );
    }


	//---------------------------------------------------------------------------------------------
    inline void MeshExtractLayoutBlock(Mesh* Result, const Mesh* Source,
                                           uint32 LayoutIndex,
                                           uint16 BlockCount,
                                           const uint64* BlockIds, bool& bOutSuccess)
	{
		check(Source);
		bOutSuccess = true;
		
		// TODO: Optimise
		Result->CopyFrom(*Source);

		UntypedMeshBufferIteratorConst itBlocks(Source->GetVertexBuffers(), MBS_LAYOUTBLOCK, LayoutIndex);

        if (itBlocks.GetFormat()!=MBF_NONE)
        {
            int32 ResultVertices = 0;
			TArray<int32> oldToNew;
			oldToNew.Init(-1,Source->GetVertexCount());
			TArray<int32> newToOld;
            newToOld.Reserve( Source->GetVertexCount() );

            if ( itBlocks.GetFormat()==MBF_UINT16 )
            {
                const uint16* pBlocks = reinterpret_cast<const uint16*>( itBlocks.ptr() );
                for ( int32 i=0; i<Source->GetVertexCount(); ++i )
                {
                    uint64 VertexBlockRelative = pBlocks[i];
					uint64 VertexBlockId = (uint64(Source->MeshIDPrefix) << 32) | VertexBlockRelative;

                    bool found = false;
                    for ( int32 j=0; j< BlockCount; ++j)
                    {
                        if (VertexBlockId == BlockIds[j])
                        {
                            found = true;
                            break;
                        }
                    }

                    if ( found )
                    {
                        oldToNew[i] = ResultVertices++;
                        newToOld.Add( i );
                    }
                }
            }
            else if (itBlocks.GetFormat() == MBF_UINT64)
			{
				const uint64* pBlocks = reinterpret_cast<const uint64*>(itBlocks.ptr());
				for (int32 i = 0; i < Source->GetVertexCount(); ++i)
				{
					uint64 VertexBlockId = pBlocks[i];

					bool found = false;
					for (int j = 0; j < BlockCount; ++j)
					{
						if (VertexBlockId == BlockIds[j])
						{
							found = true;
							break;
						}
					}

					if (found)
					{
						oldToNew[i] = ResultVertices++;
						newToOld.Add(i);
					}
				}
			}
			else

            {
                check( false );
            }

            MeshExtractFromVertices(Source, Result, oldToNew, newToOld);
        }

        Result->Surfaces.Empty();
        Result->EnsureSurfaceData();
	}

}
