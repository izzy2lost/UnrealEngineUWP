// Copyright Epic Games, Inc. All Rights Reserved.

#include "BuoyancyAlgorithms.h"
#include "BuoyancyStats.h"
#include "DrawDebugHelpers.h"
#include "Engine/Engine.h"
#include "Chaos/ParticleHandle.h"
#include "Chaos/DebugDrawQueue.h"
#include "Chaos/Utilities.h"

//
// CVars
//

extern bool bBuoyancyDebugDraw;


//
// Internal Functions
//

namespace
{
	using namespace Chaos;

	// Check to see if an object's shape is marked as already submerged
	bool IsShapeSubmerged_Internal(const TSparseArray<TBitArray<>>& SubmergedShapes, const int32 ParticleIndex, const int32 ShapeIndex)
	{
		return
			SubmergedShapes.IsValidIndex(ParticleIndex) &&
			SubmergedShapes[ParticleIndex].IsValidIndex(ShapeIndex) &&
			SubmergedShapes[ParticleIndex][ShapeIndex];
	}

	// Mark an object's shape as submerged
	void SubmergeShape_Internal(TSparseArray<TBitArray<>>& SubmergedShapes, const int32 ParticleIndex, const int32 ShapeIndex)
	{
		// If no shapes are tracked for this particle yet, add a bit array for it
		if (SubmergedShapes.IsValidIndex(ParticleIndex) == false)
		{
			SubmergedShapes.Insert(ParticleIndex, TBitArray<>(false, ShapeIndex + 1));
		}

		// If the bit array for this particle existed already but is too small, expand it
		else if (SubmergedShapes[ParticleIndex].IsValidIndex(ShapeIndex) == false)
		{
			SubmergedShapes[ParticleIndex].SetNum(ShapeIndex + 1, false);
		}

		// Mark this particle's shape as submerged
		SubmergedShapes[ParticleIndex][ShapeIndex] = true;
	}

	// NOTE: See SubdivideBounds(...) for a description of this algorithm
	//
	// TODO: Use an array + offset rather than raw ptr
	// TODO: Use a bounds stack instead of a recursive algo
	// TODO: Only build overlapping parts of hierarchy
	void SubdivideBounds_Internal(const FAABB3& Bounds, int32 NumSubdivisions, FAABB3** BoundsPtrPtr)
	{
		// We assume that the buffer pointed to by *BoundsPtr 
		const FVec3 Min = Bounds.Min();
		const FVec3 Max = Bounds.Max();
		const FVec3 Cen = Bounds.GetCenter();

		// Decrement subdivisions and track whether we have any more to go
		const bool bSubdivide = --NumSubdivisions > 0;

		// Get a pointer to the first writable element in the array of bounds,
		// or to a temporary swap space of bounds if we haven't reached the
		// leaf level yet
		FAABB3 BoundsSwap[8];
		FAABB3* BoundsPtr = bSubdivide ? BoundsSwap : *BoundsPtrPtr;

		// Generate 8 subdivisions
		BoundsPtr[0] = FAABB3(Min, Cen);
		BoundsPtr[1] = FAABB3(FVec3(Cen.X, Min.Y, Min.Z), FVec3(Max.X, Cen.Y, Cen.Z));
		BoundsPtr[2] = FAABB3(FVec3(Min.X, Cen.Y, Min.Z), FVec3(Cen.X, Max.Y, Cen.Z));
		BoundsPtr[3] = FAABB3(FVec3(Min.X, Min.Y, Cen.Z), FVec3(Cen.X, Cen.Y, Max.Z));
		BoundsPtr[4] = FAABB3(Cen, Max);
		BoundsPtr[5] = FAABB3(FVec3(Min.X, Cen.Y, Cen.Z), FVec3(Cen.X, Max.Y, Max.Z));
		BoundsPtr[6] = FAABB3(FVec3(Cen.X, Min.Y, Cen.Z), FVec3(Max.X, Cen.Y, Max.Z));
		BoundsPtr[7] = FAABB3(FVec3(Cen.X, Cen.Y, Min.Z), FVec3(Max.X, Max.Y, Cen.Z));

		if (bSubdivide)
		{
			// Recurse if we haven't reached the leaf level yet
			for (int32 Index = 0; Index < 8; ++Index)
			{
				SubdivideBounds_Internal(BoundsPtr[Index], NumSubdivisions, BoundsPtrPtr);
			}
		}
		else
		{
			// Increment the bounds ptr by 8 if we just wrote 8 leaves
			(*BoundsPtrPtr) += 8;
		}
	}
}


//
// Algorithms
//

namespace BuoyancyAlgorithms
{
	using namespace Chaos;

	bool ComputeSubmergedVolume(const FGeometryParticleHandle* ParticleA, const FGeometryParticleHandle* ParticleB, const int32 NumSubdivisions, const float MinVolume, TSparseArray<TBitArray<>>& SubmergedShapes, float& SubmergedVol, FVec3& SubmergedCoM)
	{
		SCOPE_CYCLE_COUNTER(STAT_BuoyancyAlgorithms_ComputeSubmergedVolume)

		SubmergedVol = 0.f;
		SubmergedCoM = FVec3::ZeroVector;

		const int32 ParticleIndexB = ParticleB->UniqueIdx().Idx;

		const FImplicitObject* RootImplicitA = ParticleA->GetGeometry();
		const FImplicitObject* RootImplicitB = ParticleB->GetGeometry();

		const FConstGenericParticleHandle PA = ParticleA;
		const FConstGenericParticleHandle PB = ParticleB;
		const FShapeInstanceArray& ShapeInstancesA = ParticleA->ShapeInstances();
		const FShapeInstanceArray& ShapeInstancesB = ParticleB->ShapeInstances();

		// Particle transforms
		const FRigidTransform3 ParticleWorldTransformA = PA->GetTransformPQ();
		const FRigidTransform3 ParticleWorldTransformB = PB->GetTransformPQ();
		const FRigidTransform3 ParticleTransformAToB = ParticleWorldTransformA.GetRelativeTransform(ParticleWorldTransformB);

		// Detect collisions between Implicit Hierarchy of ParticleA and Implicit Hierarchy of
		// ParticleB. Given an ImplicitObject from ParticleA (which we know overlaps the bounds
		// of some parts of ParticleB), run collision detection on ImplicitA against the implicit
		// object hierarchy of ParticleB.
		RootImplicitA->VisitLeafObjects(
			[ParticleA, &ShapeInstancesA, &ParticleWorldTransformA,
			ParticleIndexB, &ShapeInstancesB, RootImplicitB, &ParticleWorldTransformB, &ParticleTransformAToB,
			&NumSubdivisions, &MinVolume, &SubmergedShapes, &SubmergedVol, &SubmergedCoM]
			(const FImplicitObject* ImplicitA, const FRigidTransform3& RelativeTransformA, const int32 RootObjectIndexA, const int32 ObjectIndex, const int32 LeafObjectIndexA)
		{
			const FAABB3 RelativeBoundsA = ImplicitA->CalculateTransformedBounds(RelativeTransformA);
			const FAABB3 ShapeBoundsAInB = RelativeBoundsA.TransformedAABB(ParticleTransformAToB);
			const int32 ShapeIndexA = (ShapeInstancesA.IsValidIndex(RootObjectIndexA)) ? RootObjectIndexA : 0;
			const FShapeInstance* ShapeInstanceA = ShapeInstancesA[ShapeIndexA].Get();

			//
			// TODO: Dig down into ImplicitB's leaves based on its type, so that
			// we don't end up going down to the tiniest particles of a GC.
			//

			// Detect collisions between ImplicitA and Implicit Hierarchy of ParticleB
			RootImplicitB->VisitOverlappingLeafObjects(ShapeBoundsAInB,
				[ParticleA, ImplicitA, ShapeInstanceA, LeafObjectIndexA,
				&ParticleWorldTransformA, &RelativeTransformA,
				ParticleIndexB, &ParticleWorldTransformB,
				&NumSubdivisions, &MinVolume, &SubmergedShapes, &SubmergedVol, &SubmergedCoM]
				(const FImplicitObject* ImplicitB, const FRigidTransform3& RelativeTransformB, const int32 RootObjectIndexB, const int32 ObjectIndexB, const int32 LeafObjectIndexB)
			{
				// If this shape has already been submerged, skip it to avoid double-counting
				// any buoyancy contributions.
				if (IsShapeSubmerged_Internal(SubmergedShapes, ParticleIndexB, ObjectIndexB))
				{
					return;
				}

				// Get shape world transforms
				const FRigidTransform3 ShapeWorldTransformA = RelativeTransformA * ParticleWorldTransformA;
				const FRigidTransform3 ShapeWorldTransformB = RelativeTransformB * ParticleWorldTransformB;

				// Get local bounds of objects
				const FAABB3 BoxAInA = ImplicitA->BoundingBox();
				const FAABB3 BoxBInB = ImplicitB->BoundingBox();

				// AABB intersect check
				const FAABB3 ShapeWorldBoundsA = BoxAInA.TransformedAABB(ShapeWorldTransformA);
				const FAABB3 ShapeWorldBoundsB = BoxBInB.TransformedAABB(ShapeWorldTransformB);
				if (!ShapeWorldBoundsA.Intersects(ShapeWorldBoundsB)) { return; }

				// OOBB vs AABB intersect checks
				const FRigidTransform3 ShapeTransformBToA = ShapeWorldTransformB.GetRelativeTransform(ShapeWorldTransformA);
				const FAABB3 BoxBInA = BoxBInB.TransformedAABB(ShapeTransformBToA);
				if (!ImplicitA->BoundingBox().Intersects(BoxBInA)) { return; }
				const FAABB3 BoxAInB = BoxAInA.TransformedAABB(ShapeTransformBToA.Inverse());
				if (!ImplicitB->BoundingBox().Intersects(BoxAInB)) { return; }

				// Generate subdivided bounds list
				TArray<FAABB3> BoxesInB;
				SubdivideBounds(BoxBInB, NumSubdivisions, MinVolume, BoxesInB);

				// Loop over every subdivision of the shape bounds, counting up submerged portions
				bool bSubmerged = false;
				for (const FAABB3& BoxInB : BoxesInB)
				{
#if ENABLE_DRAW_DEBUG
					if (bBuoyancyDebugDraw)
					{
						// Subdivided Rigid OOBB
						Chaos::FDebugDrawQueue::GetInstance().DrawDebugBox(
							ShapeWorldTransformB.TransformPosition(BoxInB.GetCenter()),
							BoxInB.Extents() * .5f,
							ShapeWorldTransformB.GetRotation(),
							FColor::Green, false, -1.f, SDPG_Foreground, 1.f);
					}
#endif

					// Compute approximate submerged bounds & update submersion
					const float WaterZ = ShapeWorldBoundsA.Max().Z;
					FAABB3 SubmergedBoundsInB;
					if (ComputeSubmergedBounds(WaterZ, BoxInB, ShapeWorldTransformB, SubmergedBoundsInB))
					{
						// At this point we know that the shape is submerged
						bSubmerged = true;

						// This bounds box is submerged. Compute it's volume and center of mass
						// in world space, and add those contributions to the submerged quantity.
						const FVec3 LeafSubmergedCoM = ShapeWorldTransformB.TransformPosition(SubmergedBoundsInB.GetCenter());
						const float LeafSubmergedVol = SubmergedBoundsInB.GetVolume();
						SubmergedVol += LeafSubmergedVol;
						SubmergedCoM += LeafSubmergedCoM * LeafSubmergedVol;

						// Make sure the volume of the submerged portion never exceeds the total
						// volume of the leaf bounds
						const float LeafMaxVol = BoxBInB.GetVolume() + UE_SMALL_NUMBER;
						ensureAlwaysMsgf(LeafSubmergedVol <= LeafMaxVol, TEXT("BuoyancyAlgorithms::ComputeSubmergedVolume: The volume of the submerged portion of the leaf bounds has somehow exceeded the volume of the overall leaf bounds."));

#if ENABLE_DRAW_DEBUG
						if (bBuoyancyDebugDraw)
						{
							// Submerged Bounds
							Chaos::FDebugDrawQueue::GetInstance().DrawDebugBox(
								ShapeWorldTransformB.TransformPosition(SubmergedBoundsInB.GetCenter()),
								SubmergedBoundsInB.Extents() * .5f,
								ShapeWorldTransformB.GetRotation(),
								FColor::Orange, false, -1.f, SDPG_Foreground, 2.f);
						}
#endif
					}
				}

				// If the shape pair overlapped, flag the submersion
				if (bSubmerged)
				{
					SubmergeShape_Internal(SubmergedShapes, ParticleIndexB, ObjectIndexB);

#if ENABLE_DRAW_DEBUG
					if (bBuoyancyDebugDraw)
					{
						// Water OOBB
						Chaos::FDebugDrawQueue::GetInstance().DrawDebugBox(
							ShapeWorldTransformA.TransformPosition(BoxAInA.GetCenter()),
							BoxAInA.Extents() * .5f,
							ShapeWorldTransformA.GetRotation(),
							FColor::Cyan, false, -1.f, SDPG_Foreground, 2.f);

						// Rigid OOBB
						Chaos::FDebugDrawQueue::GetInstance().DrawDebugBox(
							ShapeWorldTransformB.TransformPosition(BoxBInB.GetCenter()),
							BoxBInB.Extents() * .5f,
							ShapeWorldTransformB.GetRotation(),
							FColor::Green, false, -1.f, SDPG_Foreground, 2.f);
					}
#endif
				}
			});
		});

		// We've accumulated submersion data for all submerged leaves, now we need to
		// normalize the submerged center of mass
		if (SubmergedVol > SMALL_NUMBER)
		{
			SubmergedCoM /= SubmergedVol;

#if ENABLE_DRAW_DEBUG
			if (bBuoyancyDebugDraw)
			{
				// Draw a point at the submerged CoM
				Chaos::FDebugDrawQueue::GetInstance().DrawDebugPoint(SubmergedCoM, FColor::Orange, false, -1.f, SDPG_Foreground, 5.f);
			}
#endif

			return true;
		}
		else
		{
			return false;
		}
	}


	bool ComputeSubmergedBounds(float WaterZ, const FAABB3& RigidBox, const FRigidTransform3& RigidTransform, FAABB3& OutSubmergedBounds)
	{
		SCOPE_CYCLE_COUNTER(STAT_BuoyancyAlgorithms_ComputeSubmergedBounds)

		// Get a point and a normal direction on the water representing the surface
		// of the water in the space of the rigid object.
		const FVec3 SurfaceNormal = RigidTransform.InverseTransformVector(FVec3::UpVector);
		const FVec3 SurfacePoint = RigidTransform.InverseTransformPosition(FVec3::UpVector * WaterZ);

		// Partly submerged object can have at most 10 points intersecting
		// with the water surface
		TArray<FVec3, TInlineAllocator<10>> SubmergedVertices;

		// Find bound box indices that are submerged
		for (int32 VertexIndex = 0; VertexIndex < 8; ++VertexIndex)
		{
			const FVec3 Vertex = RigidBox.GetVertex(VertexIndex);
			const float Depth = SurfaceNormal.Dot(SurfacePoint - Vertex);
			if (Depth > 0.f)
			{
				SubmergedVertices.Add(Vertex);
			}
		}

		// If no box corners were submerged, then there can be no submerged edges so stop here
		if (SubmergedVertices.Num() == 0)
		{
			return false;
		}

		// Find intersections of AABB edges with the surface and add these
		// points to the submerged verts list
		TArray<FVec3, TInlineAllocator<6>> SurfaceVertices;
		for (int32 EdgeIndex = 0; EdgeIndex < 12; ++EdgeIndex)
		{
			const FAABBEdge Edge = RigidBox.GetEdge(EdgeIndex);
			const FVec3& Vert0 = RigidBox.GetVertex(Edge.VertexIndex0);
			const FVec3& Vert1 = RigidBox.GetVertex(Edge.VertexIndex1);
			const float Depth0 = SurfaceNormal.Dot(SurfacePoint - Vert0);
			const float Depth1 = SurfaceNormal.Dot(SurfacePoint - Vert1);
			const bool bSubmerged0 = (Depth0 > 0.f);
			const bool bSubmerged1 = (Depth1 > 0.f);
			if (bSubmerged0 ^ bSubmerged1)
			{
				const float DepthDiff = Depth0 - Depth1;
				const float DepthAlpha = Depth0 / DepthDiff; // NOTE: Since one is submerged and one is not, we know that |DepthDiff| > 0
				const FVec3 SurfaceVertex = FMath::Lerp(Vert0, Vert1, DepthAlpha);
				SubmergedVertices.Add(SurfaceVertex);
			}
		}

		// If we didn't have any submerged vertices, stop here
		if (SubmergedVertices.Num() == 0)
		{
			return false;
		}

#if ENABLE_DRAW_DEBUG
		if (bBuoyancyDebugDraw)
		{
			// Draw a point at each submerged vertex
			for (const FVec3& SubmergedVertex : SubmergedVertices)
			{
				Chaos::FDebugDrawQueue::GetInstance().DrawDebugPoint(RigidTransform.TransformPosition(SubmergedVertex), FColor::Magenta, false, -1.f, SDPG_Foreground, 3.f);
			}
		}
#endif

		// Build and return an AABB which contains the submerged vertices of the rigid bounds
		OutSubmergedBounds = FAABB3(SubmergedVertices[0], SubmergedVertices[0]);
		for (int32 VertexIndex = 1; VertexIndex < SubmergedVertices.Num(); ++VertexIndex)
		{
			OutSubmergedBounds.GrowToInclude(SubmergedVertices[VertexIndex]);
		}
		return true;
	}


	bool SubdivideBounds(const FAABB3& Bounds, int32 NumSubdivisions, const float MinVolume, TArray<FAABB3>& OutBounds)
	{
		SCOPE_CYCLE_COUNTER(STAT_BuoyancyAlgorithms_SubdivideBounds)

		// Initialize bounds to an array of just the original bounds;
		OutBounds = TArray<FAABB3>{ Bounds };

		// If the bounds volume is already too small to subdivide, return the original bounds only
		const float Volume = Bounds.GetVolume();
		if (Volume < SMALL_NUMBER || Volume <= MinVolume)
		{
			return false;
		}

		// If V_0 is the volume of the outermost AABB, then the volume of
		// one box in the n'th subdivision of an AABB is given by
		// 
		// V_n = V_0 * 2^(-3 n)
		//
		// We can invert this equation to find the level of subdivisions
		// at which the volume becomes smaller than V_min.
		//
		// n < -(1/3) * log2(V_min / V_0)
		const int32 MaxNumSubdivisions = int32(-(1.f/3.f) * FMath::Log2(MinVolume / Volume));
		NumSubdivisions = FMath::Min(MaxNumSubdivisions, NumSubdivisions);

		// If we have any subdivisions to process, do them now
		if (NumSubdivisions > 0)
		{
			// Predetermine the total number of boxes we're going to generate, and allocate them
			// in a block.
			const int32 NumBounds = (int32)FMath::Pow(8.f, NumSubdivisions);
			OutBounds.SetNum(NumBounds);
			FAABB3* BoundsPtr = OutBounds.GetData();

			// Recursively generate boxes
			SubdivideBounds_Internal(Bounds, NumSubdivisions, &BoundsPtr);

			// Make sure that we didn't write too many or too few boxes
			const FAABB3* BoundsPtrMax = OutBounds.GetData() + NumBounds;
			ensureAlways(BoundsPtr == BoundsPtrMax);

			// Return the box array
			return true;
		}

		return false;
	}

	bool ComputeBuoyantForce(const FPBDRigidParticleHandle* RigidParticle, const float DeltaSeconds, const float WaterDensity, const float WaterDrag, const FVec3& GravityAccelVec, const FVec3& SubmergedCoM, const float SubmergedVol, FVec3& OutDeltaV, FVec3& OutDeltaW)
	{
		SCOPE_CYCLE_COUNTER(STAT_BuoyancyAlgorithms_ComputeBuoyantForces)

		// NOTE: We assume gravity is -Z for perf... If we want to support buoyancy for
		// weird gravity setups, this is where we'd have to fix it up.
		const FVec3 GravityDir = FVec3::DownVector;
		const float GravityAccel = FVec3::DotProduct(GravityDir, GravityAccelVec);

		// Compute buoyant force
		//
		// NOTE: This is easy to compute with Archimedes' principle
		// https://en.wikipedia.org/wiki/Buoyancy
		//
		const float BuoyantForce = WaterDensity * SubmergedVol * GravityAccel;
		//                       = [ kg / cm^3 ] * [ cm^3 ]    * [cm / s^2]
		//                       = [ kg * cm / s^2 ]
		//                       = [ force ]

		// Only proceed if buoyant force isn't vanishingly small
		if (BuoyantForce < SMALL_NUMBER)
		{
			return false;
		}

		// Get a generic particle wrapper
		FConstGenericParticleHandle RigidGeneric(RigidParticle);

		// Get inverse inertia data to compute world space accelerations
		const FVec3 WorldCoM = RigidGeneric->PCom();
		const FVec3 CoMDiff = SubmergedCoM - WorldCoM;
		const FMatrix33 WorldInvI = Utilities::ComputeWorldSpaceInertia(RigidGeneric->RCom(), RigidGeneric->ConditionedInvI());

		// Compute world buoyant force and torque
		const FVec3 WorldForce = -GravityDir * BuoyantForce;
		const FVec3 WorldTorque = Chaos::FVec3::CrossProduct(CoMDiff, WorldForce);

		// Use inertia to convert forces to accelerations
		const FVec3 LinearAccel = RigidGeneric->InvM() * WorldForce;
		const FVec3 AngularAccel = WorldInvI * WorldTorque;

		// Integrate to get delta velocities
		OutDeltaV = LinearAccel * DeltaSeconds;
		OutDeltaW = AngularAccel * DeltaSeconds;

		// Compute water drag force
		//
		// NOTE: This is a very approximate "ether drag" style model here, probably 
		// we should scale the model with submerged volume for more accuracy,
		// and apply the drag force in opposition to the linear motion of the submerged
		// center of mass.
		const float DragFactor = FMath::Max(0.f, 1.f - (WaterDrag * DeltaSeconds));

		// Account for water drag in deltas
		OutDeltaV = (DragFactor * OutDeltaV) + (DragFactor - 1.f) * RigidParticle->V();
		OutDeltaW = (DragFactor * OutDeltaW) + (DragFactor - 1.f) * RigidParticle->W();

		//
		return true;
	}
}
