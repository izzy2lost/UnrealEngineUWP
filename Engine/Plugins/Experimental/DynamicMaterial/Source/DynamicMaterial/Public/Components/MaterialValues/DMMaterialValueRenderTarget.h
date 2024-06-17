// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/MaterialValues/DMMaterialValueTexture.h"
#include "Delegates/IDelegateInstance.h"
#include "DMMaterialValueRenderTarget.generated.h"

class UDMRenderTargetRenderer;
class UTextureRenderTarget2D;
enum ETextureRenderTargetFormat : int;
struct FLinearColor;

UCLASS(BlueprintType, ClassGroup = "Material Designer")
class DYNAMICMATERIAL_API UDMMaterialValueRenderTarget : public UDMMaterialValueTexture
{
	GENERATED_BODY()

public:
	static const FString RendererPathToken;

	UDMMaterialValueRenderTarget();

	virtual ~UDMMaterialValueRenderTarget() override;

	UFUNCTION(BlueprintPure, Category = "Material Designer")
	UTextureRenderTarget2D* GetRenderTarget() const;

	UFUNCTION(BlueprintPure, Category = "Material Designer")
	const FIntPoint& GetTextureSize() const;

	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	void SetTextureSize(const FIntPoint& InTextureSize);

	UFUNCTION(BlueprintPure, Category = "Material Designer")
	ETextureRenderTargetFormat GetTextureFormat() const;

	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	void SetTextureFormat(ETextureRenderTargetFormat InTextureFormat);

	UFUNCTION(BlueprintPure, Category = "Material Designer")
	const FLinearColor& GetClearColor() const;

	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	void SetClearColor(const FLinearColor& InClearColor);

	UFUNCTION(BlueprintPure, Category = "Material Designer")
	UDMRenderTargetRenderer* GetRenderer() const;

	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	void SetRenderer(UDMRenderTargetRenderer* InRenderer);

	/**
	 * Allows outside objects to ensure our render target is valid.
	 * @param bInAsync If true, will create the render target on end of frame.
	 */
	void EnsureRenderTarget(bool bInAsync = false);

	/** Will trigger the end of frame update if it is currently queued or is invalid */
	void FlushCreateRenderTarget();

#if WITH_EDITOR
	//~ Begin IDMJsonSerializable
	virtual TSharedPtr<FJsonValue> JsonSerialize() const override;
	virtual bool JsonDeserialize(const TSharedPtr<FJsonValue>& InJsonValue) override;
	//~ End IDMJsonSerializable

	//~ Begin UDMMaterialValue
	/** Render target is handled internally. */
	virtual bool AllowEditValue() const override { return false; }
	//~ End UDMMaterialValue
#endif

	//~ Begin UDMMaterialComponent
	virtual void Update(EDMUpdateType InUpdateType) override;
#if WITH_EDITOR
	virtual void PostEditorDuplicate(UDynamicMaterialModel* InMaterialModel, UDMMaterialComponent* InParent) override;
#endif
	//~ End UDMMaterialComponent

	//~ Begin UObject
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& InPropertyChangedEvent) override;
#endif
	virtual void PostLoad() override;
	//~ End UObject

protected:
	FDelegateHandle EndOfFrameDelegateHandle;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Setter = SetTextureSize, BlueprintSetter = SetTextureSize, Category = "Material Designer|Render Target",
		meta = (AllowPrivateAccess = "true", NotKeyframeable))
	FIntPoint TextureSize;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Setter = SetTextureFormat, BlueprintSetter = SetTextureFormat, Category = "Material Designer|Render Target",
		meta = (AllowPrivateAccess = "true", NotKeyframeable))
	TEnumAsByte<ETextureRenderTargetFormat> TextureFormat;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Setter = SetClearColor, BlueprintSetter = SetClearColor, Category = "Material Designer|Render Target",
		meta = (AllowPrivateAccess = "true", NotKeyframeable))
	FLinearColor ClearColor;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Setter = SetRenderer, BlueprintSetter = SetRenderer, Category = "Material Designer",
		meta = (AllowPrivateAccess = "true", NotKeyframeable, NoCreate))
	TObjectPtr<UDMRenderTargetRenderer> Renderer;

	void AsyncCreateRenderTarget();

	void CreateRenderTarget();

	void UpdateRenderTarget();

	//~ Begin UDMMaterialComponent
	virtual UDMMaterialComponent* GetSubComponentByPath(FDMComponentPath& InPath, const FDMComponentPathSegment& InPathSegment) const override;
#if WITH_EDITOR
	virtual void OnComponentAdded() override;
	virtual void OnComponentRemoved() override;
#endif
	//~ End UDMMaterialComponent
};
