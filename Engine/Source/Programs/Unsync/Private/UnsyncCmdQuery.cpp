// Copyright Epic Games, Inc. All Rights Reserved.

#include "UnsyncCmdQuery.h"
#include "UnsyncFile.h"
#include "UnsyncHttp.h"
#include "UnsyncProxy.h"
#include "UnsyncThread.h"
#include "UnsyncUtil.h"
#include "UnsyncAuth.h"

#include <float.h>
#include <algorithm>
#include <json11.hpp>

#include <fmt/format.h>

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

	ParallelForEach(Mirrors, [](FMirrorInfo& Mirror) { Mirror.Ping = RunHttpPing(Mirror.Address, Mirror.Port); });

	std::sort(Mirrors.begin(), Mirrors.end(), [](const FMirrorInfo& InA, const FMirrorInfo& InB) {
		double A = InA.Ping > 0 ? InA.Ping : FLT_MAX;
		double B = InB.Ping > 0 ? InB.Ping : FLT_MAX;
		return A < B;
	});

	LogPrintf(ELogLevel::MachineReadable, L"[\n");

	for (size_t I = 0; I < Mirrors.size(); ++I)
	{
		const FMirrorInfo& Mirror = Mirrors[I];

		LogPrintf(ELogLevel::MachineReadable,
				  L"  {\"address\":\"%hs\", \"port\":%d, \"ok\":%hs, \"ping\":%d, \"name\":\"%hs\"}%hs\n",
				  Mirror.Address.c_str(),
				  Mirror.Port,
				  Mirror.Ping > 0 ? "true" : "false",
				  int32(Mirror.Ping * 1000.0),
				  Mirror.Name.c_str(),
				  I + 1 == Mirrors.size() ? "" : ",");
	}

	LogPrintf(ELogLevel::MachineReadable, L"]\n");

	return 0;
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

	LogPrintf(ELogLevel::MachineReadable, L"%hs\n", Response.Buffer.Data());

	return 0;
}

int32
CmdQueryFile(const FCmdQueryOptions& Options)
{
	UNSYNC_LOG(L"Downloading file: '%hs'", Options.Args.c_str());
	UNSYNC_LOG_INDENT;

	std::unique_ptr<FNativeFile> ResultWriter;

	auto OutputCallback = [&ResultWriter, &Options](uint64 Size) -> FIOWriter& {
		UNSYNC_LOG(L"Size: %llu bytes (%.3f MB)", llu(Size), SizeMb(Size));
		ResultWriter = std::make_unique<FNativeFile>(Options.OutputPath, EFileMode::CreateWriteOnly, Size);
		return *ResultWriter;
	};

	TResult<> Response = ProxyQuery::DownloadFile(Options.Remote, Options.Args, OutputCallback);

	if (Response.IsOk())
	{
		UNSYNC_LOG(L"Output written to file '%s'", Options.OutputPath.wstring().c_str());
	}
	else
	{
		LogError(Response.GetError());
	}

	return 0;
}

int32
CmdQuery(const FCmdQueryOptions& Options)
{
	if (Options.Query == "mirrors")
	{
		return CmdQueryMirrors(Options);
	}
	else if (Options.Query == "list")
	{
		return CmdQueryList(Options);
	}
	else if (Options.Query == "file")
	{
		return CmdQueryFile(Options);
	}
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
	ParallelForEach(Mirrors, [](FMirrorInfo& Mirror) { Mirror.Ping = RunHttpPing(Mirror.Address, Mirror.Port); });

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
