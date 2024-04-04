// Copyright Epic Games, Inc. All Rights Reserved.

#if defined(UE_WITH_IAS_TOOL)

#include "Command.h"

#include <IO/IoStoreOnDemand.h>

namespace UE::IoStore::Tool
{

////////////////////////////////////////////////////////////////////////////////
static FIoStoreDownloadParams BuildDownloadParams(const FContext& Context)
{
	FIoStoreDownloadParams Ret;

	Ret.Directory				= Context.Get(TEXT("-Directory"),				Ret.Directory);
	Ret.ServiceUrl				= Context.Get(TEXT("-ServiceUrl"),				Ret.ServiceUrl);
	Ret.Bucket					= Context.Get(TEXT("-Bucket"),					Ret.Bucket);
	Ret.Region					= Context.Get(TEXT("-Region"),					Ret.Region); 
	Ret.AccessKey				= Context.Get(TEXT("-AccessKey"),				Ret.AccessKey);
	Ret.SecretKey				= Context.Get(TEXT("-SecretKey"),				Ret.SecretKey);
	Ret.SessionToken			= Context.Get(TEXT("-SessionToken"),			Ret.SessionToken);
	Ret.CredentialsFile			= Context.Get(TEXT("-CredentialsFile"),			Ret.CredentialsFile);
	Ret.CredentialsFileKeyName	= Context.Get(TEXT("-CredentialsFileKeyName"),	Ret.CredentialsFileKeyName);
	Ret.MaxConcurrentDownloads	= Context.Get(TEXT("-MaxConcurrentDownloads"),	Ret.MaxConcurrentDownloads);

	if (FIoStatus Validation = Ret.Validate(); !Validation.IsOk())
	{
		FString Reason = Validation.ToString();
		Context.Abort(*Reason);
	}

	return Ret;
}

////////////////////////////////////////////////////////////////////////////////
static int32 DownloadCommandEntry(const Tool::FContext& Context)
{
	FStringView TocPath = Context.Get<FStringView>(TEXT("TocPath"));

	FIoStoreDownloadParams Params = BuildDownloadParams(Context);

	FIoStatus Status = DownloadContainerFiles(Params, TocPath.GetData());
	if (!Status.IsOk())
	{
		FString Reason = Status.ToString();
		Context.Abort(*Reason);
	}

	return 0;
}

////////////////////////////////////////////////////////////////////////////////
extern FArgumentSet S3Arguments;

static FCommand DownloadCommand(
	DownloadCommandEntry,
	TEXT("Download"),
	TEXT(""),
	{
		TArgument<FStringView>(TEXT("TocPath"),				TEXT("Bucket-relative path of the TOC to download")),
		TArgument<FStringView>(TEXT("-Directory"),			TEXT("Output directory")),
		TArgument<int32>(TEXT("-MaxConcurrentDownloads"),	TEXT("Number of downloads that happen all at once")),
		S3Arguments,
	}
);

} // namespace UE::IoStore::Tool

#endif // UE_WITH_IAS_TOOL
