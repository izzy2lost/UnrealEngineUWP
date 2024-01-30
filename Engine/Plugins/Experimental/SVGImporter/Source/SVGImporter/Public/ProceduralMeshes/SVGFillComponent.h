// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SVGDynamicMeshComponent.h"
#include "SVGFillComponent.generated.h"

USTRUCT()
struct FSVGFillShape
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FVector2D> ShapeVertices;

	FSVGFillShape(){}
	FSVGFillShape(const TArray<FVector2D>& InVertices) : ShapeVertices(InVertices){}
};

USTRUCT()
struct FSVGFillMeshData
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FSVGFillShape> ShapesToDraw;

	UPROPERTY()
	TArray<FSVGFillShape> ShapesToRemove;

	void Init(const TArray<TArray<FVector2D>>& InShapesToDraw, const TArray<TArray<FVector2D>>& InShapesToRemove)
	{
		for (const TArray<FVector2D>& Shape : InShapesToDraw)
		{
			ShapesToDraw.Emplace(Shape);
		}

		for (const TArray<FVector2D>& Shape : InShapesToRemove)
		{
			ShapesToRemove.Emplace(Shape);
		}
	}

	void GetShapesArrays(TArray<TArray<FVector2D>>& OutShapesToDraw, TArray<TArray<FVector2D>>& OutShapesToRemove)
	{
		for (const FSVGFillShape& Elem : ShapesToDraw)
		{
			OutShapesToDraw.Add(Elem.ShapeVertices);
		}

		for (const FSVGFillShape& Elem : ShapesToRemove)
		{
			OutShapesToRemove.Add(Elem.ShapeVertices);
		}
	}
};

struct FSVGFillParameters
{
	const TArray<TArray<FVector2D>>& ShapesToDraw;

	const TArray<TArray<FVector2D>>& ShapesToRemove;

	FColor Color;

	float Extrude = 0;

	bool bSimplify = false;

	float BevelDistance = 0;

	bool bSmoothShapes = false;

	float SmoothingOffset = 150.0;

	bool bUnlit = false;

	bool bCastShadow = false;

	FSVGFillParameters(const TArray<TArray<FVector2D>>& ShapesToDraw, const TArray<TArray<FVector2D>>& ShapesToRemove)
		: ShapesToDraw(ShapesToDraw),
		  ShapesToRemove(ShapesToRemove)
	{
	}
};

UCLASS(ClassGroup=(SVGImporter), Meta = (BlueprintSpawnableComponent))
class SVGIMPORTER_API USVGFillComponent : public USVGDynamicMeshComponent
{
	GENERATED_BODY()

public:
	void GenerateFillMesh(const FSVGFillParameters& InFillParameters);
	void SetSmoothFillShapes(bool bInSmoothFillShapes, float InSmoothingOffset);

	//~ Begin UObject
#if WITH_EDITOR
	virtual void PostTransacted(const FTransactionObjectEvent& TransactionEvent) override;
	virtual void PostEditImport() override;
#endif
	virtual void PostLoad() override;
	//~ End UObject

protected:
	//~ Begin USVGDynamicMesh
	virtual void RegisterDelegates() override;
	virtual void RegenerateMesh() override;
	//~ End USVGDynamicMesh

	void RefreshNormals();
	void CreateSubtractMesh(TWeakObjectPtr<UDynamicMesh>& SubtractMesh, float InExtrude) const;
	void InitBasePolygons();
	void GenerateFillMeshInternal();

	void LoadCachedMeshToDynamicMesh(const TOptional<UE::Geometry::FDynamicMesh3>& InSourceMesh);
	void ApplyExtrudeAndSubtract(float InExtrudeValue);
	void ApplySimplifyAndBevel();

	/** Applies all changes after extrusion. Includes beveling. */
	void ApplyFinalMeshChanges();

	/** Applies all changes from extrusion on. */
	void ApplyFillExtrude();

	/** Applies changes from extrude on. */
	void ApplyFillBevel();

	TOptional<UE::Geometry::FDynamicMesh3> CachedExtrudedMesh;
	TOptional<UE::Geometry::FDynamicMesh3> CachedCutPolygon;
	TOptional<UE::Geometry::FDynamicMesh3> CachedBasePolygon;

	UPROPERTY()
	bool bSimplify;

	UPROPERTY()
	bool bBypassClipper;

	UPROPERTY()
	float SmoothingOffset;

	UPROPERTY()
	FSVGFillMeshData FillMeshData;
};
