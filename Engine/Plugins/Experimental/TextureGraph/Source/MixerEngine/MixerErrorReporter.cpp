// Copyright Epic Games, Inc. All Rights Reserved.
#include "MixerErrorReporter.h"
#include "Logging/LogMacros.h"

DEFINE_LOG_CATEGORY(LogMixerError);

FMixerErrorReport FMixerErrorReporter::ReportLog(int32 ErrorId, const FString& ErrorMsg, UObject* ReferenceObj /*= nullptr*/)
{
	FMixerErrorReport Report{ErrorId, ErrorMsg, ReferenceObj};
	UE_LOG(LogMixerError, Log, TEXT("ErrorReporter: %s"), *Report.GetFormattedMessage());
	return Report;
}

FMixerErrorReport FMixerErrorReporter::ReportWarning(int32 ErrorId, const FString& ErrorMsg, UObject* ReferenceObj /*= nullptr*/)
{
	FMixerErrorReport Report{ErrorId, ErrorMsg, ReferenceObj};
	UE_LOG(LogMixerError, Warning, TEXT("ErrorReporter: %s"), *Report.GetFormattedMessage());
	return Report; 
}

FMixerErrorReport FMixerErrorReporter::ReportError(int32 ErrorId, const FString& ErrorMsg, UObject* ReferenceObj /*= nullptr*/)
{
	FMixerErrorReport Report{ErrorId, ErrorMsg, ReferenceObj};
	UE_LOG(LogMixerError, Error, TEXT("ErrorReporter: %s"), *Report.GetFormattedMessage());
	return Report; 
}


