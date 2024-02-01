// Copyright Epic Games, Inc. All Rights Reserved.

#include "DMXConflictMonitorConflictModel.h"

#include "Algo/AnyOf.h"
#include "Algo/Sort.h"
#include "DMXEditorSettings.h"
#include "Framework/Text/ITextDecorator.h"
#include "IO/DMXConflictMonitor.h"
#include "IO/DMXOutputPort.h"


#define LOCTEXT_NAMESPACE "FDMXConflictMonitorConflictModel"

namespace UE::DMX
{
	FDMXConflictMonitorConflictModel::FDMXConflictMonitorConflictModel(const TArray<TSharedRef<FDMXMonitoredOutboundDMXData>>& InConflicts)
		: Conflicts(InConflicts)
	{
		ParseConflict();
	}

	FString FDMXConflictMonitorConflictModel::GetConflictAsString() const
	{
		FString Result = Title;
		for (const FString& Detail : Details)
		{
			Result.Append(TEXT("\n") + Detail);
		}

		return Result;
	}

	void FDMXConflictMonitorConflictModel::ParseConflict()
	{
		TArray<FName> NameStacks;
		for (const TSharedRef<FDMXMonitoredOutboundDMXData>& Conflict : Conflicts)
		{
			NameStacks.AddUnique(Conflict->Trace);
		}

		// Lexical sort
		Algo::Sort(NameStacks, [](const FName& SenderA, const FName& SenderB)
			{
				return SenderA.Compare(SenderB) < 0;
			});

		const UDMXEditorSettings* EditorSettings = GetDefault<UDMXEditorSettings>();
		const uint8 Depth = EditorSettings->ConflictMonitorSettings.Depth;

		int32 CountStack = 0;
		for (const FName& NameStack : NameStacks)
		{
			++CountStack;
			const bool bFirstNameInStack = &NameStack == &NameStacks[0];

			TArray<FString> Substrings;
			NameStack.ToString().ParseIntoArray(Substrings, TEXT(","));

			// Depth filter
			if (Substrings.Num() > Depth)
			{
				Substrings.SetNum(Depth);
			}

			FString DetailsString;
			for (int32 SubstringIndex = 0; SubstringIndex < Substrings.Num(); SubstringIndex++)
			{
				const bool bFirstSubstring = &Substrings[SubstringIndex] == &Substrings[0];
				const FString CleanSubstring = FPaths::GetBaseFilename(Substrings[SubstringIndex]);

				// Create the title
				if (bFirstSubstring && bFirstNameInStack)
				{
					FString FirstString = LOCTEXT("ConflictDetected", "Send DMX Conflict: ").ToString() + CleanSubstring;
					FirstString = StyleString(FirstString, TEXT("ConflictLog.Title"));
					Title.Append(FirstString);
				}
				else if (bFirstSubstring)
				{
					FString NextConflictString = LOCTEXT("AppendConflict", " / ").ToString() + CleanSubstring;
					NextConflictString = StyleString(NextConflictString, TEXT("ConflictLog.Title"));
					Title.Append(NextConflictString);
				}

				if (Depth == 1)
				{
					break;
				}

				// Create details
				if (bFirstSubstring)
				{
					const FString Number = TEXT("\t\t") + FString::FromInt(CountStack) + TEXT(". ");
					const FString TraceString = StyleString(Number + CleanSubstring, TEXT("ConflictLog.Warning"));
					DetailsString = TraceString;
				}
				else
				{
					const FString TraceString = StyleString(TEXT(" -> ") + CleanSubstring, TEXT("ConflictLog.Warning"));
					DetailsString.Append(TraceString);
				}
			}

			Details.Add(DetailsString);
		}

		if (Depth > 1)
		{
			// Add port and universe/channel info
			const FString Port = StyleString(LOCTEXT("PortInfo", "\t\tPort: ").ToString() + GetPortNameText(), TEXT("ConflictLog.Error"));
			const FString Universe = StyleString(LOCTEXT("UniverseInfo", "Universe: ").ToString() + GetUniverseText(), TEXT("ConflictLog.Error"));
			const FString Channels = StyleString(LOCTEXT("ChannelInfo", "Channel: ").ToString() + GetChannelsText(), TEXT("ConflictLog.Error"));

			const FString DetailsSeparator = StyleString(TEXT(" - "), TEXT("ConflictLog.Error"));
			Details.Add(Port + DetailsSeparator + Universe + DetailsSeparator + Channels);
			Details.Add(TEXT(""));
		}
	}

	FString FDMXConflictMonitorConflictModel::GetPortNameText() const
	{
		static const FText InvalidPortName = LOCTEXT("InvalidPortName", "<Invalid Port>");

		TArray<FName> PortNames;
		for (const TSharedRef<FDMXMonitoredOutboundDMXData>& Conflict : Conflicts)
		{
			const FName PortName = Conflict->OutputPort.IsValid() ? *Conflict->OutputPort.Pin()->GetPortName() : *InvalidPortName.ToString();
			PortNames.AddUnique(PortName);
		}
		
		FString UniquePortString;
		for (const FName& PortName : PortNames)
		{
			UniquePortString.Append(PortName.ToString());

			if (&PortName != &PortNames.Last())
			{
				UniquePortString.Append(TEXT(", "));
			}
		}

		return UniquePortString;
	}

	FString FDMXConflictMonitorConflictModel::GetUniverseText() const
	{
		if (!Conflicts.IsEmpty())
		{
			return FString::FromInt(Conflicts[0]->LocalUniverseID);
		}
		return FString();
	}

	FString FDMXConflictMonitorConflictModel::GetChannelsText() const
	{
		TArray<int32> Channels;

		// Find conflicting channels
		for (const TSharedRef<FDMXMonitoredOutboundDMXData>& Conflict : Conflicts)
		{
			for (const TSharedRef<FDMXMonitoredOutboundDMXData>& Other : Conflicts)
			{
				// Only lexically lower, avoids duplicates and self
				if (Other->Trace.Compare(Conflict->Trace) >= 0.0)
				{
					continue;
				}

				for (const TTuple<int32, uint8>& ChannelToValuePair : Conflict->ChannelToValueMap)
				{
					if (Other->ChannelToValueMap.Contains(ChannelToValuePair.Key))
					{
						Channels.Add(ChannelToValuePair.Key);
					}
				}
			}
		}
		Algo::Sort(Channels);

		FString ChannelsString;
		constexpr int32 MaxIndex = 31;
		for (int32 ChannelIndex = 0; ChannelIndex < Channels.Num(); ChannelIndex++)
		{
			ChannelsString.Append(FString::FromInt(Channels[ChannelIndex]));

			if (ChannelIndex >= MaxIndex)
			{
				break;
			}

			if (&Channels[ChannelIndex] != &Channels.Last())
			{
				ChannelsString.Append(TEXT(", "));
			}
		}

		if (Channels.Num() >= MaxIndex + 1)
		{
			const FText MoreText = FText::Format(LOCTEXT("MoreText", " (and {0} more)"), FText::FromString(FString::FromInt(Channels.Num() - MaxIndex + 1)));
			ChannelsString.Append(MoreText.ToString());
		}

		return ChannelsString;
	}

	FString FDMXConflictMonitorConflictModel::StyleString(FString String, FString MarkupString) const
	{
		return FString::Printf(TEXT("<%s>%s</>"), *MarkupString, *String);
	}
}

#undef LOCTEXT_NAMESPACE
