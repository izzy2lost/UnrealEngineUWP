// Copyright Epic Games, Inc. All Rights Reserved.

#include "NNERuntimeIREEMetaData.h"

namespace UE::NNERuntimeIREE
{

#ifdef WITH_NNE_RUNTIME_IREE

bool FArgumentMetaData::ParseFromString(const FString& ArgumentString)
{
	FString Argument = ArgumentString;
	int32 AttributeStartIndex = Argument.Find("{");
	if (AttributeStartIndex > 0)
	{
		Argument = Argument.Mid(0, AttributeStartIndex).TrimStartAndEnd();
	}

	Name = "";
	int32 NameEnd = Argument.Find(":");
	if (NameEnd > 0)
	{
		Name = Argument.Mid(0, NameEnd).TrimStartAndEnd();
		Argument = Argument.Mid(NameEnd + 1).TrimStartAndEnd();
	}
	else
	{
		Argument = Argument.TrimStartAndEnd();
	}

	if (Argument.StartsWith("tensor"))
	{
		int32 ShapeOpenBracket = Argument.Find("<");
		int32 ShapeCloseBracket = Argument.Find(">");
		if (ShapeOpenBracket < 1 || ShapeCloseBracket <= ShapeOpenBracket)
		{
			return false;
		}
		FString ShapeTypeString = Argument.Mid(ShapeOpenBracket + 1, ShapeCloseBracket - (ShapeOpenBracket + 1)).TrimStartAndEnd();

		TArray<FString> Dims;
		FString Dim;
		while (ShapeTypeString.Split("x", &Dim, &ShapeTypeString))
		{
			Dims.Add(Dim);
		}
		Type = ShapeTypeString;

		for (int32 i = 0; i < Dims.Num(); i++)
		{
			if (Dims[i].Contains("?"))
			{
				Shape.Add(-1);
			}
			else
			{
				int32 DimVal = FCString::Atoi(*Dims[i]);
				Shape.Add(DimVal > 0 ? DimVal : -1);
			}
		}
	}
	else
	{
		Shape.SetNumUninitialized(0);
		Type = Argument;
	}

	return true;
}

bool FFunctionMetaData::ParseFromString(const FString& FunctionString)
{
	FString ResultPattern = "->";
	int32 ArgumentsOpenBracket = FunctionString.Find("(");
	int32 ArgumentsCloseBracket = FunctionString.Find(")");
	int32 ResultStart = FunctionString.Find(ResultPattern);
	if (ArgumentsOpenBracket < 1 || ArgumentsCloseBracket <= ArgumentsOpenBracket)
	{
		return false;
	}

	Name = FunctionString.Mid(0, ArgumentsOpenBracket).TrimStartAndEnd();

	FString ArgumentsString = FunctionString.Mid(ArgumentsOpenBracket + 1, ArgumentsCloseBracket - (ArgumentsOpenBracket + 1) ).TrimStartAndEnd();
	while (ArgumentsString.Len() > 0)
	{
		int32 SeparatorIndex = ArgumentsString.Find(",");
		FString ArgumentString;
		if (SeparatorIndex > 0)
		{
			ArgumentString = ArgumentsString.Mid(0, SeparatorIndex).TrimStartAndEnd();
			ArgumentsString = ArgumentsString.Mid(SeparatorIndex + 1).TrimStartAndEnd();
		}
		else
		{
			ArgumentString = ArgumentsString;
			ArgumentsString = "";
		}

		FArgumentMetaData MetaData;
		if (MetaData.ParseFromString(ArgumentString))
		{
			ArgumentMetaData.Add(MetaData);
		}
		else
		{
			return false;
		}
	}

	if (ResultStart > ArgumentsCloseBracket)
	{
		FString ResultsString = FunctionString.Mid(ResultStart + ResultPattern.Len()).TrimStartAndEnd();
		ResultsString.RemoveFromStart("(");
		ResultsString.RemoveFromEnd(")");
		ResultsString = ResultsString.TrimStartAndEnd();
		while (ResultsString.Len() > 0)
		{
			int32 SeparatorIndex = ResultsString.Find(",");
			FString ResultString;
			if (SeparatorIndex > 0)
			{
				ResultString = ResultsString.Mid(0, SeparatorIndex).TrimStartAndEnd();
				ResultsString = ResultsString.Mid(SeparatorIndex + 1).TrimStartAndEnd();
			}
			else
			{
				ResultString = ResultsString;
				ResultsString = "";
			}

			FArgumentMetaData MetaData;
			if (MetaData.ParseFromString(ResultString))
			{
				ResultMetaData.Add(MetaData);
			}
			else
			{
				return false;
			}
		}
	}

	return true;
}

bool FModuleMetaData::ParseFromString(const FString& ModuleString)
{
	FString SearchString = ModuleString;
	FString Pattern = "func.func";
	int32 MatchStart = -1;
	while ((MatchStart = SearchString.Find(Pattern)) > 0)
	{
		MatchStart += Pattern.Len();
		SearchString = SearchString.Mid(MatchStart).TrimStartAndEnd();
		if (SearchString.StartsWith("@"))
		{
			SearchString = SearchString.Mid(1);

			int32 ArgumentsEnd = SearchString.Find(")");
			FString TempString = SearchString.Mid(ArgumentsEnd + 1).TrimStartAndEnd();
			if (TempString.StartsWith("->"))
			{
				TempString = TempString.Mid(3).TrimStartAndEnd();
				if (TempString.StartsWith("("))
				{
					MatchStart = SearchString.Find(")", ESearchCase::IgnoreCase, ESearchDir::FromStart, ArgumentsEnd + 1);
				}
				else
				{
					int32 AttribtueStart = SearchString.Find("attributes", ESearchCase::IgnoreCase, ESearchDir::FromStart, ArgumentsEnd + 1);
					int32 BracketStart = SearchString.Find("{", ESearchCase::IgnoreCase, ESearchDir::FromStart, ArgumentsEnd + 1);
					MatchStart = AttribtueStart <= ArgumentsEnd ? BracketStart : FMath::Min(AttribtueStart, BracketStart);
				}
			}
			else
			{
				MatchStart = ArgumentsEnd;
			}

			FFunctionMetaData MetaData;
			if (MetaData.ParseFromString(SearchString.Mid(0, MatchStart)))
			{
				FunctionMetaData.Emplace(MetaData.Name, MetaData);
			}
		}
	}
	return FunctionMetaData.Num() > 0;
}

#else

bool FArgumentMetaData::ParseFromString(const FString& ArgumentString)
{
	return false;
}

bool FFunctionMetaData::ParseFromString(const FString& FunctionString)
{
	return false;
}

bool FModuleMetaData::ParseFromString(const FString& ModuleString)
{
	return false;
}

#endif // WITH_NNE_RUNTIME_IREE

} // UE::NNERuntimeIREE