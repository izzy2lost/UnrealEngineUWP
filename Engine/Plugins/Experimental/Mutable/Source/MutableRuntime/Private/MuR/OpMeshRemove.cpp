// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuR/OpMeshRemove.h"

#include "MuR/MeshPrivate.h"
#include "MuR/MutableTrace.h"
#include "MuR/Platform.h"


namespace mu
{

    //---------------------------------------------------------------------------------------------
    struct FIdInterval
    {
        uint64 idStart;
        int32 idPosition;
        int32 size;
    };


    void ExtractVertexIndexIntervals( TArray<FIdInterval>& intervals, const Mesh* pSource )
    {
		MeshVertexIdIteratorConst itVI( pSource );
		FIdInterval current;
		current.idStart = TNumericLimits<uint64>::Max();
		current.idPosition = 0;
		current.size = 0;
		for ( int32 sv=0; sv<pSource->GetVertexBuffers().GetElementCount(); ++sv )
        {
            uint64 id = itVI.Get();
            ++itVI;

            if (current.idStart==TNumericLimits<uint64>::Max())
            {
                current.idStart = id;
                current.idPosition = sv;
                current.size = 1;
            }
            else
            {
                if (id==current.idStart+current.size)
                {
                    ++current.size;
                }
                else
                {
                    intervals.Add(current);
                    current.idStart = id;
                    current.idPosition = sv;
                    current.size = 1;
                }
            }
        }

        if (current.idStart!=TNumericLimits<uint64>::Max())
        {
            intervals.Add(current);
        }
    }


    int64 FindPositionInIntervals( const TArray<FIdInterval>& intervals, int64 id )
    {
        for( const FIdInterval& interval: intervals )
        {
            int64 deltaId = id - interval.idStart;
            if (deltaId>=0 && deltaId<interval.size)
            {
                return interval.idPosition+deltaId;
            }
        }
        return -1;
    }


	void MeshRemoveVerticesWithMap( Mesh* Result, const uint8* RemovedVertices, uint32 RemovedVertexCount)
	{
		int32 FirstFreeVertex = 0;
		int32 RemovedIndices = 0;

        // Rebuild index buffers

        // Map from source vertex index, to new vertex index for used vertices.
        // These are indices as in the index buffer, not the absolute vertex index as in the
        // vertexbuffer MBS_VERTEXINDEX buffers.
		TArray<int32> UsedVertexMap;
		UsedVertexMap.Init( -1, Result->GetVertexCount() );
        {
            if ( Result->GetIndexBuffers().GetElementSize(0)==4 )
            {
                MeshBufferIteratorConst<MBF_UINT32,uint32,1> itSource( Result->GetIndexBuffers(), MBS_VERTEXINDEX );
                MeshBufferIterator<MBF_UINT32,uint32,1> itDest( Result->GetIndexBuffers(), MBS_VERTEXINDEX );

                int32 IndexCount = Result->GetIndexCount();
				check(IndexCount%3==0);
                for ( int32 f=0; f<IndexCount/3; ++f )
                {
                    uint32 sourceIndices[3];
                    sourceIndices[0] = (*itSource)[0];
                    sourceIndices[1] = (*itSource)[1];
                    sourceIndices[2] = (*itSource)[2];

					check(sourceIndices[0] < RemovedVertexCount);
					check(sourceIndices[1] < RemovedVertexCount);
					check(sourceIndices[2] < RemovedVertexCount);

					int32 RemoveCount =  (RemovedVertices[ sourceIndices[0] ]
                            + RemovedVertices[ sourceIndices[1] ]
                            + RemovedVertices[ sourceIndices[2] ]
                            );
					bool bFaceRemoved = RemoveCount > 0;

                    if ( !bFaceRemoved)
                    {
                        for (int32 i=0;i<3;++i)
                        {
                            uint32 sourceIndex = sourceIndices[i];

                            if (UsedVertexMap[ sourceIndex ] < 0 )
                            {
								UsedVertexMap[ sourceIndex ] = FirstFreeVertex;
                                FirstFreeVertex++;
                            }

                            uint32 destIndex = UsedVertexMap[ sourceIndex ];
                            *(uint32*)itDest.ptr() = destIndex;

                            itDest++;
                        }
                    }

                    itSource+=3;
                }

                RemovedIndices = itSource - itDest;
            }

            else if ( Result->GetIndexBuffers().GetElementSize(0)==2 )
            {
                MeshBufferIteratorConst<MBF_UINT16,uint16,1> itSource( Result->GetIndexBuffers(), MBS_VERTEXINDEX );
                MeshBufferIterator<MBF_UINT16,uint16,1> itDest( Result->GetIndexBuffers(), MBS_VERTEXINDEX );

                int32 indexCount = Result->GetIndexCount();
                for ( int32 f=0; f<indexCount/3; ++f )
                {
                    uint16 sourceIndices[3];
                    sourceIndices[0] = (*itSource)[0];
                    sourceIndices[1] = (*itSource)[1];
                    sourceIndices[2] = (*itSource)[2];

					check(sourceIndices[0] < RemovedVertexCount);
					check(sourceIndices[1] < RemovedVertexCount);
					check(sourceIndices[2] < RemovedVertexCount);

					int32 RemoveCount =  (RemovedVertices[ sourceIndices[0] ]
                            + RemovedVertices[ sourceIndices[1] ]
                            + RemovedVertices[ sourceIndices[2] ]
                            );
					bool bFaceRemoved = RemoveCount > 0;

                    if ( !bFaceRemoved)
                    {
                        for (int i=0;i<3;++i)
                        {
                            uint16 sourceIndex = sourceIndices[i];

                            if (UsedVertexMap[ sourceIndex ] < 0 )
                            {
								UsedVertexMap[ sourceIndex ] = FirstFreeVertex;
                                FirstFreeVertex++;
                            }

                            uint16 destIndex = (uint16)UsedVertexMap[ sourceIndex ];
                            *(uint16*)itDest.ptr() = destIndex;

                            itDest++;
                        }
                    }

                    itSource+=3;
                }

                RemovedIndices = itSource - itDest;
            }

            else
            {
                // Index buffer format case not implemented
                check( false );
            }

            check( RemovedIndices%3==0 );

            int32 FaceCount = Result->GetFaceCount();
            Result->GetIndexBuffers().SetElementCount( FaceCount*3-RemovedIndices );
        }


        // Rebuild the vertex buffers

		// If we had implicit indices, make them explicit or relative to keep them valid
		if (RemovedIndices && Result->AreVertexIdsImplicit())
		{
			Result->MakeVertexIndicesRelative();
		}

		// The temp array is necessary because if the vertex buffer is not sorted according to the index buffer we cannot do it in-place
		// This happens with some mesh import options.
		TArray<uint8> Temp;
		for ( int32 b=0; b<Result->GetVertexBuffers().GetBufferCount(); ++b )
        {
            int32 elemSize = Result->GetVertexBuffers().GetElementSize( b );
            const uint8* SourceData = Result->GetVertexBuffers().GetBufferData( b );

			Temp.SetNumUninitialized(FirstFreeVertex*elemSize,EAllowShrinking::No);
            uint8* DestData = Temp.GetData();

            for ( int32 v=0; v<Result->GetVertexCount(); ++v )
            {
                int32 span = 0;
                for ( int32 s=0; v+s<Result->GetVertexCount(); ++s )
                {
                    if (UsedVertexMap[v+s]>=0 )
                    {
                        if (span==0)
                        {
                            ++span;
                        }
                        else
                        {
                            if (UsedVertexMap[v+s] == UsedVertexMap[v+s-1]+1 )
                            {
                                ++span;
                            }
                            else
                            {
                                break;
                            }
                        }
                    }
                    else
                    {
                        break;
                    }
                }


                if (span>0)
                {
                    FMemory::Memcpy( DestData+elemSize*UsedVertexMap[v], SourceData+elemSize*v, elemSize*span );

                    v += span-1;
                }
            }

			// Copy from temp buffer to final vertex buffer
			FMemory::Memcpy( Result->GetVertexBuffers().GetBufferData(b), DestData, FirstFreeVertex*elemSize);
        }
        Result->GetVertexBuffers().SetElementCount( FirstFreeVertex );

        // Rebuild surface data.
        // \todo: For now make single surfaced
        Result->Surfaces.Empty();
        Result->EnsureSurfaceData();
    }


	void MeshRemoveMask(Mesh* Result, const Mesh* Source, const Mesh* Mask, bool& bOutSuccess)
    {
        MUTABLE_CPUPROFILER_SCOPE(MeshRemoveMask);
		bOutSuccess = true;

        if (!Mask->GetVertexCount() || !Source->GetVertexCount() || !Source->GetIndexCount())
        {
			bOutSuccess = false;
            return;
        }

		Result->CopyFrom(*Source);

		int32 MaskElementCount = Mask->GetVertexBuffers().GetElementCount();

        // For each source vertex, true if it is removed.
        int32 ResultVertexCount = Result->GetVertexCount();
		TArray<uint8> RemovedVertices;
		RemovedVertices.SetNumZeroed(ResultVertexCount);
        {
			TArray<FIdInterval> Intervals;
            ExtractVertexIndexIntervals(Intervals, Result);

			MeshVertexIdIteratorConst itMaskVI(Mask);
			for ( int32 mv=0; mv<MaskElementCount; ++mv )
            {
                uint64 MaskVertexId = itMaskVI.Get();
                ++itMaskVI;

                int32 IndexInSource = FindPositionInIntervals(Intervals, MaskVertexId);
                if (IndexInSource >=0)
                {
					RemovedVertices[IndexInSource] = 1;
                }
            }
        }

		MeshRemoveVerticesWithMap(Result, RemovedVertices.GetData(), (uint32)RemovedVertices.Num());
	}

}
