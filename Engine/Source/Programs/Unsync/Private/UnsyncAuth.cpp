// Copyright Epic Games, Inc. All Rights Reserved.

#include "UnsyncAuth.h"
#include "UnsyncFile.h"
#include "UnsyncHttp.h"
#include "UnsyncLog.h"
#include "UnsyncProxy.h"

#include <fmt/format.h>
#include <json11.hpp>

#if UNSYNC_USE_TLS

#	include <openssl/err.h>
#	include <openssl/rand.h>
#	include <openssl/sha.h>
#	include <openssl/evp.h>  // Base64 encoding

#endif	// UNSYNC_USE_TLS

#if UNSYNC_USE_TLS
namespace unsync {

std::string
SecureRandomBytesAsHexString(uint32 NumBytes)
{
	static constexpr int NumStackBytes				 = 64;
	unsigned char		 StackStorage[NumStackBytes] = {};
	FBuffer				 DynamicStorage;

	unsigned char* RandomState = nullptr;

	if (NumBytes <= NumStackBytes)
	{
		RandomState = StackStorage;
	}
	else
	{
		DynamicStorage.Resize(NumBytes);
		RandomState = DynamicStorage.Data();
	}

	int RandResult = RAND_bytes(RandomState, NumBytes);
	if (RandResult != 1)
	{
		int ErrorCode = ERR_get_error();
		UNSYNC_FATAL(L"Failed to generate secure random number. Error code: %d", ErrorCode);
	}

	return BytesToHexString(RandomState, NumBytes);
}

FHash256
HashSha256Bytes(const uint8* Data, uint64 Size)
{
	FHash256 Result = {};
	static_assert(sizeof(Result.Data) == SHA256_DIGEST_LENGTH, "Unexpected SHA256 output buffer size");

	SHA256_CTX ShaCtx = {};

	UNSYNC_ASSERT(SHA256_Init(&ShaCtx) == 1);
	UNSYNC_ASSERT(SHA256_Update(&ShaCtx, Data, Size) == 1);
	UNSYNC_ASSERT(SHA256_Final(Result.Data, &ShaCtx) == 1);

	return Result;
}

std::string
EncodeBase64(const uint8* Data, uint64 Size)
{
	UNSYNC_ASSERT(Size <= std::numeric_limits<int32>::max());

	std::string Result;

	const uint64 ExpectedResultLength = ((Size + 2) / 3) * 4;

	Result.resize(ExpectedResultLength);

	int NumEncodedBytes = EVP_EncodeBlock((unsigned char*)Result.data(), (const unsigned char*)Data, (int)Size);
	UNSYNC_ASSERT(NumEncodedBytes == ExpectedResultLength)

	return Result;
}

bool
DecodeBase64(std::string_view Base64Data, FBuffer& Output)
{
	const uint64 ExpectedResultLength = 3 * Base64Data.length() / 4;
	UNSYNC_ASSERT(ExpectedResultLength <= std::numeric_limits<int32>::max());

	Output.Resize(ExpectedResultLength);  // Conservative size, since EVP_DecodeBlock fills padding with 0

	int NumDecodedBytes = EVP_DecodeBlock((unsigned char*)Output.Data(), (const unsigned char*)Base64Data.data(), (int)Base64Data.length());

	return NumDecodedBytes == ExpectedResultLength;
}

void
TransformBase64VanillaToUrlSafe(std::string& Data)
{
	std::replace(Data.begin(), Data.end(), '+', '-');
	std::replace(Data.begin(), Data.end(), '/', '_');

	while (Data.ends_with('='))
	{
		Data.pop_back();
	}
}

void
TransformBase64UrlSafeToVanilla(std::string& Data)
{
	std::replace(Data.begin(), Data.end(), '-', '+');
	std::replace(Data.begin(), Data.end(), '_', '/');
}

std::string
GetPKCECodeChallenge(std::string_view CodeVerifier)
{
	FHash256	CodeVerifierHash = HashSha256Bytes((const uint8*)CodeVerifier.data(), CodeVerifier.size());
	std::string Result			 = EncodeBase64(CodeVerifierHash.Data, CodeVerifierHash.Size());

	TransformBase64VanillaToUrlSafe(Result);

	return Result;
}

static const char HttpCallbackResponseOk[] = R"(HTTP/1.1 200 OK

<!DOCTYPE html>
<html>
<body>
<center>
<h1 style="background-color:#75dd55">Success!</h1>
<p>Unsync is now authorized. You may close this page.</p>
</center>
</body>
</html>
)";

static const char HttpCallbackResponseError[] = R"(HTTP/1.1 400 Bad Request

<!DOCTYPE html>
<html>
<body>
<center>
<h1 style="background-color:#dd5555">Authorization failed!</h1>
<p>See unsync logs for details. You may close this page.</p>
</center>
</body>
</html>
)";

struct FHttpCallbackData
{
	std::string AuthCode;
	std::string State;
};

std::thread
StartHttpCallbackServer(FSocketHandle	   CallbackListenSocket,
						std::string_view   ExpectedPath,
						std::string_view   RandomState,
						FHttpCallbackData& HttpCallbackData)
{
	return std::thread([CallbackListenSocket, ExpectedPath, RandomState, &HttpCallbackData]() {
		FSocketHandle CallbackSocket = SocketAccept(CallbackListenSocket);

		static const size_t MaxRecvSize = 65536;
		char				RecvBuffer[MaxRecvSize];

		int32 ReceivedBytes = SocketRecvAny(CallbackSocket, RecvBuffer, MaxRecvSize);

		UNSYNC_VERBOSE2(L"HTTP Callback:\n%.*hs", ReceivedBytes, RecvBuffer);

		std::string_view RequestStr(RecvBuffer, ReceivedBytes);

		std::string ExpectedCallbackPrefix = std::format("GET /{}", ExpectedPath);

		if (RequestStr.starts_with(ExpectedCallbackPrefix))
		{
			// Trim request string, removing HTTP headers
			{
				size_t RequestEndPos = RequestStr.find("\n");
				if (RequestEndPos != std::string::npos)
				{
					RequestStr = RequestStr.substr(0, RequestEndPos);
				}
			}

			auto ExtractValue = [](std::string_view RequestStr, std::string_view Key) -> std::string_view {
				size_t			 Pos	= RequestStr.find(Key);
				std::string_view Result = {};
				if (Pos != std::string::npos)
				{
					Result = RequestStr.substr(Pos + Key.length());
					Result = Result.substr(0, Result.find_first_of("& \n"));
				}
				return Result;
			};

			HttpCallbackData.AuthCode = ExtractValue(RequestStr, "code=");
			HttpCallbackData.State	  = ExtractValue(RequestStr, "state=");

			if (HttpCallbackData.State == RandomState && !HttpCallbackData.AuthCode.empty())
			{
				SocketSend(CallbackSocket, HttpCallbackResponseOk, strlen(HttpCallbackResponseOk));
			}
			else
			{
				// TODO: could report more detailed error to the browser, but probably just the log file is sufficient
				SocketSend(CallbackSocket, HttpCallbackResponseError, strlen(HttpCallbackResponseError));
			}
		}
		else
		{
			const char ResponseNotFound[] = "HTTP/1.1 404 Not Found";
			SocketSend(CallbackSocket, ResponseNotFound, strlen(ResponseNotFound));
		}

		SocketClose(CallbackSocket);
	});
};

TResult<FAuthToken>
AcquireAuthToken(const FAuthDesc& AuthDesc)
{
	if (AuthDesc.AuthorizationEndpoint.empty())
	{
		return AppError(L"Authorization endpoint is required");
	}

	if (AuthDesc.TokenEndpoint.empty())
	{
		return AppError(L"Token endpoint is required");
	}

	if (AuthDesc.Callback.empty())
	{
		return AppError(L"Callback URI is required");
	}

	TResult<FRemoteDesc> CallbackServerDescResult = FRemoteDesc::FromUrl(AuthDesc.Callback);
	if (CallbackServerDescResult.IsError())
	{
		return AppError(L"Failed to parse callback URI");
	}

	const FRemoteDesc& CallbackServerDesc = CallbackServerDescResult.GetData();

	FAuthToken Result;

	FHttpConnection AuthServerConnection = FHttpConnection::CreateDefaultHttps(AuthDesc.ServerHost);

	const uint16 CallbackPortNumber = CallbackServerDesc.HostPort;

	FSocketHandle CallbackListenSocket = SocketListenTcp("127.0.0.1", CallbackPortNumber);

	std::string RandomState	  = SecureRandomBytesAsHexString(16);
	std::string NonceStr	  = SecureRandomBytesAsHexString(16);
	std::string CodeVerifier  = SecureRandomBytesAsHexString(64);
	std::string CodeChallenge = GetPKCECodeChallenge(CodeVerifier);
	std::string CallbackUrl	  = AuthDesc.Callback;

	std::string AudienceParam;
	if (!AuthDesc.Audience.empty())
	{
		AudienceParam = fmt::format("audience={}&", AuthDesc.Audience);
	}

	std::string AuthorizeUrl = fmt::format(
		"https://{}{}?"
		"client_id={}&"
		"{}"  // optional audience parameter
		"response_type=code&"
		"scope=offline_access%20profile%20openid%20email&"
		"code_challenge_method=S256&"
		"code_challenge={}&"
		"state={}&"
		"redirect_uri={}",
		AuthDesc.ServerHost,
		AuthDesc.AuthorizationEndpoint,
		AuthDesc.ClientId,
		AudienceParam,
		CodeChallenge,
		RandomState,
		CallbackUrl);

	FHttpCallbackData HttpCallbackData;

	std::thread ServerThread = StartHttpCallbackServer(CallbackListenSocket, CallbackServerDesc.RequestPath, RandomState, HttpCallbackData);

	OpenUrlInDefaultBrowser(AuthorizeUrl.c_str());

	UNSYNC_LOG(L"Waiting for HTTP callback on port %d...", int(CallbackPortNumber));

	ServerThread.join();
	SocketClose(CallbackListenSocket);

	if (RandomState != HttpCallbackData.State)
	{
		return AppError(L"Callback state value mismatch");
	}

	if (HttpCallbackData.AuthCode.empty())
	{
		return AppError(L"Did not receive authorization code callback");
	}

	std::string AccessToken;
	std::string RefreshToken;
	std::string IdToken;
	std::string TokenType;
	int64		ExpiresInSeconds = 0;

	// TODO: only try to acquire new token if close to expiry

	// Use authorization code to acquire tokens
	{
		std::string TokenPayload = fmt::format(
			"grant_type=authorization_code&"
			"client_id={}&"
			"code={}&"
			"code_verifier={}&"
			"redirect_uri={}",
			AuthDesc.ClientId,
			HttpCallbackData.AuthCode,
			CodeVerifier,
			CallbackUrl);

		FHttpRequest Request;
		Request.Url				   = AuthDesc.TokenEndpoint;
		Request.Method			   = EHttpMethod::POST;
		Request.PayloadContentType = EHttpContentType::Application_WWWFormUrlEncoded;
		Request.Payload			   = FBufferView{(const uint8*)TokenPayload.data(), (uint64)TokenPayload.size()};

		FHttpResponse Response = HttpRequest(AuthServerConnection, Request);

		if (Response.Success())
		{
			using namespace json11;
			std::string JsonString = std::string(Response.AsStringView());

			std::string JsonErrorString;
			Json		JsonObject = Json::parse(JsonString, JsonErrorString);

			if (!JsonErrorString.empty())
			{
				return AppError(fmt::format("JSON error while parsing token: {}", JsonErrorString.c_str()));
			}

			AccessToken		 = JsonObject["access_token"].string_value();
			RefreshToken	 = JsonObject["refresh_token"].string_value();
			IdToken			 = JsonObject["id_token"].string_value();
			TokenType		 = JsonObject["token_type"].string_value();
			ExpiresInSeconds = int64(JsonObject["expires_in"].number_value());

			Result.Raw = JsonString;
		}
		else
		{
			return HttpError(L"Could not acquire authorization code", Response.Code);
		}
	}

	if (AccessToken.empty())
	{
		return AppError(L"Did not receive new access token");
	}

	Result.Access  = std::move(AccessToken);
	Result.Refresh = std::move(RefreshToken);

	return ResultOk(std::move(Result));
}

TResult<FAuthUserInfo>
GetUserInfo(FHttpConnection& HttpConnection, const FAuthDesc& AuthDesc, const FAuthToken& AuthToken)
{
	std::string AuthHeader = fmt::format("Authorization: Bearer {}", AuthToken.Access);

	if (AuthDesc.UserInfoEndpoint.empty())
	{
		return AppError(L"User info endpoint is unknown");
	}

	FHttpRequest Request;
	Request.Url			  = AuthDesc.UserInfoEndpoint;
	Request.Method		  = EHttpMethod::GET;
	Request.CustomHeaders = AuthHeader;

	FHttpResponse Response = HttpRequest(HttpConnection, Request);

	if (Response.Success())
	{
		using namespace json11;

		std::string JsonString = std::string(Response.AsStringView());

		std::string JsonErrorString;
		Json		JsonObject = Json::parse(JsonString, JsonErrorString);

		if (!JsonErrorString.empty())
		{
			return AppError(fmt::format("JSON error while parsing user info: {}", JsonErrorString.c_str()));
		}

		FAuthUserInfo Result;

		Result.Sub		  = JsonObject["sub"].string_value();
		Result.Name		  = JsonObject["name"].string_value();
		Result.Nickname	  = JsonObject["nickname"].string_value();
		Result.GivenName  = JsonObject["given_name"].string_value();
		Result.FamilyName = JsonObject["family_name"].string_value();
		Result.Email	  = JsonObject["email"].string_value();

		return ResultOk(Result);
	}
	else
	{
		return HttpError(L"Could not query user info from authorization server", Response.Code);
	}
}

TResult<FAuthToken>
RefreshAuthToken(const FAuthDesc& AuthDesc, const FAuthToken& PreviousToken)
{
	if (AuthDesc.TokenEndpoint.empty())
	{
		return AppError(L"Token endpoint is unknown");
	}

	FAuthToken Result = PreviousToken;

	FHttpConnection AuthServerConnection = FHttpConnection::CreateDefaultHttps(AuthDesc.ServerHost);

	std::string AccessToken;
	std::string RefreshToken;
	std::string IdToken;
	std::string TokenType;
	int64		ExpiresInSeconds = 0;

	// Use refresh token to acquire new tokens
	{
		std::string TokenPayload = fmt::format(
			"grant_type=refresh_token&"
			"client_id={}&"
			"refresh_token={}",
			AuthDesc.ClientId,
			PreviousToken.Refresh);

		FHttpRequest Request;
		Request.Url				   = AuthDesc.TokenEndpoint;
		Request.Method			   = EHttpMethod::POST;
		Request.PayloadContentType = EHttpContentType::Application_WWWFormUrlEncoded;
		Request.Payload			   = FBufferView{(const uint8*)TokenPayload.data(), (uint64)TokenPayload.size()};

		FHttpResponse Response = HttpRequest(AuthServerConnection, Request);

		if (Response.Success())
		{
			using namespace json11;
			std::string JsonString = std::string(Response.AsStringView());

			std::string JsonErrorString;
			Json		JsonObject = Json::parse(JsonString, JsonErrorString);

			if (!JsonErrorString.empty())
			{
				return AppError(fmt::format("JSON error while parsing token: {}", JsonErrorString.c_str()));
			}

			AccessToken		 = JsonObject["access_token"].string_value();
			RefreshToken	 = JsonObject["refresh_token"].string_value();
			IdToken			 = JsonObject["id_token"].string_value();
			TokenType		 = JsonObject["token_type"].string_value();
			ExpiresInSeconds = int64(JsonObject["expires_in"].number_value());

			Result.Raw = JsonString;
		}
		else
		{
			return HttpError(L"Could not acquire authorization code", Response.Code);
		}
	}

	if (AccessToken.empty())
	{
		return AppError(L"Did not receive new access token");
	}

	Result.Access = AccessToken;

	if (!RefreshToken.empty())
	{
		Result.Refresh = RefreshToken;
	}

	return ResultOk(Result);
}

std::string
GenerateTokenId(const FRemoteDesc& RemoteDesc)
{
	FHash128 Hash = HashBlake3String<FHash128>(RemoteDesc.HostAddress);
	return HashToHexString(Hash);
}

bool
SaveRefreshToken(const FPath& Path, const FAuthToken& AuthToken)
{
	return WriteBufferToFile(Path, (const uint8*)AuthToken.Refresh.data(), AuthToken.Refresh.length());
}

TResult<FAuthToken>
LoadRefreshToken(const FPath& Path)
{
	FBuffer FileBuffer = ReadFileToBuffer(Path);
	if (FileBuffer.Size())
	{
		FAuthToken AuthToken;
		AuthToken.Refresh.append((const char*)FileBuffer.Data(), FileBuffer.Size());
		return ResultOk(AuthToken);
	}
	else
	{
		return AppError(L"Failed to load refresh token from file");
	}
}

TResult<FAuthToken>
RefreshOrAcquireToken(const FAuthDesc& AuthDesc, const FAuthToken& PreviousToken)
{
	if (!PreviousToken.Refresh.empty())
	{
		UNSYNC_VERBOSE(L"Refreshing access token");
		TResult<FAuthToken> RefreshResult = RefreshAuthToken(AuthDesc, PreviousToken);
		if (RefreshResult.IsOk())
		{
			return RefreshResult;
		}
	}

	UNSYNC_VERBOSE(L"Requesting new access token");
	return AcquireAuthToken(AuthDesc);
}

TResult<FAuthToken>
Authenticate(const FRemoteDesc& RemoteDesc, const FAuthDesc& AuthDesc)
{
	FPath UserHomePath = GetUserHomeDirectory();

	std::string TokenId = GenerateTokenId(RemoteDesc);

	FPath UnsyncSettingsPath = UserHomePath / FPath(".unsync");
	FPath TokenCachePath	 = UnsyncSettingsPath / FPath(TokenId);

	FAuthToken PreviousToken;

	if (PathExists(TokenCachePath))
	{
		TResult<FAuthToken> LoadResult = LoadRefreshToken(TokenCachePath);
		if (FAuthToken* LoadedToken = LoadResult.TryData())
		{
			UNSYNC_VERBOSE(L"Loaded cached authentication data");
			PreviousToken = std::move(*LoadedToken);
		}
	}

	TResult<FAuthToken> FreshTokenResult = RefreshOrAcquireToken(AuthDesc, PreviousToken);
	if (FreshTokenResult.IsError())
	{
		return FreshTokenResult;
	}

	if (!UserHomePath.empty())
	{
		CreateDirectories(UnsyncSettingsPath);

		// Allow saving tokens during dry run
		// TODO: need a dedicated file flag to allow writes during dry run

		const bool bPrevDryRun = GDryRun;
		GDryRun				   = false;

		bool bSaved = SaveRefreshToken(TokenCachePath, FreshTokenResult.GetData());

		GDryRun = bPrevDryRun;

		if (bSaved)
		{
			UNSYNC_VERBOSE2(L"Saved refresh token to file: %s", TokenCachePath.wstring().c_str());
		}
	}

	return FreshTokenResult;
}

bool
TryAddAuthentication(FRemoteDesc& InOutRemoteDesc)
{
	TResult<FAuthDesc> AuthDescResult = GetAuthenticationDesc(InOutRemoteDesc);
	if (AuthDescResult.IsError())
	{
		return false;
	}

	const FAuthDesc& AuthDesc = AuthDescResult.GetData();

	TResult<FAuthToken> AuthTokenResult = Authenticate(InOutRemoteDesc, AuthDesc);
	if (AuthTokenResult.IsError())
	{
		return false;
	}

	const std::string& AccessToken = AuthTokenResult->Access;

	// Authentication requires encrypted connection
	InOutRemoteDesc.bTlsEnable	   = true;
	InOutRemoteDesc.Authentication = std::make_shared<FBuffer>();
	InOutRemoteDesc.Authentication->Append((const uint8*)AccessToken.data(), AccessToken.length());

	return true;
}

TResult<FAuthDesc>
GetAuthenticationDesc(const FRemoteDesc& RemoteDesc)
{
	// TODO: possibly other backend could use automatic authentication also
	if (RemoteDesc.Protocol != EProtocolFlavor::Unsync)
	{
		return AppError(L"Authentication is only implemented for UNSYNC protocol");
	}

	TResult<ProxyQuery::FHelloResponse> HelloResponseResult = ProxyQuery::Hello(RemoteDesc);
	if (HelloResponseResult.IsError())
	{
		return MoveError<FAuthDesc>(HelloResponseResult);
	}

	if (!HelloResponseResult->SupportsAuthentication())
	{
		return AppError(L"Server does not support authentication");
	}

	FAuthDesc AuthDesc;
	AuthDesc.ClientId = HelloResponseResult->AuthClientId;
	AuthDesc.Audience = HelloResponseResult->AuthAudience;
	AuthDesc.Callback = HelloResponseResult->CallbackUri;
	if (AuthDesc.Callback.empty())
	{
		AuthDesc.Callback = "http://localhost:8080";  // sensible default
	}

	TResult<FRemoteDesc> AuthServerDescResult = FRemoteDesc::FromUrl(HelloResponseResult->AuthServerUri);
	if (AuthServerDescResult.IsError())
	{
		return MoveError<FAuthDesc>(AuthServerDescResult);
	}

	const FRemoteDesc& AuthServerDesc = AuthServerDescResult.GetData();

	AuthDesc.ServerHost = AuthServerDesc.HostAddress;

	std::string ServerApiPrefix;
	if (!AuthServerDesc.RequestPath.empty())
	{
		ServerApiPrefix = fmt::format("/{}", AuthServerDesc.RequestPath);
	}

	if (!AuthDesc.IsValid())
	{
		return AppError(L"Mandatory authentication parameters not found");
	}

	FHttpConnection AuthServerConnection = FHttpConnection::CreateDefaultHttps(AuthDesc.ServerHost);
	std::string		ConfigEndpoint		 = fmt::format("{}/.well-known/openid-configuration", ServerApiPrefix);

	FHttpResponse ConfigResponse = HttpRequest(AuthServerConnection, EHttpMethod::GET, ConfigEndpoint);
	if (ConfigResponse.Success())
	{
		using namespace json11;
		std::string JsonString = std::string(ConfigResponse.AsStringView());

		std::string JsonErrorString;
		Json		JsonObject = Json::parse(JsonString, JsonErrorString);

		std::string EndpointPrefix = fmt::format("https://{}", AuthDesc.ServerHost);

		if (JsonErrorString.empty())
		{
			auto ExtractEndpoint = [&EndpointPrefix, &JsonObject](const char* FieldName) -> std::string {
				if (auto& Field = JsonObject[FieldName]; Field.is_string())
				{
					const std::string& Value = Field.string_value();
					if (Value.starts_with(EndpointPrefix))
					{
						return Value.substr(EndpointPrefix.length());
					}
				}
				return {};
			};

			AuthDesc.AuthorizationEndpoint = ExtractEndpoint("authorization_endpoint");
			AuthDesc.TokenEndpoint		   = ExtractEndpoint("token_endpoint");
			AuthDesc.UserInfoEndpoint	   = ExtractEndpoint("userinfo_endpoint");
			AuthDesc.JwksUri			   = JsonObject["jwks_uri"].string_value();
		}
	}

	return ResultOk(AuthDesc);
}

}  // namespace unsync

#endif	// UNSYNC_USE_TLS
