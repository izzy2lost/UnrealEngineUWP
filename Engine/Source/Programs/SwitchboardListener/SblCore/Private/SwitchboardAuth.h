// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/UnrealString.h"
#include "Templates/Tuple.h"


namespace UE::SwitchboardListener
{
	FString ReadCredentialFromStdin(const FString& CredentialDesc, bool bAllowEmpty = false);

	struct FCredential
	{
		FString CredentialName;
		FString UserName;
		FString CredentialBlob;
	};

	bool SupportsPersistentCredentials();
	bool SaveCredential(const FCredential& Credential);
	TOptional<FCredential> LoadCredential(FStringView CredentialName);

	void FillSecureRandom(TArray<uint8>& InOutArray);
};


namespace UE::SwitchboardListener::Certificates
{
	/**
	 * Get the default paths for (self-signed certificate, private key).
	 * On Windows, %LocalAppData%\SwitchboardListener
	 * Otherwise, ~/.switchboardlistener/cert, NOT world-readable.
	 * 
	 * @param bCreate If true, and the paths do not exist, create them with proper permissions.
	 */
	TTuple<FString, FString> GetSelfSignedPaths(bool bCreate = false);

	/**
	 * Generate a private key + self-signed certificate, and write them to the default paths.
	 * 
	 * @param PrivateKeyPassword The password with which to encrypt the PEM private key (unencrypted if empty).
	 * @return The truncated (first 128 bits) of the SHA256 cert fingerprint for visual comparison.
	 *         If the optional is unset, an error occurred during generation or save.
	 */
	TOptional<FString> CreateSelfSigned(const FString& PrivateKeyPassword);
};
