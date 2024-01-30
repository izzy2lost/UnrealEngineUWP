// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AvaShapeRoundedPolygonDynMesh.h"
#include "AvaShapeLineDynMesh.generated.h"

UCLASS(ClassGroup="Shape", BlueprintType, CustomConstructor, Within=AvaShapeActor)
class AVALANCHESHAPES_API UAvaShapeLineDynamicMesh : public UAvaShapeRoundedPolygonDynamicMesh
{
	GENERATED_BODY()

	friend class FAvaShapeLineDynamicMeshVisualizer;

public:
	UAvaShapeLineDynamicMesh()
		: UAvaShapeLineDynamicMesh(FVector2D(50.f, 1.f))
	{
	}

	UAvaShapeLineDynamicMesh(const FVector2D& Size2D, const FLinearColor& InVertexColor = FLinearColor::White,
		float InBevelSize = 0.f, uint8 InBevelSubdivisions = 0, float InLineWidth = 5.f)
		: UAvaShapeRoundedPolygonDynamicMesh(Size2D, InVertexColor, InBevelSize, InBevelSubdivisions)
		, LineWidth(InLineWidth)
		, Vector(FVector2D(50.f, 0.f))
	{
		bAllowEditSize = false;
	}

	static const FString MeshName;
	virtual const FString& GetMeshName() const override { return MeshName; }

	UFUNCTION()
	bool SetLineWidth(float InLineWidth);
	float GetLineWidth() const { return LineWidth; }

	UFUNCTION()
	bool SetVector(const FVector2D& InVector);
	const FVector2D& GetVector() const { return Vector; }

protected:
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	void OnLineWidthChanged();
	void OnVectorChanged();

	void UpdateExtent();

	TArray<FVector2D> GenerateLineVertices() const;

	virtual bool IsMeshVisible(int32 MeshIndex) override;
	virtual void GenerateBorderVertices(TArray<FVector2D>& BorderVertices) override;

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category="Shape", meta=(ClampMin="0.0", AllowPrivateAccess="true"))
	float LineWidth;

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category="Shape", meta=(AllowPrivateAccess="true"))
	FVector2D Vector;
};
