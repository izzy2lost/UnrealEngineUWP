// Copyright Epic Games, Inc. All Rights Reserved.

#include "UnsyncCommon.h"
#include "UnsyncProxy.h"

namespace unsync {

class FBlockRequestMap;
struct FRemoteDesc;

struct FHordeProtocolImpl : FRemoteProtocolBase
{
	FHordeProtocolImpl(const FRemoteDesc& InRemoteDesc, const FBlockRequestMap* InRequestMap, FProxyPool& InProxyPool);

	virtual bool IsValid() const { return bValid; }
	virtual void Invalidate() { bValid = false; }

	virtual FDownloadResult	 Download(const TArrayView<FNeedBlock> NeedBlocks, const FBlockDownloadCallback& CompletionCallback);
	virtual TResult<FDirectoryManifest> DownloadManifest(std::string_view ManifestName);


	static TResult<ProxyQuery::FHelloResponse> QueryHello(FHttpConnection& HttpConnection);

	FProxyPool& ProxyPool;
	bool bValid = true;
};

bool RequestPathLooksLikeHordeArtifact(std::string_view RequestPath);

TResult<FDirectoryManifest> DecodeHordeManifestJson(const char* JsonString, std::string_view ArtifactRoot);

}  // namespace unsync
