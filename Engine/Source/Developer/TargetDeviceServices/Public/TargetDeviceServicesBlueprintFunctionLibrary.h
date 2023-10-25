// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "CoreMinimal.h"
#include "Containers/Map.h"
#include "TargetDeviceServicesBlueprintFunctionLibrary.generated.h"

/**
* The struct is designed to store device information
*/
USTRUCT(BlueprintType)
struct FDeviceSnapshot
{
	GENERATED_BODY()

public:

	/** Stores device's name. */
	UPROPERTY(BlueprintReadOnly, Category = "TargetDeviceServices")
	FString Name;

	/** Stores device's hostname. */
	UPROPERTY(BlueprintReadOnly, Category = "TargetDeviceServices")
	FString HostName;

	/** Stores device's type. */
	UPROPERTY(BlueprintReadOnly, Category = "TargetDeviceServices")
	FString DeviceType;

	/** Stores device's model identifier. */
	UPROPERTY(BlueprintReadOnly, Category = "TargetDeviceServices")
	FString ModelId;

	/** Stores device's connection type. */
	UPROPERTY(BlueprintReadOnly, Category = "TargetDeviceServices")
	FString DeviceConnectionType;

	/** Stores device's identifier. */
	UPROPERTY(BlueprintReadOnly, Category = "TargetDeviceServices")
	FString DeviceId;
	
	/** Stores device's operating system name. */
	UPROPERTY(BlueprintReadOnly, Category = "TargetDeviceServices")
	FString OperatingSystem;
	
	/**
	* Stores device's flag that is used to detect whether device
	* is connected (true) or disconnected (false). 
	*/
	UPROPERTY(BlueprintReadOnly, Category = "TargetDeviceServices")
	bool IsConnected;
};

/**
* The struct is a container class that stores instances of FDeviceSnapshot instances.
* The struct is a helper class to be used in blueprints as a value of TMap container.
*/
USTRUCT(BlueprintType)
struct FDeviceSnapshots
{
	GENERATED_BODY()

public:

	/** Stores array of device snapshots. */
	UPROPERTY(BlueprintReadOnly, Category = "TargetDeviceServices")
	TArray<FDeviceSnapshot> Entries;
};


/**
* The class declares a set of functions to be exposed to blueprints.
*/
UCLASS(meta = (ScriptName = "TargetDeviceServices"))
class UTargetDeviceServicesBlueprintFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_UCLASS_BODY()
	
public:
	/**
	* Fetches snapshots of devices that are available in the network.
	* 
	* @return A dictionary of devices' informational snapshots that are grouped by device type (device type string is used as a key).
	*/
	UFUNCTION(BlueprintCallable, Category = "TargetDeviceServices")
	static TARGETDEVICESERVICES_API TMap<FString, FDeviceSnapshots> GetDeviceSnapshots();
};

