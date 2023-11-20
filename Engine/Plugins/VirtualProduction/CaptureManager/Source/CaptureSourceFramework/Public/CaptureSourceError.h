// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/UnrealString.h"
#include "Templates/ValueOrError.h"

enum class ECaptureSourceError
{
	Ok = 0,
	InvalidArgument,
	NotSupported,
	NotFound,
	InternalError,
};

class CAPTURESOURCEFRAMEWORK_API FCaptureSourceError
{
public:

	FCaptureSourceError(ECaptureSourceError InError, FString InMessage = "");

	ECaptureSourceError GetErrorCode() const;
	const FString& GetMessage() const;

private:

	ECaptureSourceError ErrorCode;
	FString Message;
};

using FCaptureVoidResult = TValueOrError<void, FCaptureSourceError>;

template<typename Value>
using FCaptureValueResult = TValueOrError<Value, FCaptureSourceError>;