// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AvaArrangeBaseModifier.h"
#include "AvaHideEmptyModifier.generated.h"

class UText3DComponent;

UCLASS(BlueprintType)
class AVALANCHEMODIFIERS_API UAvaHideEmptyModifier : public UAvaArrangeBaseModifier
{
	GENERATED_BODY()
	
public:
	//~ Begin UObject
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
	//~ End UObject

	void SetInvertVisibility(bool bInInvert);
	bool GetInvertVisibility() const { return bInvertVisibility; }
	
protected:
	//~ Begin UActorModifierCoreBase
	virtual void OnModifierCDOSetup(FActorModifierCoreMetadata& InMetadata) override;
	virtual void OnModifierAdded(EActorModifierCoreEnableReason InReason) override;
	virtual void OnModifierEnabled(EActorModifierCoreEnableReason InReason) override;
	virtual void Apply() override;
	virtual void OnModifierDisabled(EActorModifierCoreDisableReason InReason) override;
	virtual void OnModifiedActorTransformed() override;
	//~ End UActorModifierCoreBase
	
	void OnTextChanged();
	void OnContainerActorChanged();
	void OnInvertVisibilityChanged();

	//~ Begin IAvaSceneTreeUpdateModifierExtension
	virtual void OnSceneTreeTrackedActorChanged(int32 InIdx, AActor* InPreviousActor, AActor* InNewActor) override;
	virtual void OnSceneTreeTrackedActorDirectChildrenChanged(int32 InIdx, const TArray<TWeakObjectPtr<AActor>>& InPreviousChildrenActors, const TArray<TWeakObjectPtr<AActor>>& InNewChildrenActors) override;
	//~ End IAvaSceneTreeUpdateModifierExtension
	
	/** The container to hide when text is empty, by default self */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="HideEmpty", meta=(DisplayName="ContainerActor", AllowPrivateAccess="true"))
	TWeakObjectPtr<AActor> ContainerActorWeak;
	
	/** Invert the behaviour and visibility of the container if text is empty */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Setter="SetInvertVisibility", Getter="GetInvertVisibility", Category="HideEmpty", meta=(AllowPrivateAccess="true"))
	bool bInvertVisibility = false;

private:
	/** Cached text component */
	UPROPERTY()
	TWeakObjectPtr<UText3DComponent> TextComponent;
};
