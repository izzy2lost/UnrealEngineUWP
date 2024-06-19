// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuR/MeshPrivate.h"
#include "MuR/Platform.h"
#include "MuR/MutableMath.h"
#include "MuR/OpMeshFormat.h"
#include "MuR/MutableTrace.h"


namespace mu
{

	struct FMeshMergeScratchMeshes
	{
		Ptr<Mesh> FirstReformat;
		Ptr<Mesh> SecondReformat;
	};

	//---------------------------------------------------------------------------------------------
	//! Merge two meshes into one new mesh
	//---------------------------------------------------------------------------------------------
	inline void MeshMerge(Mesh* Result, const Mesh* pFirst, const Mesh* pSecond, bool bMergeSurfaces, FMeshMergeScratchMeshes& ScratchMeshes)
	{
		MUTABLE_CPUPROFILER_SCOPE(MeshMerge);

		// Should never happen, but fixes static analysis warnings.
		if (!(pFirst && pSecond))
		{
			return;
		}

		// Indices
		//-----------------
		if (pFirst->GetIndexBuffers().GetBufferCount() > 0)
		{
			MUTABLE_CPUPROFILER_SCOPE(Indices);

			const int32 FirstCount = pFirst->GetIndexBuffers().GetElementCount();
			const int32 SecondCount = pSecond->GetIndexBuffers().GetElementCount();
			Result->GetIndexBuffers().SetElementCount(FirstCount + SecondCount);

			check(pFirst->GetIndexBuffers().GetBufferCount() <= 1);
			check(pSecond->GetIndexBuffers().GetBufferCount() <= 1);
			Result->GetIndexBuffers().SetBufferCount(1);

			FMeshBuffer& ResultIndexBuffer = Result->GetIndexBuffers().m_buffers[0];

			const FMeshBuffer& FirstIndexBuffer = pFirst->GetIndexBuffers().m_buffers[0];
			const FMeshBuffer& SecondIndexBuffer = pSecond->GetIndexBuffers().m_buffers[0];

			// Avoid unused variable warnings
			(void)FirstIndexBuffer;
			(void)SecondIndexBuffer;

			// This will be changed below if need to change the format of the index buffers.
			EMeshBufferFormat IndexBufferFormat = MBF_NONE;

			if (FirstCount && SecondCount)
			{
				check(!FirstIndexBuffer.m_channels.IsEmpty());
				check(FirstIndexBuffer.m_channels == SecondIndexBuffer.m_channels);
				check(FirstIndexBuffer.m_elementSize == SecondIndexBuffer.m_elementSize);

				// We need to know the total number of vertices in case we need to adjust the index buffer format.
				const uint64 totalVertexCount = pFirst->GetVertexBuffers().GetElementCount() + pSecond->GetVertexBuffers().GetElementCount();
				const uint64 maxValueBits = GetMeshFormatData(pFirst->GetIndexBuffers().m_buffers[0].m_channels[0].m_format).MaxValueBits;
				const uint64 maxSupportedVertices = uint64(1) << maxValueBits;
				
				if (totalVertexCount > maxSupportedVertices)
				{
					IndexBufferFormat = totalVertexCount > MAX_uint16 ? MBF_UINT32 : MBF_UINT16;
				}

			}
			
			if (IndexBufferFormat != MBF_NONE)
			{
				// We only support vertex indices in case of having to change the format.
				check(FirstIndexBuffer.m_channels.Num() == 1);

				ResultIndexBuffer.m_channels.SetNum(1);
				ResultIndexBuffer.m_channels[0].m_semantic = MBS_VERTEXINDEX;
				ResultIndexBuffer.m_channels[0].m_format = IndexBufferFormat;
				ResultIndexBuffer.m_channels[0].m_componentCount = 1;
				ResultIndexBuffer.m_channels[0].m_semanticIndex = 0;
				ResultIndexBuffer.m_channels[0].m_offset = 0;
				ResultIndexBuffer.m_elementSize = GetMeshFormatData(IndexBufferFormat).SizeInBytes;
			}
			else if (FirstCount)
			{
				ResultIndexBuffer.m_channels = FirstIndexBuffer.m_channels;
				ResultIndexBuffer.m_elementSize = FirstIndexBuffer.m_elementSize;
			}
			else if (SecondCount)
			{
				ResultIndexBuffer.m_channels = SecondIndexBuffer.m_channels;
				ResultIndexBuffer.m_elementSize = SecondIndexBuffer.m_elementSize;
			}

			ResultIndexBuffer.m_data.SetNum(ResultIndexBuffer.m_elementSize * (FirstCount + SecondCount));

			check(ResultIndexBuffer.m_channels.Num() == 1);
			check(ResultIndexBuffer.m_channels[0].m_semantic == MBS_VERTEXINDEX);

			if (!ResultIndexBuffer.m_data.IsEmpty())
			{
				if (FirstCount)
				{
					if (IndexBufferFormat == MBF_NONE
						|| IndexBufferFormat == FirstIndexBuffer.m_channels[0].m_format)
					{
						FMemory::Memcpy(&ResultIndexBuffer.m_data[0],
							&FirstIndexBuffer.m_data[0],
							FirstIndexBuffer.m_elementSize * FirstCount);
					}
					else
					{
						// Conversion required
						const uint8_t* pSource = &FirstIndexBuffer.m_data[0];
						uint8_t* pDest = &ResultIndexBuffer.m_data[0];
						switch (IndexBufferFormat)
						{
						case MBF_UINT32:
						{
							switch (FirstIndexBuffer.m_channels[0].m_format)
							{
							case MBF_UINT16:
							{
								for (int v = 0; v < FirstCount; ++v)
								{
									*(uint32_t*)pDest = *(const uint16*)pSource;
									pSource += FirstIndexBuffer.m_elementSize;
									pDest += ResultIndexBuffer.m_elementSize;
								}
								break;
							}

							case MBF_UINT8:
							{
								for (int v = 0; v < FirstCount; ++v)
								{
									*(uint32_t*)pDest = *(const uint8_t*)pSource;
									pSource += FirstIndexBuffer.m_elementSize;
									pDest += ResultIndexBuffer.m_elementSize;
								}
								break;
							}

							default:
								checkf(false, TEXT("Format not supported."));
								break;
							}
							break;
						}

						case MBF_UINT16:
						{
							switch (FirstIndexBuffer.m_channels[0].m_format)
							{

							case MBF_UINT8:
							{
								for (int v = 0; v < FirstCount; ++v)
								{
									*(uint16*)pDest = *(const uint8_t*)pSource;
									pSource += FirstIndexBuffer.m_elementSize;
									pDest += ResultIndexBuffer.m_elementSize;
								}
								break;
							}

							default:
								checkf(false, TEXT("Format not supported."));
								break;
							}
							break;
						}

						default:
							checkf(false, TEXT("Format not supported."));
							break;
						}
					}
				}

				if (SecondCount)
				{
					const uint8_t* pSource = &SecondIndexBuffer.m_data[0];
					uint8_t* pDest = &ResultIndexBuffer.m_data[ResultIndexBuffer.m_elementSize * FirstCount];

					uint32_t firstVertexCount = pFirst->GetVertexBuffers().GetElementCount();

					if (IndexBufferFormat == MBF_NONE
						|| IndexBufferFormat == SecondIndexBuffer.m_channels[0].m_format)
					{
						switch (SecondIndexBuffer.m_channels[0].m_format)
						{
						case MBF_INT32:
						case MBF_UINT32:
						case MBF_NINT32:
						case MBF_NUINT32:
						{
							for (int v = 0; v < SecondCount; ++v)
							{
								*(uint32_t*)pDest = firstVertexCount + *(const uint32_t*)pSource;
								pSource += SecondIndexBuffer.m_elementSize;
								pDest += ResultIndexBuffer.m_elementSize;
							}
							break;
						}

						case MBF_INT16:
						case MBF_UINT16:
						case MBF_NINT16:
						case MBF_NUINT16:
						{
							for (int v = 0; v < SecondCount; ++v)
							{
								*(uint16*)pDest = uint16(firstVertexCount) + *(const uint16*)pSource;
								pSource += SecondIndexBuffer.m_elementSize;
								pDest += ResultIndexBuffer.m_elementSize;
							}
							break;
						}

						case MBF_INT8:
						case MBF_UINT8:
						case MBF_NINT8:
						case MBF_NUINT8:
						{
							for (int v = 0; v < SecondCount; ++v)
							{
								*(uint8_t*)pDest = uint8_t(firstVertexCount) + *(const uint8_t*)pSource;
								pSource += SecondIndexBuffer.m_elementSize;
								pDest += ResultIndexBuffer.m_elementSize;
							}
							break;
						}

						default:
							checkf(false, TEXT("Format not supported."));
							break;
						}
					}
					else
					{
						// Format conversion required
						switch (IndexBufferFormat)
						{

						case MBF_UINT32:
						{
							switch (SecondIndexBuffer.m_channels[0].m_format)
							{
							case MBF_INT16:
							case MBF_UINT16:
							case MBF_NINT16:
							case MBF_NUINT16:
							{
								for (int v = 0; v < SecondCount; ++v)
								{
									*(uint32_t*)pDest = uint32_t(firstVertexCount) + *(const uint16*)pSource;
									pSource += SecondIndexBuffer.m_elementSize;
									pDest += ResultIndexBuffer.m_elementSize;
								}
								break;
							}

							case MBF_INT8:
							case MBF_UINT8:
							case MBF_NINT8:
							case MBF_NUINT8:
							{
								for (int v = 0; v < SecondCount; ++v)
								{
									*(uint32_t*)pDest = uint32_t(firstVertexCount) + *(const uint8_t*)pSource;
									pSource += SecondIndexBuffer.m_elementSize;
									pDest += ResultIndexBuffer.m_elementSize;
								}
								break;
							}

							default:
								checkf(false, TEXT("Format not supported."));
								break;
							}

							break;
						}

						case MBF_UINT16:
						{
							switch (SecondIndexBuffer.m_channels[0].m_format)
							{
							case MBF_INT8:
							case MBF_UINT8:
							case MBF_NINT8:
							case MBF_NUINT8:
							{
								for (int v = 0; v < SecondCount; ++v)
								{
									*(uint16*)pDest = uint16(firstVertexCount) + *(const uint8_t*)pSource;
									pSource += SecondIndexBuffer.m_elementSize;
									pDest += ResultIndexBuffer.m_elementSize;
								}
								break;
							}

							default:
								checkf(false, TEXT("Format not supported."));
								break;
							}

							break;
						}

						default:
							checkf(false, TEXT("Format not supported."));
							break;

						}
					}
				}
			}
		}


		// Layouts
		//-----------------
		{
			MUTABLE_CPUPROFILER_SCOPE(Layouts);

			Result->Layouts.SetNum(pFirst->Layouts.Num());
			for (int32 LayoutIndex = 0; LayoutIndex < pFirst->Layouts.Num(); ++LayoutIndex)
			{
				const Layout* pF = pFirst->Layouts[LayoutIndex].get();
				Ptr<Layout> pR = pF->Clone();

				if (LayoutIndex < pSecond->Layouts.Num())
				{
					const Layout* pS = pSecond->Layouts[LayoutIndex].get();
					pR->Blocks.Append(pS->Blocks);
				}

				Result->Layouts[LayoutIndex] = pR;
			}
		}


		// Skeleton
		//---------------------------

		// Add SkeletonIDs
		Result->SkeletonIDs = pFirst->SkeletonIDs;

		for (const int32 SkeletonID : pSecond->SkeletonIDs)
		{
			Result->SkeletonIDs.AddUnique(SkeletonID);
		}

		// Do they have the same skeleton?
		bool bMergeSkeletons = pFirst->GetSkeleton() != pSecond->GetSkeleton();

		// Are they different skeletons but with the same data?
		if (bMergeSkeletons && pFirst->GetSkeleton() && pSecond->GetSkeleton())
		{
			bMergeSkeletons = !(*pFirst->GetSkeleton() == *pSecond->GetSkeleton());
		}


		if (bMergeSkeletons)
		{
			MUTABLE_CPUPROFILER_SCOPE(MergeSkeleton);
			Ptr<Skeleton> pResultSkeleton;

			mu::SkeletonPtrConst pFirstSkeleton = pFirst->GetSkeleton();
			mu::SkeletonPtrConst pSecondSkeleton = pSecond->GetSkeleton();

			const int32 NumBonesFirst = pFirstSkeleton ? pFirstSkeleton->GetBoneCount() : 0;
			const int32 NumBonesSecond = pSecondSkeleton ? pSecondSkeleton->GetBoneCount() : 0;

			pResultSkeleton = pFirstSkeleton ? pFirstSkeleton->Clone() : new Skeleton;
			Result->SetSkeleton(pResultSkeleton);

			TArray<uint16> SecondToResultBoneIndices;
			SecondToResultBoneIndices.SetNumUninitialized(NumBonesSecond);

			// Merge pSecond and build the remap array 
			for (int32 SecondBoneIndex = 0; SecondBoneIndex < NumBonesSecond; ++SecondBoneIndex)
			{
				const FBoneName& BoneNameId = pSecondSkeleton->BoneIds[SecondBoneIndex];
				int32 Index = pResultSkeleton->FindBone(BoneNameId);

				// Add a new bone
				if (Index == INDEX_NONE)
				{
					Index = pResultSkeleton->BoneIds.Add(BoneNameId);

					// Add an incorrect index, to be fixed below in case the parent index is later in the bone array.
					pResultSkeleton->BoneParents.Add(pSecondSkeleton->BoneParents[SecondBoneIndex]);
#if WITH_EDITOR
					if (pSecondSkeleton->DebugBoneNames.IsValidIndex(SecondBoneIndex))
					{
						pResultSkeleton->DebugBoneNames.Add(pSecondSkeleton->DebugBoneNames[SecondBoneIndex]);
					}
#endif
				}

				SecondToResultBoneIndices[SecondBoneIndex] = (uint16)Index;
			}

			// Fix second mesh bone parents
			for (int32 ob = NumBonesFirst; ob < pResultSkeleton->BoneParents.Num(); ++ob)
			{
				int16 secondMeshIndex = pResultSkeleton->BoneParents[ob];
				if (secondMeshIndex != INDEX_NONE)
				{
					pResultSkeleton->BoneParents[ob] = SecondToResultBoneIndices[secondMeshIndex];
				}
			}
		}
		else
		{
			Result->SetSkeleton(pFirst->GetSkeleton());
		}


		// Surfaces
		//---------------------------
		
		// Remap bone indices if we merge surfaces since bonemaps will be merged too.
		bool bRemapBoneIndices = false;
		TArray<uint16> RemappedBoneMapIndices;

		// Used to know the format of the bone index buffer
		uint32 MaxNumBonesInBoneMaps = 0;
		const int32 NumSecondBonesInBoneMap = pSecond->BoneMap.Num();

		{
			MUTABLE_CPUPROFILER_SCOPE(Surfaces);
			
			const int32 NumFirstBonesInBoneMap = pFirst->BoneMap.Num();
			Result->BoneMap = pFirst->BoneMap;

			if (bMergeSurfaces)
			{
				// Merge BoneMaps
				RemappedBoneMapIndices.SetNumUninitialized(NumSecondBonesInBoneMap);

				for (uint16 SecondBoneMapIndex = 0; SecondBoneMapIndex < NumSecondBonesInBoneMap; ++SecondBoneMapIndex)
				{
					const int32 BoneMapIndex = Result->BoneMap.AddUnique(pSecond->BoneMap[SecondBoneMapIndex]);
					RemappedBoneMapIndices[SecondBoneMapIndex] = BoneMapIndex;

					bRemapBoneIndices = bRemapBoneIndices || BoneMapIndex != SecondBoneMapIndex;
				}

				FMeshSurface& NewSurface = Result->Surfaces.AddDefaulted_GetRef();
				NewSurface.BoneMapCount = Result->BoneMap.Num();

				int32 NumFirstSubMeshes = 0;
				for (const FMeshSurface& Surf : pFirst->Surfaces)
				{
					NewSurface.SubMeshes.Append(Surf.SubMeshes);
					NumFirstSubMeshes += Surf.SubMeshes.Num();
				}

				for (const FMeshSurface& Surf : pSecond->Surfaces)
				{
					NewSurface.SubMeshes.Append(Surf.SubMeshes);
				}

				// Fix surface Submesh ranges.
				if (NumFirstSubMeshes > 0)
				{
					const int32 NumResultSubMeshes = NewSurface.SubMeshes.Num();
				
					const FSurfaceSubMesh LastFromFirstMesh = pFirst->Surfaces.Last().SubMeshes.Last();
	
					for (int32 SecondSubMeshIndex = NumFirstSubMeshes; SecondSubMeshIndex < NumResultSubMeshes; ++SecondSubMeshIndex)
					{
						NewSurface.SubMeshes[SecondSubMeshIndex].VertexBegin += LastFromFirstMesh.VertexEnd;	
						NewSurface.SubMeshes[SecondSubMeshIndex].VertexEnd += LastFromFirstMesh.VertexEnd;	
						NewSurface.SubMeshes[SecondSubMeshIndex].IndexBegin += LastFromFirstMesh.IndexEnd;	
						NewSurface.SubMeshes[SecondSubMeshIndex].IndexEnd += LastFromFirstMesh.IndexEnd;	
					}
				}
			}
			else
			{
				// Add the BoneMap of the second mesh
				Result->BoneMap.Append(pSecond->BoneMap);

				// Add pFirst surfaces
				Result->Surfaces = pFirst->Surfaces;

				const int32 FirstVertexEnd = pFirst->GetVertexCount();
				const int32 FirstIndexEnd = pFirst->GetIndexCount();

				check(pSecond->Surfaces.Num() == 1);
				FMeshSurface& NewSurface = Result->Surfaces.Add_GetRef(pSecond->Surfaces[0]);
				
				for (FSurfaceSubMesh& SubMesh : NewSurface.SubMeshes)
				{
					SubMesh.VertexBegin += FirstVertexEnd;
					SubMesh.VertexEnd += FirstVertexEnd;
					SubMesh.IndexBegin += FirstIndexEnd;
					SubMesh.IndexEnd += FirstIndexEnd;
				}

				NewSurface.BoneMapIndex += NumFirstBonesInBoneMap;
			}

			for (const FMeshSurface& Surface : Result->Surfaces)
			{
				MaxNumBonesInBoneMaps = FMath::Max(MaxNumBonesInBoneMaps, Surface.BoneMapCount);
			}

			Result->BoneMap.Shrink();
		}


		// Pose
		//---------------------------
		{
			MUTABLE_CPUPROFILER_SCOPE(Pose);

			Result->BonePoses.Reserve(Result->GetSkeleton()->GetBoneCount());

			// Copy poses from the first mesh
			Result->BonePoses = pFirst->BonePoses;

			// Add or override bone poses
			for (const Mesh::FBonePose& SecondBonePose : pSecond->BonePoses)
			{
				const int32 ResultBoneIndex = Result->FindBonePose(SecondBonePose.BoneId);

				if (ResultBoneIndex != INDEX_NONE)
				{
					Mesh::FBonePose& ResultBonePose = Result->BonePoses[ResultBoneIndex];

					// TODO: Not sure how to tune this priority, review it.
					// For now use a similar strategy as before. 
					auto ComputeBoneMergePriority = [](const Mesh::FBonePose& BonePose)
					{
						return (EnumHasAnyFlags(BonePose.BoneUsageFlags, EBoneUsageFlags::Skinning) ? 1 : 0) +
							(EnumHasAnyFlags(BonePose.BoneUsageFlags, EBoneUsageFlags::Reshaped) ? 1 : 0);
					};

					if (ComputeBoneMergePriority(ResultBonePose) < ComputeBoneMergePriority(SecondBonePose))
					{
						//ResultBonePose.BoneName = SecondBonePose.BoneName;
						ResultBonePose.BoneTransform = SecondBonePose.BoneTransform;
						// Merge usage flags
						EnumAddFlags(ResultBonePose.BoneUsageFlags, SecondBonePose.BoneUsageFlags);
					}
				}
				else
				{
					Result->BonePoses.Add(SecondBonePose);
				}
			}

			Result->BonePoses.Shrink();
		}


		// PhysicsBodies
		//---------------------------
		{
			MUTABLE_CPUPROFILER_SCOPE(PhysicsBodies);

			// Appends InPhysicsBody to OutPhysicsBody removing Bodies that are equal, have same bone and customId and its properies are identical.	
			auto AppendPhysicsBodiesUnique = [](PhysicsBody& OutPhysicsBody, const PhysicsBody& InPhysicsBody) -> bool
			{
				TArray<FBoneName>& OutBones = OutPhysicsBody.BoneIds;
				TArray<int32>& OutCustomIds = OutPhysicsBody.BodiesCustomIds;
				TArray<FPhysicsBodyAggregate>& OutBodies = OutPhysicsBody.Bodies;

				const TArray<FBoneName>& InBones = InPhysicsBody.BoneIds;
				const TArray<int32>& InCustomIds = InPhysicsBody.BodiesCustomIds;
				const TArray<FPhysicsBodyAggregate>& InBodies = InPhysicsBody.Bodies;

				const int32 InBodyCount = InPhysicsBody.GetBodyCount();
				const int32 OutBodyCount = OutPhysicsBody.GetBodyCount();

				bool bModified = false;

				for (int32 InBodyIndex = 0; InBodyIndex < InBodyCount; ++InBodyIndex)
				{
					int32 FoundIndex = INDEX_NONE;
					for (int32 OutBodyIndex = 0; OutBodyIndex < OutBodyCount; ++OutBodyIndex)
					{
						if (InCustomIds[InBodyIndex] == OutCustomIds[OutBodyIndex] && InBones[InBodyIndex] == OutBones[OutBodyIndex])
						{
							FoundIndex = OutBodyIndex;
							break;
						}
					}

					if (FoundIndex == INDEX_NONE)
					{
						OutBones.Add(InBones[InBodyIndex]);
						OutCustomIds.Add(InCustomIds[InBodyIndex]);
						OutBodies.Add(InBodies[InBodyIndex]);

						bModified |= true;

						continue;
					}

					for (const FSphereBody& Body : InBodies[InBodyIndex].Spheres)
					{
						const int32 NumBeforeAddition = OutBodies[FoundIndex].Spheres.Num();
						bModified |= NumBeforeAddition == OutBodies[FoundIndex].Spheres.AddUnique(Body);
					}

					for (const FBoxBody& Body : InBodies[InBodyIndex].Boxes)
					{
						const int32 NumBeforeAddition = OutBodies[FoundIndex].Boxes.Num();
						bModified |= NumBeforeAddition == OutBodies[FoundIndex].Boxes.AddUnique(Body);
					}

					for (const FSphylBody& Body : InBodies[InBodyIndex].Sphyls)
					{
						const int32 NumBeforeAddition = OutBodies[FoundIndex].Sphyls.Num();
						bModified |= NumBeforeAddition == OutBodies[FoundIndex].Sphyls.AddUnique(Body);
					}

					for (const FTaperedCapsuleBody& Body : InBodies[InBodyIndex].TaperedCapsules)
					{
						const int32 NumBeforeAddition = OutBodies[FoundIndex].TaperedCapsules.Num();
						bModified |= NumBeforeAddition == OutBodies[FoundIndex].TaperedCapsules.AddUnique(Body);
					}

					for (const FConvexBody& Body : InBodies[InBodyIndex].Convex)
					{
						const int32 NumBeforeAddition = OutBodies[FoundIndex].Convex.Num();
						bModified |= NumBeforeAddition == OutBodies[FoundIndex].Convex.AddUnique(Body);
					}
				}

				return bModified;
			};

			TTuple<const PhysicsBody*, bool> SharedResultPhysicsBody = Invoke([&]()
				-> TTuple<const PhysicsBody*, bool>
			{
				if (pFirst->GetPhysicsBody() == pSecond->GetPhysicsBody())
				{
					return MakeTuple(pFirst->GetPhysicsBody().get(), true);
				}

				if (pFirst->GetPhysicsBody() && !pSecond->GetPhysicsBody())
				{
					return MakeTuple(pFirst->GetPhysicsBody().get(), true);
				}

				if (!pFirst->GetPhysicsBody() && pSecond->GetPhysicsBody())
				{
					return MakeTuple(pSecond->GetPhysicsBody().get(), true);
				}

				return MakeTuple(nullptr, false);
			});

			if (SharedResultPhysicsBody.Get<1>())
			{
				// Only one or non of the meshes has physics, share the result.
				Result->SetPhysicsBody(SharedResultPhysicsBody.Get<0>());
			}
			else
			{
				check(pFirst->GetPhysicsBody() && pSecond->GetPhysicsBody());

				Ptr<PhysicsBody> MergedResultPhysicsBody = pFirst->GetPhysicsBody()->Clone();

				MergedResultPhysicsBody->bBodiesModified =
					AppendPhysicsBodiesUnique(*MergedResultPhysicsBody, *pSecond->GetPhysicsBody()) ||
					pFirst->GetPhysicsBody()->bBodiesModified || pSecond->GetPhysicsBody()->bBodiesModified;

				Result->SetPhysicsBody(MergedResultPhysicsBody);
			}

			// Additional physics bodies.
			const int32 MaxAdditionalPhysicsResultNum = pFirst->AdditionalPhysicsBodies.Num() + pSecond->AdditionalPhysicsBodies.Num();

			Result->AdditionalPhysicsBodies.Reserve(MaxAdditionalPhysicsResultNum);
			Result->AdditionalPhysicsBodies.Append(pFirst->AdditionalPhysicsBodies);
			
			// Not very many additional bodies expected, do a quadratic search to have unique bodies based on external id.
			const int32 NumSecondAdditionalBodies = pSecond->AdditionalPhysicsBodies.Num();
			for (int32 Index = 0; Index < NumSecondAdditionalBodies; ++Index)
			{
				const int32 CustomIdToMerge = pSecond->AdditionalPhysicsBodies[Index]->CustomId;

				const mu::Ptr<const PhysicsBody>* Found = pFirst->AdditionalPhysicsBodies.FindByPredicate(
					[CustomIdToMerge](const Ptr<const mu::PhysicsBody>& Body) { return CustomIdToMerge == Body->CustomId; });

				// TODO: current usages do not expect collisions, but same Id collision with bodies modified in differnet ways
				// may need to be contemplated at some point.
				if (!Found)
				{
					Result->AdditionalPhysicsBodies.Add(pSecond->AdditionalPhysicsBodies[Index]);
				}
			}
		}

		// This affects both vertex IDs and layout block ids.
		bool bNeedsExplicitIds = pFirst->MeshIDPrefix != pSecond->MeshIDPrefix;
		if (!bNeedsExplicitIds)
		{
			// This is needed in case a mesh merges with itself.
			Result->MeshIDPrefix = pFirst->MeshIDPrefix;
		}

		// Vertices
		//-----------------
		{
            MUTABLE_CPUPROFILER_SCOPE(Vertices);

			const int32 FirstBufferCount = pFirst->VertexBuffers.m_buffers.Num();
			const int32 SecondBufferCount = pSecond->VertexBuffers.m_buffers.Num();
			const int32 FirstVertexCount = pFirst->GetVertexBuffers().GetElementCount();
			const int32 SecondVertexCount = pSecond->GetVertexBuffers().GetElementCount();

			// Check if the format of the BoneIndex buffer has to change
			bool bChangeBoneIndicesFormat = false;
			EMeshBufferFormat BoneIndexFormat = MaxNumBonesInBoneMaps > MAX_uint8 ? MBF_UINT16 : MBF_UINT8;
			bChangeBoneIndicesFormat = pFirst->GetVertexBuffers().HasAnySemanticWithDifferentFormat(MBS_BONEINDICES, BoneIndexFormat);
			if (!bChangeBoneIndicesFormat)
			{
				bChangeBoneIndicesFormat = pSecond->GetVertexBuffers().HasAnySemanticWithDifferentFormat(MBS_BONEINDICES, BoneIndexFormat);
			}

			// Step 1: Set up the initial vertex buffer structure of the result mesh.
			//-----------------------------------------------------------------------
			{
				MUTABLE_CPUPROFILER_SCOPE(ResultFormatSetup);

				// Start with the structure of the first mesh
				Result->GetVertexBuffers().SetBufferCount(FirstBufferCount);
				for (int32 BufferIndex = 0; BufferIndex < FirstBufferCount; ++BufferIndex)
				{
					Result->VertexBuffers.m_buffers[BufferIndex].m_channels = pFirst->VertexBuffers.m_buffers[BufferIndex].m_channels;
					Result->VertexBuffers.m_buffers[BufferIndex].m_elementSize = pFirst->VertexBuffers.m_buffers[BufferIndex].m_elementSize;
				}

				// See if we need to add additional buffers from the second mesh (like vertex colours or additional UV Channels)
				// This is a bit ad-hoc: we only add buffers containing all new channels
				for (const FMeshBuffer& SecondBuf : pSecond->GetVertexBuffers().m_buffers)
				{
					bool bSomeChannel = false;
					bool bAllNewChannels = true;
					for (const FMeshBufferChannel& SecondChan : SecondBuf.m_channels)
					{
						// Skip system buffers
						if (SecondChan.m_semantic == MBS_VERTEXINDEX
							||
							SecondChan.m_semantic == MBS_LAYOUTBLOCK)
						{
							continue;
						}

						bSomeChannel = true;

						int32 FoundBuffer = -1;
						int32 FoundChannel = -1;
						pFirst->GetVertexBuffers().FindChannel(SecondChan.m_semantic, SecondChan.m_semanticIndex, &FoundBuffer, &FoundChannel);
						if (FoundBuffer >= 0)
						{
							// There's at least one channel that already exists in the first mesh. Don't add the buffer.
							bAllNewChannels = false;
							continue;
						}

						// If there are additional UV channels try to add them
						if (!bAllNewChannels && SecondChan.m_semantic == MBS_TEXCOORDS
							&&
							SecondChan.m_semanticIndex > 0)
						{
							// Add additional UV channels if the previous one is found.
							FMeshBufferSet& ResultVertexBuffers = Result->GetVertexBuffers();
							ResultVertexBuffers.FindChannel(MBS_TEXCOORDS, SecondChan.m_semanticIndex - 1, &FoundBuffer, &FoundChannel);

							if (FoundBuffer >= 0)
							{
								FMeshBuffer& Buffer = ResultVertexBuffers.m_buffers[FoundBuffer];
								Buffer.m_channels.Insert(SecondChan, FoundChannel + 1);

								ResultVertexBuffers.UpdateOffsets(FoundBuffer);
							}
						}
					}

					if (bSomeChannel && bAllNewChannels)
					{
						int32 NewBufferIndex = Result->GetVertexBuffers().m_buffers.Emplace();
						Result->VertexBuffers.m_buffers[NewBufferIndex].m_channels = SecondBuf.m_channels;
						Result->VertexBuffers.m_buffers[NewBufferIndex].m_elementSize = SecondBuf.m_elementSize;
					}
				}

				// See if we need to enlarge the components of any of the result channels
				int32 ResultBufferCount = Result->GetVertexBuffers().GetBufferCount();
				for (int32 BufferIndex = 0; BufferIndex < FMath::Min(ResultBufferCount, FirstBufferCount); ++BufferIndex)
				{
					// Expand component counts in vertex channels of the format mesh
					FMeshBuffer& result = Result->GetVertexBuffers().m_buffers[BufferIndex];
					const FMeshBuffer& first = pFirst->GetVertexBuffers().m_buffers[BufferIndex];

					result.m_channels = first.m_channels;
					result.m_elementSize = first.m_elementSize;

					bool bResetOffsets = false;
					for (int32 c = 0; c < result.m_channels.Num(); ++c)
					{
						int32 sb = -1;
						int32 sc = -1;
						pSecond->GetVertexBuffers().FindChannel
						(
							result.m_channels[c].m_semantic,
							result.m_channels[c].m_semanticIndex,
							&sb, &sc
						);

						if (sb >= 0)
						{
							const FMeshBuffer& second = pSecond->GetVertexBuffers().m_buffers[sb];

							if (second.m_channels[sc].m_componentCount>result.m_channels[c].m_componentCount)
							{
								result.m_channels[c].m_componentCount = second.m_channels[sc].m_componentCount;
								bResetOffsets = true;
							}
						}
					}

					// Reset the channel offsets if necessary
					if (bResetOffsets)
					{
						Result->GetVertexBuffers().UpdateOffsets(BufferIndex);
					}
				}


				// Change the format of the bone indices buffer
				if (bChangeBoneIndicesFormat)
				{
					// Iterate all vertex buffers and update the format
					FMeshBufferSet& VertexBuffers = Result->GetVertexBuffers();
					for (int32 VertexBufferIndex = 0; VertexBufferIndex < VertexBuffers.m_buffers.Num(); ++VertexBufferIndex)
					{
						FMeshBuffer& result = VertexBuffers.m_buffers[VertexBufferIndex];

						const int32 ChannelsCount = VertexBuffers.GetBufferChannelCount(VertexBufferIndex);
						for (int32 ChannelIndex = 0; ChannelIndex < ChannelsCount; ++ChannelIndex)
						{
							if (result.m_channels[ChannelIndex].m_semantic == MBS_BONEINDICES)
							{
								result.m_channels[ChannelIndex].m_format = BoneIndexFormat;

								// Reset offsets
								int32 offset = 0;
								for (int32 AuxChannelIndex = 0; AuxChannelIndex < ChannelsCount; ++AuxChannelIndex)
								{
									result.m_channels[AuxChannelIndex].m_offset = (uint8_t)offset;
									offset += result.m_channels[AuxChannelIndex].m_componentCount
										*
										GetMeshFormatData(result.m_channels[AuxChannelIndex].m_format).SizeInBytes;
								}
								result.m_elementSize = offset;
							}
						}
					}
				}

				// Manage vertex IDs
				if (bNeedsExplicitIds)
				{
					// Make sure the result format is suitable for the explicit IDs
					Result->MakeIdsExplicit();
				}
			}


			// Step 2: Fill the result buffers
			//-----------------------------------------------------------------------
			{
				MUTABLE_CPUPROFILER_SCOPE(ResultFill);

				// We have the final result vertex buffers structure: allocate the memory for them.
				Result->VertexBuffers.SetElementCount(FirstVertexCount + SecondVertexCount);

				int32 ResultBufferCount = Result->GetVertexBuffers().GetBufferCount();
				for (int32 ResultBufferIndex = 0; ResultBufferIndex < ResultBufferCount; ++ResultBufferIndex)
				{
					FMeshBuffer& ResultBuffer = Result->VertexBuffers.m_buffers[ResultBufferIndex];

					// TODO: Don't assume the buffer order in First and Second matches Result, for more opportunities of fast-path
					bool bFirstFastPath = (ResultBufferIndex < FirstBufferCount)
						&&
						pFirst->VertexBuffers.HasSameFormat(ResultBufferIndex, Result->VertexBuffers, ResultBufferIndex);

					int32 FirstResultBufferSize = ResultBuffer.m_elementSize * FirstVertexCount;
					uint8* Destination = ResultBuffer.m_data.GetData();
					if (bFirstFastPath)
					{
						MUTABLE_CPUPROFILER_SCOPE(FirstFastPath);

						const FMeshBuffer& FirstBuffer = pFirst->VertexBuffers.m_buffers[ResultBufferIndex];
						check(FirstResultBufferSize == FirstBuffer.m_data.Num());
						FMemory::Memcpy(Destination, FirstBuffer.m_data.GetData(), FirstResultBufferSize);
					}
					else
					{
						MUTABLE_CPUPROFILER_SCOPE(FirstSlowPath);

						int32 OffsetElements = 0;
						MeshFormatBuffer(pFirst->VertexBuffers, ResultBuffer, OffsetElements, true, pFirst->MeshIDPrefix);
					}


					bool bSecondFastPath = (ResultBufferIndex < SecondBufferCount)
						&&
						pSecond->VertexBuffers.HasSameFormat(ResultBufferIndex, Result->VertexBuffers, ResultBufferIndex);

					int32 SecondResultBufferSize = ResultBuffer.m_elementSize * SecondVertexCount;
					Destination = ResultBuffer.m_data.GetData() + FirstResultBufferSize;
					if (bSecondFastPath)
					{
						MUTABLE_CPUPROFILER_SCOPE(SecondFastPath);

						const FMeshBuffer& SecondBuffer = pSecond->VertexBuffers.m_buffers[ResultBufferIndex];
						check(SecondResultBufferSize == SecondBuffer.m_data.Num());
						FMemory::Memcpy(Destination, SecondBuffer.m_data.GetData(), SecondResultBufferSize);
					}
					else
					{
						MUTABLE_CPUPROFILER_SCOPE(SecondSlowPath);

						int32 OffsetElements = FirstVertexCount;
						MeshFormatBuffer(pSecond->VertexBuffers, ResultBuffer, OffsetElements, true, pSecond->MeshIDPrefix);
					}
				}

				// TODO: This still needs to be optimized
				if (bRemapBoneIndices)
				{
					MUTABLE_CPUPROFILER_SCOPE(RemapBones);

					// We need to remap the bone indices of the second mesh vertices that we already copied to result
					check(!RemappedBoneMapIndices.IsEmpty());

					// Iterate all vertex buffers and update the format
					FMeshBufferSet& VertexBuffers = Result->GetVertexBuffers();
					for (int32 VertexBufferIndex = 0; VertexBufferIndex < VertexBuffers.m_buffers.Num(); ++VertexBufferIndex)
					{
						FMeshBuffer& ResultBuffer = VertexBuffers.m_buffers[VertexBufferIndex];

						const int32 ElemSize = VertexBuffers.GetElementSize(VertexBufferIndex);
						const int32 FirstSize = FirstVertexCount * ElemSize;

						const int32 ChannelsCount = VertexBuffers.GetBufferChannelCount(VertexBufferIndex);
						for (int32 ChannelIndex = 0; ChannelIndex < ChannelsCount; ++ChannelIndex)
						{
							if (ResultBuffer.m_channels[ChannelIndex].m_semantic != MBS_BONEINDICES)
							{
								continue;
							}

							int32 ResultOffset = FirstSize + ResultBuffer.m_channels[ChannelIndex].m_offset;

							const int32 NumComponents = ResultBuffer.m_channels[ChannelIndex].m_componentCount;

							// Bone indices may need remapping
							for (int32 VertexIndex = 0; VertexIndex < pSecond->GetVertexCount(); ++VertexIndex)
							{
								switch (BoneIndexFormat)
								{
								case MBF_INT8:
								case MBF_UINT8:
								{
									uint8* pD = reinterpret_cast<uint8*>(&ResultBuffer.m_data[ResultOffset]);

									for (int32 ComponentIndex = 0; ComponentIndex < NumComponents; ++ComponentIndex)
									{
										uint8 BoneMapIndex = pD[ComponentIndex];

										// be defensive
										if (BoneMapIndex < NumSecondBonesInBoneMap)
										{
											pD[ComponentIndex] = (uint8)RemappedBoneMapIndices[BoneMapIndex];
										}
										else
										{
											pD[ComponentIndex] = 0;
										}
									}

									ResultOffset += ElemSize;
									break;
								}

								case MBF_INT16:
								case MBF_UINT16:
								{
									uint16* pD = reinterpret_cast<uint16*>(&ResultBuffer.m_data[ResultOffset]);

									for (int32 ComponentIndex = 0; ComponentIndex < NumComponents; ++ComponentIndex)
									{
										uint16 BoneMapIndex = pD[ComponentIndex];

										// be defensive
										if (BoneMapIndex < NumSecondBonesInBoneMap)
										{
											pD[ComponentIndex] = (uint16)RemappedBoneMapIndices[BoneMapIndex];
										}
										else
										{
											pD[ComponentIndex] = 0;
										}
									}

									ResultOffset += ElemSize;
									break;
								}

								case MBF_INT32:
								case MBF_UINT32:
								{
									// Unreal does not support 32 bit bone indices
									checkf(false, TEXT("Format not supported."));
									break;
								}

								default:
									checkf(false, TEXT("Format not supported."));
								}
							}
						}
					}
				}
			}
		}


		// Tags
		Result->Tags = pFirst->Tags;

		for (const FString& SecondTag : pSecond->Tags)
		{
			Result->Tags.AddUnique(SecondTag);
		}


		// Streamed Resources
		Result->StreamedResources = pFirst->StreamedResources;

		const int32 NumStreamedResources = pSecond->StreamedResources.Num();
		for (int32 Index = 0; Index < NumStreamedResources; ++Index)
		{
			Result->StreamedResources.AddUnique(pSecond->StreamedResources[Index]);
		}
	}
	

    //---------------------------------------------------------------------------------------------
    //!
    //---------------------------------------------------------------------------------------------
    inline void ExtendSkeleton( Skeleton* pBase, const Skeleton* pOther )
    {
        TMap<int,int> otherToResult;

        int initialBones = pBase->GetBoneCount();
		for (int b = 0; pOther && b < pOther->GetBoneCount(); ++b)
		{
			int32 resultBoneIndex = pBase->FindBone(pOther->GetBoneName(b));
			if (resultBoneIndex < 0)
			{
				int32 newIndex = pBase->BoneIds.Num();
				otherToResult.Add(b, newIndex);
				pBase->BoneIds.Add(pOther->BoneIds[b]);

				// Will be remapped below
				pBase->BoneParents.Add(pOther->BoneParents[b]);

#if WITH_EDITOR
				if (pOther->DebugBoneNames.IsValidIndex(b))
				{
					pBase->DebugBoneNames.Add(pOther->DebugBoneNames[b]);
				}
#endif
			}
			else
			{
				otherToResult.Add(b, resultBoneIndex);
			}
		}

        // Fix bone parent indices of the bones added from pOther
        for ( int b=initialBones;b<pBase->GetBoneCount(); ++b)
        {
            int16_t sourceIndex = pBase->BoneParents[b];
            if (sourceIndex>=0)
            {
                pBase->BoneParents[b] = (int16_t)otherToResult[ sourceIndex ];
            }
        }
    }


    //---------------------------------------------------------------------------------------------
    //! Return null if there is no need to remap the mesh
    //---------------------------------------------------------------------------------------------
    inline void MeshRemapSkeleton(Mesh* Result, const Mesh* SourceMesh, const Skeleton* Skeleton, bool& bOutSuccess)
    {
		bOutSuccess = true;

        if (SourceMesh->GetVertexCount() == 0 || !SourceMesh->GetSkeleton() || SourceMesh->GetSkeleton()->GetBoneCount() == 0)
        {
			bOutSuccess = false;
            return;
        }

		Result->CopyFrom(*SourceMesh);
		Result->SetSkeleton(Skeleton);
    }
	
}
