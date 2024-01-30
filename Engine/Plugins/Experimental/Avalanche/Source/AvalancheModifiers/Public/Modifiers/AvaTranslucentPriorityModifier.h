// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AvaArrangeBaseModifier.h"
#include "AvaBaseModifier.h"
#include "AvaTranslucentPriorityModifier.generated.h"

struct FAvaTranslucentPriorityModifierComponentState;
class ACameraActor;
class UPrimitiveComponent;

UENUM(BlueprintType)
enum class EAvaTranslucentPriorityModifierMode : uint8
{
	/** The closer you are from the camera based on camera forward axis, the higher your sort priority will be */
	AutoCameraDistance,
	/** The higher you are in the outline tree, the higher your sort priority will be */
	AutoOutlinerTree,
	/** Set it yourself */
	Manual
};

UCLASS(BlueprintType)
class AVALANCHEMODIFIERS_API UAvaTranslucentPriorityModifier : public UAvaArrangeBaseModifier
{
	GENERATED_BODY()

	friend class UAvaTranslucentPriorityModifierShared;

public:
	//~ Begin UObject
	virtual void PostLoad() override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
	//~ End UObject

    void SetMode(EAvaTranslucentPriorityModifierMode InMode);
    EAvaTranslucentPriorityModifierMode GetMode() const
    {
	    return Mode;
    }

    void SetCameraActorWeak(const TWeakObjectPtr<ACameraActor>& InCameraActor);
	TWeakObjectPtr<ACameraActor> GetCameraActorWeak() const
	{
		return CameraActorWeak;
	}

    void SetSortPriority(int32 InSortPriority);
    int32 GetSortPriority() const
    {
	    return SortPriority;
    }

	void SetIncludeChildren(bool bInIncludeChildren);
	bool GetIncludeChildren() const
	{
		return bIncludeChildren;
	}

protected:
	//~ Begin UActorModifierCoreBase
	virtual void OnModifierCDOSetup(FActorModifierCoreMetadata& InMetadata) override;
	virtual void OnModifierAdded(EActorModifierCoreEnableReason InReason) override;
	virtual void SavePreState() override;
	virtual void RestorePreState() override;
	virtual void Apply() override;
	virtual void OnModifierDisabled(EActorModifierCoreDisableReason InReason) override;
	virtual void OnModifiedActorTransformed() override;
	//~ End UActorModifierCoreBase

	//~ Begin IAvaSceneTreeUpdateModifierExtension
	virtual void OnSceneTreeTrackedActorChildrenChanged(int32 InIdx, const TSet<TWeakObjectPtr<AActor>>& InPreviousChildrenActors, const TSet<TWeakObjectPtr<AActor>>& InNewChildrenActors) override;
	virtual void OnSceneTreeTrackedActorDirectChildrenChanged(int32 InIdx, const TArray<TWeakObjectPtr<AActor>>& InPreviousChildrenActors, const TArray<TWeakObjectPtr<AActor>>& InNewChildrenActors) override;
	virtual void OnSceneTreeTrackedActorRearranged(int32 InIdx, AActor* InRearrangedActor) override;
	//~ End IAvaSceneTreeUpdateModifierExtension

	//~ Begin IAvaRenderStateUpdateExtension
	virtual void OnRenderStateUpdated(AActor* InActor, UActorComponent* InComponent) override;
	//~ End IAvaRenderStateUpdateExtension

	//~ Begin IAvaTransformUpdateExtension
	virtual void OnTransformUpdated(AActor* InActor, bool bInParentMoved) override;
	//~ End IAvaTransformUpdateExtension

	void OnModeChanged();
	void OnCameraActorChanged();
	void OnSortPriorityChanged();
	void OnIncludeChildrenChanged();

	ACameraActor* GetDefaultCameraActor() const;

	/** The sort mode we are currently in */
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Setter, Getter, Category="TranslucentPriority", meta=(AllowPrivateAccess="true"))
	EAvaTranslucentPriorityModifierMode Mode = EAvaTranslucentPriorityModifierMode::Manual;

	/** The camera actor to compute the distance from */
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Setter, Getter, Category="TranslucentPriority", meta=(DisplayName="CameraActor", EditCondition="Mode == EAvaTranslucentPriorityModifierMode::AutoCameraDistance", AllowPrivateAccess="true"))
	TWeakObjectPtr<ACameraActor> CameraActorWeak = nullptr;

	/** The sort priority that will be set on the primitive component for manual mode */
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Setter, Getter, Category="TranslucentPriority", meta=(EditCondition="Mode == EAvaTranslucentPriorityModifierMode::Manual", EditConditionHides, AllowPrivateAccess="true"))
	int32 SortPriority = 0;

	/** If true, will include children too and update their sort priority */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Setter="SetIncludeChildren", Getter="GetIncludeChildren", Category="TranslucentPriority", meta=(AllowPrivateAccess="true"))
	bool bIncludeChildren = false;

	/** The components this modifier is managing */
	UPROPERTY()
	TSet<TWeakObjectPtr<UPrimitiveComponent>> PrimitiveComponentsWeak;

private:
	/** The previous sort priority to restore when disabling this modifier */
	UPROPERTY()
	TMap<TWeakObjectPtr<UPrimitiveComponent>, int32> PreviousSortPriorities;

	/** Used to avoid querying again the full list of component states */
	TArray<const FAvaTranslucentPriorityModifierComponentState*> CachedSortedComponentStates;
};
