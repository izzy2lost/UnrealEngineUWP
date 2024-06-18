// Copyright Epic Games, Inc. All Rights Reserved.

#include "UnsyncHorde.h"
#include "UnsyncAuth.h"
#include "UnsyncCompression.h"
#include "UnsyncFile.h"

#include <regex>
#include <json11.hpp>

namespace unsync {

FHordeProtocolImpl::FHordeProtocolImpl(const FRemoteDesc& InRemoteDesc, const FBlockRequestMap* InRequestMap, FProxyPool& InProxyPool)
: FRemoteProtocolBase(InRemoteDesc, InRequestMap)
, ProxyPool(InProxyPool)
{
}

FDownloadResult FHordeProtocolImpl::Download(const TArrayView<FNeedBlock> NeedBlocks, const FBlockDownloadCallback& CompletionCallback)
{
	std::string RequestJson = FormatBlockRequestJson(*RequestMap, NeedBlocks);

	std::string RequestUrl = fmt::format("/{}/unsync-blobs", ProxyPool.RemoteDesc.RequestPath);
	//std::string RequestUrl = fmt::format("/{}/unsync-blobs?compress=false", ProxyPool.RemoteDesc.RequestPath);

	const EStrongHashAlgorithmID StrongHasher = RequestMap->GetStrongHasher();

	std::string BearerToken = ProxyPool.GetAccessToken();

	FPooledHttpConnection HttpConnection(ProxyPool);

	if (!HttpConnection.IsValid())
	{
		UNSYNC_ERROR(L"HTTP connection cannot be used");
		return FDownloadError(EDownloadRetryMode::Abort);
	}

	FHttpRequest Request;
	Request.Method			   = EHttpMethod::POST;
	Request.PayloadContentType = EHttpContentType::Application_Json;
	Request.Payload.Data	   = reinterpret_cast<const uint8*>(RequestJson.data());
	Request.Payload.Size	   = RequestJson.length();
	Request.BearerToken		   = BearerToken;
	Request.Url				   = RequestUrl;

	FHttpResponse Response = HttpRequest(HttpConnection, Request);

	if (!Response.Success())
	{
		UNSYNC_ERROR(L"Failed to complete block request. HTTP error code: %d.", Response.Code);
		return FDownloadError(EDownloadRetryMode::Abort);
	}

	//std::string_view TransferEncoding = Response.FindHeader("transfer-encoding");
	const std::string_view ChunkContentEncoding = Response.FindHeader("x-chunk-content-encoding");
	const std::string_view ContentType			= Response.FindHeader("content-type");

	if (!UncasedStringEquals(ContentType, "application/x-horde-unsync-blob"))
	{
		std::string Value = std::string(ContentType);
		UNSYNC_ERROR(L"Got unexpected blob content type header: '%hs'", Value.c_str());
		return FDownloadError(EDownloadRetryMode::Abort);
	}

	// TODO: read body stream as it arrives using HTTP chunk callbacks
	FMemReader BufferReader(Response.Buffer);
	FIOReaderStream Reader(BufferReader);

	while (Reader.RemainingSize())
	{
		FHordeUnsyncBlobHeaderV1 BlobHeader = {};
		Reader.ReadInto(BlobHeader.Magic);
		Reader.ReadInto(BlobHeader.PayloadSize);
		Reader.ReadInto(BlobHeader.DecompressedSize);
		Reader.ReadInto(BlobHeader.DecompressedHash);

		if (BlobHeader.Magic != FHordeUnsyncBlobHeaderV1::MAGIC)
		{
			UNSYNC_ERROR(L"Got unexpected blob header identifier");
			return FDownloadError(EDownloadRetryMode::Abort);
		}

		FDownloadedBlock DownloadedBlock;
		DownloadedBlock.bCompressed = false;

		FBufferView Payload = Response.Buffer.View(Reader.Tell(), BlobHeader.PayloadSize);
		FBuffer		DecompressedBuffer;

		if (ChunkContentEncoding == "zstd")
		{
			if (BlobHeader.DecompressedSize != 0)
			{
				DecompressedBuffer				 = Decompress(Payload.Data, Payload.Size);
				DownloadedBlock.Data			 = DecompressedBuffer.Data();
				DownloadedBlock.DecompressedSize = DecompressedBuffer.Size();
			}
			else
			{
				DownloadedBlock.Data			 = DecompressedBuffer.Data();
				DownloadedBlock.DecompressedSize = DecompressedBuffer.Size();
			}
		}
		else if (ChunkContentEncoding == "" || ChunkContentEncoding == "identity")
		{
			DownloadedBlock.Data			 = Payload.Data;
			DownloadedBlock.DecompressedSize = Payload.Size;

			if (BlobHeader.DecompressedSize != BlobHeader.PayloadSize)
			{
				UNSYNC_ERROR(L"Received blob size (%llu bytes) does not match expected size (%llu bytes)",
							 llu(BlobHeader.DecompressedSize),
							 llu(BlobHeader.PayloadSize));
				return FDownloadError(EDownloadRetryMode::Abort);
			}
		}
		else
		{
			std::string Value = std::string(ChunkContentEncoding);
			UNSYNC_ERROR(L"Unexpected chunk content encoding: '%hs'", Value.c_str());
			return FDownloadError(EDownloadRetryMode::Abort);
		}

		if (DecompressedBuffer.Size() != BlobHeader.DecompressedSize)
		{
			UNSYNC_ERROR(L"Received blob size (%llu bytes) does not match expected size (%llu bytes)",
						 llu(DecompressedBuffer.Size()),
						 llu(BlobHeader.DecompressedSize));
			return FDownloadError(EDownloadRetryMode::Abort);
		}

		FGenericHash BlockHash	  = ComputeHash(DownloadedBlock.Data, DownloadedBlock.DecompressedSize, StrongHasher);

		if (BlockHash.ToHash160() != BlobHeader.DecompressedHash)
		{
			std::string BlockHashStr	= HashToHexString(BlockHash.ToHash160());
			std::string ExpectedHashStr = HashToHexString(BlobHeader.DecompressedHash);
			UNSYNC_ERROR(L"Received blob hash (%hs) does not match expected hash (%hs)", BlockHashStr.c_str(), ExpectedHashStr.c_str());
			return FDownloadError(EDownloadRetryMode::Abort);
		}

		FHash128	 BlockHash128 = BlockHash.ToHash128();

		CompletionCallback(DownloadedBlock, BlockHash128);

		Reader.Skip(BlobHeader.PayloadSize);
	}

	return ResultOk<FDownloadError>();
}

TResult<FDirectoryManifest>
FHordeProtocolImpl::DownloadManifest(std::string_view ManifestName)
{
	FPooledHttpConnection HttpConnection(ProxyPool);

	std::string BearerToken = ProxyPool.GetAccessToken();

	std::string ManifestUrl = fmt::format("/{}/unsync", ManifestName);

	UNSYNC_LOG(L"Downloading manifest from Horde: '%hs'", ManifestUrl.c_str());

	FHttpRequest Request;
	Request.Url			= ManifestUrl;
	Request.BearerToken = BearerToken;

	FHttpResponse Response = HttpRequest(HttpConnection, Request);

	if (!Response.Success())
	{
		return HttpError(Response.Code);
	}

	// Ensure response is terminated
	Response.Buffer.PushBack(0);

	if (Response.ContentType != EHttpContentType::Application_Json)
	{
		return AppError("Unexpected manifest encoding");
	}

	UNSYNC_LOG(L"Decoding manifest ...");

	return DecodeHordeManifestJson((const char*)Response.Buffer.Data(), ManifestName);
}

bool RequestPathLooksLikeHordeArtifact(std::string_view RequestPath)
{
	static const std::regex Pattern("^api\\/v\\d+\\/artifacts\\/[a-fA-F0-9]+$");
	return std::regex_match(RequestPath.begin(), RequestPath.end(), Pattern);
}

TResult<ProxyQuery::FHelloResponse>
FHordeProtocolImpl::QueryHello(FHttpConnection& HttpConnection)
{
	ProxyQuery::FHelloResponse Result;

	const std::string_view Url = "/api/v1/server/auth";

	FHttpResponse Response = HttpRequest(HttpConnection, EHttpMethod::GET, Url);

	if (!Response.Success())
	{
		UNSYNC_ERROR(L"Failed to establish connection to Horde server. Error code: %d.", Response.Code);
		return HttpError(fmt::format("{}:{}{}", HttpConnection.HostAddress.c_str(), HttpConnection.HostPort, Url), Response.Code);
	}

	using namespace json11;
	std::string JsonString = std::string(Response.AsStringView());

	std::string JsonErrorString;
	Json		JsonObject = Json::parse(JsonString, JsonErrorString);

	if (!JsonErrorString.empty())
	{
		return AppError(std::string("JSON parse error while connecting to Horde server: ") + JsonErrorString);
	}

	if (auto& Field = JsonObject["serverUrl"]; Field.is_string())
	{
		Result.AuthServerUri = Field.string_value();
	}

	if (auto& Field = JsonObject["clientId"]; Field.is_string())
	{
		Result.AuthClientId = Field.string_value();
	}

	if (auto& Field = JsonObject["localRedirectUrls"]; Field.is_array())
	{
		if (Field.array_items().size() && Field.array_items()[0].is_string())
		{
			// TODO: parse all allowed callback URIs
			Result.CallbackUri = Field.array_items()[0].string_value();
		}
	}

	return ResultOk(Result);
}

TResult<FDirectoryManifest> DecodeHordeManifestJson(const char* JsonString, std::string_view ArtifactRoot)
{
	using namespace json11;
	std::string JsonErrorString;
	Json		JsonObject = Json::parse(JsonString, JsonErrorString);

	if (!JsonErrorString.empty())
	{
		return AppError(std::string("JSON parse error while downloading manifest from Horde server: ") + JsonErrorString);
	}

	const uint32 DefaultBlockSize = uint32(64_KB); // TODO: get from manifest JSON

	FDirectoryManifest Manifest;
	Manifest.Version = FDirectoryManifest::VERSION;

	if (JsonObject["type"] != "unsync_manifest")
	{
		return AppError("Manifest JSON is expected to have a 'type' string field with 'unsync_manifest' value");
	}

	if (auto& Field = JsonObject["hash_strong"]; Field.is_string())
	{
		const std::string Value = StringToLower(Field.string_value());

		if (Value == "md5")
		{
			Manifest.Algorithm.StrongHashAlgorithmId = EStrongHashAlgorithmID::MD5;
		}
		else if (Value == "blake3.128")
		{
			Manifest.Algorithm.StrongHashAlgorithmId = EStrongHashAlgorithmID::Blake3_128;
		}
		else if (Value == "blake3.160" || Value == "iohash")
		{
			Manifest.Algorithm.StrongHashAlgorithmId = EStrongHashAlgorithmID::Blake3_160;
		}
		else if (Value == "blake3.256")
		{
			Manifest.Algorithm.StrongHashAlgorithmId = EStrongHashAlgorithmID::Blake3_256;
		}
		else
		{
			return AppError(fmt::format("Unsupported strong hash algorithm '{}'", Value));
		}
	}

	if (auto& Field = JsonObject["hash_weak"]; Field.is_string())
	{
		const std::string Value = StringToLower(Field.string_value());

		if (Value == "buzhash")
		{
			Manifest.Algorithm.WeakHashAlgorithmId = EWeakHashAlgorithmID::BuzHash;
		}
		else if (Value == "naive")
		{
			Manifest.Algorithm.WeakHashAlgorithmId = EWeakHashAlgorithmID::Naive;
		}
		else
		{
			return AppError(fmt::format("Unsupported weak hash algorithm '{}'", Value));
		}
	}

	if (auto& Field = JsonObject["chunking"]; Field.is_string())
	{
		const std::string Value = StringToLower(Field.string_value());

		if (Value == "variable")
		{
			Manifest.Algorithm.ChunkingAlgorithmId = EChunkingAlgorithmID::VariableBlocks;
		}
		else if (Value == "fixed")
		{
			Manifest.Algorithm.ChunkingAlgorithmId = EChunkingAlgorithmID::FixedBlocks;
		}
		else if (Value == "rollingbuzhash")
		{
			Manifest.Algorithm.ChunkingAlgorithmId = EChunkingAlgorithmID::VariableBlocks;
			Manifest.Algorithm.WeakHashAlgorithmId = EWeakHashAlgorithmID::BuzHash;
		}
		else
		{
			return AppError(fmt::format("Unsupported chunking algorithm '{}'", Value));
		}
	}

	const auto FileTimeNow = std::chrono::time_point<std::chrono::file_clock>::clock::now();
	const uint64 CurrentWindowsFileTime = ToWindowsFileTime(FileTimeNow);
	uint32		 NumInvalidTimestamps	= 0;

	if (auto& FiledField = JsonObject["files"]; FiledField.is_array())
	{
		for (auto& FileObject : FiledField.array_items())
		{
			std::string FileNameUtf8 = FileObject["name"].string_value();
			std::wstring FileName	  = ConvertUtf8ToWide(FileNameUtf8);

			// Don't include the actual native unsync manifest
			if (FileName.starts_with(L".unsync"))
			{
				continue;
			}

			ConvertDirectorySeparatorsToNative(FileName);

			FFileManifest FileManifest;
			FileManifest.BlockSize = DefaultBlockSize;
			FileManifest.CurrentPath = FileName;

			if (auto& Field = FileObject["size"]; Field.is_number())
			{
				FileManifest.Size = uint64(Field.number_value());
			}

			if (auto& Field = FileObject["mtime"]; Field.is_number())
			{
				FileManifest.Mtime = uint64(Field.number_value());
			}

			if (FileManifest.Mtime == 0)
			{
				FileManifest.Mtime = CurrentWindowsFileTime;
				++NumInvalidTimestamps;
			}

			if (auto& Field = FileObject["read_only"]; Field.is_bool())
			{
				FileManifest.bReadOnly = Field.bool_value();
			}

			if (auto& BlocksField = FileObject["blocks"]; BlocksField.is_array())
			{
				for (auto& BlockObject : BlocksField.array_items())
				{
					FGenericBlock Block;
					Block.Offset = uint64(BlockObject["offset"].number_value());
					Block.Size = uint32(BlockObject["size"].number_value());
					Block.HashWeak = uint32(BlockObject["hash_weak"].number_value());
					const std::string& StrongHashStr = BlockObject["hash_strong"].string_value();
					if (!ParseHashFromHexString(Manifest.Algorithm.StrongHashAlgorithmId, StrongHashStr, Block.HashStrong))
					{
						return AppError(fmt::format("Failed to parse block strong hash '{}'", StrongHashStr));
					}
					FileManifest.Blocks.push_back(Block);
				}
			}

			Manifest.Files[FileName] = FileManifest;
		}
	}

	if (NumInvalidTimestamps != 0)
	{
		UNSYNC_WARNING(L"Manifest contains files with invalid time stamps (%d) which were set to current time", NumInvalidTimestamps);
	}

	return ResultOk(std::move(Manifest));
}

static const char* GTestHordeManifestJson = R"(
{
  "type": "unsync_manifest",
  "hash_strong": "Blake3.160",
  "chunking": "Variable",
  "files": [
    {
      "name": "hello_world.txt",
      "read_only": false,
      "size": 1095,
      "blocks": [
        {
          "offset": 0,
          "size": 1095,
          "hash_strong": "1d9f987b21a19769b758f6fc6354808752620d20"
        },
        {
          "offset": 1095,
          "size": 1024,
          "hash_strong": "f52b611e85cf46f466aac6eee0f69b87dff37831"
        }
      ]
    }
  ]
}
)";

void TestHordeManifestDecode()
{
	UNSYNC_LOG(L"TestHordeManifestDecode()");
	UNSYNC_LOG_INDENT;

	TResult<FDirectoryManifest> Manifest = DecodeHordeManifestJson(GTestHordeManifestJson, "api/v2/artifacts/12345");
	if (Manifest.IsError())
	{
		LogError(Manifest.GetError());
	}
}

}
