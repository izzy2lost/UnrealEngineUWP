// Copyright Epic Games, Inc.All Rights Reserved.
#include "Chaos/Collision/MeshContactGenerator.h"
#include "Chaos/DebugDrawQueue.h"

namespace Chaos
{
	extern FRealSingle Chaos_Collision_MeshContactNormalThreshold;
	extern FRealSingle Chaos_Collision_MeshContactNormalRejectionThreshold;
	extern int32 Chaos_Collision_MeshManifoldHashSize;
}

namespace Chaos::CVars
{
	extern int32 ChaosSolverDebugDrawMeshContacts;
}

namespace Chaos::Private
{
	FMeshContactGeneratorSettings::FMeshContactGeneratorSettings()
	{
		HashSize = FMath::RoundUpToPowerOfTwo(Chaos_Collision_MeshManifoldHashSize);
		FaceNormalDotThreshold = Chaos_Collision_MeshContactNormalThreshold;
		EdgeNormalDotRejectTolerance = Chaos_Collision_MeshContactNormalRejectionThreshold;
		BarycentricTolerance = FReal(1.e-3);
		MaxContactsBufferSize = 1000;
		bCullBackFaces = true;
		bUseTwoPassLoop = true;
	}

	FMeshContactGenerator::FMeshContactGenerator(const FMeshContactGeneratorSettings& InSettings)
		: Settings(InSettings)
		, EdgeTriangleIndicesMap(InSettings.HashSize)
		, VertexContactIndicesMap(InSettings.HashSize)
	{
	}

	void FMeshContactGenerator::Reset(const int32 InMaxTriangles, const int32 InMaxContacts)
	{
		Triangles.Reset(InMaxTriangles);

		Contacts.Reset(InMaxContacts);
		ContactDatas.Reset(InMaxContacts);

		// If all edges were shared between 2 triangles we have NumEdges = (3 * NumTriangles) / 2
		// but not all are shared so lets go with NumEdges = (2 * NumTriangles)
		EdgeTriangleIndicesMap.Reset(InMaxTriangles * 2);

		// Worst case - assume all contacts are vertex contacts
		VertexContactIndicesMap.Reset(InMaxContacts);
	}

	void FMeshContactGenerator::AddTriangleContacts(const FContactPointManifold& TriangleContactPoints, const int32 LocalTriangleIndex)
	{
		// Clamp the number of contacts we can add per mesh
		if (Contacts.Num() + TriangleContactPoints.Num() > Contacts.Max())
		{
			return;
		}

		FTriangleExt& Triangle = Triangles[LocalTriangleIndex];

		// We need to know how many vertex and edge collisions a triangle has on it (from collision with other triangle 
		// that share edges and vertices) so we avoid visiting triangles if we already know the contacts. Check all
		// contacts and assigne edge/vertex status as required. See GenerateMeshContacts()
		for (const FContactPoint& ContactPoint : TriangleContactPoints)
		{
			// See if we have a vertex or edge contact
			// @todo(chaos): we should be able to produce this as output from the manifold creator
			int32 LocalVertexID0, LocalVertexID1;
			GetTriangleEdgeVerticesAtPosition(
				ContactPoint.ShapeContactPoints[1],
				Triangle.GetVertex(0), Triangle.GetVertex(1), Triangle.GetVertex(2),
				LocalVertexID0, LocalVertexID1,
				Settings.BarycentricTolerance);

			const int32 VertexID0 = (LocalVertexID0 != INDEX_NONE) ? Triangle.GetVertexIndex(LocalVertexID0) : INDEX_NONE;
			const int32 VertexID1 = (LocalVertexID1 != INDEX_NONE) ? Triangle.GetVertexIndex(LocalVertexID1) : INDEX_NONE;

			const int32 NewContactIndex = Contacts.Num();

			const FRealSingle ContactNormalDotTriangleNormal = FRealSingle(FVec3::DotProduct(ContactPoint.ShapeContactNormal, Triangle.GetNormal()));

			const bool bIsFaceContact = (ContactNormalDotTriangleNormal > Settings.FaceNormalDotThreshold);

			// Register vertex and edge collisions
			if ((VertexID0 != INDEX_NONE) && (VertexID1 != INDEX_NONE))
			{
				// Edge collisions - if it's a face normal, tell each triangle using the edge it has a face contact
				if (bIsFaceContact)
				{
					Triangles[LocalTriangleIndex].AddFaceEdgeCollision();

					const int32 OtherLocalTriangleIndex = GetOtherTriangleIndexForEdge(LocalTriangleIndex, FContactEdgeID(VertexID0, VertexID1));
					if (OtherLocalTriangleIndex != INDEX_NONE)
					{
						Triangles[OtherLocalTriangleIndex].AddFaceEdgeCollision();
					}
				}
			}
			else if ((VertexID0 != INDEX_NONE) && (VertexID1 == INDEX_NONE))
			{
				// Vertex collisions - we only keep one vertex collision per vertex
				if (FVertexContactIndex* ExistingContactIndex = VertexContactIndicesMap.Find(VertexID0))
				{
					// We have an existing contact at this vertex. 
					// If it was a face contact just ignore the new contact, 
					// otherwise keep the contact with the most face-pointing normal.
					if (!ExistingContactIndex->bIsFaceContact)
					{
						FContactPoint& ExistingContact = Contacts[ExistingContactIndex->ContactIndex];
						FTriangleContactPointData& ExistingContactData = ContactDatas[ExistingContactIndex->ContactIndex];
						if (ContactNormalDotTriangleNormal > ExistingContactData.GetContactNormalDotTriangleNormal())
						{
							ExistingContact = ContactPoint;
							ExistingContact.FaceIndex = Triangle.GetTriangleIndex();

							ExistingContactData.SetVertexID(VertexID0);
							ExistingContactData.SetContactNormalDotTriangleNormal(ContactNormalDotTriangleNormal);
							ExistingContactData.SetTriangleIndex(LocalTriangleIndex);

							ExistingContactIndex->bIsFaceContact = bIsFaceContact;
						}
					}

					// Don't add this contact
					continue;
				}

				// This is the first time we hit this vertex so store the contact index with the vertex
				VertexContactIndicesMap.Emplace(VertexID0, VertexID0, NewContactIndex, bIsFaceContact);
			}

			// Store the contact, set the face index
			Contacts.Add(ContactPoint);
			Contacts[NewContactIndex].FaceIndex = Triangle.GetTriangleIndex();

			// Set the contact metadata and enable it
			ContactDatas.AddDefaulted();
			ContactDatas[NewContactIndex].SetEdgeOrVertexID({ VertexID0, VertexID1 });
			ContactDatas[NewContactIndex].SetContactNormalDotTriangleNormal(ContactNormalDotTriangleNormal);
			ContactDatas[NewContactIndex].SetTriangleIndex(LocalTriangleIndex);
			ContactDatas[NewContactIndex].SetEnabled();
		}
	}

	void FMeshContactGenerator::ProcessGeneratedContacts(const FRigidTransform3& ConvexTransform, const FRigidTransform3& MeshToConvexTransform)
	{
		// Contacts that get pruned or corrected will show as green
		DebugDrawContacts(ConvexTransform, FColor::Green, 0.5);

		PruneAndCorrectContacts();

		// Final contacts will be red
		DebugDrawContacts(ConvexTransform, FColor::Red, 1.0);

		// Visited triangles are white, ignored triangles are gray
		DebugDrawTriangles(ConvexTransform, FColor::White, FColor::Silver);

		FinalizeContacts(MeshToConvexTransform);
	}

	void FMeshContactGenerator::PruneAndCorrectContacts()
	{
		for (int32 ContactIndex = 0; ContactIndex < Contacts.Num(); ++ContactIndex)
		{
			FTriangleContactPointData& ContactPointData = ContactDatas[ContactIndex];

			// Reject back-faces
			if (!!Settings.bCullBackFaces && (ContactPointData.GetContactNormalDotTriangleNormal() < 0))
			{
				ContactPointData.SetDisabled();
				continue;
			}

			// Fix edge normals that aren't already close to a face normal
			if (ContactPointData.GetContactNormalDotTriangleNormal() < Settings.FaceNormalDotThreshold)
			{
				FixContactNormal(ContactIndex);
			}
		}

		// re-pack the contact array
		RemoveDisabledContacts();
	}

	void FMeshContactGenerator::FixContactNormal(const int32 ContactIndex)
	{
		FContactPoint& ContactPoint = Contacts[ContactIndex];
		FTriangleContactPointData& ContactPointData = ContactDatas[ContactIndex];

		const int32 LocalTriangleIndex = ContactPointData.GetTriangleIndex();
		const FTriangleExt& Triangle = Triangles[LocalTriangleIndex];
		const FVec3& TriangleNormal = Triangle.GetNormal();

		const FReal ContactDotNormal = ContactPointData.GetContactNormalDotTriangleNormal();

		// If we have an edge or vertex contact, make sure that the normal is in a valiid range, based
		// on the triangles that share that edge or vertex.
		if (ContactPointData.IsEdge())
		{
			// We have an edge collision. The contact normal must lie between the normals of the two faces using the edge
			const int32 OtherLocalTriangleIndex = GetOtherTriangleIndexForEdge(LocalTriangleIndex, ContactPointData.GetEdgeID());
			if (OtherLocalTriangleIndex != INDEX_NONE)
			{
				const FTriangleExt& OtherTriangle = Triangles[OtherLocalTriangleIndex];
				const FVec3& OtherTriangleNormal = OtherTriangle.GetNormal();

				const FReal MinContactDotNormal = FRealSingle(FVec3::DotProduct(OtherTriangleNormal, TriangleNormal));
				if (ContactDotNormal < MinContactDotNormal)
				{
					// We are outside the valid normal range for this edge
					// If our normal was very far away from a valid normal, drop the contact
					if (MinContactDotNormal - ContactDotNormal > Settings.EdgeNormalDotRejectTolerance)
					{
						ContactPointData.SetDisabled();
						return;
					}

					// Convert the edge collision to a face collision on one of the faces, selected to get the smallest depth
					FVec3 CorrectedContactNormal;
					int32 CorrectedTriangleIndex;
					const FReal OtherContactDotNormal = FVec3::DotProduct(ContactPoint.ShapeContactNormal, OtherTriangleNormal);
					if (ContactDotNormal >= OtherContactDotNormal)
					{
						CorrectedContactNormal = (ContactDotNormal > -SMALL_NUMBER) ? TriangleNormal : -TriangleNormal;
						CorrectedTriangleIndex = LocalTriangleIndex;
					}
					else
					{
						CorrectedContactNormal = (OtherContactDotNormal > -SMALL_NUMBER) ? OtherTriangleNormal : -OtherTriangleNormal;
						CorrectedTriangleIndex = OtherLocalTriangleIndex;
					}

					// NOTE: We keep the contact depth as it is because we know that the depth is a lower-bound. 
					// We have to update the ShapeContactPoint[0] because Phi is actually derived from the positions (the value in ContactPoint is just a cache of current state)
					// @todo(chaos): face selection logic might be better if we knew the contact velocity
					ContactPoint.ShapeContactNormal = CorrectedContactNormal;
					ContactPoint.ShapeContactPoints[0] = ContactPoint.ShapeContactPoints[1] + ContactPoint.Phi * CorrectedContactNormal;
					ContactPointData.SetTriangleIndex(CorrectedTriangleIndex);
				}
			}
		}
		else if (ContactPointData.IsVertex())
		{
			// We have a vertex collision. Ensure that the contact normal is in a valid range for the vertex based on the
			// triangles that share the vertex (there can be arbitarily many of these).
			for (int32 TriangleIndex = 0; TriangleIndex < Triangles.Num(); ++TriangleIndex)
			{
				const FTriangleExt& OtherTriangle = Triangles[TriangleIndex];
				if ((TriangleIndex != LocalTriangleIndex) && OtherTriangle.HasVertexIndex(ContactPointData.GetVertexID()))
				{
					FVec3 VertexA, VertexB, VertexC;
					if (OtherTriangle.GetVertexPosition(ContactPointData.GetVertexID(), VertexA) && OtherTriangle.GetOtherVertexPositions(ContactPointData.GetVertexID(), VertexB, VertexC))
					{
						// Does the contact normal point into the infinite prism formed by extruding the triangle along the face normal?
						// It does if the contact normal dotted with the edge plane normal is negative for both edge planes on the triangle that use the vertex.
						const FVec3 EdgeDelta0 = VertexB - VertexA;
						const FVec3 EdgeDelta1 = VertexC - VertexA;
						const FVec3& OtherTriangleNormal = OtherTriangle.GetNormal();

						const FReal EdgeSign0 = FVec3::DotProduct(FVec3::CrossProduct(ContactPoint.ShapeContactNormal, VertexB - VertexA), OtherTriangleNormal);
						const FReal EdgeSign1 = FVec3::DotProduct(FVec3::CrossProduct(ContactPoint.ShapeContactNormal, VertexC - VertexA), OtherTriangleNormal);
						if (FMath::Sign(EdgeSign0) == FMath::Sign(EdgeSign1))
						{
							const FVec3 Centroid = OtherTriangle.GetCentroid();
							const FReal NormalDotCentroid = FVec3::DotProduct(ContactPoint.ShapeContactNormal, Centroid - ContactPoint.ShapeContactPoints[1]);
							if (NormalDotCentroid > 0)
							{
								// If our normal was very far away from a valid normal, drop the contact
								const FReal MinContactDotNormal = FRealSingle(FVec3::DotProduct(OtherTriangleNormal, TriangleNormal));
								if (MinContactDotNormal - ContactDotNormal > Settings.EdgeNormalDotRejectTolerance)
								{
									ContactPointData.SetDisabled();
									return;
								}

								const FReal OtherContactDotNormal = FVec3::DotProduct(ContactPoint.ShapeContactNormal, OtherTriangleNormal);
								const FVec3 CorrectedContactNormal = OtherTriangleNormal;

								ContactPoint.ShapeContactNormal = CorrectedContactNormal;
								ContactPoint.ShapeContactPoints[0] = ContactPoint.ShapeContactPoints[1] + ContactPoint.Phi * CorrectedContactNormal;
							}
						}
					}
				}
			}
		}
	}

	void FMeshContactGenerator::RemoveDisabledContacts()
	{
		// Re-pack the contact point array without re-ordering
		const int32 NumContactPoints = Contacts.Num();
		int32 DestContactIndex = 0;		// Index to where the next enabled item goes
		int32 SrcContactIndex = 0;		// Index to the next enabled item
		while (SrcContactIndex < NumContactPoints)
		{
			if (!ContactDatas[SrcContactIndex].IsEnabled())
			{
				// Find the next enabled point index to use as the copy source
				while (++SrcContactIndex < NumContactPoints)
				{
					if (ContactDatas[SrcContactIndex].IsEnabled())
					{
						break;
					}
				}

				if (SrcContactIndex == NumContactPoints)
				{
					break;
				}
			}

			// If we have removed elements, copy source to dest
			if (DestContactIndex != SrcContactIndex)
			{
				Contacts[DestContactIndex] = Contacts[SrcContactIndex];
				ContactDatas[DestContactIndex] = ContactDatas[SrcContactIndex];
			}

			++DestContactIndex;
			++SrcContactIndex;
		}

		// Clip the array to the enabled set
		Contacts.SetNum(DestContactIndex, EAllowShrinking::No);
		ContactDatas.SetNum(DestContactIndex, EAllowShrinking::No);
	}

	void FMeshContactGenerator::FinalizeContacts(const FRigidTransform3& MeshToConvexTransform)
	{
		for (int32 ContactIndex = 0; ContactIndex < Contacts.Num(); ++ContactIndex)
		{
			FContactPoint& ContactPoint = Contacts[ContactIndex];

			ContactPoint.ShapeContactPoints[1] = MeshToConvexTransform.InverseTransformPositionNoScale(ContactPoint.ShapeContactPoints[1]);
			ContactPoint.ShapeContactNormal = MeshToConvexTransform.InverseTransformVectorNoScale(ContactPoint.ShapeContactNormal);
		}
	}

	void FMeshContactGenerator::DebugDrawContacts(const FRigidTransform3& ConvexTransform, const FColor& Color, const FReal LineScale)
	{
#if CHAOS_DEBUG_DRAW
		if (CVars::ChaosSolverDebugDrawMeshContacts && FDebugDrawQueue::GetInstance().IsDebugDrawingEnabled())
		{
			check(LineScale > 0);
			const FReal Duration = 0;
			const int32 DrawPriority = 10;
			for (int32 ContactIndex = 0; ContactIndex < Contacts.Num(); ++ContactIndex)
			{
				const FTriangleContactPointData& ContactPointData = ContactDatas[ContactIndex];
				if (ContactPointData.IsEnabled())
				{
					const FContactPoint& ContactPoint = Contacts[ContactIndex];
					const FVec3 P1 = ConvexTransform.TransformPosition(ContactPoint.ShapeContactPoints[1]);
					const FVec3 N = ConvexTransform.TransformVectorNoScale(ContactPoint.ShapeContactNormal);

					// Draw the normal from the triangle face
					FDebugDrawQueue::GetInstance().DrawDebugLine(P1, P1 + LineScale * FReal(50) * N, Color, false, FRealSingle(Duration), DrawPriority, FRealSingle(1.0 / LineScale));
				}
			}
		}
#endif
	}

	void FMeshContactGenerator::DebugDrawTriangles(const FRigidTransform3& ConvexTransform, const FColor& VisitedColor, const FColor& IgnoredColor)
	{
#if CHAOS_DEBUG_DRAW
		if (CVars::ChaosSolverDebugDrawMeshContacts && FDebugDrawQueue::GetInstance().IsDebugDrawingEnabled())
		{
			// NOTE: drawing in two loops so that the visited triangle edges draw over the ignored ones (priority doesn't seem to work)
			for (int32 TriangleIndex = 0; TriangleIndex < Triangles.Num(); ++TriangleIndex)
			{
				const FTriangleExt& Triangle = Triangles[TriangleIndex];
				if (Triangle.GetVisitIndex() == INDEX_NONE)
				{
					DebugDrawTriangle(ConvexTransform, Triangle, IgnoredColor);
				}
			}
			for (int32 TriangleIndex = 0; TriangleIndex < Triangles.Num(); ++TriangleIndex)
			{
				const FTriangleExt& Triangle = Triangles[TriangleIndex];
				if (Triangle.GetVisitIndex() != INDEX_NONE)
				{
					DebugDrawTriangle(ConvexTransform, Triangle, VisitedColor);
				}
			}
		}
#endif
	}

	void FMeshContactGenerator::DebugDrawTriangle(const FRigidTransform3& ConvexTransform, const FTriangleExt& Triangle, const FColor& Color)
	{
#if CHAOS_DEBUG_DRAW
		const FReal Duration = 0;
		const FReal LineScale = 0.5;
		const int8 DrawPriority = 10;

		const FVec3 V0 = ConvexTransform.TransformPosition(Triangle.GetVertex(0));
		const FVec3 V1 = ConvexTransform.TransformPosition(Triangle.GetVertex(1));
		const FVec3 V2 = ConvexTransform.TransformPosition(Triangle.GetVertex(2));

		FDebugDrawQueue::GetInstance().DrawDebugLine(V0, V1, Color, false, FRealSingle(Duration), DrawPriority, FRealSingle(LineScale));
		FDebugDrawQueue::GetInstance().DrawDebugLine(V1, V2, Color, false, FRealSingle(Duration), DrawPriority, FRealSingle(LineScale));
		FDebugDrawQueue::GetInstance().DrawDebugLine(V2, V0, Color, false, FRealSingle(Duration), DrawPriority, FRealSingle(LineScale));
#endif
	}
}
