// Copyright Epic Games, Inc. All Rights Reserved.

#include "UnsyncCommon.h"
#include "UnsyncProxy.h"

#include <optional>

namespace unsync {

class FBlockRequestMap;
struct FRemoteDesc;
struct FAuthDesc;

struct FHordeProtocolImpl : FRemoteProtocolBase
{
	FHordeProtocolImpl(const FRemoteDesc& InRemoteDesc, const FBlockRequestMap* InRequestMap, FProxyPool& InProxyPool);

	virtual bool IsValid() const { return bValid; }
	virtual void Invalidate() { bValid = false; }

	virtual FDownloadResult	 Download(const TArrayView<FNeedBlock> NeedBlocks, const FBlockDownloadCallback& CompletionCallback);
	virtual TResult<FDirectoryManifest> DownloadManifest(std::string_view ManifestName);

	static TResult<ProxyQuery::FHelloResponse> QueryHello(FHttpConnection& HttpConnection);
	static TResult<ProxyQuery::FDirectoryListing> QueryListDirectory(FHttpConnection&	Connection,
																	 const FAuthDesc*	AuthDesc,
																	 const std::string& Path);

	FProxyPool& ProxyPool;
	bool bValid = true;
};


struct FHordeVirtualPath
{
	std::string ArtifactPrefix;

	std::optional<std::string> StreamId;
	std::optional<std::string> ArtifactId;
	std::optional<uint64>	   Change;

	static TResult<FHordeVirtualPath> FromString(std::string_view Str);
};

struct FHordeArtifactEntry
{
	uint64					 Change = 0;
	std::string				 Id;
	std::string				 Name;
	std::string				 Description;
	std::string				 StreamId;
	std::vector<std::string> Keys;
	std::vector<std::string> Metadata;
};

bool RequestPathLooksLikeHordeArtifact(std::string_view RequestPath);

TResult<FDirectoryManifest> DecodeHordeManifestJson(const char* JsonString, std::string_view ArtifactRoot);

}  // namespace unsync
