// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "DynamicMeshes/AvaShape2DDynMeshBase.h"
#include "AvaShapeRectangleDynMesh.generated.h"

USTRUCT(BlueprintType)
struct AVALANCHESHAPES_API FAvaShapeRectangleCornerSettings
{
	GENERATED_BODY()

	bool IsBeveled() const
	{
		return Type != EAvaShapeCornerType::Point && BevelSize > 0.f;
	}

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category="Shape")
	EAvaShapeCornerType Type = EAvaShapeCornerType::Point;

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category="Shape", meta=(ClampMin="0.0",DisplayName="Size"))
	float BevelSize = 0.f;

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category="Shape", meta=(ClampMax="64.0", DisplayName="Subdivisions"))
	uint8 BevelSubdivisions = UAvaShapeDynamicMeshBase::DefaultSubdivisions;

	UPROPERTY()
	FVector2D CornerPositionCache = FVector2D::ZeroVector;
};

UCLASS(ClassGroup="Shape", BlueprintType, CustomConstructor, Within=AvaShapeActor)
class AVALANCHESHAPES_API UAvaShapeRectangleDynamicMesh : public UAvaShape2DDynMeshBase
{
	GENERATED_BODY()

	friend class FAvaShapeRectangleDynamicMeshVisualizer;

public:
	static inline constexpr float MinSlantAngle = -45.f;
	static inline constexpr float MaxSlantAngle = 45.f;
	static inline constexpr float CornerMinMargin = 0.1f;

	UAvaShapeRectangleDynamicMesh()
		: UAvaShapeRectangleDynamicMesh(FVector2D(50.f, 50.f))
	{
	}

    explicit UAvaShapeRectangleDynamicMesh(const FVector2D& Size2D, const FLinearColor& InVertexColor = FLinearColor::White)
		: UAvaShape2DDynMeshBase(Size2D, InVertexColor)
	{
	}

	static const FString MeshName;
	virtual const FString& GetMeshName() const override { return MeshName; }

	UFUNCTION()
	bool SetHorizontalAlignment(EAvaHorizontalAlignment InHorizontalAlignment);
	EAvaHorizontalAlignment GetHorizontalAlignment() const { return HorizontalAlignment; }

	UFUNCTION()
	bool SetVerticalAlignment(EAvaVerticalAlignment InVerticalAlignment);
	EAvaVerticalAlignment GetVerticalAlignment() const { return VerticalAlignment; }

	UFUNCTION()
	bool SetLeftSlant(float InSlant);
	float GetLeftSlant() const { return LeftSlant; }

	UFUNCTION()
	bool SetRightSlant(float InSlant);
	float GetRightSlant() const { return RightSlant; }

	UFUNCTION()
	bool SetGlobalBevelSize(float InBevelSize);
	float GetGlobalBevelSize() const { return GlobalBevelSize; }

	/** Controls all corners bevel subdivisions */
	UFUNCTION()
	void SetGlobalBevelSubdivisions(uint8 InGlobalBevelSubdivisions);
	uint8 GetGlobalBevelSubdivisions() const { return GlobalBevelSubdivisions; }

	UFUNCTION()
	void SetTopLeft(const FAvaShapeRectangleCornerSettings& InCornerSettings);

	UFUNCTION()
	void SetTopRight(const FAvaShapeRectangleCornerSettings& InCornerSettings);

	UFUNCTION()
	void SetBottomLeft(const FAvaShapeRectangleCornerSettings& InCornerSettings);

	UFUNCTION()
	void SetBottomRight(const FAvaShapeRectangleCornerSettings& InCornerSettings);

	EAvaShapeCornerType GetTopLeftCornerType() const { return TopLeft.Type; }
	bool SetTopLeftCornerType(EAvaShapeCornerType InType);

	float GetTopLeftBevelSize() const { return TopLeft.BevelSize; }
	bool SetTopLeftBevelSize(float InSize);

	/** Bevel subdivisions for top left corner */
	uint8 GetTopLeftBevelSubdivisions() const { return TopLeft.BevelSubdivisions; }
	void SetTopLeftBevelSubdivisions(uint8 InBevelSubdivisions);

	EAvaShapeCornerType GetBottomLeftCornerType() const { return BottomLeft.Type; }
	bool SetBottomLeftCornerType(EAvaShapeCornerType InType);

	float GetBottomLeftBevelSize() const { return BottomLeft.BevelSize; }
	bool SetBottomLeftBevelSize(float InSize);

	/** Bevel subdivisions for bottom left corner */
	uint8 GetBottomLeftBevelSubdivisions() const { return BottomLeft.BevelSubdivisions; }
	void SetBottomLeftBevelSubdivisions(uint8 InBevelSubdivisions);

	EAvaShapeCornerType GetTopRightCornerType() const { return TopRight.Type; }
	bool SetTopRightCornerType(EAvaShapeCornerType InType);

	float GetTopRightBevelSize() const { return TopRight.BevelSize; }
	bool SetTopRightBevelSize(float InSize);

	/** Bevel subdivisions for top right corner */
	uint8 GetTopRightBevelSubdivisions() const { return TopRight.BevelSubdivisions; }
	void SetTopRightBevelSubdivisions(uint8 InBevelSubdivisions);

	EAvaShapeCornerType GetBottomRightCornerType() const { return BottomRight.Type; }
	bool SetBottomRightCornerType(EAvaShapeCornerType InType);

	float GetBottomRightBevelSize() const { return BottomRight.BevelSize; }
	bool SetBottomRightBevelSize(float InSize);

	/** Bevel subdivisions for bottom right corner */
	uint8 GetBottomRightBevelSubdivisions() const { return BottomRight.BevelSubdivisions; }
	void SetBottomRightBevelSubdivisions(uint8 InBevelSubdivisions);

protected:
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	float GetMaximumBevelSize() const;

	bool IsSlantAngleValid() const;
	FVector2D GetValidRangeLeftSlantAngle() const;
	FVector2D GetValidRangeRightSlantAngle() const;
	void GetValidSlantAngle(float& OutLeftSlant, float& OutRightSlant) const;

	void OnAlignmentChanged();

	void OnLeftSlantChanged();
	void OnRightSlantChanged();

	void OnGlobalBevelSizeChanged();
	void OnGlobalBevelSubdivisionsChanged();

	void OnTopLeftCornerTypeChanged();
	void OnTopLeftBevelSizeChanged();
	void OnTopLeftBevelSubdivisionsChanged();

	void OnBottomLeftCornerTypeChanged();
	void OnBottomLeftBevelSizeChanged();
	void OnBottomLeftBevelSubdivisionsChanged();

	void OnTopRightCornerTypeChanged();
	void OnTopRightBevelSizeChanged();
	void OnTopRightBevelSubdivisionsChanged();

	void OnBottomRightCornerTypeChanged();
	void OnBottomRightBevelSizeChanged();
	void OnBottomRightBevelSubdivisionsChanged();

	bool GenerateBaseMeshSections(FAvaShapeMesh& BaseMesh);

	virtual bool CreateMesh(FAvaShapeMesh& InMesh) override;

	virtual void OnSizeChanged() override;

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category="Shape", meta=(AllowPrivateAccess="true"))
	EAvaHorizontalAlignment HorizontalAlignment = EAvaHorizontalAlignment::Center;

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category="Shape", meta=(AllowPrivateAccess="true"))
	EAvaVerticalAlignment VerticalAlignment = EAvaVerticalAlignment::Center;

	/** Angle in degrees for the left slant of the rectangle */
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category="Shape", meta=(ClampMin="-45.0",ClampMax="45.0", AllowPrivateAccess="true"))
	float LeftSlant = 0.f;

	/** Angle in degrees for the right slant of the rectangle */
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category="Shape", meta=(ClampMin="-45.0",ClampMax="45.0", AllowPrivateAccess="true"))
	float RightSlant = 0.f;

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category="Shape", meta=(ClampMin="0.0", AllowPrivateAccess="true"))
	float GlobalBevelSize = 0.f;

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category="Shape", meta=(ClampMax="128.0", AllowPrivateAccess="true"))
	uint8 GlobalBevelSubdivisions = DefaultSubdivisions;

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category="Shape", meta=(AllowPrivateAccess="true"))
	FAvaShapeRectangleCornerSettings TopLeft;

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category="Shape", meta=(AllowPrivateAccess="true"))
	FAvaShapeRectangleCornerSettings TopRight;

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category="Shape", meta=(AllowPrivateAccess="true"))
	FAvaShapeRectangleCornerSettings BottomLeft;

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category="Shape", meta=(AllowPrivateAccess="true"))
	FAvaShapeRectangleCornerSettings BottomRight;
};
