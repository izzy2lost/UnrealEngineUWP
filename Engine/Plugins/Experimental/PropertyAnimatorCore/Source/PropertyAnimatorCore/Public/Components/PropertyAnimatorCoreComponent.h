// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/ActorComponent.h"
#include "Animators/PropertyAnimatorCoreBase.h"
#include "PropertyAnimatorCoreComponent.generated.h"

/** A container for controllers that holds properties in this actor */
UCLASS(MinimalAPI, ClassGroup=(Custom), AutoExpandCategories=("Animator"), HideCategories=("Activation", "Cooking", "AssetUserData", "Collision"), meta=(BlueprintSpawnableComponent))
class UPropertyAnimatorCoreComponent : public UActorComponent
{
	GENERATED_BODY()

	friend class UPropertyAnimatorCoreSubsystem;
	friend class UPropertyAnimatorCoreEditorStackCustomization;

public:
	/** Create an instance of this component class and adds it to an actor */
	static UPropertyAnimatorCoreComponent* FindOrAdd(AActor* InActor);

	UPropertyAnimatorCoreComponent();

	void SetAnimators(const TArray<TObjectPtr<UPropertyAnimatorCoreBase>>& InAnimators);
	TConstArrayView<TObjectPtr<UPropertyAnimatorCoreBase>> GetAnimators() const
	{
		return PropertyAnimators;
	}

	int32 GetAnimatorsCount() const
	{
		return PropertyAnimators.Num();
	}

	/** Set the state of all animators in this component */
	void SetAnimatorsEnabled(bool bInEnabled);
	bool GetAnimatorsEnabled() const
	{
		return bAnimatorsEnabled;
	}

	/** Set the magnitude for all animators in this component */
	void SetAnimatorsMagnitude(float InMagnitude);
	float GetAnimatorsMagnitude() const
	{
		return AnimatorsMagnitude;
	}

	/** Process a function for each controller, stops when false is returned otherwise continue until the end */
	PROPERTYANIMATORCORE_API void ForEachAnimator(TFunctionRef<bool(UPropertyAnimatorCoreBase*)> InFunction) const;

	/** Checks if this component animators should be active */
	bool ShouldAnimate() const;

protected:
	static FName GetAnimatorName(const UPropertyAnimatorCoreBase* InAnimator);

	//~ Begin UActorComponent
	virtual void OnComponentCreated() override;
	virtual void DestroyComponent(bool bPromoteChildren) override;
	virtual void TickComponent(float InDeltaTime, ELevelTick InTickType, FActorComponentTickFunction* InTickFunction) override;
	//~ End UActorComponent

	//~ Begin UObject
	virtual void PostLoad() override;
#if WITH_EDITOR
	virtual void PostEditUndo() override;
	virtual void PreEditChange(FProperty* PropertyAboutToChange) override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
	//~ End UObject

	/** Adds a new controller and returns it casted */
	template<typename InAnimatorClass = UPropertyAnimatorCoreBase, typename = typename TEnableIf<TIsDerivedFrom<InAnimatorClass, UPropertyAnimatorCoreBase>::Value>::Type>
	InAnimatorClass* AddAnimator()
	{
		const UClass* AnimatorClass = InAnimatorClass::StaticClass();
		return Cast<InAnimatorClass>(AddAnimator(AnimatorClass));
	}

	/** Adds a new animator of that class */
	UPropertyAnimatorCoreBase* AddAnimator(const UClass* InAnimatorClass);

	/** Clones an existing animator */
	UPropertyAnimatorCoreBase* CloneAnimator(UPropertyAnimatorCoreBase* InAnimator);

	/** Removes an existing animator */
	bool RemoveAnimator(UPropertyAnimatorCoreBase* InAnimator);

	/** Change global state for animators */
	void OnAnimatorsSetEnabled(const UWorld* InWorld, bool bInEnabled, bool bInTransact);

	/** Callback when PropertyAnimators changed */
	void OnAnimatorsChanged();

	/** Callback when global enabled state is changed */
	void OnAnimatorsEnabledChanged();

	/** Evaluate only specified animators */
	bool EvaluateAnimators();

	/** Animators linked to this actor, they contain only properties within this actor */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, NoClear, Export, Instanced, Setter="SetAnimators", Category="Animator", meta=(TitleProperty="AnimatorDisplayName"))
	TArray<TObjectPtr<UPropertyAnimatorCoreBase>> PropertyAnimators;

	/** Global state for all animators controlled by this component */
	UPROPERTY(EditInstanceOnly, Getter="GetAnimatorsEnabled", Setter="SetAnimatorsEnabled", Category="Animator", meta=(DisplayPriority="0", AllowPrivateAccess="true"))
	bool bAnimatorsEnabled = true;

	/** Global magnitude for all animators controlled by this component */
	UPROPERTY(EditInstanceOnly, Getter, Setter, Category="Animator", meta=(ClampMin="0", ClampMax="1", UIMin="0", UIMax="1", AllowPrivateAccess="true"))
	float AnimatorsMagnitude = 1.f;

private:
	/** Deprecated property set, will be migrated to PropertyAnimators property on load */
	UE_DEPRECATED(5.5, "Moved to PropertyAnimators")
	UPROPERTY()
	TSet<TObjectPtr<UPropertyAnimatorCoreBase>> Animators;

	/** Transient copy of property animators when changes are detected to see the diff only */
	UPROPERTY(Transient, DuplicateTransient, TextExportTransient)
	TArray<TObjectPtr<UPropertyAnimatorCoreBase>> PropertyAnimatorsInternal;

	/** Cached time sources used by this animator component */
	UPROPERTY()
	TArray<TObjectPtr<UPropertyAnimatorCoreTimeSourceBase>> TimeSourceInstances;
};