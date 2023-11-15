// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Chaos/Core.h"
#include "Chaos/PBDSoftsEvolutionFwd.h"
#include "Chaos/TriangleMesh.h"
#include "Chaos/HierarchicalSpatialHash.h"
#include "Chaos/CollectionPropertyFacade.h"
#include "Templates/PimplPtr.h"

// This does initialization for PBDCollisionSpringConstraints and PBDTriangleMeshIntersections, 
// including intersection detection and global intersection analysis
//
namespace Chaos::Softs
{
class FPBDTriangleMeshCollisions
{
public:
	struct FContourMinimizationIntersection
	{
		TVec2<int32> EdgeVertices;
		TVec3<int32> FaceVertices;
		FSolverVec3 LocalGradientVector;
		FSolverVec3 GlobalGradientVector;
	};
	
	// Vertices (and Triangles) are assigned FGIAColors by flood-filling global intersection contours.
	// ContourIndex represents which pair of global contours, up to 31 unique contour pairs--then the contour index will reused. 
	// In practice this seems to be enough unique contours. It's OK if multiple contours have the same index as long
	// as their intersecting regions don't interact, which is unlikely.
	// Contours come in pairs representing two regions intersecting (except for "loop" contours where a region of cloth intersects itself--think pinch regions). 
	// These are "Colors" represented by setting the ColorBit to 0 or 1 corresponding with the given contour index.
	struct FGIAColor
	{
		int32 ContourIndexBits = 0;
		int32 ColorBits = 0;

		static constexpr int32 LoopContourIndex = 0;
		static constexpr int32 LoopBits = 1 << LoopContourIndex;
		static constexpr int32 NonLoopMask = ~LoopBits;
		static constexpr int32 BoundaryContourIndex = 0; // Same as Loop. Loop is ColorA, Boundary is ColorB
		
		void SetContourColor(int32 ContourIndex, bool bIsColorB)
		{
			check(ContourIndex < 32);
			ContourIndexBits |= 1 << ContourIndex;
			if (bIsColorB)
			{
				ColorBits |= 1 << ContourIndex;
			}
			else
			{
				ColorBits &= ~(1 << ContourIndex);
			}
		}

		bool HasContourColorSet(int32 ContourIndex) const
		{
			check(ContourIndex < 32);
			return ContourIndexBits & (1 << ContourIndex);
		}

		bool IsLoop() const
		{
			return (ContourIndexBits & LoopBits) && (ColorBits & LoopBits);
		}

		void SetLoop()
		{
			SetContourColor(LoopContourIndex, true);
		}

		void SetBoundary()
		{
			SetContourColor(BoundaryContourIndex, false);
		}

		bool IsBoundary() const
		{
			return (ContourIndexBits & LoopBits) && !(ColorBits & LoopBits);
		}

		// Because they are opposite colors with a shared contour index. This will cause repulsion forces to attract and fix the intersection.
		// NOTE: We flip normals if ANY of the TriVertColors are opposite the PointColor or if the TriColor (used for thin regions is flipped). This does a better job for thin features
		// than only flipping normals if ALL TriVertColors are opposite.
		static bool ShouldFlipNormal(const FGIAColor& Color0, const FGIAColor& Color1)
		{
			const int32 SharedContourBits = Color0.ContourIndexBits & Color1.ContourIndexBits & NonLoopMask;
			const int32 FlippedColorBits = Color0.ColorBits ^ Color1.ColorBits;
			return FlippedColorBits & SharedContourBits;
		}
	};

	// Debug display of intersection contours
	struct FBarycentricPoint
	{
		FSolverVec2 Bary;
		TVec3<int32> Vertices;
	};

	enum struct FContourType : int8
	{
		Open = 0,
		Loop,
		BoundaryClosed,
		BoundaryOpen,
		Contour0,
		Contour1,
		Count
	};

	static bool IsEnabled(const FCollectionPropertyConstFacade& PropertyCollection)
	{
		return PropertyCollection.IsEnabled(FName(TEXT("SelfCollisionStiffness")).ToString(), false);  // Don't use UE_CHAOS_DECLARE_PROPERTYCOLLECTION_NAME here, SelfCollisionStiffness is only needed for activation
	}

	FPBDTriangleMeshCollisions(
		const int32 InOffset,
		const int32 InNumParticles,
		const FTriangleMesh& InTriangleMesh,
		const FCollectionPropertyConstFacade& PropertyCollection
	)
		:TriangleMesh(InTriangleMesh)
		, Offset(InOffset)
		, NumParticles(InNumParticles)
		, bUseSelfIntersections(GetUseSelfIntersections(PropertyCollection, false))
		, bGlobalIntersectionAnalysis(GetUseSelfIntersections(PropertyCollection, false) && GetUseGlobalIntersectionAnalysis(PropertyCollection, true))
		, bContourMinimization(GetUseSelfIntersections(PropertyCollection, false) && GetUseContourMinimization(PropertyCollection, true))
		, NumContourMinimizationPostSteps(GetUseSelfIntersections(PropertyCollection, false) ? GetNumContourMinimizationPostSteps(PropertyCollection, 0) : 0)
		, bUseGlobalPostStepContours(GetUseGlobalPostStepContours(PropertyCollection, true))
		, UseSelfIntersectionsIndex(PropertyCollection)
		, UseGlobalIntersectionAnalysisIndex(PropertyCollection)
		, UseContourMinimizationIndex(PropertyCollection)
		, NumContourMinimizationPostStepsIndex(PropertyCollection)
		, UseGlobalPostStepContoursIndex(PropertyCollection)
	{}

	FPBDTriangleMeshCollisions(
		const int32 InOffset,
		const int32 InNumParticles,
		const FTriangleMesh& InTriangleMesh,
		bool bInGlobalIntersectionAnalysis,
		bool bInContourMinimization
	)
		:TriangleMesh(InTriangleMesh)
		, Offset(InOffset)
		, NumParticles(InNumParticles)
		, bUseSelfIntersections(bInGlobalIntersectionAnalysis || bInContourMinimization)
		, bGlobalIntersectionAnalysis(bInGlobalIntersectionAnalysis)
		, bContourMinimization(bInContourMinimization)
		, UseSelfIntersectionsIndex(ForceInit)
		, UseGlobalIntersectionAnalysisIndex(ForceInit)
		, UseContourMinimizationIndex(ForceInit)
		, NumContourMinimizationPostStepsIndex(ForceInit)
		, UseGlobalPostStepContoursIndex(ForceInit)
	{}

	virtual ~FPBDTriangleMeshCollisions() = default;

	void SetProperties(const FCollectionPropertyConstFacade& PropertyCollection)
	{
		const bool bSelfIntersectionsMutable = IsUseSelfIntersectionsMutable(PropertyCollection);
		if (bSelfIntersectionsMutable)
		{
			bUseSelfIntersections = GetUseSelfIntersections(PropertyCollection);
		}
		if (bUseSelfIntersections)
		{
			if (bSelfIntersectionsMutable || IsUseGlobalIntersectionAnalysisMutable(PropertyCollection))
			{
				bGlobalIntersectionAnalysis = GetUseGlobalIntersectionAnalysis(PropertyCollection);
			}
			if (bSelfIntersectionsMutable || IsUseContourMinimizationMutable(PropertyCollection))
			{
				bContourMinimization = GetUseContourMinimization(PropertyCollection);
			}
			if (bSelfIntersectionsMutable || IsNumContourMinimizationPostStepsMutable(PropertyCollection))
			{
				NumContourMinimizationPostSteps = GetNumContourMinimizationPostSteps(PropertyCollection);
			}
			if (bSelfIntersectionsMutable || IsUseGlobalPostStepContoursMutable(PropertyCollection))
			{
				bUseGlobalPostStepContours = GetUseGlobalPostStepContours(PropertyCollection);
			}
		}
		else
		{
			bGlobalIntersectionAnalysis = bContourMinimization = false;
			NumContourMinimizationPostSteps = 0;
		}
	}

	template<typename SolverParticlesOrRange>
	CHAOS_API void Init(const SolverParticlesOrRange& Particles, const FSolverReal MinProximityQueryRadius = (FSolverReal)0.);

	template<typename SolverParticlesOrRange>
	CHAOS_API void PostStepInit(const SolverParticlesOrRange& Particles);

	void SetGlobalIntersectionAnalysis(bool bInGlobalIntersectionAnalysis) { bGlobalIntersectionAnalysis = bInGlobalIntersectionAnalysis; }
	void SetContourMinimization(bool bInContourMinimization) { bContourMinimization = bInContourMinimization; }

	int32 GetNumContourMinimizationPostSteps() const
	{
		return NumContourMinimizationPostSteps;
	}

	const FTriangleMesh::TSpatialHashType<FSolverReal>& GetSpatialHash() const { return SpatialHash; }
	const TArray<FContourMinimizationIntersection>& GetContourMinimizationIntersections() const { return ContourMinimizationIntersections; }
	const TConstArrayView<FGIAColor> GetVertexGIAColors() const { return bGlobalIntersectionAnalysis && VertexGIAColors.Num() == NumParticles ? TConstArrayView<FGIAColor>(VertexGIAColors.GetData() - Offset, NumParticles + Offset) : TConstArrayView<FGIAColor>(); }
	const TArray<FGIAColor>& GetTriangleGIAColors() const { return TriangleGIAColors; }
	const TArray<TArray<FBarycentricPoint>>& GetIntersectionContourPoints() const { return IntersectionContourPoints; }
	const TArray<FContourType>& GetIntersectionContourTypes() const { return IntersectionContourTypes; }

	// Same data but for the post step contour minimization. Just making them separate arrays
	// for debug draw purposes.
	const TArray<FContourMinimizationIntersection>& GetPostStepContourMinimizationIntersections() const { return PostStepContourMinimizationIntersections; }
	const TArray<TArray<FBarycentricPoint>>& GetPostStepIntersectionContourPoints() const { return PostStepIntersectionContourPoints; }
private:

	const FTriangleMesh& TriangleMesh;
	int32 Offset;
	int32 NumParticles;
	bool bUseSelfIntersections;
	bool bGlobalIntersectionAnalysis;
	bool bContourMinimization;
	
	int32 NumContourMinimizationPostSteps = 0;
	bool bUseGlobalPostStepContours = true;
	
	FTriangleMesh::TSpatialHashType<FSolverReal> SpatialHash;
	TArray<FContourMinimizationIntersection> ContourMinimizationIntersections;
	TArray<FGIAColor> VertexGIAColors;
	TArray<FGIAColor> TriangleGIAColors;

	// Scratch buffers used by Init and PostInit. They live here so they can be reused rather than reallocated.
	struct FScratchBuffers;
	TPimplPtr<FScratchBuffers> ScratchBuffers;

	// Debug display of intersection contours
	TArray<TArray<FBarycentricPoint>> IntersectionContourPoints;
	TArray<FContourType> IntersectionContourTypes;

	// PostStep contour data. Keeping it separate for debug drawing for now.
	TArray<FContourMinimizationIntersection> PostStepContourMinimizationIntersections;
	TArray<TArray<FBarycentricPoint>> PostStepIntersectionContourPoints;

	UE_CHAOS_DECLARE_PROPERTYCOLLECTION_NAME(UseSelfIntersections, bool);
	UE_CHAOS_DECLARE_PROPERTYCOLLECTION_NAME(UseGlobalIntersectionAnalysis, bool);
	UE_CHAOS_DECLARE_PROPERTYCOLLECTION_NAME(UseContourMinimization, bool);
	UE_CHAOS_DECLARE_PROPERTYCOLLECTION_NAME(NumContourMinimizationPostSteps, int32);
	UE_CHAOS_DECLARE_PROPERTYCOLLECTION_NAME(UseGlobalPostStepContours, bool);
};

}  // End namespace Chaos::Softs
