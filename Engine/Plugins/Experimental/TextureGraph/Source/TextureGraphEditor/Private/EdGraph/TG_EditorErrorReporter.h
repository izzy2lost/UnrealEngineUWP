// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once
#include "MixerErrorReporter.h"
#include "TG_Editor.h"
#include "Logging/TokenizedMessage.h"


class FTG_EditorErrorReporter: public FMixerErrorReporter
{
public:
	FTG_EditorErrorReporter(FTG_Editor* InEditor)
	: FMixerErrorReporter()
	, Editor(InEditor)
	{
	}
	virtual ~FTG_EditorErrorReporter() { }

	virtual FMixerErrorReport ReportLog(int32 ErrorId, const FString& ErrorMsg, UObject* ReferenceObj = nullptr) override;
	
	virtual FMixerErrorReport ReportWarning(int32 ErrorId, const FString& ErrorMsg, UObject* ReferenceObj = nullptr) override;

	virtual FMixerErrorReport ReportError(int32 ErrorId, const FString& ErrorMsg, UObject* ReferenceObj = nullptr) override;

	virtual void Clear() override;

private:
	FMixerErrorReport Report(int32 ErrorId, const FString& ErrorMsg, UObject* ReferenceObj, EMessageSeverity::Type ErrorType);

	FTG_Editor* Editor;
};

