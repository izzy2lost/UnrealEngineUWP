// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AvaGeometryBaseModifier.h"
#include "AvaNormalModifier.generated.h"

UENUM(BlueprintType)
enum class EAvaNormalModifierSplitMethod : uint8
{
	/** Do not split, leave as it is */
	None,
	/** Each vertex will have a split normal between tris */
	Vertex,
	/** Shared vertex between triangles will have a split normal */
	Triangle,
	/** Vertices of a same face grouped together will have a split normal */
	PolyGroup,
	/** Vertices above a certain angle threshold will have a split normal */
	Threshold
};

UCLASS(BlueprintType)
class AVALANCHEMODIFIERS_API UAvaNormalModifier : public UAvaGeometryBaseModifier
{
	GENERATED_BODY()

public:
	//~ Begin UObject
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
	//~ End UObject

	UFUNCTION()
	void SetAngleWeighted(bool bInAngleWeighted);
	bool GetAngleWeighted() const { return bAngleWeighted; }
	
	UFUNCTION()
	void SetAreaWeighted(bool bInAreaWeighted);
	bool GetAreaWeighted() const { return bAreaWeighted; }
	
	UFUNCTION()
	void SetInvert(bool bInInvert);
	bool GetInvert() const { return bInvert; }
	
	UFUNCTION()
	void SetSplitMethod(EAvaNormalModifierSplitMethod InSplitMethod);
	EAvaNormalModifierSplitMethod GetSplitMethod() const { return SplitMethod; }
	
	UFUNCTION()
	void SetAngleThreshold(float InAngleThreshold);
	float GetAngleThreshold() const { return AngleThreshold; }

	UFUNCTION()
	void SetPolyGroupLayerIdx(int32 InPolyGroupLayer);
	int32 GetPolyGroupLayerIdx() const;

	UFUNCTION()
	void SetPolyGroupLayer(FString& InString);
	FString GetPolyGroupLayer() const { return PolyGroupLayer; }

protected:
	//~ Begin UActorModifierCoreBase
	virtual void OnModifierCDOSetup(FActorModifierCoreMetadata& InMetadata) override;
	virtual void OnModifierAdded(EActorModifierCoreEnableReason InReason) override;
	virtual void Apply() override;
	//~ End UActorModifierCoreBase
	
	void OnAngleWeightedChanged();
	void OnAreaWeightedChanged();
	void OnInvertChanged();
	void OnSplitMethodChanged();
	void OnAngleThresholdChanged();
	void OnPolyGroupLayerChanged();

	UFUNCTION()
	TArray<FString> GetPolyGroupLayers() const;

	/** Recompute normals and weight them by angle */
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Setter="SetAngleWeighted", Getter="GetAngleWeighted", Category="Normal", meta=(AllowPrivateAccess="true"))
	bool bAngleWeighted = true;

	/** Recompute normals and weight them by area */
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Setter="SetAreaWeighted", Getter="GetAreaWeighted", Category="Normal", meta=(AllowPrivateAccess="true"))
	bool bAreaWeighted = true;

	/** Recompute normals and invert normals and triangles */
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Setter="SetInvert", Getter="GetInvert", Category="Normal", meta=(AllowPrivateAccess="true"))
	bool bInvert = false;

	/** Recompute normals and use a split method */
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Setter="SetSplitMethod", Getter="GetSplitMethod", Category="Normal", meta=(AllowPrivateAccess="true"))
	EAvaNormalModifierSplitMethod SplitMethod = EAvaNormalModifierSplitMethod::Threshold;

	/** Angle to compare and split normal when threshold method is chosen */
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Setter="SetAngleThreshold", Getter="GetAngleThreshold", Category="Normal", meta=(ClampMin="0.0", ClampMax="180.0", EditCondition="SplitMethod == EAvaNormalModifierSplitMethod::Threshold", AllowPrivateAccess="true"))
	float AngleThreshold = 60.f;

	/** PolyGroup to use to split normal from when PolyGroup method is chosen */
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Setter="SetPolyGroupLayer", Getter="GetPolyGroupLayer", Category="Normal", meta=(GetOptions="GetPolyGroupLayers", EditCondition="SplitMethod == EAvaNormalModifierSplitMethod::PolyGroup", AllowPrivateAccess="true"))
	FString PolyGroupLayer = TEXT("None");
};
