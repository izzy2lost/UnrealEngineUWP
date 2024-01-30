// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AvaClonerEffectorShared.h"
#include "Curves/RichCurve.h"
#include "GameFramework/Actor.h"
#include "Layouts/AvaClonerLayoutBase.h"
#include "NiagaraMeshRendererProperties.h"
#include "AvaPropertyChangeDispatcher.h"
#include "AvaClonerActor.generated.h"

class AActor;
class AAvaEffectorActor;
class UAvaClonerComponent;
class UAvaClonerLayoutBase;
class UNiagaraDataInterfaceCurve;
class USceneComponent;
struct FAvaClonerSystem;

UCLASS(BlueprintType, HideCategories=(Rendering,Replication,Collision,HLOD,Physics,Networking,Input,Actor,Cooking,LevelInstance,Streaming), DisplayName = "Motion Design Cloner Actor")
class AVALANCHEEFFECTORS_API AAvaClonerActor : public AActor
{
	GENERATED_BODY()

	friend class AAvaEffectorActor;
	friend class UAvaClonerLayoutBase;

	friend class FAvaClonerActorVisualizer;
	friend class FAvaClonerDetailCustomization;

public:
	static inline const FString DefaultLabel = TEXT("Cloner");

	AAvaClonerActor();

	UFUNCTION(BlueprintCallable, Category="Cloner")
	void SetTreeUpdateInterval(float InInterval);

	UFUNCTION(BlueprintPure, Category="Cloner")
	float GetTreeUpdateInterval() const
	{
		return TreeUpdateInterval;
	}

	UFUNCTION(BlueprintCallable, Category="Cloner")
	void SetMeshRenderMode(EAvaClonerMeshRenderMode InMode);

	UFUNCTION(BlueprintPure, Category="Cloner")
	EAvaClonerMeshRenderMode GetMeshRenderMode() const
	{
		return MeshRenderMode;
	}

	UFUNCTION(BlueprintCallable, Category="Cloner")
	void SetMeshFacingMode(ENiagaraMeshFacingMode InMode);

	UFUNCTION(BlueprintPure, Category="Cloner")
	ENiagaraMeshFacingMode GetMeshFacingMode() const
	{
		return MeshFacingMode;
	}

	UFUNCTION(BlueprintCallable, Category="Cloner")
	void SetMeshCastShadows(bool InbCastShadows);

	UFUNCTION(BlueprintPure, Category="Cloner")
	bool GetMeshCastShadows() const
	{
		return bMeshCastShadows;
	}

	void SetDefaultMeshes(const TArray<TObjectPtr<UStaticMesh>>& InMeshes);

	const TArray<TObjectPtr<UStaticMesh>>& GetDefaultMeshes() const
	{
		return DefaultMeshes;
	}

	UFUNCTION(BlueprintCallable, Category="Cloner", meta=(DisplayName="SetDefaultMeshes"))
	void BP_SetDefaultMeshes(const TArray<UStaticMesh*>& InMeshes);

	UFUNCTION(BlueprintPure, Category="Cloner", meta=(DisplayName="GetDefaultMeshes"))
	TArray<UStaticMesh*> BP_GetDefaultMeshes() const;

	UFUNCTION(BlueprintCallable, Category="Cloner")
	void SetSeed(int32 InSeed);

	UFUNCTION(BlueprintPure, Category="Cloner")
	int32 GetSeed() const
	{
		return Seed;
	}

	UFUNCTION(BlueprintCallable, Category="Cloner")
	void SetDeltaStepEnabled(bool bInEnabled);

	UFUNCTION(BlueprintPure, Category="Cloner")
	bool GetDeltaStepEnabled() const
	{
		return bDeltaStepEnabled;
	}

	UFUNCTION(BlueprintCallable, Category="Cloner")
	void SetDeltaStepRotation(const FRotator& InRotation);

	UFUNCTION(BlueprintPure, Category="Cloner")
	FRotator GetDeltaStepRotation() const
	{
		return DeltaStepRotation;
	}

	UFUNCTION(BlueprintCallable, Category="Cloner")
	void SetDeltaStepScale(const FVector& InScale);

	UFUNCTION(BlueprintPure, Category="Cloner")
	FVector GetDeltaStepScale() const
	{
		return DeltaStepScale;
	}

	UFUNCTION(BlueprintCallable, Category="Cloner")
	void SetEnabled(bool bInEnable);

	UFUNCTION(BlueprintPure, Category="Cloner")
	bool GetEnabled() const
	{
		return bEnabled;
	}

	UFUNCTION(BlueprintCallable, Category="Cloner")
	void SetInvertProgress(bool bInInvertProgress);

	UFUNCTION(BlueprintPure, Category="Cloner")
	bool GetInvertProgress() const
	{
		return bInvertProgress;
	}

	UFUNCTION(BlueprintCallable, Category="Cloner")
	void SetProgress(float InProgress);

	UFUNCTION(BlueprintPure, Category="Cloner")
	float GetProgress() const
	{
		return Progress;
	}

	UFUNCTION(BlueprintCallable, Category="Cloner")
	void SetLayoutName(FName InLayoutName);

	UFUNCTION(BlueprintPure, Category="Cloner")
	FName GetLayoutName() const
	{
		return LayoutName;
	}

	UFUNCTION(BlueprintCallable, Category="Cloner")
	void SetRangeEnabled(bool bInRangeEnabled);

	UFUNCTION(BlueprintPure, Category="Cloner")
	bool GetRangeEnabled() const
	{
		return bRangeEnabled;
	}

	UFUNCTION(BlueprintCallable, Category="Cloner")
	void SetRangeOffsetMin(const FVector& InRangeOffsetMin);

	UFUNCTION(BlueprintPure, Category="Cloner")
	const FVector& GetRangeOffsetMin() const
	{
		return RangeOffsetMin;
	}

	UFUNCTION(BlueprintCallable, Category="Cloner")
	void SetRangeOffsetMax(const FVector& InRangeOffsetMax);

	UFUNCTION(BlueprintPure, Category="Cloner")
	const FVector& GetRangeOffsetMax() const
	{
		return RangeOffsetMax;
	}

	UFUNCTION(BlueprintCallable, Category="Cloner")
	void SetRangeRotationMin(const FRotator& InRangeRotationMin);

	UFUNCTION(BlueprintPure, Category="Cloner")
	const FRotator& GetRangeRotationMin() const
	{
		return RangeRotationMin;
	}

	UFUNCTION(BlueprintCallable, Category="Cloner")
	void SetRangeRotationMax(const FRotator& InRangeRotationMax);

	UFUNCTION(BlueprintPure, Category="Cloner")
	const FRotator& GetRangeRotationMax() const
	{
		return RangeRotationMax;
	}

	UFUNCTION(BlueprintCallable, Category="Cloner")
	void SetRangeScaleUniform(bool bInRangeScaleUniform);

	UFUNCTION(BlueprintPure, Category="Cloner")
	bool GetRangeScaleUniform() const
	{
		return bRangeScaleUniform;
	}

	UFUNCTION(BlueprintCallable, Category="Cloner")
	void SetRangeScaleMin(const FVector& InRangeScaleMin);

	UFUNCTION(BlueprintPure, Category="Cloner")
	const FVector& GetRangeScaleMin() const
	{
		return RangeScaleMin;
	}

	UFUNCTION(BlueprintCallable, Category="Cloner")
	void SetRangeScaleMax(const FVector& InRangeScaleMax);

	UFUNCTION(BlueprintPure, Category="Cloner")
	const FVector& GetRangeScaleMax() const
	{
		return RangeScaleMax;
	}

	UFUNCTION(BlueprintCallable, Category="Cloner")
	void SetRangeScaleUniformMin(float InRangeScaleUniformMin);

	UFUNCTION(BlueprintPure, Category="Cloner")
	float GetRangeScaleUniformMin() const
	{
		return RangeScaleUniformMin;
	}

	UFUNCTION(BlueprintCallable, Category="Cloner")
	void SetRangeScaleUniformMax(float InRangeScaleUniformMax);

	UFUNCTION(BlueprintPure, Category="Cloner")
	float GetRangeScaleUniformMax() const
	{
		return RangeScaleUniformMax;
	}

	UFUNCTION(BlueprintCallable, Category="Cloner")
	void SetSpawnLoopMode(EAvaClonerSpawnLoopMode InMode);

	UFUNCTION(BlueprintPure, Category="Cloner")
	EAvaClonerSpawnLoopMode GetSpawnLoopMode() const
	{
		return SpawnLoopMode;
	}

	UFUNCTION(BlueprintCallable, Category="Cloner")
	void SetSpawnLoopIterations(int32 InIterations);

	UFUNCTION(BlueprintPure, Category="Cloner")
	int32 GetSpawnLoopIterations() const
	{
		return SpawnLoopIterations;
	}

	UFUNCTION(BlueprintCallable, Category="Cloner")
	void SetSpawnLoopInterval(float InInterval);

	UFUNCTION(BlueprintPure, Category="Cloner")
	float GetSpawnLoopInterval() const
	{
		return SpawnLoopInterval;
	}

	UFUNCTION(BlueprintCallable, Category="Cloner")
	void SetSpawnBehaviorMode(EAvaClonerSpawnBehaviorMode InMode);

	UFUNCTION(BlueprintPure, Category="Cloner")
	EAvaClonerSpawnBehaviorMode GetSpawnBehaviorMode() const
	{
		return SpawnBehaviorMode;
	}

	UFUNCTION(BlueprintCallable, Category="Cloner")
	void SetSpawnRate(float InRate);

	UFUNCTION(BlueprintPure, Category="Cloner")
	float GetSpawnRate() const
	{
		return SpawnRate;
	}

	UFUNCTION(BlueprintCallable, Category="Cloner")
	void SetLifetimeEnabled(bool bInEnabled);

	UFUNCTION(BlueprintPure, Category="Cloner")
	bool GetLifetimeEnabled() const
	{
		return bLifetimeEnabled;
	}

	UFUNCTION(BlueprintCallable, Category="Cloner")
	void SetLifetimeMin(float InMin);

	UFUNCTION(BlueprintPure, Category="Cloner")
	float GetLifetimeMin() const
	{
		return LifetimeMin;
	}

	UFUNCTION(BlueprintCallable, Category="Cloner")
	void SetLifetimeMax(float InMax);

	UFUNCTION(BlueprintPure, Category="Cloner")
	float GetLifetimeMax() const
	{
		return LifetimeMax;
	}

	UFUNCTION(BlueprintCallable, Category="Cloner")
	void SetLifetimeScaleEnabled(bool bInEnabled);

	UFUNCTION(BlueprintPure, Category="Cloner")
	bool GetLifetimeScaleEnabled() const
	{
		return bLifetimeScaleEnabled;
	}

	UFUNCTION()
	void SetLifetimeScaleCurve(const FRichCurve& InCurve);

	UFUNCTION()
	const FRichCurve& GetLifetimeScaleCurve() const
	{
		return LifetimeScaleCurve;
	}

	UFUNCTION(BlueprintPure, Category="Cloner")
	UAvaClonerLayoutBase* GetActiveLayout() const;

	template<
		typename InLayoutClass
		UE_REQUIRES(TIsDerivedFrom<InLayoutClass, UAvaClonerLayoutBase>::Value)>
	bool IsActiveLayout() const
	{
		if (const UAvaClonerLayoutBase* CurrentLayout = GetActiveLayout())
		{
			return CurrentLayout->GetClass() == InLayoutClass::StaticClass();
		}

		return false;
	}

	template<
		typename InLayoutClass
		UE_REQUIRES(TIsDerivedFrom<InLayoutClass, UAvaClonerLayoutBase>::Value)>
	InLayoutClass* GetActiveLayout() const
	{
		return Cast<InLayoutClass>(GetActiveLayout());
	}

	/** Returns the number of meshes this cloner currently handles */
	UFUNCTION(BlueprintPure, Category="Cloner")
	int32 GetMeshCount() const;

	/** Gets the number of effectors applied on this cloner */
	UFUNCTION(BlueprintPure, Category="Cloner")
	int32 GetEffectorCount() const;

#if WITH_EDITOR
	/** This will force an update of the cloner attachment tree */
	UFUNCTION(CallInEditor, Category="Cloner")
	void ForceUpdateCloner();

	/** This will spawn an effector actor and link it to this cloner */
	UFUNCTION(CallInEditor, Category="Cloner")
	void SpawnLinkedEffector();

	/** When the cloner actor is created, we spawn a default actor attached */
	void SpawnDefaultActorAttached();
#endif

	bool GetClonerInitialized() const
	{
		return bClonerInitialized;
	}

protected:
	//~ Begin UObject
	virtual void PostInitProperties() override;
	virtual void PostLoad() override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& InPropertyChangedEvent) override;
#endif
	//~ End UObject

	//~ Begin AActor
	virtual void PostActorCreated() override;
	/** Allow ticking in editor viewport too */
	virtual bool ShouldTickIfViewportsOnly() const override
	{
		return true;
	}
	/** Tick needed to keep the attachment tree updated */
	virtual void Tick(float DeltaSeconds) override;
#if WITH_EDITOR
	virtual FString GetDefaultActorLabel() const override;
#endif
	//~ End AActor

	UAvaClonerComponent* GetClonerComponent() const
	{
		return ClonerComponent;
	}

	UNiagaraDataInterfaceCurve* GetLifetimeScaleCurveDI() const
	{
		return LifetimeScaleCurveDIWeak.Get();
	}

	/** Will force a system update to refresh user parameters */
	void RequestClonerUpdate(bool bInImmediate = false);

	void UpdateLayoutOptions();
	void UpdateClonerEffectors();

	/** Used by effector actors to apply transformations to this cloner instances */
	const FAvaClonerEffectorDataInterfaces* GetEffectorDataInterfaces() const;

	/** Register a new effector and return the index of this effector to match niagara data interface */
	int32 RegisterEffector(AAvaEffectorActor* InEffector);

	/** Unregister the effector and reorganize the niagara data interface to match with the new array indexes */
	bool UnregisterEffector(AAvaEffectorActor* InEffector);

	/** Checks if an effector is registered with this cloner */
	bool IsEffectorRegistered(const AAvaEffectorActor* InEffector) const;

	/** Gets the index of an effector in this cloner */
	int32 GetEffectorIndex(AAvaEffectorActor* InEffector) const;

	/** For each valid effector that has this cloner registered */
	void ForEachEffector(TFunctionRef<bool(AAvaEffectorActor*, int32)> InFunction);

	void OnClonerTransformed(USceneComponent*, EUpdateTransformFlags, ETeleportType);
	void OnClonerMeshUpdated();
	void OnClonerSystemChanged();

	void OnEnabledChanged();
	void OnMeshRenderModeChanged();
	void OnMeshRendererOptionsChanged();
	void OnDefaultMeshesChanged();
	void OnSeedChanged();
	void OnProgressChanged();
	void OnDeltaStepChanged();
	void OnRangeOptionsChanged();
	void OnLayoutNameChanged();
	void OnSpawnOptionsChanged();
	void OnLifetimeScaleCurveChanged();
	void OnLifetimeOptionsChanged();

	/** Update sprite visibility of this cloner */
	void OnVisualizerSpriteVisibleChanged();

	bool IsClonerValid() const;

#if WITH_EDITOR
	void OnReduceMotionGhostingChanged();
	void OnCVarChanged();
#endif

	/** Is this cloner enabled/disabled */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Setter="SetEnabled", Getter="GetEnabled", Category="Cloner")
	bool bEnabled = true;

	/** Interval to update the attachment tree and update the cloner meshes */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Setter="SetTreeUpdateInterval", Getter="GetTreeUpdateInterval", Category="Cloner", AdvancedDisplay, meta=(ClampMin="0.0"))
	float TreeUpdateInterval = 0.2f;

	/** Cloner instance seed for random deterministic patterns */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Setter="SetSeed", Getter="GetSeed", Category="Cloner")
	int32 Seed = 0;

	/** Indicates how we select the mesh to render on each clones */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Setter="SetMeshRenderMode", Getter="GetMeshRenderMode", Category="Renderer")
	EAvaClonerMeshRenderMode MeshRenderMode = EAvaClonerMeshRenderMode::Iterate;

	/** Mode to indicate how clones facing is determined */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Setter="SetMeshFacingMode", Getter="GetMeshFacingMode", Category="Renderer")
	ENiagaraMeshFacingMode MeshFacingMode = ENiagaraMeshFacingMode::Default;

	/** Whether clones cast shadow, disabling will result in better performance */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Setter="SetMeshCastShadows", Getter="GetMeshCastShadows", Category="Renderer")
	bool bMeshCastShadows = true;

	/** When nothing is attached to the cloner, these meshes are used as default */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Setter="SetDefaultMeshes", Getter="GetDefaultMeshes", BlueprintSetter="BP_SetDefaultMeshes", BlueprintGetter="BP_GetDefaultMeshes", Category="Renderer")
	TArray<TObjectPtr<UStaticMesh>> DefaultMeshes;

	/** How many times do we spawn clones */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Setter="SetSpawnLoopMode", Getter="GetSpawnLoopMode", DisplayName="LoopMode", Category="Spawn")
	EAvaClonerSpawnLoopMode SpawnLoopMode = EAvaClonerSpawnLoopMode::Once;

	/** Amount of spawn iterations for clones */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Setter="SetSpawnLoopIterations", Getter="GetSpawnLoopIterations", DisplayName="LoopIterations", Category="Spawn", meta=(ClampMin="1", EditCondition="SpawnLoopMode == EAvaClonerSpawnLoopMode::Multiple", EditConditionHides))
	int32 SpawnLoopIterations = 1;

	/** Interval/Duration of spawn for clones */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Setter="SetSpawnLoopInterval", Getter="GetSpawnLoopInterval", DisplayName="LoopInterval", Category="Spawn", meta=(ClampMin="0"))
	float SpawnLoopInterval = 1.f;

	/** How does spawn occurs */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Setter="SetSpawnBehaviorMode", Getter="GetSpawnBehaviorMode", DisplayName="BehaviorMode", Category="Spawn")
	EAvaClonerSpawnBehaviorMode SpawnBehaviorMode = EAvaClonerSpawnBehaviorMode::Instant;

	/** How many clones to spawn each seconds */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Setter="SetSpawnRate", Getter="GetSpawnRate", DisplayName="Rate", Category="Spawn", meta=(ClampMin="0", EditCondition="SpawnBehaviorMode == EAvaClonerSpawnBehaviorMode::Rate", EditConditionHides))
	float SpawnRate = 1.f;

	/** Do we destroy the clones after a specific duration */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Setter="SetLifetimeEnabled", Getter="GetLifetimeEnabled", DisplayName="Enabled", Category="Lifetime")
	bool bLifetimeEnabled = false;

	/** Minimum lifetime for a clone */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Setter="SetLifetimeMin", Getter="GetLifetimeMin", DisplayName="Min", Category="Lifetime", meta=(ClampMin="0", EditCondition="bLifetimeEnabled", EditConditionHides))
	float LifetimeMin = 0.25f;

	/** Maximum lifetime for a clone */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Setter="SetLifetimeMax", Getter="GetLifetimeMax", DisplayName="Max", Category="Lifetime", meta=(ClampMin="0", EditCondition="bLifetimeEnabled", EditConditionHides))
	float LifetimeMax = 1.5f;

	/** Enable scale by lifetime */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Setter="SetLifetimeScaleEnabled", Getter="GetLifetimeScaleEnabled", DisplayName="ScaleEnabled", Category="Lifetime", meta=(EditCondition="bLifetimeEnabled", EditConditionHides))
	bool bLifetimeScaleEnabled = false;

	/** Used to expose the scale curve editor in details panel */
	UPROPERTY(Transient, DuplicateTransient, TextExportTransient)
	TWeakObjectPtr<UNiagaraDataInterfaceCurve> LifetimeScaleCurveDIWeak;

	UPROPERTY(Setter="SetLifetimeScaleCurve", Getter="GetLifetimeScaleCurve")
	FRichCurve LifetimeScaleCurve;

	/** Name of the layout to use */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Setter="SetLayoutName", Getter="GetLayoutName", Category="Layout", meta=(GetOptions="GetClonerLayoutNames"))
	FName LayoutName = NAME_None;

	/** Active layout */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Instanced, Transient, DuplicateTransient, Category="Layout", meta=(DisplayAfter="LayoutName"))
	TObjectPtr<UAvaClonerLayoutBase> ActiveLayout;

	/** Invert progress behaviour */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Setter="SetInvertProgress", Getter="GetInvertProgress", Category="Progress")
	bool bInvertProgress = false;

	/** Changes visibility of instances based on the total count, 1.f = 100% = all instances visible */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Setter="SetProgress", Getter="GetProgress", Category="Progress", meta=(ClampMin="0", ClampMax="1"))
	float Progress = 1.f;

	/** Enable steps to add delta variation on each clone instance */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Setter="SetDeltaStepEnabled", Getter="GetDeltaStepEnabled", DisplayName="Enabled", Category="Step")
	bool bDeltaStepEnabled = false;

	/** Amount of rotation difference between one step and the next one */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Setter="SetDeltaStepRotation", Getter="GetDeltaStepRotation", DisplayName="Rotation", Category="Step", meta=(EditCondition="bDeltaStepEnabled", EditConditionHides))
	FRotator DeltaStepRotation = FRotator(0.f);

	/** Amount of scale difference between one step and the next one */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Setter="SetDeltaStepScale", Getter="GetDeltaStepScale", DisplayName="Scale", Category="Step", meta=(AllowPreserveRatio, Delta="0.0001", EditCondition="bDeltaStepEnabled", EditConditionHides))
	FVector DeltaStepScale = FVector(0.f);

	/** Use random range transforms for each clones */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Setter="SetRangeEnabled", Getter="GetRangeEnabled", DisplayName="Enabled", Category="Range")
	bool bRangeEnabled = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Setter="SetRangeOffsetMin", Getter="GetRangeOffsetMin", DisplayName="OffsetMin", Category="Range", meta=(EditCondition="bRangeEnabled", EditConditionHides))
	FVector RangeOffsetMin = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Setter="SetRangeOffsetMax", Getter="GetRangeOffsetMax", DisplayName="OffsetMax", Category="Range", meta=(EditCondition="bRangeEnabled", EditConditionHides))
	FVector RangeOffsetMax = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Setter="SetRangeRotationMin", Getter="GetRangeRotationMin", DisplayName="RotationMin", Category="Range", meta=(EditCondition="bRangeEnabled", EditConditionHides))
	FRotator RangeRotationMin = FRotator::ZeroRotator;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Setter="SetRangeRotationMax", Getter="GetRangeRotationMax", DisplayName="RotationMax", Category="Range", meta=(EditCondition="bRangeEnabled", EditConditionHides))
	FRotator RangeRotationMax = FRotator::ZeroRotator;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Setter="SetRangeScaleUniform", Getter="GetRangeScaleUniform", DisplayName="ScaleUniformEnabled", Category="Range", meta=(EditCondition="bRangeEnabled", EditConditionHides))
	bool bRangeScaleUniform = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Setter="SetRangeScaleMin", Getter="GetRangeScaleMin", DisplayName="ScaleMin", Category="Range", meta=(AllowPreserveRatio, Delta="0.0001", ClampMin="0", EditCondition="bRangeEnabled && !bRangeScaleUniform", EditConditionHides))
	FVector RangeScaleMin = FVector::OneVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Setter="SetRangeScaleMax", Getter="GetRangeScaleMax", DisplayName="ScaleMax", Category="Range", meta=(AllowPreserveRatio, Delta="0.0001", ClampMin="0", EditCondition="bRangeEnabled && !bRangeScaleUniform", EditConditionHides))
	FVector RangeScaleMax = FVector::OneVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Setter="SetRangeScaleUniformMin", Getter="GetRangeScaleUniformMin", DisplayName="ScaleMin", Category="Range", meta=(Delta="0.0001", ClampMin="0", EditCondition="bRangeEnabled && bRangeScaleUniform", EditConditionHides))
	float RangeScaleUniformMin = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Setter="SetRangeScaleUniformMax", Getter="GetRangeScaleUniformMax", DisplayName="ScaleMax", Category="Range", meta=(Delta="0.0001", ClampMin="0", EditCondition="bRangeEnabled && bRangeScaleUniform", EditConditionHides))
	float RangeScaleUniformMax = 1.f;

#if WITH_EDITORONLY_DATA
	/** Toggle the sprite to visualize and click on this cloner */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, AdvancedDisplay, Category="Cloner", meta=(AllowPrivateAccess = "true"))
	bool bVisualizerSpriteVisible = true;

	/** Reduces the r.TSR.ShadingRejection.Flickering.Period from 3 (default) to 1 if enabled to avoid ghosting artifacts when moving */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, AdvancedDisplay, Category="Cloner", meta=(AllowPrivateAccess = "true"))
	bool bReduceMotionGhosting = false;
#endif

private:
#if WITH_EDITOR
	/** Hide outline selection when this cloner only is selected in viewport */
	void OnEditorSelectionChanged(UObject* InSelection);

	TOptional<bool> UseSelectionOutline;
#endif

	/** Gets all layout names available */
	UFUNCTION()
	TArray<FString> GetClonerLayoutNames() const;

	template<
		typename InLayoutClass
		UE_REQUIRES(TIsDerivedFrom<InLayoutClass, UAvaClonerLayoutBase>::Value)>
	InLayoutClass* FindOrAddLayout()
	{
		return Cast<InLayoutClass>(FindOrAddLayout(InLayoutClass::StaticClass()));
	}

	/** Find or add a layout by its class */
	UAvaClonerLayoutBase* FindOrAddLayout(TSubclassOf<UAvaClonerLayoutBase> InClass);

	/** Find or add a layout by its name */
	UAvaClonerLayoutBase* FindOrAddLayout(FName InLayoutName);

	/** Initiate and perform operation */
	void InitializeCloner();

	/** Effectors that applies to this cloner, index match niagara data interface arrays */
	UPROPERTY(Transient)
	TArray<TWeakObjectPtr<AAvaEffectorActor>> Effectors;

	/** Cloner niagara component */
	UPROPERTY()
	TObjectPtr<UAvaClonerComponent> ClonerComponent;

	/** Previously used layout instances cached */
	UPROPERTY()
	TMap<FName, TObjectPtr<UAvaClonerLayoutBase>> LayoutInstances;

	/**
	* Below properties are deprecated and no longer in use,
	* they will be migrated to new layout system on load,
	* And they will be all removed in future version
	*/
	UPROPERTY(meta=(DeprecatedProperty, DeprecationMessage="Use LayoutName instead"))
	EAvaClonerLayout Layout_DEPRECATED = EAvaClonerLayout::Grid;

	UPROPERTY(meta=(DeprecatedProperty, DeprecationMessage="Use UAvaClonerGridLayout instead"))
	FAvaClonerGridLayoutOptions GridOptions_DEPRECATED;

	UPROPERTY(meta=(DeprecatedProperty, DeprecationMessage="Use UAvaClonerLineLayout instead"))
	FAvaClonerLineLayoutOptions LineOptions_DEPRECATED;

	UPROPERTY(meta=(DeprecatedProperty, DeprecationMessage="Use UAvaClonerCircleLayout instead"))
	FAvaClonerCircleLayoutOptions CircleOptions_DEPRECATED;

	UPROPERTY(meta=(DeprecatedProperty, DeprecationMessage="Use UAvaClonerCylinderLayout instead"))
	FAvaClonerCylinderLayoutOptions CylinderOptions_DEPRECATED;

	UPROPERTY(meta=(DeprecatedProperty, DeprecationMessage="Use UAvaClonerSphereLayout instead"))
	FAvaClonerSphereLayoutOptions SphereOptions_DEPRECATED;

	UPROPERTY(meta=(DeprecatedProperty, DeprecationMessage="Use UAvaClonerHoneycombLayout instead"))
	FAvaClonerHoneycombLayoutOptions HoneycombOptions_DEPRECATED;

	UPROPERTY(meta=(DeprecatedProperty, DeprecationMessage="Use UAvaClonerMeshLayout instead"))
	FAvaClonerSampleMeshOptions SampleMeshOptions_DEPRECATED;

	UPROPERTY(meta=(DeprecatedProperty, DeprecationMessage="Use UAvaClonerSplineLayout instead"))
	FAvaClonerSampleSplineOptions SampleSplineOptions_DEPRECATED;

	/**
	 * Used to confirm migration of deprecated properties above to new layout system
	 * Newly created cloner will not migrate anything and skip this task
	 */
	UPROPERTY()
	bool bDeprecatedPropertiesMigrated = false;

	float TreeUpdateDeltaTime = 0.f;

	bool bNeedsRefresh = false;

	bool bClonerInitialized = false;

#if WITH_EDITOR
	/** Used for PECP */
	static const TAvaPropertyChangeDispatcher<AAvaClonerActor> PropertyChangeDispatcher;
#endif
};
