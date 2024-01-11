// Copyright Epic Games, Inc. All Rights Reserved.

#include "Replication/Messages/ChangeStream.h"

#include "Misc/OutputDevice.h"

TOptional<FConcertReplication_ChangeStream_PutObject> FConcertReplication_ChangeStream_PutObject::MakeFromInfo(const FConcertReplicatedObjectInfo& New)
{
	if (!New.IsValidForSendingToServer())
	{
		return {};
	}
	return FConcertReplication_ChangeStream_PutObject{ New.PropertySelection, New.ClassPath };
}

TOptional<FConcertReplication_ChangeStream_PutObject> FConcertReplication_ChangeStream_PutObject::MakeFromChange(const FConcertReplicatedObjectInfo& Base, const FConcertReplicatedObjectInfo& Desired)
{
	if (Base.IsValidForSendingToServer() && Desired.IsValidForSendingToServer())
	{
		const FConcertPropertySelection PropertySelection = Base.PropertySelection != Desired.PropertySelection ? Desired.PropertySelection : FConcertPropertySelection{};
		const FSoftClassPath ClassPath = Base.ClassPath != Desired.ClassPath ? Desired.ClassPath : FSoftClassPath{};
		return FConcertReplication_ChangeStream_PutObject{ PropertySelection, ClassPath };
	}
	return {};
}

TOptional<FConcertReplicatedObjectInfo> FConcertReplication_ChangeStream_PutObject::MakeObjectInfoIfValid() const
{
	if (Properties.ReplicatedProperties.IsEmpty() || ClassPath.IsNull())
	{
		return {};
	}
	return FConcertReplicatedObjectInfo{ ClassPath, Properties };
}

void FConcertReplication_ChangeStream_Response::LogErrors(FOutputDevice& OutputDevice) const
{
	for (const TPair<FConcertObjectInStreamID, FConcertReplicatedObjectId>& Conflict : AuthorityConflicts)
	{
		OutputDevice.Logf(TEXT("Authority: { %s } conflicts with { %s }"), *Conflict.Key.ToString(), *Conflict.Value.ToString());
	}

	for (const TPair<FConcertObjectInStreamID, EConcertPutObjectErrorCode>& Error : ObjectsToPutSemanticErrors)
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
