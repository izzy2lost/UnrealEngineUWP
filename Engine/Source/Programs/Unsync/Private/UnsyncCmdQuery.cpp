// Copyright Epic Games, Inc. All Rights Reserved.

#include "UnsyncCmdQuery.h"
#include "UnsyncFile.h"
#include "UnsyncHttp.h"
#include "UnsyncProxy.h"
#include "UnsyncThread.h"
#include "UnsyncUtil.h"

#include <float.h>
#include <algorithm>
#include <json11.hpp>

#include <fmt/format.h>

#if UNSYNC_USE_TLS
#	include "UnsyncAuth.h"
#endif	// UNSYNC_USE_TLS

namespace unsync {

using FMirrorInfoResult = TResult<std::vector<FMirrorInfo>>;

// Runs a basic HTTP request against the remote server and returns the time it took to get the response, -1 if connection could not be
// established.
static double
RunHttpPing(std::string_view Address, uint16 Port)
{
	FTimePoint		TimeBegin = TimePointNow();
	FHttpConnection Connection(Address, Port);
	FHttpRequest	Request;
	Request.Url				   = "/api/v1/ping";
	Request.Method			   = EHttpMethod::GET;
	FHttpResponse PingResponse = HttpRequest(Connection, Request);
	FTimePoint	  TimeEnd	   = TimePointNow();

	if (PingResponse.Success())
	{
		return DurationSec(TimeBegin, TimeEnd);
	}
	else
	{
		return -1;
	}
}

FMirrorInfoResult
RunQueryMirrors(const FRemoteDesc& RemoteDesc)
{
	const char*	  Url	   = "/api/v1/mirrors";
	FHttpResponse Response = HttpRequest(RemoteDesc, EHttpMethod::GET, Url);

	if (!Response.Success())
	{
		return HttpError(Url, Response.Code);
	}

	using namespace json11;
	std::string JsonString = std::string(Response.AsStringView());

	std::string JsonErrorString;
	Json		JsonObject = Json::parse(JsonString, JsonErrorString);

	if (!JsonErrorString.empty())
	{
		return AppError(std::string("JSON parse error while getting server mirrors: ") + JsonErrorString);
	}

	std::vector<FMirrorInfo> Result;

	for (const auto& Elem : JsonObject.array_items())
	{
		FMirrorInfo Info;
		for (const auto& Field : Elem.object_items())
		{
			if (Field.first == "name")
			{
				Info.Name = Field.second.string_value();
			}
			else if (Field.first == "address")
			{
				Info.Address = Field.second.string_value();
			}
			else if (Field.first == "port")
			{
				int PortValue = Field.second.int_value();
				if (PortValue > 0 && PortValue < 65536)
				{
					Info.Port = uint16(PortValue);
				}
				else
				{
					UNSYNC_WARNING(L"Unexpected port value: %d", PortValue);
					Info.Port = 0;
				}
			}
		}
		Result.push_back(Info);
	}

	return ResultOk(Result);
}

int32
CmdQueryMirrors(const FCmdQueryOptions& Options)
{
	FMirrorInfoResult MirrorsResult = RunQueryMirrors(Options.Remote);
	if (MirrorsResult.IsError())
	{
		LogError(MirrorsResult.GetError());
		return 1;
	}

	std::vector<FMirrorInfo> Mirrors = MirrorsResult.GetData();

	ParallelForEach(Mirrors.begin(), Mirrors.end(), [](FMirrorInfo& Mirror) { Mirror.Ping = RunHttpPing(Mirror.Address, Mirror.Port); });

	std::sort(Mirrors.begin(), Mirrors.end(), [](const FMirrorInfo& InA, const FMirrorInfo& InB) {
		double A = InA.Ping > 0 ? InA.Ping : FLT_MAX;
		double B = InB.Ping > 0 ? InB.Ping : FLT_MAX;
		return A < B;
	});

	LogPrintf(ELogLevel::Info, L"[\n");

	for (size_t I = 0; I < Mirrors.size(); ++I)
	{
		const FMirrorInfo& Mirror = Mirrors[I];

		LogPrintf(ELogLevel::Info,
				  L"  {\"address\":\"%hs\", \"port\":%d, \"ok\":%hs, \"ping\":%d, \"name\":\"%hs\"}%hs\n",
				  Mirror.Address.c_str(),
				  Mirror.Port,
				  Mirror.Ping > 0 ? "true" : "false",
				  int32(Mirror.Ping * 1000.0),
				  Mirror.Name.c_str(),
				  I + 1 == Mirrors.size() ? "" : ",");
	}

	LogPrintf(ELogLevel::Info, L"]\n");

	return 0;
}

#if UNSYNC_USE_TLS
int32
CmdQueryLogin(const FCmdQueryOptions& Options)
{
	TResult<FAuthToken> AuthToken = Authenticate(Options.Remote, 5 * 60);

	if (AuthToken.IsOk())
	{
		{
			FHttpConnection Connection = FHttpConnection::CreateDefaultHttps(Options.Remote);

			FHttpRequest Request;
			Request.Url			   = "/api/v1/login";
			Request.Method		   = EHttpMethod::GET;
			Request.BearerToken	   = AuthToken->Access;
			FHttpResponse Response = HttpRequest(Connection, Request);

			if (Response.Success())
			{
				UNSYNC_LOG("Login successful");
			}
			else
			{
				LogError(HttpError(Response.Code));
				return -1;
			}
		}

		UNSYNC_LOG(L"Getting user info from authentication server");
		UNSYNC_LOG_INDENT;

		TResult<FAuthDesc> AuthDescResult = GetAuthenticationDesc(Options.Remote);
		if (AuthDescResult.IsError())
		{
			LogError(AuthDescResult.GetError());
			return -1;
		}

		const FAuthDesc& AuthDesc = AuthDescResult.GetData();

		FHttpConnection AuthConnection = FHttpConnection::CreateDefaultHttps(AuthDesc.ServerHost);

		TResult<FAuthUserInfo> UserInfoResult = GetUserInfo(AuthConnection, AuthDesc, AuthToken.GetData());

		if (const FAuthUserInfo* UserInfo = UserInfoResult.TryData())
		{
			if (UserInfo->Email.length() && UserInfo->Name.length())
			{
				UNSYNC_VERBOSE(L"Authenticated user: %hs (%hs)", UserInfo->Name.c_str(), UserInfo->Email.c_str());
			}
			else if (UserInfo->Name.length())
			{
				UNSYNC_VERBOSE(L"Authenticated user: %hs", UserInfo->Name.c_str());
			}
		}

		return 0;
	}
	else
	{
		LogError(AuthToken.GetError());
		return -1;
	}
}

int32
CmdQueryList(const FCmdQueryOptions& Options)
{
	TResult<FAuthToken> AuthToken = Authenticate(Options.Remote, 5 * 60);
	if (!AuthToken.IsOk())
	{
		LogError(AuthToken.GetError());
		return -1;
	}

	FHttpConnection Connection = FHttpConnection::CreateDefaultHttps(Options.Remote);

	std::string Url = fmt::format("/api/v1/list?{}", Options.Args);

	FHttpRequest Request;
	Request.Url			   = Url;
	Request.Method		   = EHttpMethod::GET;
	Request.BearerToken	   = AuthToken->Access;
	FHttpResponse Response = HttpRequest(Connection, Request);

	Response.Buffer.PushBack(0);

	std::string	 JsonErrorString;
	json11::Json JsonObject = json11::Json::parse((const char*)Response.Buffer.Data(), JsonErrorString);

	if (!JsonErrorString.empty())
	{
		LogError(AppError(fmt::format("JSON error: {}", JsonErrorString.c_str())));
		return -1;
	}
	/*
	if (JsonObject.is_array())
	{
		int32 NumFiles = int32(JsonObject.array_items().size());
		UNSYNC_LOG("Found files: %d", NumFiles);
		for (auto& FileIt : JsonObject.array_items())
		{
			if (auto Value = FileIt["path"]; Value.is_string())
			{
				UNSYNC_LOG(L" %hs", Value.string_value().c_str());
			}
		}
	}
	*/

	wprintf(L"%hs\n", Response.Buffer.Data());

	return 0;
}
#endif	// UNSYNC_USE_TLS

int32
CmdQuery(const FCmdQueryOptions& Options)
{
	if (Options.Query == "mirrors")
	{
		return CmdQueryMirrors(Options);
	}
#if UNSYNC_USE_TLS
	else if (Options.Query == "login")
	{
		return CmdQueryLogin(Options);
	}
	else if (Options.Query == "list")
	{
		return CmdQueryList(Options);
	}
#endif	// UNSYNC_USE_TLS
	else
	{
		UNSYNC_ERROR(L"Unknown query command");
		return 1;
	}
}

TResult<FMirrorInfo>
FindClosestMirror(const FRemoteDesc& Remote)
{
	FMirrorInfoResult MirrorsResult = RunQueryMirrors(Remote);
	if (MirrorsResult.IsError())
	{
		return FError(MirrorsResult.GetError());
	}

	std::vector<FMirrorInfo> Mirrors = MirrorsResult.GetData();
	ParallelForEach(Mirrors.begin(), Mirrors.end(), [](FMirrorInfo& Mirror) { Mirror.Ping = RunHttpPing(Mirror.Address, Mirror.Port); });

	std::sort(Mirrors.begin(), Mirrors.end(), [](const FMirrorInfo& InA, const FMirrorInfo& InB) {
		double A = InA.Ping > 0 ? InA.Ping : FLT_MAX;
		double B = InB.Ping > 0 ? InB.Ping : FLT_MAX;
		return A < B;
	});

	for (const FMirrorInfo& Mirror : Mirrors)
	{
		if (Mirror.Ping > 0)
		{
			return ResultOk(Mirror);
		}
	}

	return AppError("No reachable mirror found");
}

}  // namespace unsync
