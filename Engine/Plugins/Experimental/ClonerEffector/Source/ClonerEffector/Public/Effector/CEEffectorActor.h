// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CEClonerEffectorShared.h"
#include "CEPropertyChangeDispatcher.h"
#include "GameFramework/Actor.h"
#include "PropertyBag.h"
#include "CEEffectorActor.generated.h"

class ACEClonerActor;
class UDynamicMeshComponent;

UCLASS(MinimalAPI, BlueprintType, HideCategories=(Rendering,Replication,Collision,HLOD,Physics,Networking,Input,Actor,Cooking,LevelInstance,DataLayers), DisplayName = "Motion Design Effector Actor")
class ACEEffectorActor : public AActor
{
	GENERATED_BODY()

	friend class ACEClonerActor;
	friend class FCEEditorEffectorDetailCustomization;
	friend class FAvaEffectorActorVisualizer;
	friend class UCEEffectorSubsystem;

public:
	static inline const FString DefaultLabel = TEXT("Effector");

	ACEEffectorActor();

	UFUNCTION(BlueprintCallable, Category="Effector")
	CLONEREFFECTOR_API void SetType(ECEClonerEffectorType InType);

	UFUNCTION(BlueprintPure, Category="Effector")
	ECEClonerEffectorType GetType() const
	{
		return Type;
	}

	UFUNCTION(BlueprintCallable, Category="Effector")
	CLONEREFFECTOR_API void SetInvertType(bool bInInvert);

	UFUNCTION(BlueprintPure, Category="Effector")
	bool GetInvertType() const
	{
		return bInvertType;
	}

	UFUNCTION(BlueprintCallable, Category="Effector")
	CLONEREFFECTOR_API void SetEasing(ECEClonerEasing InEasing);

	UFUNCTION(BlueprintPure, Category="Effector")
	ECEClonerEasing GetEasing() const
	{
		return Easing;
	}

	UFUNCTION(BlueprintCallable, Category="Effector")
	CLONEREFFECTOR_API void SetMagnitude(float InMagnitude);

	UFUNCTION(BlueprintPure, Category="Effector")
	float GetMagnitude() const
	{
		return Magnitude;
	}

	UFUNCTION(BlueprintCallable, Category="Effector")
	CLONEREFFECTOR_API void SetOuterRadius(float InRadius);

	UFUNCTION(BlueprintPure, Category="Effector")
	float GetOuterRadius() const
	{
		return OuterRadius;
	}

	UFUNCTION(BlueprintCallable, Category="Effector")
	CLONEREFFECTOR_API void SetInnerRadius(float InRadius);

	UFUNCTION(BlueprintPure, Category="Effector")
	float GetInnerRadius() const
	{
		return InnerRadius;
	}

	UFUNCTION(BlueprintCallable, Category="Effector")
	CLONEREFFECTOR_API void SetInnerExtent(const FVector& InExtent);

	UFUNCTION(BlueprintPure, Category="Effector")
	FVector GetInnerExtent() const
	{
		return InnerExtent;
	}

	UFUNCTION(BlueprintCallable, Category="Effector")
	CLONEREFFECTOR_API void SetOuterExtent(const FVector& InExtent);

	UFUNCTION(BlueprintPure, Category="Effector")
	FVector GetOuterExtent() const
	{
		return OuterExtent;
	}

	UFUNCTION(BlueprintCallable, Category="Effector")
	CLONEREFFECTOR_API void SetPlaneSpacing(float InSpacing);

	UFUNCTION(BlueprintPure, Category="Effector")
	float GetPlaneSpacing() const
	{
		return PlaneSpacing;
	}

	UFUNCTION(BlueprintCallable, Category="Effector")
	CLONEREFFECTOR_API void SetRadialAngle(float InAngle);

	UFUNCTION(BlueprintPure, Category="Effector")
	float GetRadialAngle() const
	{
		return RadialAngle;
	}

	UFUNCTION(BlueprintCallable, Category="Effector")
	CLONEREFFECTOR_API void SetRadialMinRadius(float InRadius);

	UFUNCTION(BlueprintPure, Category="Effector")
	float GetRadialMinRadius() const
	{
		return RadialMinRadius;
	}

	UFUNCTION(BlueprintCallable, Category="Effector")
	CLONEREFFECTOR_API void SetRadialMaxRadius(float InRadius);

	UFUNCTION(BlueprintPure, Category="Effector")
	float GetRadialMaxRadius() const
	{
		return RadialMaxRadius;
	}

	UFUNCTION(BlueprintCallable, Category="Effector")
	CLONEREFFECTOR_API void SetTorusRadius(float InRadius);

	UFUNCTION(BlueprintPure, Category="Effector")
	float GetTorusRadius() const
	{
		return TorusRadius;
	}

	UFUNCTION(BlueprintCallable, Category="Effector")
	CLONEREFFECTOR_API void SetTorusInnerRadius(float InRadius);

	UFUNCTION(BlueprintPure, Category="Effector")
	float GetTorusInnerRadius() const
	{
		return TorusInnerRadius;
	}

	UFUNCTION(BlueprintCallable, Category="Effector")
	CLONEREFFECTOR_API void SetTorusOuterRadius(float InRadius);

	UFUNCTION(BlueprintPure, Category="Effector")
	float GetTorusOuterRadius() const
	{
		return TorusOuterRadius;
	}

	UFUNCTION(BlueprintCallable, Category="Effector")
	CLONEREFFECTOR_API void SetOffset(const FVector& InOffset);

	UFUNCTION(BlueprintPure, Category="Effector")
	FVector GetOffset() const
	{
		return Offset;
	}

	UFUNCTION(BlueprintCallable, Category="Effector")
	CLONEREFFECTOR_API void SetRotation(const FRotator& InRotation);

	UFUNCTION(BlueprintPure, Category="Effector")
	FRotator GetRotation() const
	{
		return Rotation;
	}

	UFUNCTION(BlueprintCallable, Category="Effector")
	CLONEREFFECTOR_API void SetScale(const FVector& InScale);

	UFUNCTION(BlueprintPure, Category="Effector")
	FVector GetScale() const
	{
		return Scale;
	}

	UFUNCTION(BlueprintCallable, Category="Effector")
	CLONEREFFECTOR_API void SetEnabled(bool bInEnable);

	UFUNCTION(BlueprintPure, Category="Effector")
	bool GetEnabled() const
	{
		return bEnabled;
	}

	UFUNCTION(BlueprintCallable, Category="Effector")
	CLONEREFFECTOR_API void SetMode(ECEClonerEffectorMode InMode);

	UFUNCTION(BlueprintPure, Category="Effector")
	ECEClonerEffectorMode GetMode() const
	{
		return Mode;
	}

	UFUNCTION(BlueprintCallable, Category="Effector")
	CLONEREFFECTOR_API void SetTargetActor(AActor* InTargetActor);

	UFUNCTION(BlueprintPure, Category="Effector")
	AActor* GetTargetActor() const
	{
		return TargetActorWeak.Get();
	}

	void SetTargetActorWeak(const TWeakObjectPtr<AActor>& InTargetActor);

	TWeakObjectPtr<AActor> GetTargetActorWeak() const
	{
		return TargetActorWeak;
	}

	UFUNCTION(BlueprintCallable, Category="Effector")
	CLONEREFFECTOR_API void SetOrientationForceEnabled(bool bInForceEnabled);

	UFUNCTION(BlueprintPure, Category="Effector")
	bool GetOrientationForceEnabled() const
	{
		return bOrientationForceEnabled;
	}

	UFUNCTION(BlueprintCallable, Category="Effector")
	CLONEREFFECTOR_API void SetOrientationForceRate(float InForceOrientationRate);

	UFUNCTION(BlueprintPure, Category="Effector")
	float GetOrientationForceRate() const
	{
		return OrientationForceRate;
	}

	UFUNCTION(BlueprintCallable, Category="Effector")
	CLONEREFFECTOR_API void SetOrientationForceMin(const FVector& InForceOrientationMin);

	UFUNCTION(BlueprintPure, Category="Effector")
	const FVector& GetOrientationForceMin() const
	{
		return OrientationForceMin;
	}

	UFUNCTION(BlueprintCallable, Category="Effector")
	CLONEREFFECTOR_API void SetOrientationForceMax(const FVector& InForceOrientationMax);

	UFUNCTION(BlueprintPure, Category="Effector")
	const FVector& GetOrientationForceMax() const
	{
		return OrientationForceMax;
	}

	UFUNCTION(BlueprintCallable, Category="Effector")
	CLONEREFFECTOR_API void SetVortexForceEnabled(bool bInForceEnabled);

	UFUNCTION(BlueprintPure, Category="Effector")
	bool GetVortexForceEnabled() const
	{
		return bVortexForceEnabled;
	}

	UFUNCTION(BlueprintCallable, Category="Effector")
	CLONEREFFECTOR_API void SetVortexForceAmount(float InForceVortexAmount);

	UFUNCTION(BlueprintPure, Category="Effector")
	float GetVortexForceAmount() const
	{
		return VortexForceAmount;
	}

	UFUNCTION(BlueprintCallable, Category="Effector")
	CLONEREFFECTOR_API void SetVortexForceAxis(const FVector& InForceVortexAxis);

	UFUNCTION(BlueprintPure, Category="Effector")
	const FVector& GetVortexForceAxis() const
	{
		return VortexForceAxis;
	}

	UFUNCTION(BlueprintCallable, Category="Effector")
	CLONEREFFECTOR_API void SetCurlNoiseForceEnabled(bool bInForceEnabled);

	UFUNCTION(BlueprintPure, Category="Effector")
	bool GetCurlNoiseForceEnabled() const
	{
		return bCurlNoiseForceEnabled;
	}

	UFUNCTION(BlueprintCallable, Category="Effector")
	CLONEREFFECTOR_API void SetCurlNoiseForceStrength(float InForceCurlNoiseStrength);

	UFUNCTION(BlueprintPure, Category="Effector")
	float GetCurlNoiseForceStrength() const
	{
		return CurlNoiseForceStrength;
	}

	UFUNCTION(BlueprintCallable, Category="Effector")
	CLONEREFFECTOR_API void SetCurlNoiseForceFrequency(float InForceCurlNoiseFrequency);

	UFUNCTION(BlueprintPure, Category="Effector")
	float GetCurlNoiseForceFrequency() const
	{
		return CurlNoiseForceFrequency;
	}

	UFUNCTION(BlueprintCallable, Category="Effector")
	CLONEREFFECTOR_API void SetAttractionForceEnabled(bool bInForceEnabled);

	UFUNCTION(BlueprintPure, Category="Effector")
	bool GetAttractionForceEnabled() const
	{
		return bAttractionForceEnabled;
	}

	UFUNCTION(BlueprintCallable, Category="Effector")
	CLONEREFFECTOR_API void SetAttractionForceStrength(float InForceStrength);

	UFUNCTION(BlueprintPure, Category="Effector")
	float GetAttractionForceStrength() const
	{
		return AttractionForceStrength;
	}

	UFUNCTION(BlueprintCallable, Category="Effector")
	CLONEREFFECTOR_API void SetAttractionForceFalloff(float InForceFalloff);

	UFUNCTION(BlueprintPure, Category="Effector")
	float GetAttractionForceFalloff() const
	{
		return AttractionForceFalloff;
	}

	UFUNCTION(BlueprintCallable, Category="Effector")
	CLONEREFFECTOR_API void SetGravityForceEnabled(bool bInForceEnabled);

	UFUNCTION(BlueprintPure, Category="Effector")
	bool GetGravityForceEnabled() const
	{
		return bGravityForceEnabled;
	}

	UFUNCTION(BlueprintCallable, Category="Effector")
	CLONEREFFECTOR_API void SetGravityForceAcceleration(const FVector& InAcceleration);

	UFUNCTION(BlueprintPure, Category="Effector")
	FVector GetGravityForceAcceleration() const
	{
		return GravityForceAcceleration;
	}

	UFUNCTION(BlueprintCallable, Category="Effector")
	CLONEREFFECTOR_API void SetLocationStrength(const FVector& InStrength);

	UFUNCTION(BlueprintPure, Category="Effector")
	FVector GetLocationStrength() const
	{
		return LocationStrength;
	}

	UFUNCTION(BlueprintCallable, Category="Effector")
	CLONEREFFECTOR_API void SetRotationStrength(const FRotator& InStrength);

	UFUNCTION(BlueprintPure, Category="Effector")
	FRotator GetRotationStrength() const
	{
		return RotationStrength;
	}

	UFUNCTION(BlueprintCallable, Category="Effector")
	CLONEREFFECTOR_API void SetScaleStrength(const FVector& InStrength);

	UFUNCTION(BlueprintPure, Category="Effector")
	FVector GetScaleStrength() const
	{
		return ScaleStrength;
	}

	UFUNCTION(BlueprintCallable, Category="Effector")
	CLONEREFFECTOR_API void SetPan(const FVector& InPan);

	UFUNCTION(BlueprintPure, Category="Effector")
	FVector GetPan() const
	{
		return Pan;
	}

	UFUNCTION(BlueprintCallable, Category="Effector")
	CLONEREFFECTOR_API void SetFrequency(float InFrequency);

	UFUNCTION(BlueprintPure, Category="Effector")
	float GetFrequency() const
	{
		return Frequency;
	}

	UFUNCTION(BlueprintCallable, Category="Effector")
	CLONEREFFECTOR_API void SetColor(const FLinearColor& InColor);

	UFUNCTION(BlueprintPure, Category="Effector")
	const FLinearColor& GetColor() const
	{
		return Color;
	}

#if WITH_EDITOR
	UFUNCTION(BlueprintCallable, Category="Effector")
	CLONEREFFECTOR_API void SetVisualizerComponentVisible(bool bInVisible);

	UFUNCTION(BlueprintPure, Category="Effector")
	bool GetVisualizerComponentVisible() const
	{
		return bVisualizerComponentVisible;
	}

	UFUNCTION(BlueprintCallable, Category="Effector")
	CLONEREFFECTOR_API void SetVisualizerSpriteVisible(bool bInVisible);

	UFUNCTION(BlueprintPure, Category="Effector")
	bool GetVisualizerSpriteVisible() const
	{
		return bVisualizerSpriteVisible;
	}
#endif

protected:
	/** Used to trigger a refresh on linked cloner */
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnEffectorIdentifierChanged, ACEEffectorActor* /** InEffectorActor */)
	static FOnEffectorIdentifierChanged OnEffectorRefreshClonerDelegate;

	static constexpr int32 InnerVisualizerId = 0;
	static constexpr int32 OuterVisualizerId = 1;
	static constexpr TCHAR VisualizerColorName[] = TEXT("VisualizerColor");

	//~ Begin AActor
	virtual void Destroyed() override;
	virtual void PostActorCreated() override;
#if WITH_EDITOR
	virtual FString GetDefaultActorLabel() const override;
#endif
	//~ End AActor

	//~ Begin UObject
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& InPropertyChangedEvent) override;
	virtual void PostEditImport() override;
	virtual void PostEditUndo() override;
#endif
	virtual void PostLoad() override;
	virtual void PostDuplicate(EDuplicateMode::Type InDuplicateMode) override;
	//~ End UObject

	void RegisterToChannel();
	int32 GetChannelIdentifier() const;
	void OnEffectorSubsystemInitialized(const UWorld* InWorld);

	FCEClonerEffectorChannelData& GetChannelData();

	void OnEffectorTransformed(USceneComponent* InUpdatedComponent, EUpdateTransformFlags InUpdateTransformFlags, ETeleportType InTeleport);

	/** Will update all values of this effector on all clones */
	void OnEffectorChanged();

	/** Called when effector is enabled/disabled */
	void OnEnabledChanged();

	/** Called when we disable this effector */
	void OnEffectorDisabled();

	/** Update mode for this effector */
	void OnModeChanged();

	/** Update shape type for this effector */
	void OnTypeChanged();

	/** Update values for box type effector */
	void OnBoxChanged();

	/** Update values for sphere type effector */
	void OnSphereChanged();

	/** Update values for plane type effector */
	void OnPlaneChanged();

	/** Update values for radial type effector */
	void OnRadialChanged();

	/** Update values for torus type effector */
	void OnTorusChanged();

	/** Update values for unbound type effector */
	void OnUnboundChanged();

	/** Called when transform of effector has changed */
	void OnTransformChanged();

	/** Update values for easing */
	void OnEasingChanged();

	/** Update values for magnitude */
	void OnMagnitudeChanged();

	/** Called when default mode options are changed */
	void OnTransformOptionsChanged();

	/** Update actor targeted by this effector on cloners */
	void OnTargetActorChanged();

	/** Called when target mode options are changed */
	void OnTargetOptionsChanged();

	/** Called when noise field mode options are changed */
	void OnNoiseFieldOptionsChanged();

	/** Called when force options are changed */
	void OnForceOptionsChanged();

	/** Called when force enabled state changed */
	void OnForceEnabledChanged();

	/** Update particle color affected by this effector */
    void OnColorChanged();

#if WITH_EDITOR
	/** Update visualizers options of this effector */
	void OnVisualizerOptionsChanged();

	/** Called when developer settings are changed */
	void OnEffectorDeveloperSettingsChanged(UObject* InSettings, FPropertyChangedEvent& InEvent);

	/** Update the mesh of a component visualizer */
	void UpdateVisualizer(int32 InVisualizerId, TFunction<void(UDynamicMesh*)> InMeshFunction) const;
#endif

	/** Update all types options */
	void UpdateEffectorTypes();

	/** Is this effector enabled/disabled on linked cloners */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Setter="SetEnabled", Getter="GetEnabled", Category="Effector")
	bool bEnabled = true;

	/** The ratio effect of the effector on clones */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Setter, Getter, Category="Effector", meta=(ClampMin="0", ClampMax="1"))
	float Magnitude = 1.f;

	/** Affected clones color passed over to material */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Setter, Getter, Category="Effector")
	FLinearColor Color = FLinearColor::Red;

	/** Type of effector to apply on cloners instances */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Setter, Getter, Category="Type")
	ECEClonerEffectorType Type = ECEClonerEffectorType::Sphere;

	/** Invert the type effect, instead of affecting the inside of a zone, will affect the outside */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Setter="SetInvertType", Getter="GetInvertType", Category="Type")
	bool bInvertType = false;

	/** Weight easing function applied to lerp transforms */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Setter, Getter, Category="Type", meta=(EditCondition="Type != ECEClonerEffectorType::Unbound", EditConditionHides))
	ECEClonerEasing Easing = ECEClonerEasing::Linear;

	/** Inner radius of sphere, all clones inside will be affected with a maximum weight */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Setter, Getter, Category="Type", meta=(ClampMin="0", EditCondition="Type == ECEClonerEffectorType::Sphere", EditConditionHides))
	float InnerRadius = 50.f;

	/** Outer radius of sphere, all clones outside will not be affected */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Setter, Getter, Category="Type", meta=(ClampMin="0", EditCondition="Type == ECEClonerEffectorType::Sphere", EditConditionHides))
	float OuterRadius = 200.f;

	/** Inner extent of box, all clones inside will be affected with a maximum weight */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Setter, Getter, Category="Type", meta=(ClampMin="0", AllowPreserveRatio, EditCondition="Type == ECEClonerEffectorType::Box", EditConditionHides))
	FVector InnerExtent = FVector(50.f);

	/** Outer extent of box, all clones outside will not be affected */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Setter, Getter, Category="Type", meta=(ClampMin="0", AllowPreserveRatio, EditCondition="Type == ECEClonerEffectorType::Box", EditConditionHides))
	FVector OuterExtent = FVector(200.f);

	/** Plane spacing, everything inside this zone will be affected */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Setter, Getter, Category="Type", meta=(ClampMin="0", EditCondition="Type == ECEClonerEffectorType::Plane", EditConditionHides))
	float PlaneSpacing = 200.f;

	/** Radial angle in degree, everything within the angle will be affected */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Setter, Getter, Category="Type", meta=(ClampMin="0", ClampMax="360", EditCondition="Type == ECEClonerEffectorType::Radial", EditConditionHides))
	float RadialAngle = 180.f;

	/** Minimum radius for the radial effect to be applied on clones, below clones will not be affected */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Setter, Getter, Category="Type", meta=(ClampMin="0", EditCondition="Type == ECEClonerEffectorType::Radial", EditConditionHides))
	float RadialMinRadius = 0.f;

	/** Maximum radius for the radial effect to be applied on clones, above clones will not be affected */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Setter, Getter, Category="Type", meta=(ClampMin="0", EditCondition="Type == ECEClonerEffectorType::Radial", EditConditionHides))
	float RadialMaxRadius = 1000.f;

	/** Main torus radius from center to the edge where inner and outer tube will be revolved */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Setter, Getter, Category="Type", meta=(ClampMin="0", EditCondition="Type == ECEClonerEffectorType::Torus", EditConditionHides))
	float TorusRadius = 250.f;

	/** Minimum revolved radius for the torus effect, clones contained inside will be affected with a maximum weight */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Setter, Getter, Category="Type", meta=(ClampMin="0", EditCondition="Type == ECEClonerEffectorType::Torus", EditConditionHides))
	float TorusInnerRadius = 50.f;

	/** Maximum revolved radius for the torus effect, clones outside of it will not be affected */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Setter, Getter, Category="Type", meta=(ClampMin="0", EditCondition="Type == ECEClonerEffectorType::Torus", EditConditionHides))
	float TorusOuterRadius = 200.f;

	/** Mode of effector for each clones instances */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Setter, Getter, Category="Mode")
	ECEClonerEffectorMode Mode = ECEClonerEffectorMode::Default;

	/** Offset applied on affected clones */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Setter, Getter, Category="Mode", meta=(EditCondition="Mode == ECEClonerEffectorMode::Default", EditConditionHides))
	FVector Offset = FVector::ZeroVector;

	/** Rotation applied on affected clones */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Setter, Getter, Category="Mode", meta=(ClampMin="-180", ClampMax="180", UIMin="-180", UIMax="180", EditCondition="Mode == ECEClonerEffectorMode::Default", EditConditionHides))
	FRotator Rotation = FRotator::ZeroRotator;

	/** Scale applied on affected clones */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Setter, Getter, Category="Mode", meta=(ClampMin="0", AllowPreserveRatio, Delta="0.01", EditCondition="Mode == ECEClonerEffectorMode::Default", EditConditionHides))
	FVector Scale = FVector::OneVector;

	/** The actor to track when mode is set to target */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Setter, Getter, Category="Mode", meta=(DisplayName="TargetActor", EditCondition="Mode == ECEClonerEffectorMode::Target", EditConditionHides))
	TWeakObjectPtr<AActor> TargetActorWeak = nullptr;

	UPROPERTY()
	TWeakObjectPtr<AActor> InternalTargetActorWeak = nullptr;

	/** Amplitude of the noise field for location */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Setter, Getter, Category="Mode", meta=(EditCondition="Mode == ECEClonerEffectorMode::NoiseField", EditConditionHides))
	FVector LocationStrength = FVector::ZeroVector;

	/** Amplitude of the noise field for rotation */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Setter, Getter, Category="Mode", meta=(EditCondition="Mode == ECEClonerEffectorMode::NoiseField", EditConditionHides))
	FRotator RotationStrength = FRotator::ZeroRotator;

	/** Amplitude of the noise field for scale */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Setter, Getter, Category="Mode", meta=(ClampMin="0", AllowPreserveRatio, Delta="0.01", EditCondition="Mode == ECEClonerEffectorMode::NoiseField", EditConditionHides))
	FVector ScaleStrength = FVector::OneVector;

	/** Panning to offset the noise field sampling */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Setter, Getter, Category="Mode", meta=(EditCondition="Mode == ECEClonerEffectorMode::NoiseField", EditConditionHides))
	FVector Pan = FVector::ZeroVector;

	/** Intensity of the noise field */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Setter, Getter, Category="Mode", meta=(ClampMin="0", EditCondition="Mode == ECEClonerEffectorMode::NoiseField", EditConditionHides))
	float Frequency = 0.5f;

	/** Enable orientation force to allow each clone instance to rotate around its pivot */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Setter="SetOrientationForceEnabled", Getter="GetOrientationForceEnabled", Category="Force")
	bool bOrientationForceEnabled = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Setter, Getter, Category="Force", meta=(Delta="0.0001", ClampMin="0", EditCondition="bOrientationForceEnabled", EditConditionHides))
	float OrientationForceRate = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Setter, Getter, Category="Force", meta=(EditCondition="bOrientationForceEnabled", EditConditionHides))
	FVector OrientationForceMin = FVector(-0.1f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Setter, Getter, Category="Force", meta=(EditCondition="bOrientationForceEnabled", EditConditionHides))
	FVector OrientationForceMax = FVector(0.1f);

	/** Enable vortex force to allow each clone instance to rotate around a specific axis on the cloner pivot */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Setter="SetVortexForceEnabled", Getter="GetVortexForceEnabled", Category="Force")
	bool bVortexForceEnabled = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Setter, Getter, Category="Force", meta=(EditCondition="bVortexForceEnabled", EditConditionHides))
	float VortexForceAmount = 10000.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Setter, Getter, Category="Force", meta=(EditCondition="bVortexForceEnabled", EditConditionHides))
	FVector VortexForceAxis = FVector::ZAxisVector;

	/** Enable curl noise force to allow each clone instance to add random location variation */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Setter="SetCurlNoiseForceEnabled", Getter="GetCurlNoiseForceEnabled", Category="Force")
	bool bCurlNoiseForceEnabled = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Setter, Getter, Category="Force", meta=(EditCondition="bCurlNoiseForceEnabled", EditConditionHides))
	float CurlNoiseForceStrength = 1000.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Setter, Getter, Category="Force", meta=(EditCondition="bCurlNoiseForceEnabled", EditConditionHides))
	float CurlNoiseForceFrequency = 10.f;

	/** Enable attraction force to allow each clone instances to gravitate toward a location */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Setter="SetAttractionForceEnabled", Getter="GetAttractionForceEnabled", Category="Force")
	bool bAttractionForceEnabled = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Setter, Getter, Category="Force", meta=(EditCondition="bAttractionForceEnabled", EditConditionHides))
	float AttractionForceStrength = 1000.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Setter, Getter, Category="Force", meta=(EditCondition="bAttractionForceEnabled", EditConditionHides))
	float AttractionForceFalloff = 0.1f;

	/** Enable gravity force to pull particles based on an acceleration vector */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Setter="SetGravityForceEnabled", Getter="GetGravityForceEnabled", Category="Force")
	bool bGravityForceEnabled = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Setter, Getter, Category="Force", meta=(EditCondition="bGravityForceEnabled", EditConditionHides))
	FVector GravityForceAcceleration = FVector(0, 0, -980.f);

#if WITH_EDITORONLY_DATA
	/** Visibility of the components visualizer */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, AdvancedDisplay, Category="Effector")
	bool bVisualizerComponentVisible = true;

	/** Toggle the sprite to visualize and click on this effector */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, AdvancedDisplay, Category="Effector")
	bool bVisualizerSpriteVisible = true;
#endif

private:
	void OnTargetActorTransformChanged(USceneComponent* InUpdatedComponent, EUpdateTransformFlags InFlags, ETeleportType InTeleport);

	UFUNCTION()
	void OnTargetActorDestroyed(AActor* InActor);

	/** Visualizer component for inner effector type */
	UPROPERTY(Transient)
	TObjectPtr<UDynamicMeshComponent> InnerVisualizerComponent;

	/** Visualizer component for outer effector type */
	UPROPERTY(Transient)
	TObjectPtr<UDynamicMeshComponent> OuterVisualizerComponent;

#if WITH_EDITORONLY_DATA
	/** Dynamic material for the inner visualizer */
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> InnerVisualizerMaterial;

	/** Dynamic material for the outer visualizer */
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> OuterVisualizerMaterial;

	/** Used to hold data related to visualizer for updates */
	UPROPERTY(Transient, NonTransactional)
	FInstancedPropertyBag VisualizerData;
#endif

	/** Transient effector channel data */
	UPROPERTY(VisibleInstanceOnly, Transient, DuplicateTransient, TextExportTransient, NonTransactional, AdvancedDisplay, Category="Effector")
	FCEClonerEffectorChannelData ChannelData;

#if WITH_EDITOR
	/** Used for PECP */
	static TCEPropertyChangeDispatcher<ACEEffectorActor> PropertyChangeDispatcher;
#endif
};
