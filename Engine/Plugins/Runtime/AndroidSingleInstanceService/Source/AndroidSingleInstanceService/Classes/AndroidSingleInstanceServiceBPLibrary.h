// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "AndroidSingleInstanceServiceBPLibrary.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FButtonPressedMC, FString, Name);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FiveParams(FAndroidEventReceivedMC, int32, EventId, FString, Param1, int32, Param2, int32, Param3, float, Param4);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FAndroidDataReceivedStringMC, int32, DataId, FString, Key, FString, Data);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FAndroidDataReceivedStringArrayMC, int32, DataId, FString, Key, const TArray<FString>&, Data);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FAndroidDataReceivedBooleanMC, int32, DataId, FString, Key, bool, Data);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FAndroidDataReceivedBooleanArrayMC, int32, DataId, FString, Key, const TArray<bool>&, Data);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FAndroidDataReceivedByteMC, int32, DataId, FString, Key, int8, Data);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FAndroidDataReceivedByteArrayMC, int32, DataId, FString, Key, const TArray<int8>&, Data);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FAndroidDataReceivedIntMC, int32, DataId, FString, Key, int32, Data);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FAndroidDataReceivedIntArrayMC, int32, DataId, FString, Key, const TArray<int32>&, Data);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FAndroidDataReceivedShortMC, int32, DataId, FString, Key, int16, Data);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FAndroidDataReceivedShortArrayMC, int32, DataId, FString, Key, const TArray<int16>&, Data);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FAndroidDataReceivedLongMC, int32, DataId, FString, Key, int64, Data);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FAndroidDataReceivedLongArrayMC, int32, DataId, FString, Key, const TArray<int64>&, Data);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FAndroidDataReceivedFloatMC, int32, DataId, FString, Key, float, Data);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FAndroidDataReceivedFloatArrayMC, int32, DataId, FString, Key, const TArray<float>&, Data);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FAndroidDataReceivedDoubleMC, int32, DataId, FString, Key, double, Data);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FAndroidDataReceivedDoubleArrayMC, int32, DataId, FString, Key, const TArray<double>&, Data);

UENUM()
namespace EASISActiveType
{
	enum Type : int
	{
		None = 0 UMETA(DisplayName = "None"),
		LocalOnly = 1 UMETA(DisplayName = "Local only"),
	};
}

UCLASS()
class UAndroidSingleInstanceServiceBPLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_UCLASS_BODY()
public:
	/** Request startup of Android FileServer */
	UFUNCTION(BlueprintCallable, Category = "AndroidSingleInstanceService")
	static bool StartSingleInstanceService();

	/** Request termination of Android FileServer */
	UFUNCTION(BlueprintCallable, Category = "AndroidSingleInstanceService")
	static bool StopSingleInstanceService();

	/** Check if Android FileServer is running */
	UFUNCTION(BlueprintCallable, Category = "AndroidSingleInstanceService")
	static TEnumAsByte<EASISActiveType::Type> IsFileServerRunning();

	/** Get application name from package */
	UFUNCTION(BlueprintCallable, Category = "AndroidUtils")
	static FString GetApplicationNameFromPackage(const FString& PackageName);

	/** Sends a string to Java */
	UFUNCTION(BlueprintCallable, Category = "AndroidUtils")
	static bool SendString(const FString& Value);

	/** Returns if startup movie is playing */
	UFUNCTION(BlueprintPure, Category = "AndroidUtils")
	static bool IsStartupMoviePlaying();

	/** Stops startup movie if active either paused or playing over duration of fade out */
	UFUNCTION(BlueprintCallable, Category = "AndroidUtils")
	static void StopStartupMovie(bool Pause, int Duration);

	/** Set alpha on a view by id over some duration in Java */
	UFUNCTION(BlueprintCallable, Category = "AndroidUtils")
	static bool AnimateAlpha(const FString& Key, float TargetAlpha, int Duration);

	/** Set visibiity of a view by id in Java */
	UFUNCTION(BlueprintCallable, Category = "AndroidUtils")
	static bool SetVisibility(const FString& Key, bool Visible);

	/** Enables an ImageButton or Button TextView in Java */
	UFUNCTION(BlueprintCallable, Category = "AndroidUtils")
	static bool EnableButton(const FString& Key, bool Enabled);

	/** Sets the text on a TextView in Java */
	UFUNCTION(BlueprintCallable, Category = "AndroidUtils")
	static bool SendText(const FString& Key, const FString& Value);

	/** Get the UV for corners of the surfaceview */
	UFUNCTION(BlueprintCallable, Category="AndroidUtils")
	static FBox2D GetSurfaceViewUVs();

	/** Sends event with parameters to engine callback */
	UFUNCTION(BlueprintCallable, Category = "AndroidUtils")
	static void SendAndroidEvent(int32 EventId, const FString& Param1, int32 Param2, int32 Param3, float Param4);

	DECLARE_DYNAMIC_DELEGATE_OneParam(FOnButtonPressed, FString, Name);

	/** Register a delegate to call to receive button presses from Java */
	UFUNCTION(BlueprintCallable, Category = "AndroidUtils")
	static void RegisterButtonPressDelegate(FOnButtonPressed Delegate);

	DECLARE_DYNAMIC_DELEGATE_FiveParams(FOnAndroidEventReceived, int32, EventId, FString, Param1, int32, Param2, int32, Param3, float, Param4);

	/** Register a delegate to call to receive events from Java */
	UFUNCTION(BlueprintCallable, Category = "AndroidUtils")
	static void RegisterAndroidEventReceivedDelegate(FOnAndroidEventReceived Delegate);

	DECLARE_DYNAMIC_DELEGATE_ThreeParams(FOnAndroidDataReceivedString, int32, DataId, FString, Key, FString, Data);
	DECLARE_DYNAMIC_DELEGATE_ThreeParams(FOnAndroidDataReceivedStringArray, int32, DataId, FString, Key, const TArray<FString>&, Data);

	/** Register a delegate to call to receive string data from Java */
	UFUNCTION(BlueprintCallable, Category = "AndroidUtils")
	static void RegisterAndroidDataReceivedStringDelegate(FOnAndroidDataReceivedString Delegate);

	/** Register a delegate to call to receive string array data from Java */
	UFUNCTION(BlueprintCallable, Category = "AndroidUtils")
	static void RegisterAndroidDataReceivedStringArrayDelegate(FOnAndroidDataReceivedStringArray Delegate);

	DECLARE_DYNAMIC_DELEGATE_ThreeParams(FOnAndroidDataReceivedBoolean, int32, DataId, FString, Key, bool, Data);
	DECLARE_DYNAMIC_DELEGATE_ThreeParams(FOnAndroidDataReceivedBooleanArray, int32, DataId, FString, Key, const TArray<bool>&, Data);

	/** Register a delegate to call to receive boolean data from Java */
	UFUNCTION(BlueprintCallable, Category = "AndroidUtils")
	static void RegisterAndroidDataReceivedBooleanDelegate(FOnAndroidDataReceivedBoolean Delegate);

	/** Register a delegate to call to receive boolean array data from Java */
	UFUNCTION(BlueprintCallable, Category = "AndroidUtils")
	static void RegisterAndroidDataReceivedBooleanArrayDelegate(FOnAndroidDataReceivedBooleanArray Delegate);

	DECLARE_DYNAMIC_DELEGATE_ThreeParams(FOnAndroidDataReceivedByte, int32, DataId, FString, Key, int8, Data);
	DECLARE_DYNAMIC_DELEGATE_ThreeParams(FOnAndroidDataReceivedByteArray, int32, DataId, FString, Key, const TArray<int8>&, Data);

	/** Register a delegate to call to receive byte data from Java */
	UFUNCTION(BlueprintCallable, Category = "AndroidUtils")
	static void RegisterAndroidDataReceivedByteDelegate(FOnAndroidDataReceivedByte Delegate);

	/** Register a delegate to call to receive byte array data from Java */
	UFUNCTION(BlueprintCallable, Category = "AndroidUtils")
	static void RegisterAndroidDataReceivedByteArrayDelegate(FOnAndroidDataReceivedByteArray Delegate);

	DECLARE_DYNAMIC_DELEGATE_ThreeParams(FOnAndroidDataReceivedInt, int32, DataId, FString, Key, int32, Data);
	DECLARE_DYNAMIC_DELEGATE_ThreeParams(FOnAndroidDataReceivedIntArray, int32, DataId, FString, Key, const TArray<int32>&, Data);

	/** Register a delegate to call to receive int data from Java */
	UFUNCTION(BlueprintCallable, Category = "AndroidUtils")
	static void RegisterAndroidDataReceivedIntDelegate(FOnAndroidDataReceivedInt Delegate);

	/** Register a delegate to call to receive int array data from Java */
	UFUNCTION(BlueprintCallable, Category = "AndroidUtils")
	static void RegisterAndroidDataReceivedIntArrayDelegate(FOnAndroidDataReceivedIntArray Delegate);

	DECLARE_DYNAMIC_DELEGATE_ThreeParams(FOnAndroidDataReceivedShort, int32, DataId, FString, Key, int16, Data);
	DECLARE_DYNAMIC_DELEGATE_ThreeParams(FOnAndroidDataReceivedShortArray, int32, DataId, FString, Key, const TArray<int16>&, Data);

	/** Register a delegate to call to receive short data from Java */
	UFUNCTION(BlueprintCallable, Category = "AndroidUtils")
	static void RegisterAndroidDataReceivedShortDelegate(FOnAndroidDataReceivedShort Delegate);

	/** Register a delegate to call to receive short array data from Java */
	UFUNCTION(BlueprintCallable, Category = "AndroidUtils")
	static void RegisterAndroidDataReceivedShortArrayDelegate(FOnAndroidDataReceivedShortArray Delegate);

	DECLARE_DYNAMIC_DELEGATE_ThreeParams(FOnAndroidDataReceivedLong, int32, DataId, FString, Key, int64, Data);
	DECLARE_DYNAMIC_DELEGATE_ThreeParams(FOnAndroidDataReceivedLongArray, int32, DataId, FString, Key, const TArray<int64>&, Data);

	/** Register a delegate to call to receive long data from Java */
	UFUNCTION(BlueprintCallable, Category = "AndroidUtils")
	static void RegisterAndroidDataReceivedLongDelegate(FOnAndroidDataReceivedLong Delegate);

	/** Register a delegate to call to receive long array data from Java */
	UFUNCTION(BlueprintCallable, Category = "AndroidUtils")
	static void RegisterAndroidDataReceivedLongArrayDelegate(FOnAndroidDataReceivedLongArray Delegate);

	DECLARE_DYNAMIC_DELEGATE_ThreeParams(FOnAndroidDataReceivedFloat, int32, DataId, FString, Key, float, Data);
	DECLARE_DYNAMIC_DELEGATE_ThreeParams(FOnAndroidDataReceivedFloatArray, int32, DataId, FString, Key, const TArray<float>&, Data);

	/** Register a delegate to call to receive float data from Java */
	UFUNCTION(BlueprintCallable, Category = "AndroidUtils")
	static void RegisterAndroidDataReceivedFloatDelegate(FOnAndroidDataReceivedFloat Delegate);

	/** Register a delegate to call to receive float array data from Java */
	UFUNCTION(BlueprintCallable, Category = "AndroidUtils")
	static void RegisterAndroidDataReceivedFloatArrayDelegate(FOnAndroidDataReceivedFloatArray Delegate);

	DECLARE_DYNAMIC_DELEGATE_ThreeParams(FOnAndroidDataReceivedDouble, int32, DataId, FString, Key, double, Data);
	DECLARE_DYNAMIC_DELEGATE_ThreeParams(FOnAndroidDataReceivedDoubleArray, int32, DataId, FString, Key, const TArray<double>&, Data);

	/** Register a delegate to call to receive double data from Java */
	UFUNCTION(BlueprintCallable, Category = "AndroidUtils")
	static void RegisterAndroidDataReceivedDoubleDelegate(FOnAndroidDataReceivedDouble Delegate);

	/** Register a delegate to call to receive double array data from Java */
	UFUNCTION(BlueprintCallable, Category = "AndroidUtils")
	static void RegisterAndroidDataReceivedDoubleArrayDelegate(FOnAndroidDataReceivedDoubleArray Delegate);
};

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "CoreMinimal.h"
#endif
