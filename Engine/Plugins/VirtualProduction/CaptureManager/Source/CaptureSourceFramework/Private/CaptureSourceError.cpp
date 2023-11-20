// Copyright Epic Games, Inc. All Rights Reserved.

#include "CaptureSourceError.h"

FCaptureSourceError::FCaptureSourceError(ECaptureSourceError InErrorCode, FString InMessage)
	: ErrorCode(InErrorCode)
	, Message(MoveTemp(InMessage))
{
}

ECaptureSourceError FCaptureSourceError::GetErrorCode() const
{
	return ErrorCode;
}

const FString& FCaptureSourceError::GetMessage() const
{
	return Message;
}