// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Internationalization/Text.h"
#include "UObject/NameTypes.h"

namespace PCGGrammar
{
	enum class EModuleType
	{
		Base,
		Stochastic,
		Priority
	};

	struct FModuleDescriptor
	{
		struct FSubmodule
		{
			FSubmodule(const FName InID, const int8 InWeight)
				: ID(InID)
				, Weight(InWeight)
			{}

			FName ID;
			int8 Weight;
		};

		EModuleType Type = EModuleType::Base;
		int32 Repetitions = 1;
		// The matched beginning and ending indices of the grammar string for this module
		TPair<int32, int32> GrammarStartEndIndices;
		TArray<FSubmodule> Submodules;
	};
}

struct PCG_API FPCGGrammarResult
{
	enum class ELogType
	{
		Log = ELogVerbosity::Log,
		Warning = ELogVerbosity::Warning,
		Error = ELogVerbosity::Error
	};

	struct FLog
	{
		FLog(FText InMessage, ELogType InVerbosity = ELogType::Log)
			: Message(std::move(InMessage))
			, Verbosity(InVerbosity)
		{}

		FText Message;
		ELogType Verbosity = ELogType::Log;

		bool operator==(const FLog& Other) const { return Message.EqualTo(Other.Message) && Verbosity == Other.Verbosity; }
	};

	void AddLog(FText Message, ELogType Verbosity = ELogType::Log);

	const TArray<FLog>& GetLogs() const{ return Logs; }

	bool bSuccess = true;
	TArray<PCGGrammar::FModuleDescriptor> Modules;

private:
	TArray<FLog> Logs;
};

namespace PCGGrammar
{
	PCG_API FPCGGrammarResult Parse(const FString& Grammar, bool bValidateGrammar = true);
}
