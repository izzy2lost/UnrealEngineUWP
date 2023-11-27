// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Tickable.h"
#include "UObject/Object.h"

#include "DMXControlConsoleElementController.generated.h"

class IDMXControlConsoleFaderGroupElement;
class UDMXControlConsoleFaderBase;
class UDMXControlConsoleFaderGroup;
class UDMXControlConsoleFloatOscillator;


/**
 * A controller for handling inputs for more than one element at once. 
 * An element can be possessed by one controller at a time.
 */
UCLASS(AutoExpandCategories = ("DMX Element Controller", "DMX Element Controller|Oscillator"))
class DMXCONTROLCONSOLE_API UDMXControlConsoleElementController
	: public UObject
	, public FTickableGameObject
{
	GENERATED_BODY()

public:
	/** Possesses the given Element. An Element Controller can possess more than one Element at once. */
	void Possess(const TScriptInterface<IDMXControlConsoleFaderGroupElement>& InElement);

	/** Possesses the given array of Elements. An Element Controller can possess more than one Element at once. */
	void Possess(TArray<TScriptInterface<IDMXControlConsoleFaderGroupElement>> InElements);

	/** Unpossesses the given Element, if valid */
	void UnPossess(const TScriptInterface<IDMXControlConsoleFaderGroupElement>& InElement);

	/** Clears all the Elements in this Controller */
	void ClearElements();

	/** Returns the Fader Group this Controller resides in */
	virtual UDMXControlConsoleFaderGroup& GetOwnerFaderGroupChecked() const;

	/** Returns the index of the Controller in the owner Fader Group */
	virtual int32 GetIndex() const;

	/** Gets the array of Elements in this Controller */
	const TArray<TScriptInterface<IDMXControlConsoleFaderGroupElement>>& GetElements() const { return Elements; }

	/** Gets the array of Faders in this Controller */
	TArray<UDMXControlConsoleFaderBase*> GetFaders() const;

	/** Gets the name of the Controller */
	const FString& GetControllerName() const { return ControllerName; };

	/** Generates a string using the names of all the Elements in the Controller */
	FString GenerateControllerNameByElementsNames() const;

	/** Sets the name of the Controller */
	void SetControllerName(const FString& NewName);

	/** Returns the value of the Controller */
	float GetValue() const { return Value; }

	/** Sets the value of the Controller and all its Elements */
	void SetValue(float NewValue);

	/** Returns the min value of the Controller */
	float GetMinValue() const { return MinValue; }

	/** Sets the min value of the Controller and all its Elements */
	virtual void SetMinValue(float NewMinValue);

	/** Returns the max value of the Controller */
	float GetMaxValue() const { return MaxValue; }

	/** Sets the max value of the Controller and all its Elements */
	virtual void SetMaxValue(float NewMaxValue);

	/** True if the Elements in this Controller can send DMX data */
	bool IsMuted() const { return bIsMuted; }

	/** Sets the mute state of this Controller */
	void SetMute(bool bMute);

	/** Mutes/Unmutes this Controller */
	void ToggleMute();

	/** True if the value of the Elements in this Controller can't be changed */
	bool IsLocked() const { return bIsLocked; }

	/** Sets the lock state of this Controller */
	void SetLock(bool bLock);

	/** Locks/Unlocks this Controller */
	void ToggleLock();

#if WITH_EDITOR
	/** Gets the activity state of the Controller */
	bool IsActive() const;

	/** True if any of the Elements in the Controller matches the Control Console filtering system */
	bool IsMatchingFilter() const;
#endif // WITH_EDITOR

	/** Destroys the Controller */
	virtual void Destroy();

	// Property Name getters
	FORCEINLINE static FName GetFaderNamePropertyName() { return GET_MEMBER_NAME_CHECKED(UDMXControlConsoleElementController, ControllerName); }
	FORCEINLINE static FName GetValuePropertyName() { return GET_MEMBER_NAME_CHECKED(UDMXControlConsoleElementController, Value); }
	FORCEINLINE static FName GetMinValuePropertyName() { return GET_MEMBER_NAME_CHECKED(UDMXControlConsoleElementController, MinValue); }
	FORCEINLINE static FName GetMaxValuePropertyName() { return GET_MEMBER_NAME_CHECKED(UDMXControlConsoleElementController, MaxValue); }
	FORCEINLINE static FName GetElementsPropertyName() { return GET_MEMBER_NAME_CHECKED(UDMXControlConsoleElementController, Elements); }
#if WITH_EDITOR
	FORCEINLINE static FName GetFloatOscillatorClassPropertyName() { return GET_MEMBER_NAME_CHECKED(UDMXControlConsoleElementController, FloatOscillatorClass); }
#endif // WITH_EDITOR
	FORCEINLINE static FName GetFloatOscillatorPropertyName() { return GET_MEMBER_NAME_CHECKED(UDMXControlConsoleElementController, FloatOscillator); }
	FORCEINLINE static FName GetIsMutedPropertyName() { return GET_MEMBER_NAME_CHECKED(UDMXControlConsoleElementController, bIsMuted); }
	FORCEINLINE static FName GetIsLockedPropertyName() { return GET_MEMBER_NAME_CHECKED(UDMXControlConsoleElementController, bIsLocked); }

protected:
	//~ Begin UObject interface
	virtual void PostInitProperties() override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif // WITH_EDITOR
	//~ End UObject interface

	//~ Begin FTickableGameObject interface
	virtual void Tick(float DeltaTime) override;
	virtual bool IsTickable() const override;
	virtual bool IsTickableInEditor() const override { return true; };
	virtual ETickableTickType GetTickableTickType() const override;
	virtual TStatId GetStatId() const override;
	//~ End FTickableGameObject interface

private:
	/** Synchronizes each Element of the Controller to its parameters */
	void SyncElements() const;

	/** The current name of this Controller */
	UPROPERTY(EditAnywhere, Category = "DMX Element Controller")
	FString ControllerName;

	/** The current value of the Controller */
	UPROPERTY(EditAnywhere, meta = (HideEditConditionToggle, EditCondition = "!bIsLocked"), Category = "DMX Element Controller")
	float Value = 0.f;

	/** The minimum Controller Value */
	UPROPERTY(EditAnywhere, meta = (EditCondition = "!bIsLocked"), Category = "DMX Element Controller")
	float MinValue = 0.f;

	/** The maximum Controller Value */
	UPROPERTY(EditAnywhere, meta = (EditCondition = "!bIsLocked"), Category = "DMX Element Controller")
	float MaxValue = 1.f;

	/** The array of Elements in this Controller */
	UPROPERTY()
	TArray<TScriptInterface<IDMXControlConsoleFaderGroupElement>> Elements;

#if WITH_EDITORONLY_DATA
	/** Oscillator that is used for this Controller */
	UPROPERTY(EditAnywhere, meta = (DisplayName = "Oscillator Class", ShowDisplayNames), Category = "DMX Element Controller|Oscillator")
	TSoftClassPtr<UDMXControlConsoleFloatOscillator> FloatOscillatorClass;
#endif // WITH_EDITORONLY_DATA

	/** Float Oscillator applied to this Controller */
	UPROPERTY(VisibleAnywhere, Instanced, Meta = (DisplayName = "Oscillator"), Category = "DMX Element Controller|Oscillator")
	TObjectPtr<UDMXControlConsoleFloatOscillator> FloatOscillator;

	/** If true, the Elements in the Controller don't send DMX */
	UPROPERTY(EditAnywhere, Category = "DMX Element Controller")
	bool bIsMuted = false;

	/** If true, the value of the Elements in the Controller can't be changed */
	UPROPERTY(EditAnywhere, Category = "DMX Element Controller")
	bool bIsLocked = false;
};
