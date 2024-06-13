// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/RenderTargetRenderers/DMRenderTargetWidgetRendererBase.h"
#include "DMRenderTargetUMGWidgetRenderer.generated.h"

class FWidgetRenderer;
class UWidget;

UCLASS(BlueprintType, ClassGroup = "Material Designer", meta = (DisplayName = "Material Designer Render Target Widget Renderer"))
class DYNAMICMATERIAL_API UDMRenderTargetUMGWidgetRenderer : public UDMRenderTargetWidgetRendererBase
{
	GENERATED_BODY()

public:
	UDMRenderTargetUMGWidgetRenderer();

	UFUNCTION(BlueprintPure, Category = "Material Designer")
	TSubclassOf<UWidget> GetWidgetClass() const { return WidgetClass; }

	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	void SetWidgetClass(TSubclassOf<UWidget> InWidgetClass);

#if WITH_EDITOR
	//~ Begin IDMJsonSerializable
	virtual TSharedPtr<FJsonValue> JsonSerialize() const override;
	virtual bool JsonDeserialize(const TSharedPtr<FJsonValue>& InJsonValue) override;
	//~ End IDMJsonSerializable

	//~ Begin UObject
	virtual void PostEditChangeProperty(FPropertyChangedEvent& InPropertyChangedEvent) override;
	//~ End UObject
#endif

protected:
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Setter = SetWidgetClass, BlueprintSetter = SetWidgetClass, Category = "Material Designer",
		meta = (NotKeyframeable, AllowPrivateAccess = "true", NoCreate))
	TSubclassOf<UWidget> WidgetClass;

	UPROPERTY()
	TObjectPtr<UWidget> WidgetInstance;

	//~ Begin UDMRenderTargetWidgetRendererBase
	virtual void CreateWidgetInstance() override;
	//~ End UDMRenderTargetWidgetRendererBase
};
