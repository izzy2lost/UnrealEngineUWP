// Copyright Epic Games, Inc. All Rights Reserved.

#include "Grammar/PCGGrammarParser.h"

#include "Algo/Transform.h"
#include "Internationalization/Internationalization.h"
#include "Internationalization/Regex.h"

#define LOCTEXT_NAMESPACE "PCGGrammar"

void FPCGGrammarResult::AddLog(FText Message, ELogType Verbosity)
{
	Logs.AddUnique({ std::move(Message), Verbosity });
	if (Verbosity == ELogType::Error)
	{
		bSuccess = false;
	}
}

namespace PCGGrammar
{
	namespace Patterns
	{
		static constexpr const TCHAR* Base = TEXT("(\\[([^\\]]+)\\](?:\\s*(\\d+|\\*))?)");
		static constexpr const TCHAR* Stochastic = TEXT("(\\{([^\\}]+)\\}(?:\\s*(\\d++|\\*))?)");
		static constexpr const TCHAR* Priority = TEXT("(<([^>]+)>(?:\\s*(\\d++|\\*))?)");
	}

	namespace Tokens
	{
		static constexpr const TCHAR* SubmoduleDelimiter = TEXT(",");
		static constexpr const TCHAR* WeightDelimiter = TEXT(":");
		static constexpr const TCHAR* InfiniteRepetition = TEXT("*");
	}

	enum ECaptureGroups : int
	{
		// Capture Group 0 is the matched expression for this iteration
		MatchedExpression = 0,

		// Capture group 1 is the whole group of submodules, including [ ]
		WholeGroupOfSubmodules = 1,

		// Capture group 2 is the submodule expression. Ie. [A,B], {A,B}, or <A,B> -> A,B
		SubmoduleExpression = 2,

		// Capture group 3 is the number of repetitions - [A,B]3 -> 3
		NumberOfRepetitionsOrAsterisk = 3
	};

	bool FindModules(
		const FString& Grammar,
		const EModuleType ModuleType,
		TArray<TPair<int32, int32>, TInlineAllocator<16>>& OutMatchedIndices,
		FPCGGrammarResult& OutGrammarResult)
	{
		const TCHAR* Pattern = nullptr;
		switch (ModuleType)
		{
			case EModuleType::Base:
				Pattern = Patterns::Base;
				break;
			case EModuleType::Stochastic:
				Pattern = Patterns::Stochastic;
				break;
			case EModuleType::Priority:
				Pattern = Patterns::Priority;
				break;
			default:
				checkNoEntry();
				return false;
		}

		check(Pattern);

		bool bFoundMatch = false;

		TArray<FName> SubmoduleNames;
		TArray<int32> SubmoduleWeights;

		FRegexMatcher ModuleMatcher(FRegexPattern(Pattern), Grammar);
		while (ModuleMatcher.FindNext())
		{
			bFoundMatch = true;

			const FString SubmoduleCapture = ModuleMatcher.GetCaptureGroup(ECaptureGroups::SubmoduleExpression);

			if (SubmoduleCapture.IsEmpty())
			{
				FText Message = FText::Format(LOCTEXT("NoModuleNameFound", "Unable to find module name match within the grammar declaration: {0}"), FText::FromString(ModuleMatcher.GetCaptureGroup(ECaptureGroups::MatchedExpression)));
				OutGrammarResult.AddLog(std::move(Message), FPCGGrammarResult::ELogType::Warning);
				continue;
			}

			TArray<FString> SubmoduleStrings;
			SubmoduleCapture.ParseIntoArray(SubmoduleStrings, Tokens::SubmoduleDelimiter);

			SubmoduleNames.Reset(SubmoduleStrings.Num());
			SubmoduleWeights.Reset(SubmoduleStrings.Num());

			for (const FString& SubmoduleString : SubmoduleStrings)
			{
				TArray<FString> SubmoduleIDString;
				SubmoduleString.ParseIntoArray(SubmoduleIDString, Tokens::WeightDelimiter);

				if (SubmoduleIDString.IsEmpty())
				{
					FText Message = FText::Format(LOCTEXT("InvalidSubmoduleString", "Parsed submodule string is invalid."), FText::FromString(ModuleMatcher.GetCaptureGroup(ECaptureGroups::MatchedExpression)));
					OutGrammarResult.AddLog(std::move(Message), FPCGGrammarResult::ELogType::Warning);
					continue;
				}

				SubmoduleIDString[0].TrimStartAndEndInline();
				if (SubmoduleIDString[0].IsEmpty())
				{
					FText Message = FText::Format(LOCTEXT("EmptyModuleID", "Module ID must not be empty."), FText::FromString(ModuleMatcher.GetCaptureGroup(ECaptureGroups::MatchedExpression)));
					OutGrammarResult.AddLog(std::move(Message), FPCGGrammarResult::ELogType::Warning);
					continue;
				}

				SubmoduleNames.Emplace(SubmoduleIDString[0]);
				SubmoduleWeights.Emplace(1);

				if (SubmoduleIDString.Num() > 1)
				{
					if (ModuleType != EModuleType::Stochastic)
					{
						FText Message = FText::Format(LOCTEXT("WeightOnNonStochasticType", "Weight added to non-stochastic module type."), FText::FromString(ModuleMatcher.GetCaptureGroup(ECaptureGroups::MatchedExpression)));
						OutGrammarResult.AddLog(std::move(Message), FPCGGrammarResult::ELogType::Warning);
						continue;
					}

					if (SubmoduleIDString.Num() > 2)
					{
						FText Message = LOCTEXT("MultiCharacterWeightDelimiter", "Multi-character delimiter for weight. Weight ignored.");
						OutGrammarResult.AddLog(std::move(Message), FPCGGrammarResult::ELogType::Warning);
						continue;
					}

					SubmoduleIDString[1].TrimStartAndEndInline();
					if (!SubmoduleIDString[1].IsNumeric())
					{
						FText Message = FText::Format(LOCTEXT("InvalidWeightCharacter", "Invalid weight character '{0}' Weight ignored."), FText::FromString(SubmoduleIDString[1]));
						OutGrammarResult.AddLog(std::move(Message), FPCGGrammarResult::ELogType::Warning);
						continue;
					}

					SubmoduleWeights.Last() = FCString::Atoi(*SubmoduleIDString[1]);
				}
			}

			// Capture repetition count
			int32 Repetitions = 1;
			const FString RepetitionString = ModuleMatcher.GetCaptureGroup(ECaptureGroups::NumberOfRepetitionsOrAsterisk);
			if (RepetitionString.IsNumeric())
			{
				Repetitions = FCString::Atoi(*RepetitionString);
			}
			else if (!RepetitionString.IsEmpty())
			{
				// It should only be the infinite repetition character, to match the
				ensure(RepetitionString == Tokens::InfiniteRepetition);
				Repetitions = -1;
			}

			FModuleDescriptor& Module = OutGrammarResult.Modules.Emplace_GetRef();
			Module.GrammarStartEndIndices = {ModuleMatcher.GetMatchBeginning(), ModuleMatcher.GetMatchEnding()};
			Module.Type = ModuleType;
			Module.Repetitions = Repetitions;

			for (int Index = 0; Index < SubmoduleNames.Num(); ++Index)
			{
				Module.Submodules.Emplace(SubmoduleNames[Index], SubmoduleWeights[Index]);
			}

			OutMatchedIndices.Emplace(ModuleMatcher.GetMatchBeginning(), ModuleMatcher.GetMatchEnding());
		}

		return bFoundMatch;
	}

	PCG_API FPCGGrammarResult Parse(const FString& Grammar, bool bValidateGrammar)
	{
		FPCGGrammarResult Result;

		if (Grammar.IsEmpty())
		{
			Result.AddLog(LOCTEXT("EmptyGrammar", "Grammar is empty."));
			return Result;
		}

		bool bFoundMatch = true;
		TArray<TPair<int32, int32>, TInlineAllocator<16>> MatchedIndices;

		bFoundMatch |= FindModules(Grammar, EModuleType::Base, MatchedIndices, Result);
		bFoundMatch |= FindModules(Grammar, EModuleType::Stochastic, MatchedIndices, Result);
		bFoundMatch |= FindModules(Grammar, EModuleType::Priority, MatchedIndices, Result);

		if (!bFoundMatch)
		{
			Result.AddLog(LOCTEXT("NoModuleMatch", "Unable to find module match in grammar."), FPCGGrammarResult::ELogType::Warning);
			return Result;
		}

		// Sort by start index - where they appeared in the grammar.
		Algo::Sort(Result.Modules, [](const FModuleDescriptor& LHS, const FModuleDescriptor& RHS)
		{
			return LHS.GrammarStartEndIndices.Key < RHS.GrammarStartEndIndices.Key;
		});

		if (bValidateGrammar)
		{
			FString RemainingCharacters = Grammar;

			MatchedIndices.Sort([](const TPair<int32, int32>& Pair1, const TPair<int32, int32>& Pair2)
			{
				return Pair1.Value < Pair2.Value;
			});

			// Must remove in inverse order to avoid conflict
			for (int I = MatchedIndices.Num() - 1; I >= 0; --I)
			{
				const TPair<int32, int32>& Pair = MatchedIndices[I];
				RemainingCharacters.RemoveAt(Pair.Key, Pair.Value - Pair.Key, EAllowShrinking::No);
			}

			RemainingCharacters.TrimStartAndEndInline();
			if (!RemainingCharacters.IsEmpty())
			{
				Result.AddLog(FText::Format(LOCTEXT("ExtraCharactersInGrammar", "Extraneous characters in grammar {0}"), FText::FromString(RemainingCharacters)), FPCGGrammarResult::ELogType::Warning);
			}
		}

		return Result;
	}
}

#undef LOCTEXT_NAMESPACE
