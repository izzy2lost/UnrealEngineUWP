// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "DynamicMeshes/AvaShape2DDynMeshBase.h"
#include "AvaShapeChevronDynMesh.generated.h"

UCLASS(ClassGroup="Shape", BlueprintType, CustomConstructor, Within=AvaShapeActor)
class AVALANCHESHAPES_API UAvaShapeChevronDynamicMesh : public UAvaShape2DDynMeshBase
{
	GENERATED_BODY()

public:
	UAvaShapeChevronDynamicMesh()
		: UAvaShapeChevronDynamicMesh(FVector2D(50.f, 50.f))
	{ }

	UAvaShapeChevronDynamicMesh(
		const FVector2D& Size2D,
		const FLinearColor& InVertexColor = FLinearColor::White,
		const float InRatioChevron = 0.5)
		: UAvaShape2DDynMeshBase(Size2D, InVertexColor)
		, RatioChevron(InRatioChevron)
	{
	}

	static const FString MeshName;
	virtual const FString& GetMeshName() const override { return MeshName; }

	UFUNCTION()
	bool SetRatioChevron(float InRatio);
	float GetRatioChevron() const { return RatioChevron; }

protected:
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	virtual void OnRatioChevronChanged();

	virtual bool IsMeshVisible(int32 MeshIndex) override;
	virtual bool CreateMesh(FAvaShapeMesh& InMesh) override;

	// represents the ratio for the chevron size
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category="Shape", meta=(ClampMin="0.0", ClampMax="0.99", DisplayName="Ratio Chevron", AllowPrivateAccess="true"))
	float RatioChevron;
};
