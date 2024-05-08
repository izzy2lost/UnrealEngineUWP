// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuR/OpMeshFormat.h"

#include "Containers/Array.h"
#include "Containers/Map.h"
#include "HAL/PlatformCrt.h"
#include "HAL/PlatformMath.h"
#include "HAL/UnrealMemory.h"
#include "Misc/AssertionMacros.h"
#include "MuR/ConvertData.h"
#include "MuR/Layout.h"
#include "MuR/MeshBufferSet.h"
#include "MuR/MeshPrivate.h"
#include "MuR/MutableMath.h"
#include "MuR/MutableTrace.h"
#include "MuR/Ptr.h"
#include "MuR/RefCounted.h"
#include "MuR/MutableRuntimeModule.h"

#include "GPUSkinPublicDefs.h"

namespace mu
{

	//-------------------------------------------------------------------------------------------------
	void MeshFormatBuffer( const FMeshBufferSet& Source, FMeshBufferSet& Result, int32 bufferIndex )
	{
		int vCount = Source.GetElementCount();

		int b = bufferIndex;
		{
			// For every channel in this buffer
			for (int c = 0; c < Result.GetBufferChannelCount(b); ++c)
			{
				// Find this channel in the source mesh
				EMeshBufferSemantic resultSemantic;
				int resultSemanticIndex;
				EMeshBufferFormat resultFormat;
				int resultComponents;
				int resultOffset;
				Result.GetChannel
				(
					b, c,
					&resultSemantic, &resultSemanticIndex,
					&resultFormat, &resultComponents,
					&resultOffset
				);

				int sourceBuffer;
				int sourceChannel;
				Source.FindChannel
				(resultSemantic, resultSemanticIndex, &sourceBuffer, &sourceChannel);

				int resultElemSize = Result.GetElementSize(b);
				int resultChannelSize = GetMeshFormatData(resultFormat).SizeInBytes * resultComponents;
				uint8_t* pResultBuf = Result.GetBufferData(b);
				pResultBuf += resultOffset;

				if (sourceBuffer < 0)
				{
					// Not found: fill with zeros.

					// Special case for derived channel data
					bool generated = false;

					// If we have to add colour channels, we will add them as white, to be neutral.
					// \todo: normal channels also should have special values.
					if (resultSemantic == MBS_COLOUR)
					{
						generated = true;

						switch (resultFormat)
						{
						case MBF_FLOAT32:
						{
							for (int v = 0; v < vCount; ++v)
							{
								float* pTypedResultBuf = (float*)pResultBuf;
								for (int i = 0; i < resultComponents; ++i)
								{
									pTypedResultBuf[i] = 1.0f;
								}
								pResultBuf += resultElemSize;
							}
							break;
						}

						case MBF_NUINT8:
						{
							for (int v = 0; v < vCount; ++v)
							{
								uint8_t* pTypedResultBuf = (uint8_t*)pResultBuf;
								for (int i = 0; i < resultComponents; ++i)
								{
									pTypedResultBuf[i] = 255;
								}
								pResultBuf += resultElemSize;
							}
							break;
						}

						case MBF_NUINT16:
						{
							for (int v = 0; v < vCount; ++v)
							{
								uint16* pTypedResultBuf = (uint16*)pResultBuf;
								for (int i = 0; i < resultComponents; ++i)
								{
									pTypedResultBuf[i] = 65535;
								}
								pResultBuf += resultElemSize;
							}
							break;
						}

						default:
							// Format not implemented
							check(false);
							break;
						}
					}

					if (!generated)
					{
						// TODO: and maybe raise a warning?
						for (int v = 0; v < vCount; ++v)
						{
							FMemory::Memzero(pResultBuf, resultChannelSize);
							pResultBuf += resultElemSize;
						}
					}
				}
				else
				{
					// Get the data about the source format
					EMeshBufferSemantic sourceSemantic;
					int sourceSemanticIndex;
					EMeshBufferFormat sourceFormat;
					int sourceComponents;
					int sourceOffset;
					Source.GetChannel
					(
						sourceBuffer, sourceChannel,
						&sourceSemantic, &sourceSemanticIndex,
						&sourceFormat, &sourceComponents,
						&sourceOffset
					);
					check(sourceSemantic == resultSemantic
						&&
						sourceSemanticIndex == resultSemanticIndex);

					int32 sourceElemSize = Source.GetElementSize(sourceBuffer);
					const uint8* pSourceBuf = Source.GetBufferData(sourceBuffer);
					pSourceBuf += sourceOffset;

					// Copy element by element
					for (int v = 0; v < vCount; ++v)
					{
						if (resultFormat == sourceFormat && resultComponents == sourceComponents)
						{
							FMemory::Memcpy(pResultBuf, pSourceBuf, resultChannelSize);
						}
						else if (resultFormat == MBF_PACKEDDIR8_W_TANGENTSIGN
							||
							resultFormat == MBF_PACKEDDIRS8_W_TANGENTSIGN)
						{
							check(sourceComponents >= 3);
							check(resultComponents == 4);

							// convert the 3 first components
							for (int i = 0; i < 3; ++i)
							{
								if (i < sourceComponents)
								{
									ConvertData
									(
										i,
										pResultBuf, resultFormat,
										pSourceBuf, sourceFormat
									);
								}
							}


							// Add the tangent sign
							uint8* pData = reinterpret_cast<uint8*>(pResultBuf);

							// Look for the full tangent space
							int32 tanXBuf, tanXChan, tanYBuf, tanYChan, tanZBuf, tanZChan;
							Source.FindChannel(MBS_TANGENT, resultSemanticIndex, &tanXBuf, &tanXChan);
							Source.FindChannel(MBS_BINORMAL, resultSemanticIndex, &tanYBuf, &tanYChan);
							Source.FindChannel(MBS_NORMAL, resultSemanticIndex, &tanZBuf, &tanZChan);

							if (tanXBuf >= 0 && tanYBuf >= 0 && tanZBuf >= 0)
							{
								UntypedMeshBufferIteratorConst xIt(Source, MBS_TANGENT, resultSemanticIndex);
								UntypedMeshBufferIteratorConst yIt(Source, MBS_BINORMAL, resultSemanticIndex);
								UntypedMeshBufferIteratorConst zIt(Source, MBS_NORMAL, resultSemanticIndex);

								xIt += v;
								yIt += v;
								zIt += v;

								FMatrix44f Mat(xIt.GetAsVec3f(), yIt.GetAsVec3f(), zIt.GetAsVec3f(), FVector3f(0, 0, 0));

								uint8 sign = 0;
								if (resultFormat == MBF_PACKEDDIR8_W_TANGENTSIGN)
								{
									sign = Mat.RotDeterminant() < 0 ? 0 : 255;
								}
								else if (resultFormat == MBF_PACKEDDIRS8_W_TANGENTSIGN)
								{
									sign = Mat.RotDeterminant() < 0 ? -128 : 127;
								}
								pData[3] = sign;
							}
							else
							{
								// At least initialize it to avoid garbage.
								pData[3] = 0;
							}
						}
						else
						{
							// Convert formats
							for (int i = 0; i < resultComponents; ++i)
							{
								if (i < sourceComponents)
								{
									ConvertData
									(
										i,
										pResultBuf, resultFormat,
										pSourceBuf, sourceFormat
									);
								}
								else
								{
									// Add zeros. TODO: Warning?
									FMemory::Memzero
									(
										pResultBuf + GetMeshFormatData(resultFormat).SizeInBytes * i,
										GetMeshFormatData(resultFormat).SizeInBytes
									);
								}
							}


							// Extra step to normalise some semantics in some formats
							// TODO: Make it optional, and add different normalisation types n, n^2
							// TODO: Optimise
							if (sourceSemantic == MBS_BONEWEIGHTS)
							{
								if (resultFormat == MBF_NUINT8)
								{
									uint8_t* pData = (uint8_t*)pResultBuf;
									uint8_t accum = 0;
									for (int i = 0; i < resultComponents; ++i)
									{
										accum += pData[i];
									}
									pData[0] += 255 - accum;
								}

								else if (resultFormat == MBF_NUINT16)
								{
									uint16* pData = (uint16*)pResultBuf;
									uint16 accum = 0;
									for (int i = 0; i < resultComponents; ++i)
									{
										accum += pData[i];
									}
									pData[0] += 65535 - accum;
								}
							}
						}

						pResultBuf += resultElemSize;
						pSourceBuf += sourceElemSize;
					}
				}
			}
		}
	}


	//-------------------------------------------------------------------------------------------------
	void FormatBufferSet
	(
		const FMeshBufferSet& Source,
		FMeshBufferSet& Result,
		bool bKeepSystemBuffers,
		bool bIgnoreMissingChannels,
		bool bIsVertexBuffer
	)
	{
		if (bIgnoreMissingChannels)
		{
			// Remove from the result the channels that are not present in the source, and re-pack the
			// offsets.
			for (int b = 0; b < Result.GetBufferCount(); ++b)
			{
				TArray<EMeshBufferSemantic> resultSemantics;
				TArray<int> resultSemanticIndexs;
				TArray<EMeshBufferFormat> resultFormats;
				TArray<int> resultComponentss;
				TArray<int> resultOffsets;
				int offset = 0;

				// For every channel in this buffer
				for (int c = 0; c < Result.GetBufferChannelCount(b); ++c)
				{
					EMeshBufferSemantic resultSemantic;
					int resultSemanticIndex;
					EMeshBufferFormat resultFormat;
					int resultComponents;

					// Find this channel in the source mesh
					Result.GetChannel
					(
						b, c,
						&resultSemantic, &resultSemanticIndex,
						&resultFormat, &resultComponents,
						nullptr
					);

					int sourceBuffer;
					int sourceChannel;
					Source.FindChannel
					(resultSemantic, resultSemanticIndex, &sourceBuffer, &sourceChannel);

					if (sourceBuffer >= 0)
					{
						resultSemantics.Add(resultSemantic);
						resultSemanticIndexs.Add(resultSemanticIndex);
						resultFormats.Add(resultFormat);
						resultComponentss.Add(resultComponents);
						resultOffsets.Add(offset);

						offset += GetMeshFormatData(resultFormat).SizeInBytes * resultComponents;
					}
				}

				if (resultSemantics.IsEmpty())
				{
					Result.SetBuffer(b, 0, 0, nullptr, nullptr, nullptr, nullptr, nullptr);
				}
				else
				{
					Result.SetBuffer(b, offset, resultSemantics.Num(),
						&resultSemantics[0],
						&resultSemanticIndexs[0],
						&resultFormats[0],
						&resultComponentss[0],
						&resultOffsets[0]);
				}
			}
		}


		// For every vertex buffer in result
		int32 vCount = Source.GetElementCount();
		Result.SetElementCount(vCount);
		for (int32 b = 0; b < Result.GetBufferCount(); ++b)
		{
			MeshFormatBuffer(Source, Result, b);
		}


		// Detect internal system buffers and clone them unmodified.
		if (bKeepSystemBuffers)
		{
			for (int32 b = 0; b < Source.GetBufferCount(); ++b)
			{
				bool bIsSystemBuffer = false;

				// Detect system buffers and clone them unmodified.
				if (Source.GetBufferChannelCount(b) == 1)
				{
					EMeshBufferSemantic sourceSemantic;
					int32 sourceSemanticIndex;
					EMeshBufferFormat sourceFormat;
					int32 sourceComponents;
					int32 sourceOffset;
					Source.GetChannel
					(
						b, 0,
						&sourceSemantic, &sourceSemanticIndex,
						&sourceFormat, &sourceComponents,
						&sourceOffset
					);

					if (sourceSemantic == MBS_LAYOUTBLOCK
						||
						(bIsVertexBuffer && sourceSemantic == MBS_VERTEXINDEX))
					{
						bIsSystemBuffer = true;
					}
				}

				if (bIsSystemBuffer)
				{
					Result.AddBuffer(Source, b);
				}
			}
		}

	}



	//-------------------------------------------------------------------------------------------------
	void MeshFormat
	(
		Mesh* Result, 
		const Mesh* PureSource,
		const Mesh* Format,
		bool keepSystemBuffers,
		bool formatVertices,
		bool formatIndices,
		bool ignoreMissingChannels,
		bool& bOutSuccess
	)
	{
		MUTABLE_CPUPROFILER_SCOPE(MeshFormat);
		bOutSuccess = true;

		if (!PureSource) 
		{
			check(false);
			bOutSuccess = false;	
			return;
		}

		if (!Format)
		{
			check(false);
			bOutSuccess = false;
			return;
		}

		Ptr<const Mesh> Source = PureSource;

		Result->CopyFrom(*Format);
		Result->MeshIDPrefix = Source->MeshIDPrefix;

		// Make sure that the bone indices will fit in this format, or extend it.
		if (formatVertices)
		{
			const FMeshBufferSet& VertexBuffers = Source->GetVertexBuffers();

			const int32 BufferCount = VertexBuffers.GetBufferCount();
			for (int32 BufferIndex = 0; BufferIndex < BufferCount; ++BufferIndex)
			{
				const int32 ChannelCount = VertexBuffers.GetBufferChannelCount(BufferIndex);
				for (int32 ChannelIndex = 0; ChannelIndex < ChannelCount; ++ChannelIndex)
				{
					const FMeshBufferChannel& Channel = VertexBuffers.m_buffers[BufferIndex].m_channels[ChannelIndex];

					if (Channel.m_semantic == MBS_BONEINDICES)
					{
						int32 resultBuf = 0;
						int32 resultChan = 0;
						FMeshBufferSet& formBuffs = Result->GetVertexBuffers();
						formBuffs.FindChannel(MBS_BONEINDICES, Channel.m_semanticIndex, &resultBuf, &resultChan);
						if (resultBuf >= 0)
						{
							UntypedMeshBufferIteratorConst it(VertexBuffers, MBS_BONEINDICES, Channel.m_semanticIndex);
							int32_t maxBoneIndex = 0;
							for (int v = 0; v < VertexBuffers.GetElementCount(); ++v)
							{
								// If MAX_TOTAL_INFLUENCES ever changed, the next line would no longer work or compile and 
								// GetAsVec12i would need to be changed accordingly
								int32 va[MAX_TOTAL_INFLUENCES];
								it.GetAsInt32Vec(va, MAX_TOTAL_INFLUENCES);
								for (int c = 0; c < it.GetComponents(); ++c)
								{
									maxBoneIndex = FMath::Max(maxBoneIndex, va[c]);
								}
								++it;
							}

							EMeshBufferFormat& format = formBuffs.m_buffers[resultBuf].m_channels[resultChan].m_format;
							if (maxBoneIndex > 0xffff && (format == MBF_UINT8 || format == MBF_UINT16))
							{
								format = MBF_UINT32;
								formBuffs.UpdateOffsets(resultBuf);
							}
							else if (maxBoneIndex > 0x7fff && (format == MBF_INT8 || format == MBF_INT16))
							{
								format = MBF_UINT32;
								formBuffs.UpdateOffsets(resultBuf);
							}
							else if (maxBoneIndex > 0xff && format == MBF_UINT8)
							{
								format = MBF_UINT16;
								formBuffs.UpdateOffsets(resultBuf);
							}
							else if (maxBoneIndex > 0x7f && format == MBF_INT8)
							{
								format = MBF_INT16;
								formBuffs.UpdateOffsets(resultBuf);
							}
						}
					}
				}
			}
		}

		if (formatVertices)
		{
			FormatBufferSet(Source->GetVertexBuffers(), Result->GetVertexBuffers(),
				keepSystemBuffers, ignoreMissingChannels, true);
		}
		else
		{
			Result->VertexBuffers = Source->GetVertexBuffers();
		}

		if (formatIndices)
		{
			// \todo Make sure that the vertex indices will fit in this format, or extend it.
			FormatBufferSet(Source->GetIndexBuffers(), Result->GetIndexBuffers(), keepSystemBuffers,
				ignoreMissingChannels, false);
		}
		else
		{
			Result->IndexBuffers = Source->GetIndexBuffers();
		}

		// Copy the rest of the data
		Result->SetSkeleton(Source->GetSkeleton());
		Result->SetPhysicsBody(Source->GetPhysicsBody());

		Result->Layouts.Empty();
		for (const Ptr<const Layout>& Layout : Source->Layouts)
		{
			Result->Layouts.Add(Layout->Clone());
		}

		Result->Tags = Source->Tags;
		Result->StreamedResources = Source->StreamedResources;

		Result->AdditionalBuffers = Source->AdditionalBuffers;

		Result->BonePoses = Source->BonePoses;
		Result->BoneMap = Source->BoneMap;

		Result->SkeletonIDs = Source->SkeletonIDs;

		// A shallow copy is done here, it should not be a problem.
		Result->AdditionalPhysicsBodies = Source->AdditionalPhysicsBodies;

		Result->Surfaces = Source->Surfaces;

		Result->ResetStaticFormatFlags();
		Result->EnsureSurfaceData();
	}


	void MeshOptimizeBuffers( Mesh* InMesh )
	{
		if (!InMesh)
		{
			return;
		}

		FMeshBufferSet& VertexBuffers = InMesh->VertexBuffers;

		// Reduce the number of influences if possible
		constexpr int32 SemanticIndex = 0;
		
		UntypedMeshBufferIteratorConst WeightIt(VertexBuffers, MBS_BONEWEIGHTS, SemanticIndex);
		if (WeightIt.ptr())
		{
			int32 BufferInfluences = WeightIt.GetComponents();
			int32 RealInfluences = 0;

			for (int32 VertexIndex = 0; VertexIndex < VertexBuffers.GetElementCount(); ++VertexIndex)
			{
				int32 ThisVertexInfluences = 0;

				switch (WeightIt.GetFormat())
				{
				case MBF_NUINT8:
				{
					const uint8* Data = reinterpret_cast<const uint8*>(WeightIt.ptr());
					for (int32 InfluenceIndex = 0; InfluenceIndex < BufferInfluences; ++InfluenceIndex)
					{
						if (*Data>0)
						{
							++ThisVertexInfluences;
						}
						++Data;
					}
					break;
				}

				default:
					// Unsupported
					check(false);
					break;
				}

				++WeightIt;

				RealInfluences = FMath::Max(RealInfluences,ThisVertexInfluences);
			}

			if (RealInfluences<BufferInfluences)
			{
				// Remove the useless influences from the buffer.

				// \todo: This is a generic innefficient way
				FMeshBufferSet NewVertexBuffers;
				NewVertexBuffers.m_buffers = VertexBuffers.m_buffers;

				for (FMeshBuffer& Buffer: NewVertexBuffers.m_buffers)
				{
					int32 OffsetDelta = 0;
					for (FMeshBufferChannel& Channel : Buffer.m_channels)
					{
						Channel.m_offset += OffsetDelta;

						if (Channel.m_semanticIndex == SemanticIndex
							&&
							(Channel.m_semantic == MBS_BONEWEIGHTS || Channel.m_semantic == MBS_BONEINDICES)
							)
						{
							Channel.m_componentCount = RealInfluences;
							OffsetDelta -= (BufferInfluences - RealInfluences) * GetMeshFormatData(Channel.m_format).SizeInBytes;
						}
					}

					Buffer.m_elementSize += OffsetDelta;
				}

				FormatBufferSet( VertexBuffers, NewVertexBuffers, true, false, true);

				InMesh->VertexBuffers = NewVertexBuffers;
			}
		}

	}
}
