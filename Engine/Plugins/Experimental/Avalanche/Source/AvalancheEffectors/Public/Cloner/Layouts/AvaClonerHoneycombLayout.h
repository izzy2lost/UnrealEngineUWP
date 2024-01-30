// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AvaClonerEffectorShared.h"
#include "AvaClonerLayoutBase.h"
#include "AvaPropertyChangeDispatcher.h"
#include "AvaClonerHoneycombLayout.generated.h"

UCLASS(BlueprintType)
class AVALANCHEEFFECTORS_API UAvaClonerHoneycombLayout : public UAvaClonerLayoutBase
{
	GENERATED_BODY()

	friend class FAvaClonerActorVisualizer;
	
public:
	UAvaClonerHoneycombLayout()
		: UAvaClonerLayoutBase(
			TEXT("Honeycomb")
			, TEXT("/Script/Niagara.NiagaraSystem'/Avalanche/ClonerResources/Systems/NS_ClonerHoneycomb.NS_ClonerHoneycomb'")
		)
	{}
	
	UFUNCTION(BlueprintCallable, Category="Cloner|Layout|Honeycomb")
	void SetPlane(EAvaClonerPlane InPlane);

	UFUNCTION(BlueprintPure, Category="Cloner|Layout|Honeycomb")
	EAvaClonerPlane GetPlane() const
	{
		return Plane;
	}

	UFUNCTION(BlueprintCallable, Category="Cloner|Layout|Honeycomb")
	void SetWidthCount(int32 InWidthCount);
	
	UFUNCTION(BlueprintPure, Category="Cloner|Layout|Honeycomb")
	int32 GetWidthCount() const
	{
		return WidthCount;
	}

	UFUNCTION(BlueprintCallable, Category="Cloner|Layout|Honeycomb")
	void SetHeightCount(int32 InHeightCount);
	
	UFUNCTION(BlueprintPure, Category="Cloner|Layout|Honeycomb")
	int32 GetHeightCount() const
	{
		return HeightCount;
	}

	UFUNCTION(BlueprintCallable, Category="Cloner|Layout|Honeycomb")
	void SetWidthOffset(float InWidthOffset);
	
	UFUNCTION(BlueprintPure, Category="Cloner|Layout|Honeycomb")
	float GetWidthOffset() const
	{
		return WidthOffset;
	}

	UFUNCTION(BlueprintCallable, Category="Cloner|Layout|Honeycomb")
	void SetHeightOffset(float InHeightOffset);
	
	UFUNCTION(BlueprintPure, Category="Cloner|Layout|Honeycomb")
	float GetHeightOffset() const
	{
		return HeightOffset;
	}

	UFUNCTION(BlueprintCallable, Category="Cloner|Layout|Honeycomb")
	void SetHeightSpacing(float InHeightSpacing);
	
	UFUNCTION(BlueprintPure, Category="Cloner|Layout|Honeycomb")
	float GetHeightSpacing() const
	{
		return HeightSpacing;
	}

	UFUNCTION(BlueprintCallable, Category="Cloner|Layout|Honeycomb")
	void SetWidthSpacing(float InWidthSpacing);

	UFUNCTION(BlueprintPure, Category="Cloner|Layout|Honeycomb")
	float GetWidthSpacing() const
	{
		return WidthSpacing;
	}

	//~ Begin UObject
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& InPropertyChangedEvent) override;
#endif
	//~ End UObject

protected:
	virtual void OnLayoutParametersChanged(UAvaClonerComponent* InComponent) override;
	
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Setter="SetPlane", Getter="GetPlane", Category="Layout")
	EAvaClonerPlane Plane = EAvaClonerPlane::XY;

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Setter="SetWidthCount", Getter="GetWidthCount", Category="Layout")
	int32 WidthCount = 3;

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Setter="SetHeightCount", Getter="GetHeightCount", Category="Layout")
	int32 HeightCount = 3;
	
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Setter="SetWidthOffset", Getter="GetWidthOffset", Category="Layout")
	float WidthOffset = 0.f;

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Setter="SetHeightOffset", Getter="GetHeightOffset", Category="Layout")
	float HeightOffset = 0.5f;

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Setter="SetWidthSpacing", Getter="GetWidthSpacing", Category="Layout")
	float WidthSpacing = 105.f;
	
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Setter="SetHeightSpacing", Getter="GetHeightSpacing", Category="Layout")
	float HeightSpacing = 105.f;

private:
#if WITH_EDITOR
	/** Used for PECP */
	static const TAvaPropertyChangeDispatcher<UAvaClonerHoneycombLayout> PropertyChangeDispatcher;
#endif
};