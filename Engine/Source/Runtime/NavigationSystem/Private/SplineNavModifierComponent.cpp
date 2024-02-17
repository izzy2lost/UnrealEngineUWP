// Copyright Epic Games, Inc. All Rights Reserved.

#include "SplineNavModifierComponent.h"

#include "AI/NavigationSystemBase.h"
#include "AI/Navigation/NavigationRelevantData.h"
// #include "BezierUtilities.h"
#include "Components/SplineComponent.h"
#include "VisualLogger/VisualLogger.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(SplineNavModifierComponent)

namespace
{
	// Todo: temporary solution to circular dependency with BezierUtilities's new location, remove after fixing circular dependency
	void TessellateRecursive(TArray<FVector>& Output, const FVector& P0, const FVector& P1, const FVector& P2, const FVector& P3, const float ToleranceSqr, int Level, const int MaxLevel)
	{
		// Handle degenerate segment.
		FVector Dir = P3 - P0;
		if (Dir.IsNearlyZero())
		{
			Output.Add(P3);
			return;
		}

		// If the control points are close enough to approximate a line within tolerance, stop recursing.
		Dir = Dir.GetUnsafeNormal();
		const FVector RelP1 = P1 - P0;
		const FVector RelP2 = P2 - P0;
		const FVector ProjP1 = Dir * FVector::DotProduct(Dir, RelP1);
		const FVector ProjP2 = Dir * FVector::DotProduct(Dir, RelP2);
		const float DistP1Sqr = FVector::DistSquared(RelP1, ProjP1);
		const float DistP2Sqr = FVector::DistSquared(RelP2, ProjP2);
		if (DistP1Sqr < ToleranceSqr && DistP2Sqr < ToleranceSqr)
		{
			Output.Add(P3);
			return;
		}

		if (Level < MaxLevel)
		{
			// Split the curve in half and recurse.
			const FVector P01 = FMath::Lerp(P0, P1, 0.5f);
			const FVector P12 = FMath::Lerp(P1, P2, 0.5f);
			const FVector P23 = FMath::Lerp(P2, P3, 0.5f);
			const FVector P012 = FMath::Lerp(P01, P12, 0.5f);
			const FVector P123 = FMath::Lerp(P12, P23, 0.5f);
			const FVector P0123 = FMath::Lerp(P012, P123, 0.5f);

			TessellateRecursive(Output, P0, P01, P012, P0123, ToleranceSqr, Level + 1, MaxLevel);
			TessellateRecursive(Output, P0123, P123, P23, P3, ToleranceSqr, Level + 1, MaxLevel);
		}
	}

	void Tessellate(TArray<FVector>& Output, const FVector& P0, const FVector& P1, const FVector& P2, const FVector& P3, const float Tolerance, const int MaxLevel = 6)
	{
		TessellateRecursive(Output, P0, P1, P2, P3, Tolerance * Tolerance, 0, MaxLevel);
	}

	// Fetch the spline component from the actor
	const USplineComponent* GetSpline(const AActor* Owner)
	{
		if (!Owner)
		{
			UE_LOG(LogNavigation, Warning, TEXT("USplineNavModifierComponent has no owner, cannot proceed"));
			return nullptr;
		}

		const USplineComponent* Spline = Owner->GetComponentByClass<USplineComponent>();
		UE_CVLOG_UELOG(!Spline, Owner, LogNavigation, Warning, TEXT("USplineNavModifierComponent attached to \"%s\" could not find a spline component, cannot proceed"), *Owner->GetName());

		return Spline;
	}

	// Subdivide the spline into linear segments, adapting to its curvature (more curvy means more linear segments)
	void SubdivideSpline(TArray<FVector>& OutSubdivisions, const USplineComponent& Spline, const float SubdivisionThreshold)
	{
		// Sample at least 2 points
		const int32 NumSplinePoints = FMath::Max(Spline.GetNumberOfSplinePoints(), 2);

		// The USplineComponent's Hermite spline tangents are 3 times larger than Bezier tangents and we need to convert before tessellation
		constexpr double HermiteToBezierFactor = 3.0;

		FSplinePoint PrevSplinePoint;
		for (int32 PointIndex = 0; PointIndex < NumSplinePoints; PointIndex++)
		{
			FSplinePoint CurrSplinePoint = Spline.GetSplinePointAt(PointIndex, ESplineCoordinateSpace::World);

			if (PointIndex > 0)
			{
				// The first point of the segment is appended before tessellation since UE::CubicBezier::Tessellate does not add it
				OutSubdivisions.Add(PrevSplinePoint.Position);

				// Convert this segment of the spline from Hermite to Bezier and subdivide it 
				Tessellate(OutSubdivisions,
					PrevSplinePoint.Position,
					PrevSplinePoint.Position + PrevSplinePoint.LeaveTangent / HermiteToBezierFactor,
					CurrSplinePoint.Position - CurrSplinePoint.ArriveTangent / HermiteToBezierFactor,
					CurrSplinePoint.Position,
					SubdivisionThreshold);
			}

			PrevSplinePoint = CurrSplinePoint;
		}
	}
}

void USplineNavModifierComponent::CalculateBounds() const
{
	const USplineComponent* Spline = GetSpline(GetOwner());
	if (!Spline)
	{
		return;
	}

	const double Buffer = FMath::Max(StrokeWidth / 2.0, StrokeHeight / 2.0);
	Bounds = Spline->CalcBounds(Spline->GetComponentTransform()).GetBox().ExpandBy(Buffer);
}

void USplineNavModifierComponent::GetNavigationData(FNavigationRelevantData& Data) const
{
	const USplineComponent* Spline = GetSpline(GetOwner());
	if (!Spline)
	{
		return;
	}

	// Build a rectangle in the YZ plane used to sample the spline at each cross section
	constexpr int32 NumCrossSectionVertices = 4;
	const double StrokeHalfWidth = StrokeWidth / 2.0;
	const double StrokeHalfHeight = StrokeHeight / 2.0;
	TStaticArray<FVector, NumCrossSectionVertices> CrossSectionRect;
	CrossSectionRect[0] = FVector(0.0, -StrokeHalfWidth, -StrokeHalfHeight);
	CrossSectionRect[1] = FVector(0.0,  StrokeHalfWidth, -StrokeHalfHeight);
	CrossSectionRect[2] = FVector(0.0,  StrokeHalfWidth,  StrokeHalfHeight);
	CrossSectionRect[3] = FVector(0.0, -StrokeHalfWidth,  StrokeHalfHeight);

	// Vertices (in an arbitrary order) of a prism which will enclose each segment of the spline
	TStaticArray<FVector, NumCrossSectionVertices * 2> Tube;

	// Subdivide the spline so that high curvature sections get smaller and more linear segments than straighter sections
	TArray<FVector> Subdivisions;
	SubdivideSpline(Subdivisions, *Spline, GetSudivisionThreshold());
	const int32 NumSubdivisions = Subdivisions.Num();

	// Create volumes from the spline subdivisions and use them to mark the nav mesh with the given are
	const FTransform ComponentTransform = Spline->GetComponentTransform();
	int32 PrevIndex = Spline->IsClosedLoop() ? (NumSubdivisions - 1) : INDEX_NONE;
	for (int32 SubdivisionIndex = 0; SubdivisionIndex < NumSubdivisions; SubdivisionIndex++)
	{
		if (SubdivisionIndex > 0)
		{
			// Compute the rotation of this tube segment
			const double TubeAngle = (Subdivisions[SubdivisionIndex] - Subdivisions[PrevIndex]).HeadingAngle();
			const FQuat TubeRotation(FVector::UnitZ(), TubeAngle);
			
			// Compute the vertices of this tube segment
			for (int i = 0; i < NumCrossSectionVertices; i++)
			{
				// For each vertex of the tube segment, first rotate about the positive Z axis, then translate to the subdivision point
				Tube[i] = (TubeRotation * CrossSectionRect[i]) + Subdivisions[PrevIndex];
				Tube[i + NumCrossSectionVertices] = (TubeRotation * CrossSectionRect[i]) + Subdivisions[SubdivisionIndex];
			}

			// From the tube construct a convex hull whose volume will be used to mark the nav mesh with the selected AreaClass
			const FAreaNavModifier NavModifier(Tube, ENavigationCoordSystem::Type::Unreal, ComponentTransform, AreaClass);
			Data.Modifiers.Add(NavModifier);

			PrevIndex = SubdivisionIndex;
		}
	}
}

float USplineNavModifierComponent::GetSudivisionThreshold() const
{
	switch (SubdivisionLOD)
	{
	case ESubdivisionLOD::Ultra:
		return 10.0f;
	case ESubdivisionLOD::High:
		return 100.0f;
	case ESubdivisionLOD::Medium:
		return 250.0f;
	case ESubdivisionLOD::Low:
	default: // Fallthrough
		return 500.0f;
	}
}
