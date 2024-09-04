// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Async/Async.h"
#include "CoreMinimal.h"
#include "EpicRtcVideoCommon.h"
#include "HAL/IConsoleManager.h"
#include "Misc/CommandLine.h"
#include "Video/CodecUtils/CodecUtilsH264.h"
#include "Video/CodecUtils/CodecUtilsVP9.h"

namespace UE::PixelStreaming2
{
	template <typename T>
	inline void CommandLineParseValue(const TCHAR* Match, TAutoConsoleVariable<T>& CVar)
	{
		T Value;
		if (FParse::Value(FCommandLine::Get(), Match, Value))
			CVar->Set(Value, ECVF_SetByCommandline);
	};

	template <typename T>
	inline void CommandLineParseValue(const TCHAR* Match, T& OutVar)
	{
		T Value;
		if (FParse::Value(FCommandLine::Get(), Match, Value))
			OutVar = Value;
	};

	inline void CommandLineParseValue(const TCHAR* Match, TAutoConsoleVariable<FString>& CVar, bool bStopOnSeparator = false)
	{
		FString Value;
		if (FParse::Value(FCommandLine::Get(), Match, Value, bStopOnSeparator))
			CVar->Set(*Value, ECVF_SetByCommandline);
	};

	inline void CommandLineParseOption(const TCHAR* Match, TAutoConsoleVariable<bool>& CVar)
	{
		FString ValueMatch(Match);
		ValueMatch.Append(TEXT("="));
		FString Value;
		if (FParse::Value(FCommandLine::Get(), *ValueMatch, Value))
		{
			if (Value.Equals(FString(TEXT("true")), ESearchCase::IgnoreCase))
			{
				CVar->Set(true, ECVF_SetByCommandline);
			}
			else if (Value.Equals(FString(TEXT("false")), ESearchCase::IgnoreCase))
			{
				CVar->Set(false, ECVF_SetByCommandline);
			}
		}
		else if (FParse::Param(FCommandLine::Get(), Match))
		{
			CVar->Set(true, ECVF_SetByCommandline);
		}
	}

	template <typename T>
	void DoOnGameThread(T&& Func)
	{
		if (IsInGameThread())
		{
			Func();
		}
		else
		{
			AsyncTask(ENamedThreads::GameThread, [Func]() { Func(); });
		}
	}

	template <typename T>
	void DoOnGameThreadAndWait(uint32 Timeout, T&& Func)
	{
		if (IsInGameThread())
		{
			Func();
		}
		else
		{
			FEvent* TaskEvent = FPlatformProcess::GetSynchEventFromPool();
			AsyncTask(ENamedThreads::GameThread, [Func, TaskEvent]() {
				Func();
				TaskEvent->Trigger();
			});
			TaskEvent->Wait(Timeout);
			FPlatformProcess::ReturnSynchEventToPool(TaskEvent);
		}
	}

	inline FString H264ProfileLevelIdToString(EH264Profile Profile, UE::AVCodecCore::H264::EH264Level Level)
	{
		using namespace UE::AVCodecCore::H264;

		if (Level == EH264Level::Level_1b)
		{
			switch (Profile)
			{
				case EH264Profile::ConstrainedBaseline:
					return TEXT("42f00b");
				case EH264Profile::Baseline:
					return TEXT("42100b");
				case EH264Profile::Main:
					return TEXT("4d100b");
					// Level 1b is not allowed for other profiles.
				default:
					return TEXT("");
			}
		}

		FString ProfileIdcIopString;

		switch (Profile)
		{
			case EH264Profile::ConstrainedBaseline:
				ProfileIdcIopString = TEXT("42e0");
				break;
			case EH264Profile::Baseline:
				ProfileIdcIopString = TEXT("4200");
				break;
			case EH264Profile::Main:
				ProfileIdcIopString = TEXT("4d00");
				break;
			case EH264Profile::ConstrainedHigh:
				ProfileIdcIopString = TEXT("640c");
				break;
			case EH264Profile::High:
				ProfileIdcIopString = TEXT("6400");
				break;
			case EH264Profile::High444:
				ProfileIdcIopString = TEXT("f400");
				break;
				// Unrecognized profile.
			default:
				return TEXT("");
		}

		return FString::Printf(TEXT("%s%02x"), *ProfileIdcIopString, Level);
	}

	inline FEpicRtcParameterPairArray* CreateH264Format(EH264Profile Profile, UE::AVCodecCore::H264::EH264Level Level)
	{
		FString ProfileString = H264ProfileLevelIdToString(Profile, Level);

		check(!ProfileString.IsEmpty());

		// TODO (Migration): RTCP-7028 picRtcStringView needs a way to own the memory passed into it
		// return new FEpicRtcParameterPairArray(
		// {
		// 	EpicRtcParameterPair{
		// 		._key = EpicRtcStringView{ ._ptr = "profile-level-id", ._length = 16 },
		// 		._value = EpicRtcStringView{ ._ptr = TCHAR_TO_ANSI(*ProfileString), ._length = (uint64_t)ProfileString.Len() }
		// 	},
		// 	EpicRtcParameterPair{
		// 		._key = EpicRtcStringView{ ._ptr = "packetization-mode", ._length = 18 },
		// 		._value = EpicRtcStringView{ ._ptr = "1", ._length = 1 }
		// 	},
		// 	EpicRtcParameterPair{
		// 		._key = EpicRtcStringView{ ._ptr = "level-asymmetry-allowed", ._length = 23 },
		// 		._value = EpicRtcStringView{ ._ptr = "1", ._length = 1 }
		// 	}
		// });

		using namespace UE::AVCodecCore::H264;
		if (Profile == EH264Profile::ConstrainedBaseline && Level == EH264Level::Level_3_1)
		{
			return new FEpicRtcParameterPairArray(
				{ EpicRtcParameterPair{
					  ._key = EpicRtcStringView{ ._ptr = "profile-level-id", ._length = 16 },
					  ._value = EpicRtcStringView{ ._ptr = "42e01f", ._length = 6 } },
					EpicRtcParameterPair{
						._key = EpicRtcStringView{ ._ptr = "packetization-mode", ._length = 18 },
						._value = EpicRtcStringView{ ._ptr = "1", ._length = 1 } },
					EpicRtcParameterPair{
						._key = EpicRtcStringView{ ._ptr = "level-asymmetry-allowed", ._length = 23 },
						._value = EpicRtcStringView{ ._ptr = "1", ._length = 1 } } });
		}
		else if (Profile == EH264Profile::Baseline && Level == EH264Level::Level_3_1)
		{
			return new FEpicRtcParameterPairArray(
				{ EpicRtcParameterPair{
					  ._key = EpicRtcStringView{ ._ptr = "profile-level-id", ._length = 16 },
					  ._value = EpicRtcStringView{ ._ptr = "42001f", ._length = 6 } },
					EpicRtcParameterPair{
						._key = EpicRtcStringView{ ._ptr = "packetization-mode", ._length = 18 },
						._value = EpicRtcStringView{ ._ptr = "1", ._length = 1 } },
					EpicRtcParameterPair{
						._key = EpicRtcStringView{ ._ptr = "level-asymmetry-allowed", ._length = 23 },
						._value = EpicRtcStringView{ ._ptr = "1", ._length = 1 } } });
		}
		else
		{
			return nullptr;
		}
	}

	inline FEpicRtcParameterPairArray* CreateVP9Format(UE::AVCodecCore::VP9::EProfile Profile)
	{
		using namespace UE::AVCodecCore::VP9;
		if (Profile == EProfile::Profile0)
		{
			return new FEpicRtcParameterPairArray(
				{ EpicRtcParameterPair{
					._key = EpicRtcStringView{ ._ptr = "profile-id", ._length = 10 },
					._value = EpicRtcStringView{ ._ptr = "0", ._length = 1 } } });
		}
		else if (Profile == EProfile::Profile1)
		{
			return new FEpicRtcParameterPairArray(
				{ EpicRtcParameterPair{
					._key = EpicRtcStringView{ ._ptr = "profile-id", ._length = 10 },
					._value = EpicRtcStringView{ ._ptr = "1", ._length = 1 } } });
		}
		else if (Profile == EProfile::Profile2)
		{
			return new FEpicRtcParameterPairArray(
				{ EpicRtcParameterPair{
					._key = EpicRtcStringView{ ._ptr = "profile-id", ._length = 10 },
					._value = EpicRtcStringView{ ._ptr = "2", ._length = 1 } } });
		}
		else if (Profile == EProfile::Profile3)
		{
			return new FEpicRtcParameterPairArray(
				{ EpicRtcParameterPair{
					._key = EpicRtcStringView{ ._ptr = "profile-id", ._length = 10 },
					._value = EpicRtcStringView{ ._ptr = "3", ._length = 1 } } });
		}
		else
		{
			return nullptr;
		}
	}

	inline void MemCpyStride(void* Dest, const void* Src, size_t DestStride, size_t SrcStride, size_t Height)
	{
		char*		DestPtr = static_cast<char*>(Dest);
		const char* SrcPtr = static_cast<const char*>(Src);
		size_t		Row = Height;
		while (Row--)
		{
			FMemory::Memcpy(DestPtr + DestStride * Row, SrcPtr + SrcStride * Row, DestStride);
		}
	}

	inline size_t SerializeToBuffer(TArray<uint8>& Buffer, size_t Pos, const void* Data, size_t DataSize)
	{
		check(Pos + DataSize <= Buffer.Num());
		FMemory::Memcpy(Buffer.GetData() + Pos, Data, DataSize);
		return Pos + DataSize;
	}

} // namespace UE::PixelStreaming2
