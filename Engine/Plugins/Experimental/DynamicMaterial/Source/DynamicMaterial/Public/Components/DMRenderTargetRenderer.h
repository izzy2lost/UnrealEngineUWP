// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "DMMaterialComponent.h"
#include "DMRenderTargetRenderer.generated.h"

class UDMMaterialValueRenderTarget;
template <typename T> class TSubclassOf;

UCLASS(Abstract, BlueprintType, Blueprintable, ClassGroup = "Material Designer", meta = (DisplayName = "Material Designer Render Target Renderer"))
class DYNAMICMATERIAL_API UDMRenderTargetRenderer : public UDMMaterialComponent
{
	GENERATED_BODY()

public:
	FDelegateHandle EndOfFrameDelegateHandle;

	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	static UDMRenderTargetRenderer* CreateRenderTargetRenderer(TSubclassOf<UDMRenderTargetRenderer>
		InRendererClass, UDMMaterialValueRenderTarget* InRenderTargetValue);

	template<typename InRendererClass
		UE_REQUIRES(std::is_base_of_v<UDMRenderTargetRenderer, InRendererClass>)>
	static InRendererClass* CreateRenderTargetRenderer(UDMMaterialValueRenderTarget* InRenderTargetValue)
	{
		return Cast<InRendererClass>(CreateRenderTargetRenderer(InRendererClass::StaticClass(), InRenderTargetValue));
	}

	UFUNCTION(BlueprintPure, Category = "Material Designer")
	UDMMaterialValueRenderTarget* GetRenderTargetValue() const;

	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	void UpdateRenderTarget();

	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	void AsyncUpdateRenderTarget();

	/** Will trigger the end of frame update if it is currently queued */
	void FlushUpdateRenderTarget();

	UFUNCTION(BlueprintPure, Category = "Material Designer")
	bool IsUpdating() const { return bUpdating; }

protected:
	bool bUpdating = false;

	virtual void UpdateRenderTarget_Internal() {}
};
