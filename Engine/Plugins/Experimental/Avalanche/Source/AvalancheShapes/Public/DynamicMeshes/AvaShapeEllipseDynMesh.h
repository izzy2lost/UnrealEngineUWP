// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "DynamicMeshes/AvaShape2DDynMeshBase.h"
#include "AvaShapeEllipseDynMesh.generated.h"

UCLASS(ClassGroup="Shape", BlueprintType, CustomConstructor, Within=AvaShapeActor)
class AVALANCHESHAPES_API UAvaShapeEllipseDynamicMesh : public UAvaShape2DDynMeshBase
{
	GENERATED_BODY()

	friend class FAvaShapeEllipseDynamicMeshVisualizer;

public:
	static constexpr uint8 MinNumSides = 3;
	static constexpr uint8 MaxNumSides = 128;

	UAvaShapeEllipseDynamicMesh()
		: UAvaShapeEllipseDynamicMesh(FVector2D(50.f, 50.f))
	{ }

	UAvaShapeEllipseDynamicMesh(const FVector2D& Size2D, const FLinearColor& InVertexColor = FLinearColor::White,
		uint8 InNumSides = 64, float InAngleDegree = 360.f, float InStartDegree = 0.f)
		: UAvaShape2DDynMeshBase(Size2D, InVertexColor)
		, NumSides(InNumSides)
		, AngleDegree(InAngleDegree)
		, StartDegree(InStartDegree)
	{
		bDoNotRecenterVertices = true;
	}

	static const FString MeshName;
	virtual const FString& GetMeshName() const override { return MeshName; }

	UFUNCTION()
	bool SetNumSides(uint8 InNumSides);
	uint8 GetNumSides() const { return NumSides; }

	UFUNCTION()
	bool SetAngleDegree(float InDegree);
	float GetAngleDegree() const { return AngleDegree; }

	UFUNCTION()
	bool SetStartDegree(float InDegree);
	float GetStartDegree() const { return StartDegree; }

protected:
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	virtual void OnNumSidesChanged();
	virtual void OnAngleDegreeChanged();
	virtual void OnStartDegreeChanged();

	virtual bool IsMeshVisible(int32 MeshIndex) override;
	virtual bool CreateMesh(FAvaShapeMesh& InMesh) override;

	// The number of sides for the shape
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category="Shape", meta=(ClampMin="3.0", ClampMax="128.0", DisplayName="Sides", AllowPrivateAccess="true"))
	uint8 NumSides;

	// represents the angle in degree for the ellipse
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category="Shape", meta=(ClampMin="0.0", ClampMax="360.0", DisplayName="Angle degree", AllowPrivateAccess="true"))
	float AngleDegree;

	// represents the starting angle in degree for the ellipse
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category="Shape", meta=(ClampMin="0.0", ClampMax="360.0", DisplayName="Start Degree", AllowPrivateAccess="true"))
	float StartDegree;
};
