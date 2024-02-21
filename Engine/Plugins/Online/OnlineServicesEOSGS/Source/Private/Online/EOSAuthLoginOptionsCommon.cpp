// Copyright Epic Games, Inc. All Rights Reserved.

#include "Online/EOSAuthLoginOptionsCommon.h"

#include "EOSShared.h"

namespace UE::Online {

const FEOSAuthTranslationTraits* FEOSAuthLoginOptionsCommon::GetLoginTranslatorTraits(FName Name)
{
	static const TMap<FName, FEOSAuthTranslationTraits> SupportedLoginTranslatorTraits = {
		{ LoginCredentialsType::Password, { EOS_ELoginCredentialType::EOS_LCT_Password, EEOSAuthTranslationFlags::SetId | EEOSAuthTranslationFlags::SetTokenFromString } },
		{ LoginCredentialsType::ExchangeCode, { EOS_ELoginCredentialType::EOS_LCT_ExchangeCode, EEOSAuthTranslationFlags::SetTokenFromString } },
		{ LoginCredentialsType::PersistentAuth, { EOS_ELoginCredentialType::EOS_LCT_PersistentAuth, EEOSAuthTranslationFlags::None } },
		{ LoginCredentialsType::Developer, { EOS_ELoginCredentialType::EOS_LCT_Developer, EEOSAuthTranslationFlags::SetId | EEOSAuthTranslationFlags::SetTokenFromString } },
		{ LoginCredentialsType::RefreshToken, { EOS_ELoginCredentialType::EOS_LCT_RefreshToken, EEOSAuthTranslationFlags::SetTokenFromString } },
		{ LoginCredentialsType::AccountPortal, { EOS_ELoginCredentialType::EOS_LCT_AccountPortal, EEOSAuthTranslationFlags::SetId | EEOSAuthTranslationFlags::SetTokenFromString } },
		{ LoginCredentialsType::ExternalAuth, { EOS_ELoginCredentialType::EOS_LCT_ExternalAuth, EEOSAuthTranslationFlags::SetTokenFromExternalAuth } },
	};

	return SupportedLoginTranslatorTraits.Find(Name);
}

const FEOSExternalAuthTranslationTraits* FEOSAuthLoginOptionsCommon::GetExternalAuthTranslationTraits(FName ExternalAuthType)
{
	static const TMap<FName, FEOSExternalAuthTranslationTraits> SupportedExternalAuthTraits = {
		{ ExternalLoginType::Epic, { EOS_EExternalCredentialType::EOS_ECT_EPIC } },
		{ ExternalLoginType::SteamSessionTicket, { EOS_EExternalCredentialType::EOS_ECT_STEAM_SESSION_TICKET } },
		{ ExternalLoginType::PsnIdToken, { EOS_EExternalCredentialType::EOS_ECT_PSN_ID_TOKEN } },
		{ ExternalLoginType::XblXstsToken, { EOS_EExternalCredentialType::EOS_ECT_XBL_XSTS_TOKEN } },
		{ ExternalLoginType::DiscordAccessToken, { EOS_EExternalCredentialType::EOS_ECT_DISCORD_ACCESS_TOKEN } },
		{ ExternalLoginType::GogSessionTicket, { EOS_EExternalCredentialType::EOS_ECT_GOG_SESSION_TICKET } },
		{ ExternalLoginType::NintendoIdToken, { EOS_EExternalCredentialType::EOS_ECT_NINTENDO_ID_TOKEN } },
		{ ExternalLoginType::NintendoNsaIdToken, { EOS_EExternalCredentialType::EOS_ECT_NINTENDO_NSA_ID_TOKEN } },
		{ ExternalLoginType::UplayAccessToken, { EOS_EExternalCredentialType::EOS_ECT_UPLAY_ACCESS_TOKEN } },
		{ ExternalLoginType::OpenIdAccessToken, { EOS_EExternalCredentialType::EOS_ECT_OPENID_ACCESS_TOKEN } },
		{ ExternalLoginType::DeviceIdAccessToken, { EOS_EExternalCredentialType::EOS_ECT_DEVICEID_ACCESS_TOKEN } },
		{ ExternalLoginType::AppleIdToken, { EOS_EExternalCredentialType::EOS_ECT_APPLE_ID_TOKEN } },
		{ ExternalLoginType::GoogleIdToken, { EOS_EExternalCredentialType::EOS_ECT_GOOGLE_ID_TOKEN } },
		{ ExternalLoginType::OculusUserIdNonce, { EOS_EExternalCredentialType::EOS_ECT_OCULUS_USERID_NONCE } },
		{ ExternalLoginType::ItchioJwt, { EOS_EExternalCredentialType::EOS_ECT_ITCHIO_JWT } },
		{ ExternalLoginType::ItchioKey, { EOS_EExternalCredentialType::EOS_ECT_ITCHIO_KEY } },
		{ ExternalLoginType::EpicIdToken, { EOS_EExternalCredentialType::EOS_ECT_EPIC_ID_TOKEN } },
		{ ExternalLoginType::AmazonAccessToken, { EOS_EExternalCredentialType::EOS_ECT_AMAZON_ACCESS_TOKEN } },
	};

	return SupportedExternalAuthTraits.Find(ExternalAuthType);
}

FEOSAuthLoginOptionsCommon::FEOSAuthLoginOptionsCommon(FEOSAuthLoginOptionsCommon&& Other)
{
	*this = MoveTemp(Other);
}

FEOSAuthLoginOptionsCommon& FEOSAuthLoginOptionsCommon::operator=(FEOSAuthLoginOptionsCommon&& Other)
{
	CredentialsData = Other.CredentialsData;

	// Pointer fixup.
	if (CredentialsData.Id)
	{
		IdUtf8 = MoveTemp(Other.IdUtf8);
		CredentialsData.Id = IdUtf8.GetData();
	}
	if (CredentialsData.Token)
	{
		TokenUtf8 = MoveTemp(Other.TokenUtf8);
		CredentialsData.Token = TokenUtf8.GetData();
	}
	if (CredentialsData.SystemAuthCredentialsOptions)
	{
		// todo
	}

	Credentials = &CredentialsData;
	ApiVersion = Other.ApiVersion;
	ScopeFlags = Other.ScopeFlags;

	Other.Credentials = nullptr;
	return *this;
}

FEOSAuthLoginOptionsCommon::FEOSAuthLoginOptionsCommon()
{
	// EOS_Auth_LoginOptions init
	UE_EOS_CHECK_API_MISMATCH(EOS_AUTH_LOGIN_API_LATEST, 3);
	ApiVersion = 2;
	Credentials = &CredentialsData;
	ScopeFlags = EOS_EAuthScopeFlags::EOS_AS_NoFlags;
	LoginFlags = 0;

	// EOS_Auth_Credentials init
	UE_EOS_CHECK_API_MISMATCH(EOS_AUTH_CREDENTIALS_API_LATEST, 4);
	CredentialsData.ApiVersion = 4;
	CredentialsData.Id = nullptr;
	CredentialsData.Token = nullptr;
	CredentialsData.Type = EOS_ELoginCredentialType::EOS_LCT_Password;
	CredentialsData.SystemAuthCredentialsOptions = nullptr;
	CredentialsData.ExternalType = EOS_EExternalCredentialType::EOS_ECT_EPIC;
}
}