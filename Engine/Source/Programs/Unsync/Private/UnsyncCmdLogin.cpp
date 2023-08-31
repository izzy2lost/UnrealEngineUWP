// Copyright Epic Games, Inc. All Rights Reserved.

#include "UnsyncCmdLogin.h"
#include "UnsyncAuth.h"
#include "UnsyncHttp.h"
#include "UnsyncLog.h"
#include "UnsyncProxy.h"

namespace unsync {

std::string_view
GetJwtPayload(std::string_view JwtData)
{
	size_t PayloadOffset = JwtData.find('.');
	if (PayloadOffset == std::string::npos)
	{
		return {};
	}
	PayloadOffset += 1;	 // skip the delimiter

	size_t SignatureOffset = JwtData.find('.', PayloadOffset + 1);
	if (SignatureOffset == std::string::npos)
	{
		return {};
	}
	SignatureOffset += 1;  // skip the delimiter

	size_t PayloadLength = SignatureOffset - PayloadOffset - 1;

	return JwtData.substr(PayloadOffset, PayloadLength);
}

int32
CmdLogin(const FCmdLoginOptions& Options)
{
	const int32 RefreshThreshold = Options.bForceRefresh ? INT_MAX : 5 * 60;

	TResult<FAuthToken> AuthTokenResult = Authenticate(Options.Remote, RefreshThreshold);

	if (AuthTokenResult.IsOk())
	{
		if (!Options.bQuick)
		{
			FHttpConnection Connection = FHttpConnection::CreateDefaultHttps(Options.Remote);

			FHttpRequest Request;
			Request.Url			   = "/api/v1/login";
			Request.Method		   = EHttpMethod::GET;
			Request.BearerToken	   = AuthTokenResult->Access;
			FHttpResponse Response = HttpRequest(Connection, Request);

			if (Response.Success())
			{
				UNSYNC_VERBOSE("Login successful");
			}
			else
			{
				LogError(HttpError(Response.Code));
				return -1;
			}
		}

		if (Options.bPrint)
		{
			if (Options.bDecode)
			{
				std::string JwtPayload = std::string(GetJwtPayload(AuthTokenResult->Access));
				TransformBase64UrlSafeToVanilla(JwtPayload);

				FBuffer DecodedTokenData;
				if (!DecodeBase64(JwtPayload, DecodedTokenData))
				{
					UNSYNC_ERROR("Failed to Base64-decode access token");
					return -1;
				}
				DecodedTokenData.Append(0);

				LogPrintf(ELogLevel::MachineReadable, L"%hs\n", (const char*)DecodedTokenData.Data());
			}
			else
			{
				LogPrintf(ELogLevel::MachineReadable, L"%hs\n", AuthTokenResult->Raw.c_str());
			}
		}

		return 0;
	}
	else
	{
		LogError(AuthTokenResult.GetError());
		return -1;
	}
}

}  // namespace unsync
