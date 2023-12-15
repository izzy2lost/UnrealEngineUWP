// Copyright Epic Games, Inc. All Rights Reserved.

#include "ShaderSource.h"
#include "Containers/StringConv.h"
#include "HAL/UnrealMemory.h"

FShaderSource::FShaderSource(FShaderSource::FViewType InSrc, int32 AdditionalSlack)
{
	Set(InSrc, AdditionalSlack);
}

void FShaderSource::Set(FShaderSource::FViewType InSrc, int32 AdditionalSlack)
{
	SetLen(InSrc.Len() + AdditionalSlack);
	FShaderSource::CharType* SourceData = Source.GetData();
	FMemory::Memcpy(SourceData, InSrc.GetData(), sizeof(FShaderSource::CharType) * InSrc.Len());
}

void FShaderSource::Set(FAnsiStringView InSrc)
{
	TStringConvert<ANSICHAR, TCHAR> Convert;
	SetLen(InSrc.Len());
	Convert.Convert(Source.GetData(), Source.Num(), InSrc.GetData(), InSrc.Len());
}

FShaderSource& FShaderSource::operator=(FShaderSource::FStringType&& InSrc)
{
	int32 InInitialLen = InSrc.Len();
	// input char array already has a null terminator appended, so can add one less padding char
	TArray<CharType>& InSrcArray = InSrc.GetCharArray();
	InSrcArray.AddZeroed(ShaderSourceSimdPadding - 1);
	Source = MoveTemp(InSrcArray);
	return *this;
}

FArchive& operator<<(FArchive& Ar, FShaderSource& Src)
{
	Ar << Src.Source;
	return Ar;
}

