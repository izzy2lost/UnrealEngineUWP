// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Online/Auth.h"
#include "Online/OnlineAsyncOp.h"

#if defined(EOS_PLATFORM_BASE_FILE_NAME)
#include EOS_PLATFORM_BASE_FILE_NAME
#endif

#include "eos_auth_types.h"

namespace UE::Online {

enum class EEOSAuthTranslationFlags : uint8
{
	None = 0,
	SetId = 1 << 0,
	SetTokenFromString = 1 << 1,
	SetTokenFromExternalAuth = 1 << 2,
};
ENUM_CLASS_FLAGS(EEOSAuthTranslationFlags);

struct FEOSAuthTranslationTraits
{
	EOS_ELoginCredentialType Type;
	EEOSAuthTranslationFlags Flags;
};

struct FEOSExternalAuthTranslationTraits
{
	EOS_EExternalCredentialType Type;
};

class FEOSAuthLoginOptionsCommon : public EOS_Auth_LoginOptions
{
public:
	FEOSAuthLoginOptionsCommon(const FEOSAuthLoginOptionsCommon&) = delete;
	FEOSAuthLoginOptionsCommon& operator=(const FEOSAuthLoginOptionsCommon&) = delete;
	FEOSAuthLoginOptionsCommon(FEOSAuthLoginOptionsCommon&&);
	FEOSAuthLoginOptionsCommon& operator=(FEOSAuthLoginOptionsCommon&&);

protected:
	FEOSAuthLoginOptionsCommon();

	static const FEOSAuthTranslationTraits* GetLoginTranslatorTraits(FName Name);
	static const FEOSExternalAuthTranslationTraits* GetExternalAuthTranslationTraits(FName ExternalAuthType);

	static bool InitSystemAuthCredentialOptions(FEOSAuthLoginOptionsCommon& Options) { return true; }

	EOS_Auth_Credentials CredentialsData;
	TArray<char> IdUtf8;
	TArray<char> TokenUtf8;
};
}