// Copyright Epic Games, Inc. All Rights Reserved.

#include "AndroidSingleInstanceServiceBPLibrary.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AndroidSingleInstanceServiceBPLibrary)

FButtonPressedMC GlobalOnButtonPressed;
FAndroidEventReceivedMC GlobalOnAndroidEventReceived;

void UAndroidSingleInstanceServiceBPLibrary::RegisterButtonPressDelegate(FOnButtonPressed Delegate)
{
	GlobalOnButtonPressed.Add(Delegate);
}

void UAndroidSingleInstanceServiceBPLibrary::RegisterAndroidEventReceivedDelegate(FOnAndroidEventReceived Delegate)
{
	GlobalOnAndroidEventReceived.Add(Delegate);
}

#define REGISTER_DATA_RECEIVED_DELEGATE(_TYPE_)	\
FAndroidDataReceived ## _TYPE_ ## MC GlobalOnAndroidDataReceived ## _TYPE_; \
void UAndroidSingleInstanceServiceBPLibrary::RegisterAndroidDataReceived ## _TYPE_ ## Delegate(FOnAndroidDataReceived ## _TYPE_ Delegate) \
{ \
	GlobalOnAndroidDataReceived ## _TYPE_.Add(Delegate); \
}

REGISTER_DATA_RECEIVED_DELEGATE(String);
REGISTER_DATA_RECEIVED_DELEGATE(StringArray);
REGISTER_DATA_RECEIVED_DELEGATE(Boolean);
REGISTER_DATA_RECEIVED_DELEGATE(BooleanArray);
REGISTER_DATA_RECEIVED_DELEGATE(Byte);
REGISTER_DATA_RECEIVED_DELEGATE(ByteArray);
REGISTER_DATA_RECEIVED_DELEGATE(Int);
REGISTER_DATA_RECEIVED_DELEGATE(IntArray);
REGISTER_DATA_RECEIVED_DELEGATE(Short);
REGISTER_DATA_RECEIVED_DELEGATE(ShortArray);
REGISTER_DATA_RECEIVED_DELEGATE(Long);
REGISTER_DATA_RECEIVED_DELEGATE(LongArray);
REGISTER_DATA_RECEIVED_DELEGATE(Float);
REGISTER_DATA_RECEIVED_DELEGATE(FloatArray);
REGISTER_DATA_RECEIVED_DELEGATE(Double);
REGISTER_DATA_RECEIVED_DELEGATE(DoubleArray);

#if PLATFORM_ANDROID

#include "Engine/Engine.h"
#include "Android/AndroidJNI.h"
#include "Android/AndroidApplication.h"
#include "Android/AndroidWindow.h"
#include "Async/TaskGraphInterfaces.h"

#if USE_ANDROID_JNI
#include <jni.h>
#endif
#endif

bool UAndroidSingleInstanceServiceBPLibrary::StartSingleInstanceService()
{
	bool result = false;
#if PLATFORM_ANDROID
	if (JNIEnv* Env = FAndroidApplication::GetJavaEnv())
	{
		static jmethodID StartFunc = FJavaWrapper::FindMethod(Env, FJavaWrapper::GameActivityClassID, "AndroidThunkJava_AndroidSingleInstanceService_Start", "()Z", false);
		if (StartFunc != nullptr)
		{
			result = FJavaWrapper::CallBooleanMethod(Env, FJavaWrapper::GameActivityThis, StartFunc);
		}
	}
#endif
	return result;
}

bool UAndroidSingleInstanceServiceBPLibrary::StopSingleInstanceService()
{
	bool result = false;
#if PLATFORM_ANDROID
	if (JNIEnv* Env = FAndroidApplication::GetJavaEnv())
	{
		static jmethodID StopFunc = FJavaWrapper::FindMethod(Env, FJavaWrapper::GameActivityClassID, "AndroidThunkJava_AndroidSingleInstanceService_Stop", "()Z", false);
		if (StopFunc != nullptr)
		{
			result = FJavaWrapper::CallBooleanMethod(Env, FJavaWrapper::GameActivityThis, StopFunc);
		}
	}
#endif
	return result;
}

TEnumAsByte<EASISActiveType::Type> UAndroidSingleInstanceServiceBPLibrary::IsFileServerRunning()
{
	TEnumAsByte<EASISActiveType::Type> result = EASISActiveType::None;
#if PLATFORM_ANDROID
	if (JNIEnv* Env = FAndroidApplication::GetJavaEnv())
	{
		static jmethodID IsRunningFunc = FJavaWrapper::FindMethod(Env, FJavaWrapper::GameActivityClassID, "AndroidThunkJava_AndroidSingleInstanceService_IsRunning", "()I", false);
		if (IsRunningFunc != nullptr)
		{
			result = (TEnumAsByte<EASISActiveType::Type>)FJavaWrapper::CallIntMethod(Env, FJavaWrapper::GameActivityThis, IsRunningFunc);
		}
	}
#endif
	return result;
}

#if PLATFORM_ANDROID

#ifndef USE_ANDROID_MIXEDSAMPLE
#define USE_ANDROID_MIXEDSAMPLE 0
#endif

UAndroidSingleInstanceServiceBPLibrary::UAndroidSingleInstanceServiceBPLibrary(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

FString UAndroidSingleInstanceServiceBPLibrary::GetApplicationNameFromPackage(const FString& PackageName)
{
	FString Result = FString("");
#if USE_ANDROID_MIXEDSAMPLE
	if (JNIEnv* Env = FAndroidApplication::GetJavaEnv())
	{
		static jmethodID JavaFunc = FJavaWrapper::FindMethod(Env, FJavaWrapper::GameActivityClassID, "AndroidThunkJava_MixedSample_GetApplicationNameFromPackage", "(Ljava/lang/String;)Ljava/lang/String;", false);
		if (JavaFunc != nullptr)
		{
			auto Argument = FJavaHelper::ToJavaString(Env, PackageName);
			Result = FJavaHelper::FStringFromLocalRef(Env, (jstring)FJavaWrapper::CallObjectMethod(Env, FJavaWrapper::GameActivityThis, JavaFunc, *Argument));
		}
	}
#endif
	return Result;
}

bool UAndroidSingleInstanceServiceBPLibrary::SendString(const FString& Value)
{
	bool bResult = false;
#if USE_ANDROID_MIXEDSAMPLE
	if (JNIEnv* Env = FAndroidApplication::GetJavaEnv())
	{
		static jmethodID JavaFunc = FJavaWrapper::FindMethod(Env, FJavaWrapper::GameActivityClassID, "AndroidThunkJava_MixedSample_ReceiveString", "(Ljava/lang/String;)Z", false);
		if (JavaFunc != nullptr)
		{
			auto Argument = FJavaHelper::ToJavaString(Env, Value);
			bResult = FJavaWrapper::CallBooleanMethod(Env, FJavaWrapper::GameActivityThis, JavaFunc, *Argument);
		}
	}
#endif
	return bResult;
}

bool UAndroidSingleInstanceServiceBPLibrary::SendText(const FString& Key, const FString& Value)
{
	bool bResult = false;
#if USE_ANDROID_MIXEDSAMPLE
	if (JNIEnv* Env = FAndroidApplication::GetJavaEnv())
	{
		static jmethodID JavaFunc = FJavaWrapper::FindMethod(Env, FJavaWrapper::GameActivityClassID, "AndroidThunkJava_MixedSample_SetText", "(Ljava/lang/String;Ljava/lang/String;)Z", false);
		if (JavaFunc != nullptr)
		{
			auto Argument1 = FJavaHelper::ToJavaString(Env, Key);
			auto Argument2 = FJavaHelper::ToJavaString(Env, Value);
			bResult = FJavaWrapper::CallBooleanMethod(Env, FJavaWrapper::GameActivityThis, JavaFunc, *Argument1, *Argument2);
		}
	}
#endif
	return bResult;
}

FBox2D UAndroidSingleInstanceServiceBPLibrary::GetSurfaceViewUVs()
{
	float RealScreenWidth = 1024.0f;
	float RealScreenHeight = 800.0f;

	if (FString* ConfigRulesWidth = FAndroidMisc::GetConfigRulesVariable(TEXT("screenWidth")))
	{
		RealScreenWidth = FCString::Atof(**ConfigRulesWidth);
	}
	if (FString* ConfigRulesHeight = FAndroidMisc::GetConfigRulesVariable(TEXT("screenHeight")))
	{
		RealScreenHeight = FCString::Atof(**ConfigRulesHeight);
	}

	FPlatformRect Rect = FAndroidWindow::GetScreenRect();

	STANDALONE_DEBUG_LOGf(LogAndroid, TEXT("** GetSurfaceViewUVs: Real WxH: %f,%f,  Bounds: %d,%d - %d,%d"), RealScreenWidth, RealScreenHeight, Rect.Left, Rect.Top, Rect.Right, Rect.Bottom);

	FVector2D UVMin = FVector2D(Rect.Left / RealScreenWidth, Rect.Top / RealScreenHeight);
	FVector2D UVMax = FVector2D(Rect.Right / RealScreenWidth, Rect.Bottom / RealScreenHeight);

	return FBox2D(UVMin, UVMax);
}

bool UAndroidSingleInstanceServiceBPLibrary::IsStartupMoviePlaying()
{
	bool bResult = false;
#if USE_ANDROID_MIXEDSAMPLE
	if (JNIEnv* Env = FAndroidApplication::GetJavaEnv())
	{
		static jmethodID JavaFunc = FJavaWrapper::FindMethod(Env, FJavaWrapper::GameActivityClassID, "AndroidThunkJava_MixedSample_IsStartupMoviePlaying", "()Z", false);
		if (JavaFunc != nullptr)
		{
			bResult = FJavaWrapper::CallBooleanMethod(Env, FJavaWrapper::GameActivityThis, JavaFunc);
		}
	}
#endif
	return bResult;
}

void UAndroidSingleInstanceServiceBPLibrary::StopStartupMovie(bool Paused, int Duration)
{
#if USE_ANDROID_MIXEDSAMPLE
	if (JNIEnv* Env = FAndroidApplication::GetJavaEnv())
	{
		static jmethodID JavaFunc = FJavaWrapper::FindMethod(Env, FJavaWrapper::GameActivityClassID, "AndroidThunkJava_MixedSample_StopStartupMovie", "(ZI)V", false);
		if (JavaFunc != nullptr)
		{
			FJavaWrapper::CallVoidMethod(Env, FJavaWrapper::GameActivityThis, JavaFunc, Paused, Duration);
		}
	}
#endif
}

bool UAndroidSingleInstanceServiceBPLibrary::AnimateAlpha(const FString& Key, float TargetAlpha, int Duration)
{
	bool bResult = false;
#if USE_ANDROID_MIXEDSAMPLE
	if (JNIEnv* Env = FAndroidApplication::GetJavaEnv())
	{
		static jmethodID JavaFunc = FJavaWrapper::FindMethod(Env, FJavaWrapper::GameActivityClassID, "AndroidThunkJava_MixedSample_AnimateAlpha", "(Ljava/lang/String;FI)Z", false);
		if (JavaFunc != nullptr)
		{
			auto Argument = FJavaHelper::ToJavaString(Env, Key);
			bResult = FJavaWrapper::CallBooleanMethod(Env, FJavaWrapper::GameActivityThis, JavaFunc, *Argument, TargetAlpha, Duration);
		}
	}
#endif
	return bResult;
}

bool UAndroidSingleInstanceServiceBPLibrary::SetVisibility(const FString& Key, bool Visible)
{
	bool bResult = false;
#if USE_ANDROID_MIXEDSAMPLE
	if (JNIEnv* Env = FAndroidApplication::GetJavaEnv())
	{
		static jmethodID JavaFunc = FJavaWrapper::FindMethod(Env, FJavaWrapper::GameActivityClassID, "AndroidThunkJava_MixedSample_SetVisibility", "(Ljava/lang/String;Z)Z", false);
		if (JavaFunc != nullptr)
		{
			auto Argument = FJavaHelper::ToJavaString(Env, Key);
			bResult = FJavaWrapper::CallBooleanMethod(Env, FJavaWrapper::GameActivityThis, JavaFunc, *Argument, Visible);
		}
	}
#endif
	return bResult;
}

bool UAndroidSingleInstanceServiceBPLibrary::EnableButton(const FString& Key, bool Enabled)
{
	bool bResult = false;
#if USE_ANDROID_MIXEDSAMPLE
	if (JNIEnv* Env = FAndroidApplication::GetJavaEnv())
	{
		static jmethodID JavaFunc = FJavaWrapper::FindMethod(Env, FJavaWrapper::GameActivityClassID, "AndroidThunkJava_MixedSample_EnableButton", "(Ljava/lang/String;Z)Z", false);
		if (JavaFunc != nullptr)
		{
			auto Argument = FJavaHelper::ToJavaString(Env, Key);
			bResult = FJavaWrapper::CallBooleanMethod(Env, FJavaWrapper::GameActivityThis, JavaFunc, *Argument, Enabled);
		}
	}
#endif
	return bResult;
}

#define EVENTTYPE_INIT						0
#define EVENTTYPE_POST_ENGINE_INIT			1
#define EVENTTYPE_ENGINELOOP_INIT_COMPLETE	2
#define EVENTTYPE_FRAME_BEGIN				3
#define EVENTTYPE_FRAME_END					4
#define EVENTTYPE_PRE_LOAD_MAP				5
#define EVENTTYPE_POST_LOAD_MAP				6
#define EVENTTYPE_ACTION					7

void UAndroidSingleInstanceServiceBPLibrary::SendAndroidEvent(int32 EventId, const FString& Param1, int32 Param2, int32 Param3, float Param4)
{
#if USE_ANDROID_STANDALONE
	extern void AndroidThunkCpp_Engine_SendEvent(int32 event, const FString& param1, int32 param2, int32 param3, float param4);
	AndroidThunkCpp_Engine_SendEvent(EVENTTYPE_ACTION, Param1, Param2, Param3, Param4);
#endif
}

// receives calls from Java for events
JNI_METHOD void Java_com_epicgames_makeaar_Engine_nativeEventReceived(JNIEnv* jenv, jobject thiz, jint eventId, jstring param1, jint param2, jint param3, jfloat param4)
{
	FString ParamString = FJavaHelper::FStringFromParam(jenv, param1);

	if (FTaskGraphInterface::IsRunning())
	{
		STANDALONE_DEBUG_LOGf(LogAndroid,TEXT("nativeEventReceived: %d, %s, %d, %d, %f"), eventId, *ParamString, param2, param3, param4);
		FSimpleDelegateGraphTask::CreateAndDispatchWhenReady(
			FSimpleDelegateGraphTask::FDelegate::CreateLambda([=]()
		{
			// trigger blueprint event
			GlobalOnAndroidEventReceived.Broadcast(eventId, ParamString, param2, param3, param4);
		}), TStatId(), nullptr, ENamedThreads::GameThread);
	}
	else
	{
		STANDALONE_DEBUG_LOGf(LogAndroid,TEXT("nativeEventReceived: %d, %s, %d, %d, %f - IGNORED (engine not ready)"), eventId, *ParamString, param2, param3, param4);
	}
}

// receives calls from MixedSampleActivity for pressed button
JNI_METHOD int Java_com_epicgames_makeaar_MixedSampleActivity_nativePressedButton(JNIEnv* jenv, jobject thiz, jstring name)
{
	FString ButtonName = FJavaHelper::FStringFromParam(jenv, name);

	STANDALONE_DEBUG_LOGf(LogAndroid,TEXT("nativePressedButton: %s"), *ButtonName);

	if (FTaskGraphInterface::IsRunning())
	{
		FSimpleDelegateGraphTask::CreateAndDispatchWhenReady(
			FSimpleDelegateGraphTask::FDelegate::CreateLambda([=]()
		{
			// trigger blueprint event
			GlobalOnButtonPressed.Broadcast(ButtonName);
		}), TStatId(), nullptr, ENamedThreads::GameThread);
		return 1;
	}

	return 0;
}

// receives calls from Java for data
JNI_METHOD void Java_com_epicgames_makeaar_Engine_nativeDataReceivedString(JNIEnv* jenv, jobject thiz, jint dataId, jstring key, jstring value)
{
	FString KeyString = FJavaHelper::FStringFromParam(jenv, key);
	FString Value = FJavaHelper::FStringFromParam(jenv, value);

	if (FTaskGraphInterface::IsRunning())
	{
		STANDALONE_DEBUG_LOGf(LogAndroid,TEXT("nativeEventDataReceivedString: %d, %s, %s"), dataId, *KeyString, *Value);
		FSimpleDelegateGraphTask::CreateAndDispatchWhenReady(
			FSimpleDelegateGraphTask::FDelegate::CreateLambda([=]()
		{
			// trigger blueprint event
			GlobalOnAndroidDataReceivedString.Broadcast(dataId, KeyString, Value);
		}), TStatId(), nullptr, ENamedThreads::GameThread);
	}
	else
	{
		STANDALONE_DEBUG_LOGf(LogAndroid,TEXT("nativeDataReceivedString: %d, %s, %s - IGNORED (engine not ready)"), dataId, *KeyString, *Value);
	}
}

JNI_METHOD void Java_com_epicgames_makeaar_Engine_nativeDataReceivedStringArray(JNIEnv* jenv, jobject thiz, jint dataId, jstring key, jobjectArray value)
{
	FString KeyString = FJavaHelper::FStringFromParam(jenv, key);
	TArray<FString> Values;
	if (value != nullptr)
	{
		int32 Count = jenv->GetArrayLength(value);
		for (int32 Index=0; Index < Count; Index++)
		{
			Values.Add(FJavaHelper::FStringFromLocalRef(jenv, (jstring)(jenv->GetObjectArrayElement(value, Index))));
		}
	}

	if (FTaskGraphInterface::IsRunning())
	{
		STANDALONE_DEBUG_LOGf(LogAndroid,TEXT("nativeEventDataReceivedStringArray: %d, %s"), dataId, *KeyString);
		FSimpleDelegateGraphTask::CreateAndDispatchWhenReady(
			FSimpleDelegateGraphTask::FDelegate::CreateLambda([=]()
		{
			// trigger blueprint event
			GlobalOnAndroidDataReceivedStringArray.Broadcast(dataId, KeyString, Values);
		}), TStatId(), nullptr, ENamedThreads::GameThread);
	}
	else
	{
		STANDALONE_DEBUG_LOGf(LogAndroid,TEXT("nativeDataReceivedStringArray: %d, %s - IGNORED (engine not ready)"), dataId, *KeyString);
	}
}

#define NAUTIL_STRING(_IN) #_IN

#define NATIVE_DATA_RECEIVED_VALUE(_NAMETYPE_, _JAVATYPE_, _CPPTYPE_) \
JNI_METHOD void Java_com_epicgames_makeaar_Engine_nativeDataReceived ## _NAMETYPE_ (JNIEnv* jenv, jobject thiz, jint dataId, jstring key, _JAVATYPE_ value) \
{ \
	FString KeyString = FJavaHelper::FStringFromParam(jenv, key); \
	_CPPTYPE_ Value = (_CPPTYPE_)value; \
	\
	if (FTaskGraphInterface::IsRunning()) \
	{ \
		STANDALONE_DEBUG_LOGf(LogAndroid,TEXT("nativeEventReceived" NAUTIL_STRING(_NAMETYPE_) ": %d, %s"), dataId, *KeyString); \
		FSimpleDelegateGraphTask::CreateAndDispatchWhenReady( \
			FSimpleDelegateGraphTask::FDelegate::CreateLambda([=]() \
		{ \
			GlobalOnAndroidDataReceived ## _NAMETYPE_.Broadcast(dataId, KeyString, Value); \
		}), TStatId(), nullptr, ENamedThreads::GameThread); \
	} \
	else \
	{ \
		STANDALONE_DEBUG_LOGf(LogAndroid,TEXT("nativeDataReceived" NAUTIL_STRING(_NAMETYPE_) ": %d, %s - IGNORED (engine not ready)"), dataId, *KeyString); \
	} \
}

NATIVE_DATA_RECEIVED_VALUE(Boolean, jboolean, bool);
NATIVE_DATA_RECEIVED_VALUE(Byte, jbyte, int8);
NATIVE_DATA_RECEIVED_VALUE(Int, jint, int32);
NATIVE_DATA_RECEIVED_VALUE(Short, jshort, int16);
NATIVE_DATA_RECEIVED_VALUE(Long, jlong, int64);
NATIVE_DATA_RECEIVED_VALUE(Float, jfloat, float);
NATIVE_DATA_RECEIVED_VALUE(Double, jdouble, double);

#define NATIVE_DATA_RECEIVED_VALUEARRAY(_NAMETYPE_, _JAVATYPE_, _CPPTYPE_) \
JNI_METHOD void Java_com_epicgames_makeaar_Engine_nativeDataReceived ## _NAMETYPE_ ## Array(JNIEnv* jenv, jobject thiz, jint dataId, jstring key, _JAVATYPE_ ## Array value) \
{ \
	FString KeyString = FJavaHelper::FStringFromParam(jenv, key); \
	TArray<_CPPTYPE_> Values; \
	if (value != nullptr) \
	{ \
		int32 Count = jenv->GetArrayLength(value); \
		_JAVATYPE_ *values = jenv->Get ## _NAMETYPE_ ## ArrayElements(value, 0); \
		for (int32 Index = 0; Index < Count; Index++) \
		{ \
			Values.Add((_CPPTYPE_)values[Index]); \
		} \
		jenv->Release ## _NAMETYPE_ ## ArrayElements(value, values, 0); \
	} \
	\
	if (FTaskGraphInterface::IsRunning()) \
	{ \
		STANDALONE_DEBUG_LOGf(LogAndroid,TEXT("nativeEventReceived" NAUTIL_STRING(_NAMETYPE_) "Array: %d, %s"), dataId, *KeyString); \
		FSimpleDelegateGraphTask::CreateAndDispatchWhenReady( \
			FSimpleDelegateGraphTask::FDelegate::CreateLambda([=]() \
		{ \
			GlobalOnAndroidDataReceived ## _NAMETYPE_ ## Array.Broadcast(dataId, KeyString, Values); \
		}), TStatId(), nullptr, ENamedThreads::GameThread); \
	} \
	else \
	{ \
		STANDALONE_DEBUG_LOGf(LogAndroid,TEXT("nativeDataReceived" NAUTIL_STRING(_NAMETYPE_) "Array: %d, %s - IGNORED (engine not ready)"), dataId, *KeyString); \
	} \
}

NATIVE_DATA_RECEIVED_VALUEARRAY(Boolean, jboolean, bool);
NATIVE_DATA_RECEIVED_VALUEARRAY(Byte, jbyte, int8);
NATIVE_DATA_RECEIVED_VALUEARRAY(Int, jint, int32);
NATIVE_DATA_RECEIVED_VALUEARRAY(Short, jshort, int16);
NATIVE_DATA_RECEIVED_VALUEARRAY(Long, jlong, int64);
NATIVE_DATA_RECEIVED_VALUEARRAY(Float, jfloat, float);
NATIVE_DATA_RECEIVED_VALUEARRAY(Double, jdouble, double);

#else

UAndroidSingleInstanceServiceBPLibrary::UAndroidSingleInstanceServiceBPLibrary(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

FString UAndroidSingleInstanceServiceBPLibrary::GetApplicationNameFromPackage(const FString& PackageName)
{
	FString Result = FString("Unknown");
	return Result;
}

bool UAndroidSingleInstanceServiceBPLibrary::SendString(const FString& Value)
{
	// do nothing
	return false;
}

bool UAndroidSingleInstanceServiceBPLibrary::SendText(const FString& Key, const FString& Value)
{
	// do nothing
	return false;
}

FBox2D UAndroidSingleInstanceServiceBPLibrary::GetSurfaceViewUVs()
{
	return FBox2D(FVector2D(0.0f, 0.0f), FVector2D(1.0f, 1.0f));
}

bool UAndroidSingleInstanceServiceBPLibrary::IsStartupMoviePlaying()
{
	return false;
}

void UAndroidSingleInstanceServiceBPLibrary::StopStartupMovie(bool Paused, int Duration)
{
	// do nothing
	return;
}

bool UAndroidSingleInstanceServiceBPLibrary::AnimateAlpha(const FString& Key, float TargetAlpha, int Duration)
{
	// do nothing
	return false;
}

bool UAndroidSingleInstanceServiceBPLibrary::SetVisibility(const FString& Key, bool Visible)
{
	// do nothing
	return false;
}

bool UAndroidSingleInstanceServiceBPLibrary::EnableButton(const FString& Key, bool Enabled)
{
	// do nothing
	return false;
}

void UAndroidSingleInstanceServiceBPLibrary::SendAndroidEvent(int32 EventId, const FString& Param1, int32 Param2, int32 Param3, float Param4)
{
}

#endif

