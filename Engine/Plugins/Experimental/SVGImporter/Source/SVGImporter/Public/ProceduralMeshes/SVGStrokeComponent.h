// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SVGDynamicMeshComponent.h"
#include "SVGStrokeComponent.generated.h"

/** @note: the below two enums should match the native ones exactly, so they can be cast directly */

UENUM()
enum class EPolygonOffsetJoinType : uint8
{
	Square, /** Uniform squaring on all convex edge joins. */
	Round,  /** Arcs on all convex edge joins. */
	Miter,  /** Squaring of convex edge joins with acute angles ("spikes"). Use in combination with MiterLimit. */
};

UENUM()
enum class EPolygonOffsetEndType : uint8
{
	Polygon, /** Offsets only one side of a closed path */
	Joined,  /** Offsets both sides of a path, with joined ends */
	Butt,    /** Offsets both sides of a path, with square blunt ends */
	Square,  /** Offsets both sides of a path, with square extended ends */
	Round,   /** Offsets both sides of a path, with round extended ends */
};

struct FSVGStrokeParameters
{
	const TArray<FVector>& StrokePoints;

	float Thickness = 0;

	FColor InColor;

	bool bIsClosed = false;

	bool bIsClockwise = false;

	EPolygonOffsetJoinType JoinStyle{};

	float Extrude = 0;

	bool bUnlit = false;

	bool bCastShadow = false;

	FSVGStrokeParameters(const TArray<FVector>& InPoints)
		: StrokePoints(InPoints)
	{
	}
};

UCLASS(ClassGroup=(SVGImporter), Meta = (BlueprintSpawnableComponent))
class SVGIMPORTER_API USVGStrokeComponent : public USVGDynamicMeshComponent
{
	GENERATED_BODY()

public:
	void GenerateStrokeMesh(const FSVGStrokeParameters& InStrokeParameters);

	void SetJointStyle(EPolygonOffsetJoinType InJoinStyle);
	void SetStrokeWidth(float InStrokesWidth);

	//~ Begin UObject
#if WITH_EDITOR
	virtual void PostTransacted(const FTransactionObjectEvent& TransactionEvent) override;
#endif
	//~ End UObject

protected:
	//~ Begin USVGDynamicMesh
	virtual void RegisterDelegates() override;
	virtual void RegenerateMesh() override;
	//~ End USVGDynamicMesh

	void GenerateStrokeMeshInternal();
	void GenerateMainStrokeGeometry(float InThickness, float InExtrude);
	void ApplyStrokeExtrude();

	void CleanupStrokeMesh();

	UPROPERTY()
	TArray<FVector> StrokePoints;

	UPROPERTY()
	float StrokeThickness;

	UPROPERTY()
	bool bStrokeIsClosed;

	UPROPERTY()
	bool bStrokeIsClockwise;

	UPROPERTY()
	EPolygonOffsetJoinType JoinStyle;
};
