// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "RemoteControlSignature.h"
#include "UObject/Object.h"
#include "RemoteControlSignatureRegistry.generated.h"

UCLASS(MinimalAPI)
class URemoteControlSignatureRegistry : public UObject
{
	GENERATED_BODY()

public:
	TConstArrayView<FRCSignature> GetSignatures() const
	{
		return Signatures;
	}

	/**
	 * Find an existing Signature matching the given id
	 * @param InSignatureId the id of the signature to find
	 * @return const pointer to the signature if found, nullptr otherwise 
	 */
	REMOTECONTROL_API const FRCSignature* FindSignature(const FGuid& InSignatureId) const;

	/**
	 * Find an existing Signature matching the given id
	 * @param InSignatureId the id of the signature to find
	 * @return pointer to the signature if found, nullptr otherwise 
	 */
	REMOTECONTROL_API FRCSignature* FindSignatureMutable(const FGuid& InSignatureId);

	/** Adds a new Signature to this registry */
	REMOTECONTROL_API FRCSignature& AddSignature();

	/**
	 * Removes the Signature with the matching Signature Id
	 * @param InSignatureId the id of the signature to remove
	 * @return the number of signatures removed
	 */
	REMOTECONTROL_API int32 RemoveSignature(const FGuid& InSignatureId);

	/** Removes all the Signature in this registry */
	REMOTECONTROL_API void EmptySignatures();

private:
	/** Holds all the Signatures in this Registry */
	UPROPERTY()
	TArray<FRCSignature> Signatures;
};
