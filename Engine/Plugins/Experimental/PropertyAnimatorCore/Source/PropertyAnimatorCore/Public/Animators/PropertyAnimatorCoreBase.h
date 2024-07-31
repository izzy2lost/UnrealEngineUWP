// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Properties/PropertyAnimatorCoreContext.h"
#include "Properties/PropertyAnimatorCoreData.h"
#include "UObject/Object.h"
#include "PropertyAnimatorCoreBase.generated.h"

class UPropertyAnimatorCoreComponent;
class UPropertyAnimatorCoreGroupBase;
class UPropertyAnimatorCoreTimeSourceBase;

UENUM(BlueprintType)
enum class EPropertyAnimatorPropertySupport : uint8
{
	None = 0,
	Incomplete = 1 << 0,
	Complete = 1 << 1,
	All = Incomplete | Complete
};

/** Abstract base class for any Animator, holds a set of linked properties */
UCLASS(MinimalAPI, Abstract, EditInlineNew, AutoExpandCategories=("Animator"))
class UPropertyAnimatorCoreBase : public UObject
{
	GENERATED_BODY()

	friend class UPropertyAnimatorCoreComponent;
	friend class UPropertyAnimatorCoreContext;
	friend class UPropertyAnimatorCoreEditorStackCustomization;
	friend class FPropertyAnimatorCoreEditorDetailCustomization;

public:
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnAnimatorUpdated, UPropertyAnimatorCoreBase* /* InAnimator */)

	/** Called when a Animator is created */
	PROPERTYANIMATORCORE_API static FOnAnimatorUpdated OnAnimatorCreatedDelegate;

	/** Called when a Animator is removed */
	PROPERTYANIMATORCORE_API static FOnAnimatorUpdated OnAnimatorRemovedDelegate;

	/** Called when a Animator is renamed */
	PROPERTYANIMATORCORE_API static FOnAnimatorUpdated OnAnimatorRenamedDelegate;

	DECLARE_MULTICAST_DELEGATE_TwoParams(FOnAnimatorPropertyUpdated, UPropertyAnimatorCoreBase* /* InAnimator */, const FPropertyAnimatorCoreData& /** InProperty */)

	/** Called when a property is linked to a Animator */
	PROPERTYANIMATORCORE_API static FOnAnimatorPropertyUpdated OnAnimatorPropertyLinkedDelegate;

	/** Called when a property is unlinked to a Animator */
	PROPERTYANIMATORCORE_API static FOnAnimatorPropertyUpdated OnAnimatorPropertyUnlinkedDelegate;

	static constexpr const TCHAR* TimeElapsedParameterName = TEXT("TimeElapsed");
	static constexpr const TCHAR* MagnitudeParameterName = TEXT("Magnitude");
	static constexpr const TCHAR* FrequencyParameterName = TEXT("Frequency");
	static constexpr const TCHAR* AlphaParameterName = TEXT("Alpha");

#if WITH_EDITOR
	PROPERTYANIMATORCORE_API static FName GetLinkedPropertiesPropertyName();
#endif

	PROPERTYANIMATORCORE_API UPropertyAnimatorCoreBase();

	PROPERTYANIMATORCORE_API AActor* GetAnimatorActor() const;

	PROPERTYANIMATORCORE_API UPropertyAnimatorCoreComponent* GetAnimatorComponent() const;

	/** Set the state of this animator */
	PROPERTYANIMATORCORE_API void SetAnimatorEnabled(bool bInIsEnabled);
	bool GetAnimatorEnabled() const
	{
		return bAnimatorEnabled;
	}

	/** Set the time source name to use */
	PROPERTYANIMATORCORE_API void SetTimeSourceName(FName InTimeSourceName);
	FName GetTimeSourceName() const
	{
		return TimeSourceName;
	}

	/** Get the active time source */
	UPropertyAnimatorCoreTimeSourceBase* GetActiveTimeSource() const
	{
		return ActiveTimeSource;
	}

	/** Set the display name of this animator */
	PROPERTYANIMATORCORE_API void SetAnimatorDisplayName(FName InName);
	FString GetAnimatorDisplayName() const
	{
		return AnimatorDisplayName.ToString();
	}

	/** Gets the Animator original name */
	PROPERTYANIMATORCORE_API FName GetAnimatorOriginalName() const;

	/** Get all linked properties within this animator */
	PROPERTYANIMATORCORE_API TSet<FPropertyAnimatorCoreData> GetLinkedProperties() const;

	/** Get linked properties count within this animator */
	PROPERTYANIMATORCORE_API int32 GetLinkedPropertiesCount() const;

	/** Link property to this Animator to be able to drive it */
	PROPERTYANIMATORCORE_API bool LinkProperty(const FPropertyAnimatorCoreData& InLinkProperty);

	/** Unlink property from this Animator */
	PROPERTYANIMATORCORE_API bool UnlinkProperty(const FPropertyAnimatorCoreData& InUnlinkProperty);

	/** Checks if this Animator is controlling this property */
	PROPERTYANIMATORCORE_API bool IsPropertyLinked(const FPropertyAnimatorCoreData& InPropertyData) const;

	/** Checks if this animator is controlling all properties */
	bool IsPropertiesLinked(const TSet<FPropertyAnimatorCoreData>& InProperties) const;

	/** Returns all inner properties that are controlled by this Animator linked to member property */
	PROPERTYANIMATORCORE_API TSet<FPropertyAnimatorCoreData> GetInnerPropertiesLinked(const FPropertyAnimatorCoreData& InPropertyData) const;

	/**
	 * Checks recursively for properties that are supported by this Animator, calls IsPropertySupported to check
	 * Stops when the InSearchDepth has been reached otherwise continues to gather supported properties
	 */
	PROPERTYANIMATORCORE_API bool GetPropertiesSupported(const FPropertyAnimatorCoreData& InPropertyData, TSet<FPropertyAnimatorCoreData>& OutProperties, uint8 InSearchDepth = 1, EPropertyAnimatorPropertySupport InSupportExpected = EPropertyAnimatorPropertySupport::All) const;

	/** Retrieves the support level of a property */
	PROPERTYANIMATORCORE_API EPropertyAnimatorPropertySupport GetPropertySupport(const FPropertyAnimatorCoreData& InPropertyData) const;

	/** Checks if a property support is available */
	PROPERTYANIMATORCORE_API bool HasPropertySupport(const FPropertyAnimatorCoreData& InPropertyData, EPropertyAnimatorPropertySupport InSupportExpected = EPropertyAnimatorPropertySupport::All) const;

	/** Override this to check if a property is supported by this animator */
	virtual EPropertyAnimatorPropertySupport IsPropertySupported(const FPropertyAnimatorCoreData& InPropertyData) const
	{
		return EPropertyAnimatorPropertySupport::None;
	}

	/** Get the context for the linked property */
	PROPERTYANIMATORCORE_API UPropertyAnimatorCoreContext* GetLinkedPropertyContext(const FPropertyAnimatorCoreData& InProperty) const;

	/** Get the casted context for the linked property */
	template<typename InContextClass
		UE_REQUIRES(TIsDerivedFrom<InContextClass, UPropertyAnimatorCoreContext>::Value)>
	InContextClass* GetLinkedPropertyContext(const FPropertyAnimatorCoreData& InProperty) const
	{
		return Cast<InContextClass>(GetLinkedPropertyContext(InProperty));
	}

protected:
	//~ Begin UObject
	PROPERTYANIMATORCORE_API virtual void BeginDestroy() override;
	PROPERTYANIMATORCORE_API virtual void PostLoad() override;
	PROPERTYANIMATORCORE_API virtual void PostEditImport() override;
	PROPERTYANIMATORCORE_API virtual void PreDuplicate(FObjectDuplicationParameters& InDupParams) override;
	PROPERTYANIMATORCORE_API virtual void PostDuplicate(EDuplicateMode::Type InDuplicateMode) override;
#if WITH_EDITOR
	PROPERTYANIMATORCORE_API virtual void PreEditUndo() override;
	PROPERTYANIMATORCORE_API virtual void PostEditUndo() override;
	PROPERTYANIMATORCORE_API virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
	//~ End UObject

	/** Update display name based on linked properties */
	void UpdateAnimatorDisplayName();

	/** Use this to process each linked properties and resolve it, even virtual ones */
	template<typename InContextClass
		UE_REQUIRES(TIsDerivedFrom<InContextClass, UPropertyAnimatorCoreContext>::Value)>
	bool ForEachLinkedProperty(TFunctionRef<bool(InContextClass*, const FPropertyAnimatorCoreData&)> InFunction, bool bInResolve = true) const
	{
		for (const TObjectPtr<UPropertyAnimatorCoreContext>& LinkedProperty : LinkedProperties)
		{
			if (InContextClass* PropertyContext = Cast<InContextClass>(LinkedProperty.Get()))
			{
				if (bInResolve)
				{
					for (const FPropertyAnimatorCoreData& ResolvedPropertyData : PropertyContext->ResolveProperty(false))
					{
						if (!ResolvedPropertyData.IsResolved())
						{
							continue;
						}

						if (!InFunction(PropertyContext, ResolvedPropertyData))
						{
							return false;
						}
					}
				}
				else
				{
					if (!InFunction(PropertyContext, LinkedProperty->GetAnimatedProperty()))
					{
						return false;
					}
				}
			}
		}

		return true;
	}

	/** Used to evaluate linked properties, assign the result in the property bag and return true on success to update property value */
	template<typename InContextClass
		UE_REQUIRES(TIsDerivedFrom<InContextClass, UPropertyAnimatorCoreContext>::Value)>
	void EvaluateEachLinkedProperty(
		TFunctionRef<bool(
			InContextClass* /** InPropertyContext */
			, const FPropertyAnimatorCoreData& /** InResolvedProperty */
			, FInstancedPropertyBag& /** OutEvaluation */
			, int32 InRangeIndex
			, int32 InRangeMax)> InFunction
		)
	{
		checkf(bEvaluatingProperties, TEXT("EvaluateEachLinkedProperty can only be called in EvaluateProperties"))

		for (const TObjectPtr<UPropertyAnimatorCoreContext>& LinkedProperty : LinkedProperties)
		{
			if (InContextClass* PropertyContext = Cast<InContextClass>(LinkedProperty.Get()))
			{
				if (!PropertyContext->IsAnimated())
				{
					continue;
				}

				const TArray<FPropertyAnimatorCoreData> ResolvedProperties = PropertyContext->ResolveProperty(true);

				for (int32 Index = 0; Index < ResolvedProperties.Num(); Index++)
				{
					const FPropertyAnimatorCoreData& ResolvedPropertyData = ResolvedProperties[Index];

					if (!ResolvedPropertyData.IsResolved())
					{
						continue;
					}

					if (InFunction(PropertyContext, ResolvedPropertyData, EvaluatedPropertyValues, Index, ResolvedProperties.Num() - 1))
					{
						PropertyContext->CommitEvaluationResult(ResolvedPropertyData, EvaluatedPropertyValues);
					}
				}
			}
		}
	}

	virtual void OnAnimatorDisplayNameChanged() {}

	virtual void OnAnimatorAdded() {}
	virtual void OnAnimatorRemoved() {}

	PROPERTYANIMATORCORE_API virtual void OnAnimatorEnabled();
	PROPERTYANIMATORCORE_API virtual void OnAnimatorDisabled();

	virtual void OnTimeSourceChanged() {}

	/** Returns the property context class to use */
	PROPERTYANIMATORCORE_API virtual TSubclassOf<UPropertyAnimatorCoreContext> GetPropertyContextClass(const FPropertyAnimatorCoreData& InProperty);

	virtual void OnPropertyLinked(UPropertyAnimatorCoreContext* InLinkedProperty, EPropertyAnimatorPropertySupport InSupport) {}
	virtual void OnPropertyUnlinked(UPropertyAnimatorCoreContext* InUnlinkedProperty) {}

	/** Apply animators effect on linked properties */
	virtual void EvaluateProperties(FInstancedPropertyBag& InParameters) {}

private:
	/** Restore modified properties to original state */
	void RestoreProperties(bool bInForce = false);

	/** Allocate and saves properties in the property bag */
	void SaveProperties();

	/** Called by the component to evaluate this animator */
	void EvaluateAnimator(FInstancedPropertyBag& InParameters);

	void OnObjectReplaced(const TMap<UObject*, UObject*>& InReplacementMap);

	void OnPropertyGroupsChanged();

	void OnAnimatorEnabledChanged();

	void CleanLinkedProperties();

	void OnTimeSourceNameChanged();

	/** Called after an action that causes the owner to change */
	void ResolvePropertiesOwner(AActor* InNewOwner = nullptr);

	UPropertyAnimatorCoreTimeSourceBase* FindOrAddTimeSource(FName InTimeSourceName);

	UFUNCTION()
	TArray<FName> GetTimeSourceNames() const;

	/** Enable control of properties linked to this Animator */
	UPROPERTY(EditInstanceOnly, Getter="GetAnimatorEnabled", Setter="SetAnimatorEnabled", Category="Animator", meta=(DisplayPriority="0", AllowPrivateAccess="true"))
	bool bAnimatorEnabled = true;

	/** Display name as title property for component array, hide it but must be visible to editor for array title property */
	UPROPERTY(VisibleInstanceOnly, Category="Animator", meta=(EditCondition="false", EditConditionHides))
	FName AnimatorDisplayName;

	/** Context for properties linked to this Animator */
	UPROPERTY(EditInstanceOnly, NoClear, Export, Instanced, EditFixedSize, Category="Animator", meta=(EditFixedOrder))
	TArray<TObjectPtr<UPropertyAnimatorCoreContext>> LinkedProperties;

	/** Groups for properties linked to this Animator */
	UPROPERTY(EditInstanceOnly, NoClear, Export, Instanced, Category="Animator")
	TArray<TObjectPtr<UPropertyAnimatorCoreGroupBase>> PropertyGroups;

	/** The time source to use */
	UPROPERTY(EditInstanceOnly, Setter="SetTimeSourceName", Getter="GetTimeSourceName", Category="Animator", meta=(GetOptions="GetTimeSourceNames"))
	FName TimeSourceName = NAME_None;

	/** Active time source with its options, determined by its name */
	UPROPERTY(VisibleInstanceOnly, Instanced, Transient, DuplicateTransient, Category="Animator")
	TObjectPtr<UPropertyAnimatorCoreTimeSourceBase> ActiveTimeSource;

	/** The cached time source used by this Animator */
	UE_DEPRECATED(5.5, "Use TimeSources instead")
	UPROPERTY(meta=(DeprecatedProperty, DeprecationMessage="Use TimeSources instead"))
	TMap<FName, TObjectPtr<UPropertyAnimatorCoreTimeSourceBase>> TimeSourcesInstances;

	/** Cached time sources used by this animator */
	UPROPERTY()
	TArray<TObjectPtr<UPropertyAnimatorCoreTimeSourceBase>> TimeSources;

	/** Evaluated property container, reset on every update round */
	UPROPERTY(Transient, DuplicateTransient, NonTransactional)
	FInstancedPropertyBag EvaluatedPropertyValues;

	/** Are we evaluating properties currently */
	bool bEvaluatingProperties = false;
};