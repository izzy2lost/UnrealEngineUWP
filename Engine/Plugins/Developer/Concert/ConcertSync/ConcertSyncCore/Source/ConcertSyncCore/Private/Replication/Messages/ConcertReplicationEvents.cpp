// Copyright Epic Games, Inc. All Rights Reserved.

#include "Replication/Messages/ConcertReplicationEvents.h"

#include "Misc/OutputDevice.h"

TOptional<FConcertChangeStream_PutObject> FConcertChangeStream_PutObject::MakeFromInfo(const FReplicatedObjectInfo& New)
{
	if (!New.IsValidForSendingToServer())
	{
		return {};
	}
	return FConcertChangeStream_PutObject{ New.PropertySelection, New.ClassPath };
}

TOptional<FConcertChangeStream_PutObject> FConcertChangeStream_PutObject::MakeFromChange(const FReplicatedObjectInfo& Base, const FReplicatedObjectInfo& Desired)
{
	if (Base.IsValidForSendingToServer() && Desired.IsValidForSendingToServer())
	{
		const FConcertPropertySelection PropertySelection = Base.PropertySelection != Desired.PropertySelection ? Desired.PropertySelection : FConcertPropertySelection{};
		const FSoftClassPath ClassPath = Base.ClassPath != Desired.ClassPath ? Desired.ClassPath : FSoftClassPath{};
		return FConcertChangeStream_PutObject{ PropertySelection, ClassPath };
	}
	return {};
}

TOptional<FReplicatedObjectInfo> FConcertChangeStream_PutObject::MakeObjectInfoIfValid() const
{
	if (Properties.ReplicatedProperties.IsEmpty() || ClassPath.IsNull())
	{
		return {};
	}
	return FReplicatedObjectInfo{ ClassPath, Properties };
}

void FConcertChangeStream_Response::LogErrors(FOutputDevice& OutputDevice) const
{
	for (const TPair<FObjectInStreamID, FReplicatedObjectId>& Conflict : AuthorityConflicts)
	{
		OutputDevice.Logf(TEXT("Authority: { %s } conflicts with { %s }"), *Conflict.Key.ToString(), *Conflict.Value.ToString());
	}

	for (const TPair<FObjectInStreamID, EConcertPutObjectErrorCode>& Error : ObjectsToPutSemanticErrors)
	{
		const FString ErrorCodeAsString = [&Error]()
		{
			switch (Error.Value)
			{
			case EConcertPutObjectErrorCode::UnresolvedStream: return TEXT("Unresolved stream");
			case EConcertPutObjectErrorCode::MissingData: return TEXT("Missing data");
			default: return TEXT("Unknown");
			}
		}();
		OutputDevice.Logf(TEXT("Semantic error: %s - %s"), *Error.Key.ToString(), *ErrorCodeAsString);
	}

	for (const FGuid& Stream : FailedStreamCreation)
	{
		OutputDevice.Logf(TEXT("Failed to create stream: %s"), *Stream.ToString());
	}
}
