// Copyright Epic Games, Inc. All Rights Reserved.
#include "CoreMinimal.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformTime.h"
#include "HAL/Runnable.h"
#include "HAL/RunnableThread.h"
#include "HttpManager.h"
#include "HttpRetrySystem.h"
#include "Http.h"
#include "HttpPath.h"
#include "HttpRouteHandle.h"
#include "IHttpRouter.h"
#include "HttpServerModule.h"
#include "Misc/CommandLine.h"
#include "TestHarness.h"
#include "Serialization/JsonSerializerMacros.h"

/**
 *  HTTP Tests
 *  -----------------------------------------------------------------------------------------------
 *
 *  PURPOSE:
 *
 *	Integration Tests to make sure all kinds of HTTP client features in C++ work well on different platforms,
 *  including but not limited to error handing, retrying, threading, streaming, SSL and profiling.
 *
 *  Refer to WebTests/README.md for more info about how to run these tests
 * 
 *  -----------------------------------------------------------------------------------------------
 */

#define HTTP_TAG "[HTTP]"
#define HTTP_TIME_DIFF_TOLERANCE 0.5f

extern TAutoConsoleVariable<bool> CVarHttpInsecureProtocolEnabled;
extern TAutoConsoleVariable<bool> CVarHttpRetrySystemNonGameThreadSupportEnabled;

class FMockHttpModule : public FHttpModule
{
public:
	using FHttpModule::HttpConnectionTimeout;
	using FHttpModule::HttpTotalTimeout;
	using FHttpModule::HttpActivityTimeout;
};

class FHttpTestLogLevelInitializer
{
public:
	FHttpTestLogLevelInitializer()
		: OldVerbosity(LogHttp.GetVerbosity())
	{
		FParse::Bool(FCommandLine::Get(), TEXT("very_verbose="), bVeryVerbose);
		if (bVeryVerbose)
		{
			LogHttp.SetVerbosity(ELogVerbosity::VeryVerbose);
		}
	}

	~FHttpTestLogLevelInitializer()
	{
		if (OldVerbosity != LogHttp.GetVerbosity())
		{
			LogHttp.SetVerbosity(OldVerbosity);
		}
	}

	void DisableWarningsInThisTest()
	{
		if (!bVeryVerbose)
		{
			LogHttp.SetVerbosity(ELogVerbosity::Error);
		}
	}

	bool bVeryVerbose = false;
	ELogVerbosity::Type OldVerbosity;
};

class FHttpModuleTestFixture
{
public:
	FHttpModuleTestFixture()
		: WebServerIp(TEXT("127.0.0.1"))
		, WebServerHttpPort(8000)
		, bRunHeavyTests(false)
		, bRetryEnabled(true)
	{
		ParseSettingsFromCommandLine();

		bRetryEnabled &= CVarHttpRetrySystemNonGameThreadSupportEnabled.GetValueOnAnyThread();

		HttpModule = new FMockHttpModule();
		IModuleInterface* Module = HttpModule;
		Module->StartupModule();

		CVarHttpInsecureProtocolEnabled->Set(true);
	}

	virtual ~FHttpModuleTestFixture()
	{
		IModuleInterface* Module = HttpModule;
		Module->ShutdownModule();
		delete Module;
	}

	void ParseSettingsFromCommandLine()
	{
		FParse::Value(FCommandLine::Get(), TEXT("web_server_ip="), WebServerIp);
		FParse::Bool(FCommandLine::Get(), TEXT("run_heavy_tests="), bRunHeavyTests);
		FParse::Bool(FCommandLine::Get(), TEXT("retry_enabled="), bRetryEnabled);
	}

	void DisableWarningsInThisTest()
	{
		HttpTestLogLevelInitializer.DisableWarningsInThisTest();
	}

	const FString UrlWithInvalidPortToTestConnectTimeout() const { return TEXT("http://10.255.255.1:8765"); } // non-routable IP address with a random port
	const FString UrlBase() const { return FString::Format(TEXT("http://{0}:{1}"), { *WebServerIp, WebServerHttpPort }); }
	const FString UrlHttpTests() const { return FString::Format(TEXT("{0}/webtests/httptests"), { *UrlBase() }); }
	const FString UrlToTestMethods() const { return FString::Format(TEXT("{0}/methods"), { *UrlHttpTests() }); }
	const FString UrlStreamDownload(uint32 Chunks, uint32 ChunkSize, uint32 ChunkLatency=0) { return FString::Format(TEXT("{0}/streaming_download/{1}/{2}/{3}/"), { *UrlHttpTests(), Chunks, ChunkSize, ChunkLatency }); }
	const FString UrlStreamUpload() { return FString::Format(TEXT("{0}/streaming_upload_put"), { *UrlHttpTests() }); }
	const FString UrlMockLatency(uint32 Latency) const { return FString::Format(TEXT("{0}/mock_latency/{1}/"), { *UrlHttpTests(), Latency }); }

	FString WebServerIp;
	uint32 WebServerHttpPort;
	FMockHttpModule* HttpModule;
	bool bRunHeavyTests;
	bool bRetryEnabled;
	FHttpTestLogLevelInitializer HttpTestLogLevelInitializer;
};

TEST_CASE_METHOD(FHttpModuleTestFixture, "Shutdown http module without issue when there are ongoing http requests.", HTTP_TAG)
{
	DisableWarningsInThisTest();

	uint32 ChunkSize = 1024 * 1024;
	TArray<uint8> DataChunk;
	DataChunk.SetNum(ChunkSize);
	FMemory::Memset(DataChunk.GetData(), 'd', ChunkSize);

	for (int32 i = 0; i < 10; ++i)
	{
		IHttpRequest* LeakingHttpRequest = FPlatformHttp::ConstructRequest(); // Leaking in purpose to make sure it's ok

		TSharedRef<IHttpRequest> HttpRequest = HttpModule->CreateRequest();
		HttpRequest->SetURL(UrlToTestMethods());
		HttpRequest->SetVerb(TEXT("PUT"));
		// TODO: Use some shared data, like cookie, openssl session etc.
		HttpRequest->SetContent(DataChunk);
		HttpRequest->OnProcessRequestComplete().BindLambda([](FHttpRequestPtr HttpRequest, FHttpResponsePtr HttpResponse, bool bSucceeded) {
			CHECK(bSucceeded);
		});
		HttpRequest->ProcessRequest();
	}

	HttpModule->GetHttpManager().Tick(0.0f);
}

class FMockRetryManager : public FHttpRetrySystem::FManager
{
public:
	using FHttpRetrySystem::FManager::FManager;
	using FHttpRetrySystem::FManager::RequestList;
	using FHttpRetrySystem::FManager::FHttpRetryRequestEntry;
	using FHttpRetrySystem::FManager::RetryTimeoutRelativeSecondsDefault;

	bool IsEmpty()
	{
		FScopeLock ScopeLock(&RequestListLock);
		return RequestList.IsEmpty();
	}
};

class FWaitUntilCompleteHttpFixture : public FHttpModuleTestFixture
{
public:
	FWaitUntilCompleteHttpFixture()
	{
		HttpModule->GetHttpManager().SetRequestAddedDelegate(FHttpManagerRequestAddedDelegate::CreateRaw(this, &FWaitUntilCompleteHttpFixture::OnRequestAdded));
		HttpModule->GetHttpManager().SetRequestCompletedDelegate(FHttpManagerRequestCompletedDelegate::CreateRaw(this, &FWaitUntilCompleteHttpFixture::OnRequestCompleted));

		if (bRetryEnabled)
		{
			HttpRetryManager = MakeShared<FMockRetryManager>(FHttpRetrySystem::FRetryLimitCountSetting(RetryLimitCount), FHttpRetrySystem::FRetryTimeoutRelativeSecondsSetting(/*RetryTimeoutRelativeSeconds*/));
		}
	}

	~FWaitUntilCompleteHttpFixture()
	{
		WaitUntilAllHttpRequestsComplete();

		HttpModule->GetHttpManager().SetRequestAddedDelegate(FHttpManagerRequestAddedDelegate());
		HttpModule->GetHttpManager().SetRequestCompletedDelegate(FHttpManagerRequestCompletedDelegate());
	}

	void OnRequestAdded(const FHttpRequestRef& Request)
	{
		++OngoingRequests;
	}

	void OnRequestCompleted(const FHttpRequestRef& Request)
	{
		ensure(--OngoingRequests >= 0);
	}

	void WaitUntilAllHttpRequestsComplete()
	{
		while (HasOngoingRequest())
		{
			HttpModule->GetHttpManager().Tick(TickFrequency);
			FPlatformProcess::Sleep(TickFrequency);
		}

		// In case in http thread the http request complete and set OngoingRequests to 0, http manager never 
		// had chance to Tick and remove the request
		HttpModule->GetHttpManager().Tick(TickFrequency);
	}

	bool HasOngoingRequest() const
	{
		return OngoingRequests != 0 || (bRetryEnabled && !HttpRetryManager->IsEmpty());
	}

	TSharedRef<IHttpRequest> CreateRequest()
	{
		return bRetryEnabled ? HttpRetryManager->CreateRequest() : HttpModule->CreateRequest();
	}

	std::atomic<int32> OngoingRequests = 0;
	float TickFrequency = 1.0f / 60; /*60 FPS*/;

	uint32 RetryLimitCount = 0;
	TSharedPtr<FMockRetryManager> HttpRetryManager;
};

TEST_CASE_METHOD(FWaitUntilCompleteHttpFixture, "Http Methods", HTTP_TAG)
{
	TSharedRef<IHttpRequest> HttpRequest = CreateRequest();
	CHECK(HttpRequest->GetVerb() == TEXT("GET"));

	HttpRequest->SetURL(UrlToTestMethods());

	SECTION("Default GET")
	{
	}
	SECTION("GET")
	{
		HttpRequest->SetVerb(TEXT("GET"));
	}
	SECTION("POST")
	{
		HttpRequest->SetVerb(TEXT("POST"));
	}
	SECTION("PUT")
	{
		HttpRequest->SetVerb(TEXT("PUT"));
	}
	SECTION("DELETE")
	{
		HttpRequest->SetVerb(TEXT("DELETE"));
	}

	HttpRequest->OnProcessRequestComplete().BindLambda([](FHttpRequestPtr HttpRequest, FHttpResponsePtr HttpResponse, bool bSucceeded) {
		CHECK(bSucceeded);
		REQUIRE(HttpResponse != nullptr);
		CHECK(HttpResponse->GetResponseCode() == 200);
	});
	HttpRequest->ProcessRequest();
}

TEST_CASE_METHOD(FWaitUntilCompleteHttpFixture, "Can process https request", HTTP_TAG)
{
	TSharedRef<IHttpRequest> HttpRequest = HttpModule->CreateRequest();
	HttpRequest->SetVerb(TEXT("GET"));
	HttpRequest->SetURL(TEXT("https://www.unrealengine.com/"));
	HttpRequest->OnProcessRequestComplete().BindLambda([](FHttpRequestPtr HttpRequest, FHttpResponsePtr HttpResponse, bool bSucceeded) {
		CHECK(bSucceeded);
		REQUIRE(HttpResponse != nullptr);
	});
	HttpRequest->ProcessRequest();
}

TEST_CASE_METHOD(FWaitUntilCompleteHttpFixture, "Get large response content without chunks", HTTP_TAG)
{
	TSharedRef<IHttpRequest> HttpRequest = CreateRequest();
	HttpRequest->SetURL(FString::Format(TEXT("{0}/get_large_response_without_chunks/{1}/"), { *UrlHttpTests(), 1024 * 1024/*bytes_number*/}));
	HttpRequest->SetVerb(TEXT("GET"));
	HttpRequest->OnProcessRequestComplete().BindLambda([](FHttpRequestPtr HttpRequest, FHttpResponsePtr HttpResponse, bool bSucceeded) {
		CHECK(bSucceeded);
		REQUIRE(HttpResponse != nullptr);
		CHECK(HttpResponse->GetResponseCode() == 200);
	});
	HttpRequest->ProcessRequest();
}

TEST_CASE_METHOD(FWaitUntilCompleteHttpFixture, "Http request connect timeout", HTTP_TAG)
{
	DisableWarningsInThisTest();

	HttpModule->HttpActivityTimeout = 3.0f; // Make sure this won't be triggered before establishing connection
	HttpModule->HttpConnectionTimeout = 15.0f;

	TSharedRef<IHttpRequest> HttpRequest = CreateRequest();

	HttpRequest->SetURL(UrlWithInvalidPortToTestConnectTimeout());
	HttpRequest->SetVerb(TEXT("GET"));

	const double StartTime = FPlatformTime::Seconds();

	HttpRequest->OnProcessRequestComplete().BindLambda([StartTime](FHttpRequestPtr HttpRequest, FHttpResponsePtr HttpResponse, bool bSucceeded) {
		CHECK(!bSucceeded);
		CHECK(HttpResponse == nullptr);
		CHECK(HttpRequest->GetStatus() == EHttpRequestStatus::Failed);
		CHECK(HttpRequest->GetFailureReason() == EHttpFailureReason::ConnectionError);
		const double DurationInSeconds  = FPlatformTime::Seconds() - StartTime;
		double HttpTimeDiffTolerance = 1.5;
#if WITH_CURL_XCURL
		HttpTimeDiffTolerance += 3.0; // It seems xCurl takes up to 3 more seconds for connect timeout
#endif
		CHECK(FMath::IsNearlyEqual(DurationInSeconds, 15.0, HttpTimeDiffTolerance));
	});
	HttpRequest->ProcessRequest();
}

TEST_CASE_METHOD(FWaitUntilCompleteHttpFixture, "Streaming http download", HTTP_TAG)
{
	uint32 Chunks = 3;
	uint32 ChunkSize = 1024*1024;

	TSharedRef<IHttpRequest> HttpRequest = CreateRequest();
	HttpRequest->SetURL(UrlStreamDownload(Chunks, ChunkSize));
	HttpRequest->SetVerb(TEXT("GET"));

	TSharedRef<int64> TotalBytesReceived = MakeShared<int64>(0);

	SECTION("Success without stream provided")
	{
		HttpRequest->OnProcessRequestComplete().BindLambda([Chunks, ChunkSize](FHttpRequestPtr HttpRequest, FHttpResponsePtr HttpResponse, bool bSucceeded) {
			CHECK(bSucceeded);
			REQUIRE(HttpResponse != nullptr);
			CHECK(HttpResponse->GetResponseCode() == 200);
			CHECK(!HttpResponse->GetAllHeaders().IsEmpty());
			CHECK(HttpResponse->GetContentLength() == Chunks * ChunkSize);
		});
	}
	SECTION("Success with customized stream")
	{
		class FTestHttpReceiveStream final : public FArchive
		{
		public:
			FTestHttpReceiveStream(TSharedRef<int64> InTotalBytesReceived)
				: TotalBytesReceived(InTotalBytesReceived)
			{
			}

			virtual void Serialize(void* V, int64 Length) override
			{
				*TotalBytesReceived += Length;
			}

			TSharedRef<int64> TotalBytesReceived;
		};

		TSharedRef<FTestHttpReceiveStream> Stream = MakeShared<FTestHttpReceiveStream>(TotalBytesReceived);
		CHECK(HttpRequest->SetResponseBodyReceiveStream(Stream));

		HttpRequest->OnProcessRequestComplete().BindLambda([Chunks, ChunkSize, TotalBytesReceived](FHttpRequestPtr HttpRequest, FHttpResponsePtr HttpResponse, bool bSucceeded) {
			CHECK(bSucceeded);
			REQUIRE(HttpResponse != nullptr);
			CHECK(HttpResponse->GetResponseCode() == 200);
			CHECK(!HttpResponse->GetAllHeaders().IsEmpty());
			CHECK(HttpResponse->GetContentLength() == Chunks * ChunkSize);
			CHECK(HttpResponse->GetContent().IsEmpty());
			CHECK(*TotalBytesReceived == Chunks * ChunkSize);
		});
	}
	SECTION("Success with customized stream delegate")
	{
		FHttpRequestStreamDelegate Delegate;
		Delegate.BindLambda([TotalBytesReceived](void* Ptr, int64 Length) {
			*TotalBytesReceived += Length;
			return true;
		});
		CHECK(HttpRequest->SetResponseBodyReceiveStreamDelegate(Delegate));

		HttpRequest->OnProcessRequestComplete().BindLambda([Chunks, ChunkSize, TotalBytesReceived](FHttpRequestPtr HttpRequest, FHttpResponsePtr HttpResponse, bool bSucceeded) {
			CHECK(bSucceeded);
			REQUIRE(HttpResponse != nullptr);
			CHECK(HttpResponse->GetResponseCode() == 200);
			CHECK(!HttpResponse->GetAllHeaders().IsEmpty());
			CHECK(HttpResponse->GetContentLength() == Chunks * ChunkSize);
			CHECK(HttpResponse->GetContent().IsEmpty());
			CHECK(*TotalBytesReceived == Chunks * ChunkSize);
		});
	}
	SECTION("Use customized stream to receive response body but failed when serialize")
	{
		DisableWarningsInThisTest();

		class FTestHttpReceiveStream final : public FArchive
		{
		public:
			FTestHttpReceiveStream(TSharedRef<int64> InTotalBytesReceived)
				: TotalBytesReceived(InTotalBytesReceived)
			{
			}

			virtual void Serialize(void* V, int64 Length) override
			{
				*TotalBytesReceived += Length;
				SetError();
			}

			TSharedRef<int64> TotalBytesReceived;
		};

		TSharedRef<FTestHttpReceiveStream> Stream = MakeShared<FTestHttpReceiveStream>(TotalBytesReceived);
		CHECK(HttpRequest->SetResponseBodyReceiveStream(Stream));

		HttpRequest->OnProcessRequestComplete().BindLambda([ChunkSize, TotalBytesReceived](FHttpRequestPtr HttpRequest, FHttpResponsePtr HttpResponse, bool bSucceeded) {
			CHECK(!bSucceeded);
			CHECK(HttpResponse != nullptr);
			CHECK(*TotalBytesReceived <= ChunkSize);
		});
	}
	SECTION("Use customized stream delegate to receive response body but failed when call")
	{
		DisableWarningsInThisTest();

		FHttpRequestStreamDelegate Delegate;
		Delegate.BindLambda([TotalBytesReceived](void* Ptr, int64 Length) {
			*TotalBytesReceived += Length;
			return false;
		});
		CHECK(HttpRequest->SetResponseBodyReceiveStreamDelegate(Delegate));

		HttpRequest->OnProcessRequestComplete().BindLambda([ChunkSize, TotalBytesReceived](FHttpRequestPtr HttpRequest, FHttpResponsePtr HttpResponse, bool bSucceeded) {
			CHECK(!bSucceeded);
			CHECK(HttpResponse != nullptr);
			CHECK(*TotalBytesReceived <= ChunkSize);
		});
	}
	SECTION("Success with file stream to receive response body")
	{
		FString Filename = FString(FPlatformProcess::UserSettingsDir()) / TEXT("TestStreamDownload.dat");
		FArchive* RawFile = IFileManager::Get().CreateFileWriter(*Filename);
		CHECK(RawFile != nullptr);
		TSharedRef<FArchive> FileToWrite = MakeShareable(RawFile);
		CHECK(HttpRequest->SetResponseBodyReceiveStream(FileToWrite));

		HttpRequest->OnProcessRequestComplete().BindLambda([Chunks, ChunkSize, Filename, FileToWrite](FHttpRequestPtr HttpRequest, FHttpResponsePtr HttpResponse, bool bSucceeded) {
			CHECK(bSucceeded);
			REQUIRE(HttpResponse != nullptr);
			CHECK(HttpResponse->GetContentLength() == Chunks * ChunkSize);
			CHECK(HttpResponse->GetContent().IsEmpty());
			CHECK(HttpResponse->GetResponseCode() == 200);
			CHECK(!HttpResponse->GetAllHeaders().IsEmpty());

			FileToWrite->FlushCache();
			FileToWrite->Close();

			TSharedRef<FArchive> FileToRead = MakeShareable(IFileManager::Get().CreateFileReader(*Filename));
			CHECK(FileToRead->TotalSize() == Chunks * ChunkSize);
			FileToRead->Close();

			IFileManager::Get().Delete(*Filename);
		});
	}

	HttpRequest->ProcessRequest();
}

TEST_CASE_METHOD(FWaitUntilCompleteHttpFixture, "Can run parallel stream download requests", HTTP_TAG)
{
	uint32 Chunks = 5;
	uint32 ChunkSize = 1024*1024;

	for (int i = 0; i < 3; ++i)
	{
		TSharedRef<IHttpRequest> HttpRequest = CreateRequest();
		HttpRequest->SetURL(UrlStreamDownload(Chunks, ChunkSize));
		HttpRequest->SetVerb(TEXT("GET"));
		HttpRequest->OnProcessRequestComplete().BindLambda([Chunks, ChunkSize](FHttpRequestPtr HttpRequest, FHttpResponsePtr HttpResponse, bool bSucceeded) {
			CHECK(HttpResponse->GetContentLength() == Chunks * ChunkSize);
			CHECK(bSucceeded);
			CHECK(HttpResponse->GetResponseCode() == 200);
		});
		HttpRequest->ProcessRequest();
	}
}

TEST_CASE_METHOD(FWaitUntilCompleteHttpFixture, "Can download big file exceeds 32 bits", HTTP_TAG)
{
	if (!bRunHeavyTests)
	{
		return;
	}

	// 5 * 1024 * 1024 * 1024 BYTES = 5368709120 BYTES = 5 GB
	uint64 Chunks = 5 * 1024;
	uint64 ChunkSize = 1024 * 1024;

	TSharedRef<IHttpRequest> HttpRequest = CreateRequest();
	HttpRequest->SetURL(UrlStreamDownload(Chunks, ChunkSize));
	HttpRequest->SetVerb(TEXT("GET"));

	TSharedRef<int64> TotalBytesReceived = MakeShared<int64>(0);
	FHttpRequestStreamDelegate Delegate;
	Delegate.BindLambda([TotalBytesReceived](void* Ptr, int64 Length) {
		*TotalBytesReceived += Length;
		return true;
	});
	HttpRequest->SetResponseBodyReceiveStreamDelegate(Delegate);

	HttpRequest->OnProcessRequestComplete().BindLambda([Chunks, ChunkSize, TotalBytesReceived](FHttpRequestPtr HttpRequest, FHttpResponsePtr HttpResponse, bool bSucceeded) {
		CHECK(bSucceeded);
		REQUIRE(HttpResponse != nullptr);
		CHECK(HttpResponse->GetContentLength() == Chunks * ChunkSize);
		CHECK(HttpResponse->GetContent().IsEmpty());
		CHECK(*TotalBytesReceived == Chunks * ChunkSize);
		CHECK(HttpResponse->GetResponseCode() == 200);
	});
	HttpRequest->ProcessRequest();
}

TEST_CASE_METHOD(FWaitUntilCompleteHttpFixture, "Streaming http upload from memory", HTTP_TAG)
{
	TSharedRef<IHttpRequest> HttpRequest = CreateRequest();
	HttpRequest->SetURL(FString::Format(TEXT("{0}/streaming_upload_post"), { *UrlHttpTests() }));
	HttpRequest->SetVerb(TEXT("POST"));

	const char* BoundaryLabel = "test_http_boundary";
	HttpRequest->SetHeader(TEXT("Content-Type"), FString::Format(TEXT("multipart/form-data; boundary={0}"), { BoundaryLabel }));

	// Data will be sent by chunks in http request
	const uint32 FileSize = 10*1024*1024;
	char* FileData = (char*)FMemory::Malloc(FileSize + 1);
	FMemory::Memset(FileData, 'd', FileSize);
	FileData[FileSize + 1] = '\0';

	TArray<uint8> ContentData;
	const int32 ContentMaxSize = FileSize + 256/*max length of format string*/;
	ContentData.Reserve(ContentMaxSize);
	const int32 ContentLength = FCStringAnsi::Snprintf(
		(char*)ContentData.GetData(),
		ContentMaxSize,
		"--%s\r\n"
		"Content-Disposition: form-data; name=\"file\"; filename=\"bigfile.zip\"\r\n"
		"Content-Type: application/octet-stream\r\n\r\n"
		"%s\r\n"
		"--%s--",
		BoundaryLabel, FileData, BoundaryLabel);

	FMemory::Free(FileData);

	CHECK(ContentLength > 0);
	CHECK(ContentLength < ContentMaxSize);
	ContentData.SetNumUninitialized(ContentLength);
	HttpRequest->SetContent(MoveTemp(ContentData));

	HttpRequest->OnProcessRequestComplete().BindLambda([](FHttpRequestPtr HttpRequest, FHttpResponsePtr HttpResponse, bool bSucceeded) {
		CHECK(bSucceeded);
		REQUIRE(HttpResponse != nullptr);
		CHECK(HttpResponse->GetResponseCode() == 200);
	});
	HttpRequest->ProcessRequest();
}

class FTestHttpUploadStream final : public FArchive
{
public:
	FTestHttpUploadStream(uint64 InTotalSize)
		: FakeTotalSize(InTotalSize)
	{
	}

	virtual void Serialize(void* V, int64 Length) override
	{
		for (int64 i = 0; i < Length; ++i)
		{
			((char*)V)[i] = 'd';
		}

		CurrentPos += Length;
	}

	virtual int64 TotalSize() override
	{
		return FakeTotalSize;
	}

	virtual void Seek(int64 InPos) 
	{ 
		CurrentPos = InPos;
	}

	virtual int64 Tell() override
	{
		return CurrentPos;
	}

	uint64 FakeTotalSize;
	uint64 CurrentPos = 0;
};

TEST_CASE_METHOD(FWaitUntilCompleteHttpFixture, "Can upload big file exceeds 32 bits", HTTP_TAG)
{
	if (!bRunHeavyTests)
	{
		return;
	}

	// TODO: Back to check later. xCurl 2206.4.0.0 doesn't work with file bigger than 32 bits
	// 5 * 1024 * 1024 * 1024 BYTES = 5368709120 BYTES = 5 GB
	//const uint64 TotalSize = 5368709120;
	//const uint64 TotalSize = 4294967296;
	//const uint64 TotalSize = 4294967295;
	//const uint64 TotalSize = 2147483648;
	const uint64 TotalSize = 2147483647;
	TSharedRef<FTestHttpUploadStream> Stream = MakeShared<FTestHttpUploadStream>(TotalSize);

	TSharedRef<IHttpRequest> HttpRequest = CreateRequest();
	HttpRequest->SetURL(UrlStreamUpload());
	HttpRequest->SetVerb(TEXT("PUT"));
	HttpRequest->SetContentFromStream(Stream);
	HttpRequest->SetHeader(TEXT("Content-Disposition"), TEXT("attachment;filename=TestStreamUpload.dat"));
	HttpRequest->OnProcessRequestComplete().BindLambda([Stream, TotalSize](FHttpRequestPtr HttpRequest, FHttpResponsePtr HttpResponse, bool bSucceeded) {
		CHECK(bSucceeded);
		REQUIRE(HttpResponse != nullptr);
		CHECK(HttpResponse->GetResponseCode() == 200);
		CHECK(Stream->CurrentPos == TotalSize);
	});
	HttpRequest->ProcessRequest();
}

namespace UE
{
namespace TestHttp
{

void WriteTestFile(const FString& TestFileName, uint64 TestFileSize)
{
	FArchive* RawFile = IFileManager::Get().CreateFileWriter(*TestFileName);
	CHECK(RawFile != nullptr);
	TSharedRef<FArchive> FileToWrite = MakeShareable(RawFile);
	char* FileData = (char*)FMemory::Malloc(TestFileSize);
	FMemory::Memset(FileData, 'd', TestFileSize);
	FileToWrite->Serialize(FileData, TestFileSize);
	FileToWrite->FlushCache();
	FileToWrite->Close();
	FMemory::Free(FileData);
}

}
}

#define HTTP_TEST_TIMEOUT_CHUNK_SIZE 16*1024 // Use a big chunk size so it triggers data received callback in time on all platforms

TEST_CASE_METHOD(FWaitUntilCompleteHttpFixture, "Http request activity timeout", HTTP_TAG)
{
	DisableWarningsInThisTest();

	float ReceiveTimeoutSetting = 3.0f;
	HttpModule->HttpActivityTimeout = ReceiveTimeoutSetting;

	TSharedPtr<IHttpRequest> HttpRequest = CreateRequest();
	HttpRequest->SetURL(UrlStreamDownload(3/*Chunks*/, HTTP_TEST_TIMEOUT_CHUNK_SIZE, 5/*ChunkLatency*/));
	HttpRequest->SetVerb(TEXT("GET"));

	const double StartTime = FPlatformTime::Seconds();

	HttpRequest->OnProcessRequestComplete().BindLambda([StartTime, ReceiveTimeoutSetting](FHttpRequestPtr HttpRequest, FHttpResponsePtr HttpResponse, bool bSucceeded) {
		CHECK(!bSucceeded);
		CHECK(HttpRequest->GetStatus() == EHttpRequestStatus::Failed);
		CHECK(HttpRequest->GetFailureReason() == EHttpFailureReason::ConnectionError);

		const double DurationInSeconds  = FPlatformTime::Seconds() - StartTime;
#if WITH_CURL_XCURL
		// Unlike libCurl, currently there is an issue in xCurl that it triggers CURLINFO_HEADER_OUT even if can't 
		// connect. Had to disable that code, make sure not to treat that event as connected
		// So it takes 5s to receive the first chunk to be considered as connected, then start response timer and 
		// take 3s to response timeout
		CHECK(FMath::IsNearlyEqual(DurationInSeconds, ReceiveTimeoutSetting + 5, HTTP_TIME_DIFF_TOLERANCE));
#else
		CHECK(FMath::IsNearlyEqual(DurationInSeconds, ReceiveTimeoutSetting, HTTP_TIME_DIFF_TOLERANCE));
#endif

	});
	HttpRequest->ProcessRequest();
}

TEST_CASE_METHOD(FWaitUntilCompleteHttpFixture, "Http request receive won't timeout for streaming request", HTTP_TAG)
{
	HttpModule->HttpActivityTimeout = 3.0f;

	TSharedPtr<IHttpRequest> HttpRequest = CreateRequest();
	HttpRequest->SetURL(UrlStreamDownload(3/*Chunks*/, HTTP_TEST_TIMEOUT_CHUNK_SIZE, 2/*ChunkLatency*/)); // Needs 6s to complete
	HttpRequest->SetVerb(TEXT("GET"));

	const double StartTime = FPlatformTime::Seconds();
	HttpRequest->OnProcessRequestComplete().BindLambda([this, StartTime](FHttpRequestPtr HttpRequest, FHttpResponsePtr HttpResponse, bool bSucceeded) {
		CHECK(bSucceeded);
		REQUIRE(HttpResponse != nullptr);
		CHECK(HttpResponse->GetResponseCode() == 200);
		const double DurationInSeconds = FPlatformTime::Seconds() - StartTime;
		CHECK(DurationInSeconds > HttpModule->HttpActivityTimeout);
	});
	HttpRequest->ProcessRequest();
}

TEST_CASE_METHOD(FWaitUntilCompleteHttpFixture, "Http request total timeout with get", HTTP_TAG)
{
	DisableWarningsInThisTest();

	float TotalTimeoutSetting = 3.0f;
	HttpModule->HttpTotalTimeout = TotalTimeoutSetting;
	HttpModule->HttpConnectionTimeout = 5.0f;

	TSharedPtr<IHttpRequest> HttpRequest = CreateRequest();
	HttpRequest->SetURL(UrlMockLatency(10));
	HttpRequest->SetVerb(TEXT("GET"));

	const double StartTime = FPlatformTime::Seconds();

	HttpRequest->OnProcessRequestComplete().BindLambda([StartTime, TotalTimeoutSetting](FHttpRequestPtr HttpRequest, FHttpResponsePtr HttpResponse, bool bSucceeded) {
		CHECK(!bSucceeded);
		CHECK(HttpRequest->GetStatus() == EHttpRequestStatus::Failed);
		CHECK(HttpRequest->GetFailureReason() == EHttpFailureReason::TimedOut);
		const double DurationInSeconds  = FPlatformTime::Seconds() - StartTime;
		CHECK(FMath::IsNearlyEqual(DurationInSeconds, TotalTimeoutSetting, HTTP_TIME_DIFF_TOLERANCE));
	});
	HttpRequest->ProcessRequest();
}

TEST_CASE_METHOD(FWaitUntilCompleteHttpFixture, "Http request total timeout with streaming download", HTTP_TAG)
{
	DisableWarningsInThisTest();

	float TimeoutSetting = 3.0f;
	HttpModule->HttpActivityTimeout = 2.5f; // Make sure it won't fail because of receive timeout
	HttpModule->HttpTotalTimeout = TimeoutSetting;

	if (bRetryEnabled)
	{
		TimeoutSetting = 4.0f; // This will override http module default timeout
		HttpRetryManager->RetryTimeoutRelativeSecondsDefault = TimeoutSetting;
	}

	TSharedPtr<IHttpRequest> HttpRequest;
	SECTION("Use default timeout from http module or retry manager depends on bRetryEnabled")
	{
		HttpRequest = CreateRequest();
	}
	SECTION("Override from http request")
	{
		TimeoutSetting = 5.0f; // This will override default timeout in http module and retry manager

		if (bRetryEnabled)
		{
			HttpRequest = HttpRetryManager->CreateRequest(FHttpRetrySystem::FRetryLimitCountSetting(), TimeoutSetting);
		}
		else
		{
			HttpRequest = HttpModule->CreateRequest();
			HttpRequest->SetTimeout(TimeoutSetting);
		}
	}

	HttpRequest->SetURL(UrlStreamDownload(4/*Chunks*/, HTTP_TEST_TIMEOUT_CHUNK_SIZE, 2/*ChunkLatency*/)); // Needs 8s to complete
	HttpRequest->SetVerb(TEXT("GET"));

	const double StartTime = FPlatformTime::Seconds();

	HttpRequest->OnProcessRequestComplete().BindLambda([StartTime, TimeoutSetting](FHttpRequestPtr HttpRequest, FHttpResponsePtr HttpResponse, bool bSucceeded) {
		CHECK(!bSucceeded);
		CHECK(HttpRequest->GetStatus() == EHttpRequestStatus::Failed);
		CHECK(HttpRequest->GetFailureReason() == EHttpFailureReason::TimedOut);
		const double DurationInSeconds  = FPlatformTime::Seconds() - StartTime;
		CHECK(FMath::IsNearlyEqual(DurationInSeconds, TimeoutSetting, HTTP_TIME_DIFF_TOLERANCE));
	});
	HttpRequest->ProcessRequest();
}

TEST_CASE_METHOD(FWaitUntilCompleteHttpFixture, "Streaming http upload from file by PUT can work well", HTTP_TAG)
{
	FString Filename = FString(FPlatformProcess::UserSettingsDir()) / TEXT("TestStreamUpload.dat");
	UE::TestHttp::WriteTestFile(Filename, 5*1024*1024/*5MB*/);

	TSharedRef<IHttpRequest> HttpRequest = CreateRequest();
	HttpRequest->SetURL(UrlStreamUpload());
	HttpRequest->SetVerb(TEXT("PUT"));
	HttpRequest->SetHeader(TEXT("Content-Disposition"), TEXT("attachment;filename=TestStreamUpload.dat"));
	HttpRequest->SetContentAsStreamedFile(Filename);
	HttpRequest->OnProcessRequestComplete().BindLambda([Filename](FHttpRequestPtr HttpRequest, FHttpResponsePtr HttpResponse, bool bSucceeded) {
		CHECK(bSucceeded);
		CHECK(HttpResponse->GetResponseCode() == 200);
		IFileManager::Get().Delete(*Filename);
	});
	HttpRequest->ProcessRequest();
}

TEST_CASE_METHOD(FWaitUntilCompleteHttpFixture, "Redirect enabled by default and can work well", HTTP_TAG)
{
	TSharedRef<IHttpRequest> HttpRequest = CreateRequest();
	HttpRequest->SetURL(FString::Format(TEXT("{0}/redirect_from"), { *UrlHttpTests() }));
	HttpRequest->SetVerb(TEXT("GET"));
	HttpRequest->OnProcessRequestComplete().BindLambda([](FHttpRequestPtr HttpRequest, FHttpResponsePtr HttpResponse, bool bSucceeded) {
		CHECK(bSucceeded);
		CHECK(HttpResponse->GetResponseCode() == 200);
	});
	HttpRequest->ProcessRequest();
}


class FWaitUntilQuitFromTestFixture : public FWaitUntilCompleteHttpFixture
{
public:
	FWaitUntilQuitFromTestFixture()
	{
	}

	~FWaitUntilQuitFromTestFixture()
	{
		WaitUntilQuitFromTest();
	}

	void WaitUntilQuitFromTest()
	{
		while (!bQuitRequested)
		{
			HttpModule->GetHttpManager().Tick(TickFrequency);
			FPlatformProcess::Sleep(TickFrequency);
		}
	}

	bool bQuitRequested = false;
};

TEST_CASE_METHOD(FWaitUntilQuitFromTestFixture, "Http request can be reused", HTTP_TAG)
{
	TSharedRef<IHttpRequest> HttpRequest = CreateRequest();
	HttpRequest->SetURL(UrlToTestMethods());
	HttpRequest->SetVerb(TEXT("POST"));

	HttpRequest->OnProcessRequestComplete().BindLambda([this](FHttpRequestPtr HttpRequest, FHttpResponsePtr HttpResponse, bool bSucceeded) {
		CHECK(bSucceeded);
		CHECK(HttpResponse->GetResponseCode() == 200);

		// Using a different URL
		uint32 Chunks = 3;
		uint32 ChunkSize = 1024;
		HttpRequest->SetURL(UrlStreamDownload(Chunks, ChunkSize));
		HttpRequest->SetVerb(TEXT("GET"));
		HttpRequest->OnProcessRequestComplete().BindLambda([this, Chunks, ChunkSize](FHttpRequestPtr HttpRequest, FHttpResponsePtr HttpResponse, bool bSucceeded) {
			CHECK(bSucceeded);
			REQUIRE(HttpResponse != nullptr);
			CHECK(HttpResponse->GetResponseCode() == 200);
			CHECK(HttpResponse->GetContentLength() == Chunks * ChunkSize);

			// Simulate retry with same URL info
			HttpRequest->OnProcessRequestComplete().BindLambda([this, Chunks, ChunkSize](FHttpRequestPtr HttpRequest, FHttpResponsePtr HttpResponse, bool bSucceeded) {
				CHECK(bSucceeded);
				REQUIRE(HttpResponse != nullptr);
				CHECK(HttpResponse->GetResponseCode() == 200);
				CHECK(HttpResponse->GetContentLength() == Chunks * ChunkSize);
				bQuitRequested = true;
			});
			HttpRequest->ProcessRequest();
		});
		HttpRequest->ProcessRequest();
	});
	HttpRequest->ProcessRequest();
}


// Response shared ptr should be able to be kept by user code and valid to access without http request
class FValidateResponseDependencyFixture : public FWaitUntilCompleteHttpFixture
{
public:
	DECLARE_DELEGATE(FValidateResponseDependencyDelegate);

	~FValidateResponseDependencyFixture()
	{
		WaitUntilAllHttpRequestsComplete();

		ValidateResponseDependencyDelegate.ExecuteIfBound();
	}

	FValidateResponseDependencyDelegate ValidateResponseDependencyDelegate;
};

TEST_CASE_METHOD(FValidateResponseDependencyFixture, "Http query with parameters", HTTP_TAG)
{
	struct FQueryWithParamsResponse : public FJsonSerializable
	{
		int32 VarInt;
		FString VarStr;

		BEGIN_JSON_SERIALIZER
			JSON_SERIALIZE("var_int", VarInt);
			JSON_SERIALIZE("var_str", VarStr);
		END_JSON_SERIALIZER
	};

	TSharedRef<IHttpRequest> HttpRequest = HttpModule->CreateRequest();
	FString UrlQueryWithParams = FString::Format(TEXT("{0}/query_with_params/?var_int=3&var_str=abc"), { *UrlHttpTests() });
	HttpRequest->SetURL(UrlQueryWithParams);
	HttpRequest->SetVerb(TEXT("GET"));
	HttpRequest->OnProcessRequestComplete().BindLambda([this, UrlQueryWithParams](FHttpRequestPtr HttpRequest, FHttpResponsePtr HttpResponse, bool bSucceeded) {
		CHECK(bSucceeded);
		REQUIRE(HttpResponse != nullptr);
		CHECK(HttpResponse->GetResponseCode() == 200);

		CHECK(HttpRequest->GetURL() == UrlQueryWithParams);

		FQueryWithParamsResponse QueryWithParamsResponse;
		REQUIRE(QueryWithParamsResponse.FromJson(HttpResponse->GetContentAsString()));

		CHECK(FString::FromInt(QueryWithParamsResponse.VarInt) == HttpRequest->GetURLParameter(TEXT("var_int")));
		CHECK(QueryWithParamsResponse.VarStr == HttpRequest->GetURLParameter(TEXT("var_str")));

		CHECK(FString::FromInt(QueryWithParamsResponse.VarInt) == HttpResponse->GetURLParameter(TEXT("var_int")));
		CHECK(QueryWithParamsResponse.VarStr == HttpResponse->GetURLParameter(TEXT("var_str")));

		ValidateResponseDependencyDelegate.BindLambda([HttpResponse, UrlQueryWithParams, QueryWithParamsResponse](){
			// Validate all interfaces of http response can be called without accessing the destroyed http request
			CHECK(HttpResponse->GetResponseCode() == 200);
			CHECK(!HttpResponse->GetContent().IsEmpty());
			CHECK(!HttpResponse->GetContentAsString().IsEmpty());
			CHECK(HttpResponse->GetContentType() == TEXT("application/json"));
			CHECK(HttpResponse->GetHeader("Content-Type") == TEXT("application/json"));
			CHECK(!HttpResponse->GetAllHeaders().IsEmpty());
			CHECK(HttpResponse->GetURL() == UrlQueryWithParams);
			CHECK(HttpResponse->GetURLParameter(TEXT("var_int")) == FString::FromInt(QueryWithParamsResponse.VarInt));
			CHECK(HttpResponse->GetURLParameter(TEXT("var_str")) == QueryWithParamsResponse.VarStr);
		});
	});
	HttpRequest->ProcessRequest();
}

class FThreadedHttpRunnable : public FRunnable
{
public:
	DECLARE_DELEGATE(FRunActualTestCodeDelegate);

	FRunActualTestCodeDelegate& OnRunFromThread()
	{
		return ThreadCallback;
	}

	// FRunnable interface
	virtual uint32 Run() override
	{
		ThreadCallback.ExecuteIfBound();
		return 0;
	}

	void StartTestHttpThread(bool bBlockGameThread)
	{
		bBlockingGameThreadTick = bBlockGameThread;

		RunnableThread = TSharedPtr<FRunnableThread>(FRunnableThread::Create(this, TEXT("Test Http Thread")));

		while (bBlockingGameThreadTick)
		{
			float TickFrequency = 1.0f / 60; /*60 FPS*/;
			FPlatformProcess::Sleep(TickFrequency);
		}
	}

	void UnblockGameThread()
	{
		bBlockingGameThreadTick = false;
	}

private:
	FRunActualTestCodeDelegate ThreadCallback;
	TSharedPtr<FRunnableThread> RunnableThread;
	std::atomic<bool> bBlockingGameThreadTick = true;
};

class FWaitThreadedHttpFixture : public FWaitUntilCompleteHttpFixture
{
public:
	~FWaitThreadedHttpFixture()
	{
		WaitUntilAllHttpRequestsComplete();
	}

	FThreadedHttpRunnable ThreadedHttpRunnable;
};

TEST_CASE_METHOD(FWaitThreadedHttpFixture, "Http streaming download request can work in non game thread", HTTP_TAG)
{
	ThreadedHttpRunnable.OnRunFromThread().BindLambda([this]() {
		TSharedRef<IHttpRequest> HttpRequest = CreateRequest();
		HttpRequest->SetURL(UrlStreamDownload(3/*Chunks*/, 1024/*ChunkSize*/));
		HttpRequest->SetVerb(TEXT("GET"));
		HttpRequest->SetDelegateThreadPolicy(EHttpRequestDelegateThreadPolicy::CompleteOnHttpThread);

		class FTestHttpReceiveStream final : public FArchive
		{
		public:
			virtual void Serialize(void* V, int64 Length) override
			{
				// No matter what's the thread policy, Serialize always get called in http thread.
				CHECK(!IsInGameThread());
			}
		};
		CHECK(HttpRequest->SetResponseBodyReceiveStream(MakeShared<FTestHttpReceiveStream>()));

		HttpRequest->OnProcessRequestComplete().BindLambda([this](FHttpRequestPtr HttpRequest, FHttpResponsePtr HttpResponse, bool bSucceeded) {
			// EHttpRequestDelegateThreadPolicy::CompleteOnHttpThread was used, so not in game thread here
			CHECK(!IsInGameThread());
			CHECK(bSucceeded);
			REQUIRE(HttpResponse != nullptr);
			CHECK(HttpResponse->GetResponseCode() == 200);
			CHECK(!HttpResponse->GetAllHeaders().IsEmpty());
			ThreadedHttpRunnable.UnblockGameThread();
		});

		HttpRequest->ProcessRequest();
	});

	ThreadedHttpRunnable.StartTestHttpThread(true/*bBlockGameThread*/);
}

TEST_CASE_METHOD(FWaitThreadedHttpFixture, "Http download request progress callback can be received in http thread", HTTP_TAG)
{
	std::atomic<bool> bRequestProgressTriggered = false;
	ThreadedHttpRunnable.OnRunFromThread().BindLambda([this, &bRequestProgressTriggered]() {
		TSharedRef<IHttpRequest> HttpRequest = CreateRequest();
		HttpRequest->SetURL(UrlStreamDownload(10/*Chunks*/, 1024*1024/*ChunkSize*/));
		HttpRequest->SetVerb(TEXT("GET"));

		HttpRequest->SetDelegateThreadPolicy(EHttpRequestDelegateThreadPolicy::CompleteOnHttpThread);
		HttpRequest->OnRequestProgress64().BindLambda([this, &bRequestProgressTriggered](FHttpRequestPtr Request, uint64 /*BytesSent*/, uint64 BytesReceived) {
			if (!bRequestProgressTriggered)
			{
				// Only do these checks once, because when http request complete, this callback also get triggered
				CHECK(BytesReceived > 0);
				CHECK(BytesReceived < 10/*Chunks*/ * 1024*1024/*ChunkSize*/);
				CHECK(!IsInGameThread());
				CHECK(Request->GetStatus() == EHttpRequestStatus::Processing);
				bRequestProgressTriggered = true;
			}
		});
		HttpRequest->OnProcessRequestComplete().BindLambda([this](FHttpRequestPtr /*HttpRequest*/, FHttpResponsePtr /*HttpResponse */, bool bSucceeded) {
			CHECK(bSucceeded);
			ThreadedHttpRunnable.UnblockGameThread();
		});

		HttpRequest->ProcessRequest();
	});

	ThreadedHttpRunnable.StartTestHttpThread(true/*bBlockGameThread*/);

	CHECK(bRequestProgressTriggered);
}

TEST_CASE_METHOD(FWaitUntilCompleteHttpFixture, "Http request pre check will fail", HTTP_TAG)
{
	DisableWarningsInThisTest();

	TSharedRef<IHttpRequest> HttpRequest = HttpModule->CreateRequest();

	SECTION("when verb was set to empty")
	{
		HttpRequest->SetURL(UrlToTestMethods());
		HttpRequest->SetVerb(TEXT(""));
	}
	SECTION("when url protocol is not valid")
	{
		HttpRequest->SetURL("http_abc://www.epicgames.com");
		HttpRequest->SetVerb(TEXT("GET"));
	}
	SECTION("when url was not set")
	{
		HttpRequest->SetVerb(TEXT("GET"));
	}

	HttpRequest->OnProcessRequestComplete().BindLambda([](FHttpRequestPtr HttpRequest, FHttpResponsePtr HttpResponse, bool bSucceeded) {
		CHECK(!bSucceeded);
	});

	HttpRequest->ProcessRequest();
}

class FValidateHeaderReceiveOrderFixture : public FWaitUntilCompleteHttpFixture
{
public:
	~FValidateHeaderReceiveOrderFixture()
	{
		WaitUntilAllHttpRequestsComplete();
	}

	std::atomic<bool> bHeaderReceived = false;
	std::atomic<bool> bCompleteCallbackTriggered = false;
};

TEST_CASE_METHOD(FValidateHeaderReceiveOrderFixture, "Http request header received callback will be called by thread policy", HTTP_TAG)
{
	TSharedRef<IHttpRequest> HttpRequest = CreateRequest();
	HttpRequest->SetURL(UrlStreamDownload(2/*Chunks*/, 1024/*ChunkSize*/));
	HttpRequest->SetVerb(TEXT("GET"));

	SECTION("in http thread")
	{
		HttpRequest->SetDelegateThreadPolicy(EHttpRequestDelegateThreadPolicy::CompleteOnHttpThread);
		HttpRequest->OnHeaderReceived().BindLambda([this](FHttpRequestPtr Request, const FString& HeaderName, const FString& HeaderValue) {
			CHECK(!bCompleteCallbackTriggered);
			CHECK(!IsInGameThread());
			bHeaderReceived = true;
		});
	}
	SECTION("in game thread")
	{
		HttpRequest->SetDelegateThreadPolicy(EHttpRequestDelegateThreadPolicy::CompleteOnGameThread);
		HttpRequest->OnHeaderReceived().BindLambda([this](FHttpRequestPtr Request, const FString& HeaderName, const FString& HeaderValue) {
			CHECK(!bCompleteCallbackTriggered);
			CHECK(IsInGameThread());
			bHeaderReceived = true;
		});
	}

	HttpRequest->OnProcessRequestComplete().BindLambda([this](FHttpRequestPtr /*HttpRequest*/, FHttpResponsePtr /*HttpResponse */, bool bSucceeded) {
		CHECK(bHeaderReceived);
		bCompleteCallbackTriggered = true;
		CHECK(bSucceeded);
	});

	HttpRequest->ProcessRequest();
}

class FValidateStatusCodeReceiveOrderFixture : public FWaitUntilCompleteHttpFixture
{
public:
	~FValidateStatusCodeReceiveOrderFixture()
	{
		WaitUntilAllHttpRequestsComplete();
	}

	std::atomic<bool> bStatusCodeReceived = false;
	std::atomic<bool> bCompleteCallbackTriggered = false;
};

TEST_CASE_METHOD(FValidateStatusCodeReceiveOrderFixture, "Http request status code received callback will be called by thread policy", HTTP_TAG)
{
	TSharedRef<IHttpRequest> HttpRequest = CreateRequest();
	HttpRequest->SetURL(UrlStreamDownload(20/*Chunks*/, 1024*1024/*ChunkSize*/));
	HttpRequest->SetVerb(TEXT("GET"));

	SECTION("in http thread")
	{
		HttpRequest->SetDelegateThreadPolicy(EHttpRequestDelegateThreadPolicy::CompleteOnHttpThread);
		HttpRequest->OnStatusCodeReceived().BindLambda([this](FHttpRequestPtr Request, int32 StatusCode) {
			CHECK(StatusCode == 200);
			CHECK(!bCompleteCallbackTriggered);
			CHECK(!IsInGameThread());
			bStatusCodeReceived = true;
		});
	}
	SECTION("in game thread")
	{
		HttpRequest->SetDelegateThreadPolicy(EHttpRequestDelegateThreadPolicy::CompleteOnGameThread);
		HttpRequest->OnStatusCodeReceived().BindLambda([this](FHttpRequestPtr Request, int32 StatusCode) {
			CHECK(StatusCode == 200);
			CHECK(!bCompleteCallbackTriggered);
			CHECK(IsInGameThread());
			bStatusCodeReceived = true;
		});
	}

	HttpRequest->OnProcessRequestComplete().BindLambda([this](FHttpRequestPtr /*HttpRequest*/, FHttpResponsePtr /*HttpResponse */, bool bSucceeded) {
		CHECK(bStatusCodeReceived);
		bCompleteCallbackTriggered = true;
		CHECK(bSucceeded);
	});

	HttpRequest->ProcessRequest();
}

namespace UE
{
namespace TestHttp
{

void SetupURLRequestFilter(FHttpModule* HttpModule)
{
	// Pre check will fail when domain is not allowed
	UE::Core::FURLRequestFilter::FRequestMap SchemeMap;
	SchemeMap.Add(TEXT("http"), TArray<FString>{TEXT("epicgames.com")});
	UE::Core::FURLRequestFilter Filter{SchemeMap};
	HttpModule->GetHttpManager().SetURLRequestFilter(Filter);
}

}
}

// Pre-check failed requests won't be added into http manager, so it can't rely on the requested added/completed callback in FWaitUntilCompleteHttpFixture
TEST_CASE_METHOD(FWaitUntilQuitFromTestFixture, "Http request pre check will fail by thread policy", HTTP_TAG)
{
	DisableWarningsInThisTest();

	// Pre check will fail when domain is not allowed
	UE::TestHttp::SetupURLRequestFilter(HttpModule);

	TSharedRef<IHttpRequest> HttpRequest = CreateRequest();
	HttpRequest->SetVerb(TEXT("GET"));
	HttpRequest->SetURL(UrlToTestMethods());

	SECTION("on game thread")
	{
		HttpRequest->OnProcessRequestComplete().BindLambda([this](FHttpRequestPtr HttpRequest, FHttpResponsePtr HttpResponse, bool bSucceeded) {
			CHECK(IsInGameThread());
			CHECK(!bSucceeded);
			bQuitRequested = true;
		});
	}
	SECTION("on http thread")
	{
		HttpRequest->SetDelegateThreadPolicy(EHttpRequestDelegateThreadPolicy::CompleteOnHttpThread);
		HttpRequest->OnProcessRequestComplete().BindLambda([this](FHttpRequestPtr HttpRequest, FHttpResponsePtr HttpResponse, bool bSucceeded) {
			CHECK(!IsInGameThread());
			CHECK(!bSucceeded);
			bQuitRequested = true;
		});
	}

	HttpRequest->ProcessRequest();
}

class FWaitUnitilQuitFromTestThreadedFixture : public FWaitUntilQuitFromTestFixture
{
public:
	~FWaitUnitilQuitFromTestThreadedFixture()
	{
		WaitUntilQuitFromTest();
	}

	FThreadedHttpRunnable ThreadedHttpRunnable;
};

// Pre-check failed requests won't be added into http manager, so it can't rely on the requested added/completed callback in FWaitUntilCompleteHttpFixture
TEST_CASE_METHOD(FWaitUnitilQuitFromTestThreadedFixture, "Threaded http request pre check will fail by thread policy", HTTP_TAG)
{
	DisableWarningsInThisTest();

	ThreadedHttpRunnable.OnRunFromThread().BindLambda([this]() {
		// Pre check will fail when domain is not allowed
		UE::TestHttp::SetupURLRequestFilter(HttpModule);

		TSharedRef<IHttpRequest> HttpRequest = CreateRequest();
		HttpRequest->SetVerb(TEXT("GET"));
		HttpRequest->SetURL(UrlToTestMethods());

		SECTION("on game thread")
		{
			HttpRequest->OnProcessRequestComplete().BindLambda([this](FHttpRequestPtr HttpRequest, FHttpResponsePtr HttpResponse, bool bSucceeded) {
				CHECK(IsInGameThread());
				CHECK(!bSucceeded);
				bQuitRequested = true;
			});
		}
		SECTION("on http thread")
		{
			HttpRequest->SetDelegateThreadPolicy(EHttpRequestDelegateThreadPolicy::CompleteOnHttpThread);
			HttpRequest->OnProcessRequestComplete().BindLambda([this](FHttpRequestPtr HttpRequest, FHttpResponsePtr HttpResponse, bool bSucceeded) {
				CHECK(!IsInGameThread());
				CHECK(!bSucceeded);
				bQuitRequested = true;
			});
		}

		HttpRequest->ProcessRequest();
	});

	ThreadedHttpRunnable.StartTestHttpThread(false/*bBlockGameThread*/);
}

TEST_CASE_METHOD(FWaitUntilCompleteHttpFixture, "Cancel http request connect before timeout", HTTP_TAG)
{
	DisableWarningsInThisTest();

	TSharedRef<IHttpRequest> HttpRequest = CreateRequest();
	HttpRequest->SetURL(UrlWithInvalidPortToTestConnectTimeout());
	HttpRequest->SetVerb(TEXT("GET"));
	HttpRequest->SetTimeout(7);
	const double StartTime = FPlatformTime::Seconds();
	HttpRequest->OnProcessRequestComplete().BindLambda([StartTime](FHttpRequestPtr HttpRequest, FHttpResponsePtr HttpResponse, bool bSucceeded) {
		CHECK(!bSucceeded);
		const double DurationInSeconds = FPlatformTime::Seconds() - StartTime;
		CHECK(DurationInSeconds < 2.0);
		CHECK(HttpRequest->GetFailureReason() == EHttpFailureReason::Cancelled);
	});
	HttpRequest->ProcessRequest();
	FPlatformProcess::Sleep(0.5);
	HttpRequest->CancelRequest();
}

class FThreadedBatchRequestsFixture : public FWaitThreadedHttpFixture
{
public:
	void LaunchBatchRequests(uint32 BatchSize)
	{
		for (uint32 i = 0; i < BatchSize; ++i)
		{
			TSharedRef<IHttpRequest> HttpRequest = CreateRequest();
			HttpRequest->SetURL(UrlStreamDownload(3, 1024*1024));
			HttpRequest->SetVerb(TEXT("GET"));
			HttpRequest->ProcessRequest();
		}
	}

	void BlockUntilFlushed()
	{
		if (bRetryEnabled)
		{
			HttpRetryManager->BlockUntilFlushed(5.0);
		}
		else
		{
			HttpModule->GetHttpManager().Flush(EHttpFlushReason::Default);
		}
	}
};

TEST_CASE_METHOD(FThreadedBatchRequestsFixture, "Retry manager and http manager is thread safe", HTTP_TAG)
{
	DisableWarningsInThisTest();

	ThreadedHttpRunnable.OnRunFromThread().BindLambda([this]() {
		LaunchBatchRequests(10);
		BlockUntilFlushed();
	});
	ThreadedHttpRunnable.StartTestHttpThread(false/*bBlockGameThread*/);

	LaunchBatchRequests(10);
	BlockUntilFlushed();
}

#if (PLATFORM_WINDOWS && !WITH_CURL_XCURL) || PLATFORM_MAC || PLATFORM_UNIX

TEST_CASE_METHOD(FWaitUntilCompleteHttpFixture, "Scheme besides http and https can work if allowed by settings", HTTP_TAG)
{
	bool bShouldSucceed = false;
	SECTION("when allowed")
	{
		bShouldSucceed = true;
	}
	SECTION("when not allowed")
	{
		DisableWarningsInThisTest();
		// Pre check will fail when scheme is not listed
		UE::TestHttp::SetupURLRequestFilter(HttpModule);
	}

	FString Filename = FString(FPlatformProcess::UserSettingsDir()) / TEXT("TestProtocolAllowed.dat");
	UE::TestHttp::WriteTestFile(Filename, 10/*Bytes*/);

	TSharedRef<IHttpRequest> HttpRequest = HttpModule->CreateRequest();
	HttpRequest->SetURL(FString(TEXT("file://")) + Filename.Replace(TEXT(" "), TEXT("%20")));
	HttpRequest->SetVerb(TEXT("GET"));
	HttpRequest->OnProcessRequestComplete().BindLambda([Filename, bShouldSucceed](FHttpRequestPtr HttpRequest, FHttpResponsePtr HttpResponse, bool bSucceeded) {
		CHECK(bSucceeded == bShouldSucceed);
		IFileManager::Get().Delete(*Filename);
	});
	HttpRequest->ProcessRequest();
}

class FLocalHttpServerFixture : public FWaitUntilCompleteHttpFixture
{
public:
	FLocalHttpServerFixture()
	{
		HttpServerModule = new FHttpServerModule();
		IModuleInterface* Module = HttpServerModule;
		Module->StartupModule();

		HttpRouter = HttpServerModule->GetHttpRouter(LocalHttpServerPort);
		CHECK(HttpRouter.IsValid());
	}

	void StartServerWithHandler(const FHttpPath& HttpPath, EHttpServerRequestVerbs Verb, FHttpRequestHandler RequestHandler)
	{
		CHECK(HttpRouteHandle == nullptr);
		HttpRouteHandle = HttpRouter->BindRoute(HttpPath, Verb, RequestHandler);
		HttpServerModule->StartAllListeners();
	}

	~FLocalHttpServerFixture()
	{
		while (HasOngoingRequest())
		{
			HttpServerModule->Tick(TickFrequency);
			HttpModule->GetHttpManager().Tick(TickFrequency);
			FPlatformProcess::Sleep(TickFrequency);
		}

		HttpRouter->UnbindRoute(HttpRouteHandle);
		HttpRouter.Reset();

		IModuleInterface* Module = HttpServerModule;
		Module->ShutdownModule();
		delete Module;
	}

	TSharedPtr<IHttpRouter> HttpRouter;
	FHttpRouteHandle HttpRouteHandle;
	FHttpServerModule* HttpServerModule = nullptr;
	uint32 LocalHttpServerPort = 9000;
};

TEST_CASE_METHOD(FLocalHttpServerFixture, "Local http server can serve large file", HTTP_TAG)
{
	const uint32 FileSize = 100 * 1024 * 1024; // 100 MB seems good enough to repro SE_EWOULDBLOCK or SE_TRY_AGAIN on Mac
	StartServerWithHandler(FHttpPath(TEXT("/large_file")), EHttpServerRequestVerbs::VERB_GET, FHttpRequestHandler::CreateLambda([FileSize](const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete) {
		TArray<uint8> ResultData;
		ResultData.SetNum(FileSize);
		FMemory::Memset(ResultData.GetData(), 'd', FileSize);

		OnComplete(FHttpServerResponse::Create(MoveTemp(ResultData), TEXT("text/text")));
		return true;
	}));

	// Start client request
	TSharedRef<IHttpRequest> HttpRequest = CreateRequest();
	HttpRequest->SetURL(TEXT("http://localhost:9000/large_file"));
	HttpRequest->SetVerb(TEXT("GET"));
	HttpRequest->OnProcessRequestComplete().BindLambda([FileSize](FHttpRequestPtr HttpRequest, FHttpResponsePtr HttpResponse, bool bSucceeded) {
		CHECK(bSucceeded);
		REQUIRE(HttpResponse != nullptr);
		CHECK(HttpResponse->GetContentLength() == FileSize);
	});
	HttpRequest->ProcessRequest();
}

#endif // (PLATFORM_WINDOWS && !WITH_CURL_XCURL) || PLATFORM_MAC || PLATFORM_UNIX

// TODO: Add cancel test, with multiple cancel calls

