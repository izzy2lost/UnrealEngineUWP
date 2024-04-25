// Copyright Epic Games, Inc. All Rights Reserved.

#include "RemoteControlSignatureRegistry.h"

#define LOCTEXT_NAMESPACE "RemoteControlSignatureRegistry"

const FRCSignature* URemoteControlSignatureRegistry::FindSignature(const FGuid& InSignatureId) const
{
	return Signatures.FindByKey(InSignatureId);
}

FRCSignature* URemoteControlSignatureRegistry::FindSignatureMutable(const FGuid& InSignatureId)
{
	return Signatures.FindByKey(InSignatureId);
}

FRCSignature& URemoteControlSignatureRegistry::AddSignature()
{
	FRCSignature& Signature = Signatures.AddDefaulted_GetRef();
	Signature.DisplayName = LOCTEXT("NewSignatureDisplayName", "New Signature");
	Signature.Id = FGuid::NewGuid();
	return Signature;
}

int32 URemoteControlSignatureRegistry::RemoveSignature(const FGuid& InSignatureId)
{
	return Signatures.RemoveAll([&InSignatureId](const FRCSignature& InSignature)
		{
			return InSignature.Id == InSignatureId;
		});
}

void URemoteControlSignatureRegistry::EmptySignatures()
{
	Signatures.Empty();
}

#undef LOCTEXT_NAMESPACE
