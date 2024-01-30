// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "RemoteControlPreset.h"
#include "AvalancheRemoteControl.generated.h"

AVALANCHEMEDIA_API DECLARE_LOG_CATEGORY_EXTERN(LogAvaMediaRemoteControl, Log, All);

/**
 * Return code for Remote Control operations.
 */
UENUM()
enum class EAvaRemoteControlResult : int8
{
	/** Operation completed successfully */
	Completed = 0,
	/** Operation was not necessary. */
	UpToDate = 1,	
	/** Input parameter is invalid */ 
	InvalidParameter = -1,
	/** Object reference read access failed. */
	ReadAccessDenied = -2,
	/** Object reference write access failed. */
	WriteAccessDenied = -3,
	/** Serialization from property failed. */
	ReadPropertyFailed = -4,
	/** Serialization to property failed. */
	WritePropertyFailed = -5
};

namespace UE::AvalancheRemoteControl
{
	/** Gets the value (as bytes) of the given property (remote control entity). Returns true if the operation was successful, false otherwise. */
	AVALANCHEMEDIA_API EAvaRemoteControlResult GetValueOfEntity(const TSharedPtr<const FRemoteControlEntity>& InRemoteControlEntity, TArray<uint8>& OutValue);
	AVALANCHEMEDIA_API EAvaRemoteControlResult GetValueOfEntity(const TSharedPtr<const FRemoteControlEntity>& InRemoteControlEntity, FString& OutValue);
	
	/** Set the value (as bytes) of the given property (remote control entity). Returns true if the operation was successful, false otherwise. */
	AVALANCHEMEDIA_API EAvaRemoteControlResult SetValueOfEntity(const TSharedPtr<FRemoteControlEntity>& InRemoteControlEntity, const TArrayView<const uint8>& InValue);
	AVALANCHEMEDIA_API EAvaRemoteControlResult SetValueOfEntity(const TSharedPtr<FRemoteControlEntity>& InRemoteControlEntity, const FString& InValue);

	/** Gets the value (as bytes) of the given controller. Returns true if the operation was successful, false otherwise. */
	AVALANCHEMEDIA_API EAvaRemoteControlResult GetValueOfController(URCVirtualPropertyBase* InController, TArray<uint8>& OutValue);
	AVALANCHEMEDIA_API EAvaRemoteControlResult GetValueOfController(URCVirtualPropertyBase* InController, FString& OutValue);
	
	/** Set the value (as bytes) of the given controller. Returns true if the operation was successful, false otherwise. */
	AVALANCHEMEDIA_API EAvaRemoteControlResult SetValueOfController(URCVirtualPropertyBase* InController, const TArrayView<const uint8>& InValue);
	AVALANCHEMEDIA_API EAvaRemoteControlResult SetValueOfController(URCVirtualPropertyBase* InController, const FString& InValue);

	/** Retrieve the entity ids that are controlled by the given controller. */
	AVALANCHEMEDIA_API bool GetEntitiesControlledByController(const URemoteControlPreset* InRemoteControlPreset, const URCVirtualPropertyBase* InVirtualProperty, TSet<FGuid>& OutEntityIds);

	inline bool Failed(EAvaRemoteControlResult InResult)
	{
		// Negative result codes are failure.
		return InResult < EAvaRemoteControlResult::Completed;
	}

	AVALANCHEMEDIA_API FString EnumToString(EAvaRemoteControlResult InValue);
	
	/**
	 * Utility class to temporarily push the enabled state of the controller's behaviours.
	 * This class is intended to be used temporarily on local stack. Lifetime of virtual property is not managed.
	 */
	class AVALANCHEMEDIA_API FScopedPushControllerBehavioursEnable
	{
	public:
		FScopedPushControllerBehavioursEnable(URCVirtualPropertyBase* InVirtualProperty, bool bInBehavioursEnabled);
		~FScopedPushControllerBehavioursEnable();
		
	private:
		URCVirtualPropertyBase* VirtualProperty = nullptr;
		TArray<bool> PreviousBehavioursEnabled;
	};
}