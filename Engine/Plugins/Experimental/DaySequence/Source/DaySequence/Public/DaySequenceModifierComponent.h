// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/SceneComponent.h"
#include "Tickable.h"
#include "Components/BoxComponent.h"
#include "Containers/Ticker.h"
#include "DaySequenceActor.h"
#include "DaySequenceConditionSet.h"
#include "DrawDebugHelpers.h"	// Defines ENABLE_DRAW_DEBUG
#include "Generators/MovieSceneEasingFunction.h"

#include "DaySequenceModifierComponent.generated.h"

class UMaterialInterface;
class UShapeComponent;
namespace ECollisionEnabled { enum Type : int; }
namespace EEndPlayReason { enum Type : int; }
struct FComponentReference;

class UDaySequence;
class UDaySequenceCollectionAsset;
class UMovieSceneSubSection;

struct FDaySequenceCollectionEntry;

#if ENABLE_DRAW_DEBUG
namespace UE::DaySequence
{
	struct FDaySequenceDebugEntry;
}
#endif

/** Enum specifying how to control a day / night cycle from a modifier */
UENUM(BlueprintType)
enum class EDayNightCycleMode : uint8
{
	/** (default) Make no changes to the day/night cycle time */
	Default,
	/** Force the day/night cycle to be fixed at the specified constant time */
	FixedTime,
	/** Set an initial time for the day/night cycle when this modifier is enabled */
	StartAtSpecifiedTime,
	/** Use a random, fixed time for the day/night cycle */
	RandomFixedTime,
	/** Start the day/night cycle at a random time, and allow it to continue from there */
	RandomStartTime,
};

/** Enum specifying how to blend into and out of a UDaySequenceModifierComponent that is using a trigger volume */
UENUM(BlueprintType)
enum class EDaySequenceModifierBlendMode : uint8
{
	/** Do not blend into the volume - modifier will immediately active when enabled */
	None,
	/** Blend the modifier in based on the distance within the volume. Only functional when EnableDistanceVolumeBlends is called. */
	Distance,
};

#if WITH_EDITOR
	/**
	 * Editor-only tickable class that allows us to enable trigger volume previews based on
	 * persepective camera position in the level viewport.
	 */
	struct FDaySequenceModifierComponentTickableBase : FTickableGameObject
	{
		virtual void UpdateEditorPreview(float DeltaTime)
		{}

		void Tick(float DeltaTime) override
		{
			/// Overridden here to work around ambiguous Tick function on USceneComponent
			// Re-trigger the function as a differnt named virtual function
			UpdateEditorPreview(DeltaTime);
		}
	};
#else
	/** Empty in non-editor builds */
	struct FDaySequenceModifierComponentTickableBase {};
#endif

UCLASS()
class UDaySequenceModifierEasingFunction
	: public UObject
	, public IMovieSceneEasingFunction
{
public:
	GENERATED_BODY()
	
	enum class EEasingFunctionType
	{
		EaseIn,
		EaseOut
	};
	
	void Initialize(EEasingFunctionType EasingType);
	
	virtual float Evaluate(float Interp) const override;

private:
	TFunction<float(float)> EvaluateImpl;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnPostReinitializeSubSequences);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnPostEnableModifier);

UCLASS(MinimalAPI, BlueprintType, Blueprintable, config=Game, HideCategories=(Physics,Navigation,Collision,HLOD,Rendering,Cooking,Mobile,RayTracing,AssetUserData), meta=(BlueprintSpawnableComponent))
class UDaySequenceModifierComponent
	: public USceneComponent
	, public FDaySequenceModifierComponentTickableBase
{
public:
	GENERATED_BODY()

	UDaySequenceModifierComponent(const FObjectInitializer& Init);

#if WITH_EDITOR
	DAYSEQUENCE_API static void SetVolumePreviewLocation(const FVector& Location);
	DAYSEQUENCE_API static void SetIsSimulating(bool bInIsSimulating);
#endif

	/**
	 * Bind this component to the specified day sequence actor.
	 * Will not add our overrides to the sub-sequence until EnableModifier is called.
	 */
	UFUNCTION(BlueprintCallable, Category="Day Sequence")
	DAYSEQUENCE_API void BindToDaySequenceActor(ADaySequenceActor* DaySequenceActor);

	/**
	 * Unbind this component from its day sequence actor if valid.
	 * Will removes the sub-sequence from the root sequence if it's set up.
	 */
	UFUNCTION(BlueprintCallable, Category="Day Sequence")
	DAYSEQUENCE_API void UnbindFromDaySequenceActor();

	/**
	 * Enable this component.
	 */
	UFUNCTION(BlueprintCallable, Category="Day Sequence")
	DAYSEQUENCE_API void EnableComponent();

	/**
	 * Disable this component.
	 * Will remove the sub-sequence from the root sequence if it's set up.
	 */
	UFUNCTION(BlueprintCallable, Category="Day Sequence")
	DAYSEQUENCE_API void DisableComponent();

	/**
	 * Enable this modifier by enabling its sub-sequence within the root Day Sequence
	 * Will initialize a new sub-sequence track if necessary.
	 */
	UFUNCTION(BlueprintCallable, Category="Day Sequence")
	DAYSEQUENCE_API void EnableModifier();

	/**
	 * Disable this modifier by disabling its sub-sequence track
	 */
	UFUNCTION(BlueprintCallable, Category="Day Sequence")
	DAYSEQUENCE_API void DisableModifier();

public:

	/**
	 * Remove all the Sequencer tracks within our procedural Day Sequence
	 */
	UFUNCTION(BlueprintCallable, Category="Day Sequence")
	DAYSEQUENCE_API void ResetOverrides();

	/**
	 * Add a new override for the static time of day on the day-sequence actor
	 *
	 * @param Hours         The static time of day in hours to use
	 */
	UFUNCTION(BlueprintCallable, Category="Day Sequence")
	DAYSEQUENCE_API void AddStaticTimeOfDayOverride(ADaySequenceActor* Actor, float Hours);

	/**
	 * Add a new boolean override for the specified property
	 *
	 * @param Object        The object that contains the property we want to override. Must either be the ADaySequenceActor or one of its components
	 * @param PropertyName  The name of the property to override
	 * @param Value         The value to override
	 */
	UFUNCTION(BlueprintCallable, Category="Day Sequence")
	DAYSEQUENCE_API void AddBoolOverride(UObject* Object, FName PropertyName, bool bValue);

	/**
	 * Add a new scalar override for the specified property
	 *
	 * @param Object        The object that contains the property we want to override. Must either be the ADaySequenceActor or one of its components
	 * @param PropertyName  The name of the property to override
	 * @param Value         The value to override
	 */
	UFUNCTION(BlueprintCallable, Category="Day Sequence")
	DAYSEQUENCE_API void AddScalarOverride(UObject* Object, FName PropertyName, double Value);

	/**
	 * Add a new vector override for the specified property
	 *
	 * @param Object        The object that contains the property we want to override. Must either be the ADaySequenceActor or one of its components
	 * @param PropertyName  The name of the property to override
	 * @param Value         The value to override
	 */
	UFUNCTION(BlueprintCallable, Category="Day Sequence")
	DAYSEQUENCE_API void AddVectorOverride(UObject* Object, FName PropertyName, FVector Value);

	/**
	 * Add a new color override for the specified property
	 *
	 * @param Object        The object that contains the property we want to override. Must either be the ADaySequenceActor or one of its components
	 * @param PropertyName  The name of the property to override.
	 * @param Value         The value to override
	 */
	UFUNCTION(BlueprintCallable, Category="Day Sequence")
	DAYSEQUENCE_API void AddColorOverride(UObject* Object, FName PropertyName, FLinearColor Value);

	/**
	 * Add a new transform override for the specified property
	 *
	 * @param Object        The object that contains the property we want to override. Must either be the ADaySequenceActor or one of its components
	 * @param Value         The value to override
	 */
	UFUNCTION(BlueprintCallable, Category="Day Sequence")
	DAYSEQUENCE_API void AddTransformOverride(UObject* Object, FTransform Value);

	/**
	 * Add a new material override for the specified material element index
	 *
	 * @param Object          The object that contains the property we want to override. Must either be the ADaySequenceActor or one of its components
	 * @param MaterialIndex   The index of the material element to override
	 * @param Value           The material to assign
	 */
	UFUNCTION(BlueprintCallable, Category="Day Sequence")
	DAYSEQUENCE_API void AddMaterialOverride(UObject* Object, int32 MaterialIndex, UMaterialInterface* Value);

	/**
	 * Add a new scalar material parameter override for the specified material element index and parameter name
	 *
	 * @param Object          The object that contains the material we want to override. Must either be the ADaySequenceActor or one of its components
	 * @param MaterialIndex   The index of the material element to override
	 * @param ParameterName   The name of the parameter to override
	 * @param Value           The scalar value to assign to the material parameter
	 */
	UFUNCTION(BlueprintCallable, Category="Day Sequence")
	DAYSEQUENCE_API void AddScalarMaterialParameterOverride(UObject* Object, int32 MaterialIndex, FName ParameterName, float Value);

	/**
	 * Add a new color material parameter override for the specified material element index and parameter name
	 *
	 * @param Object          The object that contains the material we want to override. Must either be the ADaySequenceActor or one of its components
	 * @param MaterialIndex   The index of the material element to override
	 * @param ParameterName   The name of the parameter to override
	 * @param Value           The color value to assign to the material parameter
	 */
	UFUNCTION(BlueprintCallable, Category="Day Sequence")
	DAYSEQUENCE_API void AddColorMaterialParameterOverride(UObject* Object, int32 MaterialIndex, FName ParameterName, FLinearColor Value);

	/**
	 * Add a new visibility override for the specified property
	 *
	 * @param Object        The object that contains the property we want to override. Must either be the ADaySequenceActor or one of its components
	 * @param Value         The value to override
	 */
	UFUNCTION(BlueprintCallable, Category="Day Sequence")
	DAYSEQUENCE_API void AddVisibilityOverride(UObject* Object, bool bValue);

	/**
	 * Sets the user day sequence
	 */
	UFUNCTION(BlueprintCallable, Category="Day Sequence")
	DAYSEQUENCE_API void SetUserDaySequence(UDaySequence* InDaySequence);

	/**
	 * Sets whether to enable modifier based on volume
	 */
	DAYSEQUENCE_API void SetUseVolume(bool bState);

	/**
	 * Enable distance-based volume blends based on the location of the specified actor relative to our volume.
	 * FinalWeight = Min(DistanceVolumeBlendWeight, CustomVolumeBlendWeight).
	 */
	UFUNCTION(BlueprintCallable, Category="Day Sequence")
	DAYSEQUENCE_API void EnableDistanceVolumeBlends(APlayerController* InActor);

	/**
	 * Determines the number of volumes the current blend target is
	 * overlapping and returns true if that number is greater than 0
	 */
	UFUNCTION(BlueprintCallable, Category="Day Sequence")
	DAYSEQUENCE_API bool IsBlendTargetInAnyVolume();
	
	/**
	 * Sets a custom blend weight for volume based blends.
	 * FinalWeight = Min(DistanceVolumeBlendWeight, CustomVolumeBlendWeight).
	 */
	UFUNCTION(BlueprintCallable, Category="Day Sequence")
	DAYSEQUENCE_API void SetCustomVolumeBlendWeight(float Weight);

	/**
	 * Get the current blend weight.
	 */
	UFUNCTION(BlueprintCallable, Category="Day Sequence")
	float GetCurrentBlendWeight() const;
	
	float UpdateBlendWeight() const;

	/**
	 * Applies the given collision type to all valid volume shape components.
	 */
	UFUNCTION(BlueprintCallable, Category="Day Sequence")
	void SetVolumeCollisionEnabled(const ECollisionEnabled::Type InCollisionType) const;

	void EmptyVolumeShapeComponents();
	void AddVolumeShapeComponent(const FComponentReference& InShapeReference);

	void InvalidateMuteStates() const;

#if ENABLE_DRAW_DEBUG
	bool ShouldShowDebugInfo() const;
#endif
	
public:

	/*~ Begin UActorComponent interface */
	void BeginPlay() override;
	void EndPlay(EEndPlayReason::Type Reason) override;
	void OnRegister() override;
	void OnUnregister() override;
	/*~ End UActorComponent interface */

	/** Bound to delegate on the DaySequenceActor that allows all modifiers to do work at appropriate times at the specific actors tick interval. */
	void DaySequenceUpdate();
	
#if WITH_EDITOR
	/*~ Begin FTickableGameObject interface */
	void UpdateEditorPreview(float DeltaTime) override;
	TStatId GetStatId() const override;
	ETickableTickType GetTickableTickType() const override;
	bool IsTickableWhenPaused() const override { return true; }
	bool IsTickableInEditor() const override { return true; }
	bool IsTickable() const override { return true; }
	/*~ End FTickableGameObject interface */

	void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;

#endif

protected:

	virtual bool CanBeEnabled() const;

	UPROPERTY(Instanced, Transient)
	TObjectPtr<UDaySequenceModifierEasingFunction> EasingFunction;

	TArray<UShapeComponent*> GetVolumeShapeComponents() const;

protected:

	void SetInitialTimeOfDay();

protected:

	/** Non-serialized target actor we are currently bound to */
	UPROPERTY(Transient, BlueprintReadWrite, Category="Day Sequence")
	TObjectPtr<ADaySequenceActor> TargetActor;

	/** When set, the shape components will be used for the modifier volume, otherwise the default Box component will be used. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Day Sequence", meta=(UseComponentPicker, AllowedClasses="/Script/Engine.ShapeComponent", AllowAnyActor))
	TArray<FComponentReference> VolumeShapeComponents;

	/** The actor to use for distance-based volume blend calculations */
	UPROPERTY(Transient)
	TWeakObjectPtr<APlayerController> ExternalVolumeBlendTarget;

	/** An optional user-provided Day Sequence - used instead of our procedurally generated one if set */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Day Sequence", meta=(EditCondition="!bUseCollection", EditConditionHides, DisplayAfter="bUseCollection"))
	TObjectPtr<UDaySequence> UserDaySequence;

	/** The procedurally generated sequence containing our override tracks. Owned by this component. */
	UPROPERTY(Transient)
	TObjectPtr<UDaySequence> ProceduralDaySequence;

	UPROPERTY(EditAnywhere, Category="Day Sequence", meta=(EditCondition="bUseCollection", EditConditionHides, DisplayAfter="bUseCollection"))
	TObjectPtr<UDaySequenceCollectionAsset> DaySequenceCollection;
	
	/** User-defined bias. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Day Sequence", meta=(EditCondition="!bIgnoreBias"))
	int32 Bias;

	/** The time to use for the day/night cycle. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Time", DisplayName="Time", meta=(DisplayAfter="DayNightCycle", EditCondition="DayNightCycle==EDayNightCycleMode::FixedTime || DayNightCycle==EDayNightCycleMode::StartAtSpecifiedTime", EditConditionHides))
	float DayNightCycleTime;

	/** Defines the blend range based on BlendMode. When BlendMode == Distance, this is interpreted as world units */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Day Sequence", meta=(DisplayAfter="BlendMode", EditCondition="BlendMode!=EDaySequenceModifierBlendMode::None", EditConditionHides))
	float BlendAmount;

	/** Cached blend factor last time we ticked in the range 0-1. Used to prevent superfluous updates on a paused day sequence. */
	float CachedBlendFactor;

	/** Defines a custom blend weight. The final blend weight is the minimum between the Distance blend weight and CustomVolumeBlendWeight. */
	UPROPERTY(Transient)
	float CustomVolumeBlendWeight;

	/** Changes the way this modifier controls the day/night cycle time */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Time", DisplayName="Day/Night Cycle")
	EDayNightCycleMode DayNightCycle;

	/** Changes the way this modifier blends in and out */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Day Sequence")
	EDaySequenceModifierBlendMode BlendMode;

	/** Blueprint exposed delegate invoked after the component's subsequences are reinitialized. */
	UPROPERTY(BlueprintAssignable, Transient, meta=(AllowPrivateAccess="true"))
	FOnPostReinitializeSubSequences OnPostReinitializeSubSequences;

	/** Blueprint exposed delegate invoked after the modifier component is enabled. */
	UPROPERTY(BlueprintAssignable, Transient, meta=(AllowPrivateAccess="true"))
	FOnPostEnableModifier OnPostEnableModifier;
	
	/** When enabled, these overrides will always override all settings regardless of their bias */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Day Sequence", meta=(InlineEditConditionToggle))
	uint8 bIgnoreBias : 1;

	/** Whether to use the bounding volume for enabling this modifier. If not enabled this modifier will always be active. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Day Sequence")
	uint8 bUseVolume : 1;

	/** Flag used track whether or not this component is enabled or disabled. */
	UPROPERTY(Transient, DuplicateTransient, BlueprintReadOnly, Category="Day Sequence")
	uint8 bIsComponentEnabled : 1;
	
	/** Non-serialized variable for tracking whether our overrides are enabled or not */
	UPROPERTY(Transient, DuplicateTransient, BlueprintReadOnly, Category="Day Sequence")
	uint8 bIsEnabled : 1;

	/** Preview this day sequence modifier in the editor */
	UPROPERTY(Transient, BlueprintReadWrite, EditAnywhere, Category="Day Sequence")
	uint8 bPreview : 1;

	/** Flag to keep track of whether we need to unpause the day sequence when we are disabled */
	uint8 bUnpauseOnDisable : 1;

	/** If true, hide UserDaySequence and expose DaySequenceCollection. */
	UPROPERTY(EditAnywhere, Category="Day Sequence")
	uint8 bUseCollection : 1;

private:
	/**
	 * Get the blend position (handles preview and game world).
	 * @return Returns true if we have a valid blend position and InPosition was set, false if InPosition was not set.
	 */
	bool GetBlendPosition(FVector& InPosition) const;
	float GetDistanceBlendFactorForShape(const UShapeComponent* Shape, const FVector& Position) const;

	mutable float CachedDistanceBlendFactor;
	float GetCachedDistanceBlendFactor() const { return CachedDistanceBlendFactor; }
	float GetDistanceBlendFactor(const FVector& Position) const;
	/**
	 * Creates and adds or marks for preserve all subsections that this modifier is responsible for.
	 * Optionally provided a map of all sections that exist in the root sequence to a bool flag used to mark that section as still relevant.
	 */
	void ReinitializeSubSequence(ADaySequenceActor::FSubSectionPreserveMap* SectionsToPreserve);
	UMovieSceneSubSection* InitializeDaySequence(const FDaySequenceCollectionEntry& SequenceAsset);
	void RemoveSubSequenceTrack();
	FGuid GetOrCreateProceduralBinding(UObject* Object);

	void UpdateCachedExternalShapes() const;
	mutable TArray<TWeakObjectPtr<UShapeComponent>> CachedExternalShapes;
	mutable bool bCachedExternalShapesInvalid = true;
	
	int32 OccupiedVolumes = 0;
	
	/*~ Transient state for active gameplay */
	TWeakObjectPtr<UMovieSceneSubSection> WeakSubSection;
	TArray<TWeakObjectPtr<UMovieSceneSubSection>> SubSections;

	UE::DaySequence::FOnInvalidateMuteStates OnInvalidateMuteStates;
	
#if ENABLE_DRAW_DEBUG
	const FName ShowDebug_ModifierCategory = "DaySequence_Modifiers";
	
	void OnDebugLevelChanged(int32 InDebugLevel);
	
	/** Determines whether or not this modifier will show debug info */
	int32 DebugLevel;

	TSharedPtr<UE::DaySequence::FDaySequenceDebugEntry> DebugEntry;
	TArray<TSharedPtr<UE::DaySequence::FDaySequenceDebugEntry>> SubSectionDebugEntries;
#endif
};
