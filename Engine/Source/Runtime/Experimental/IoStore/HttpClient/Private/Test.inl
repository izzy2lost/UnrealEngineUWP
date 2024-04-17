// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

namespace UE::IoStore::HTTP
{

// {{{1 test ...................................................................

#if !(UE_BUILD_SHIPPING|UE_BUILD_TEST)

////////////////////////////////////////////////////////////////////////////////
static void MiscTest()
{
#define CRLF "\r\n"
	struct {
		FAnsiStringView Input;
		int32 Output;
	} FmtTestCases[] = {
		{ "", -1 },
		{ "abcd", -1 },
		{ "abcd\r", -1 },
		{ CRLF "\r\r", -1 },
		{ CRLF CRLF, 4 },
		{ "abc" CRLF CRLF, 7 },
	};
	for (const auto [Input, Output] : FmtTestCases)
	{
		check(FindMessageTerminal(Input.GetData(), Input.Len()) == Output);
	}

	FMessageOffsets MsgOut;
	check(ParseMessage("", MsgOut) == -1);
	check(ParseMessage("MR", MsgOut) == -1);
	check(ParseMessage("HTTP/1.1", MsgOut) == -1);
	check(ParseMessage("HTTP/1.1 ", MsgOut) == -1);
	check(ParseMessage("HTTP/1.1 1" CRLF, MsgOut) > 0);
	check(ParseMessage("HTTP/1.1    1" CRLF, MsgOut) > 0);
	check(ParseMessage("HTTP/1.1 100 " CRLF, MsgOut) > 0);
	check(ParseMessage("HTTP/1.1 100  Message of some sort    " CRLF, MsgOut) > 0);
	check(ParseMessage("HTTP/1.1 100 _Message with a \r in it" CRLF, MsgOut) == -1);

	bool AllIsWell = true;
	auto NotExpectedToBeCalled = [&AllIsWell] (auto, auto)
	{
		AllIsWell = false;
		return false;
	};

	EnumerateHeaders("",		NotExpectedToBeCalled); check(AllIsWell);
	EnumerateHeaders(CRLF,		NotExpectedToBeCalled); check(AllIsWell);
	EnumerateHeaders("foo",		NotExpectedToBeCalled); check(AllIsWell);
	EnumerateHeaders(" foo",	NotExpectedToBeCalled); check(AllIsWell);
	EnumerateHeaders(" foo ",	NotExpectedToBeCalled); check(AllIsWell);
	EnumerateHeaders("foo:bar",	NotExpectedToBeCalled); check(AllIsWell);

	auto IsBar = [&] (auto, auto Value) { return AllIsWell = (Value == "bar"); };
	EnumerateHeaders("foo: bar" CRLF,		IsBar); check(AllIsWell);
	EnumerateHeaders("foo: bar \t" CRLF,	IsBar); check(AllIsWell);
	EnumerateHeaders("foo:\tbar " CRLF,		IsBar); check(AllIsWell);
	EnumerateHeaders("foo:bar " CRLF,		IsBar); check(AllIsWell);
	EnumerateHeaders("foo:bar" CRLF "!",	IsBar); check(AllIsWell);
	EnumerateHeaders("foo:bar" CRLF " ",	IsBar); check(AllIsWell);
	EnumerateHeaders("foo:bar" CRLF "n:ej",	IsBar); check(AllIsWell);

	check(CrudeToInt("") < 0);
	check(CrudeToInt("X") < 0);
	check(CrudeToInt("/") < 0);
	check(CrudeToInt(":") < 0);
	check(CrudeToInt("-1") < -1);
	check(CrudeToInt("0") == 0);
	check(CrudeToInt("9") == 9);
	check(CrudeToInt("493") == 493);

	FUrlOffsets UrlOut;

	check(ParseUrl("", UrlOut) == -1);
	check(ParseUrl("abc://asd/", UrlOut) == -1);
	check(ParseUrl("http://", UrlOut) == -1);
	check(ParseUrl("http://:/", UrlOut) == -1);
	check(ParseUrl("http://@:/", UrlOut) == -1);
	check(ParseUrl("http://foo:ba:r/", UrlOut) == -1);
	check(ParseUrl("http://foo@ba:r/", UrlOut) == -1);
	check(ParseUrl("http://foo@ba:r", UrlOut) == -1);
	check(ParseUrl("http://foo@ba:/", UrlOut) == -1);
	check(ParseUrl("http://foo@ba@9/", UrlOut) == -1);
	check(ParseUrl("http://@ba:9/", UrlOut) == -1);
	check(ParseUrl(
		"http://zzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz"
		"zzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz"
		"zzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz"
		"zzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz"
		"zzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz.com",
		UrlOut) == -1);

	check(ParseUrl("http://ab-c.com/", UrlOut) > 0);
	check(ParseUrl("http://a@bc.com/", UrlOut) > 0);
	check(ParseUrl("https://abc.com", UrlOut) > 0);
	check(ParseUrl("https://abc.com:999", UrlOut) > 0);
	check(ParseUrl("https://abc.com:999/", UrlOut) > 0);
	check(ParseUrl("https://foo:bar@abc.com:999", UrlOut) > 0);
	check(ParseUrl("https://foo:bar@abc.com:999/", UrlOut) > 0);
	check(ParseUrl("https://foo_bar@abc.com:999", UrlOut) > 0);
	check(ParseUrl("https://foo_bar@abc.com:999/", UrlOut) > 0);

	for (int32 i : { 0x10, 0x20, 0x40, 0x7f, 0xff })
	{
		char Url[] = "http://stockholm.patchercache.epicgames.net:123";
		char Buffer[512];
		std::memset(Buffer, i, sizeof(Buffer));
		std::memcpy(Buffer, Url, sizeof(Url) - 1);
		check(ParseUrl(FAnsiStringView(Buffer, sizeof(Url) - 1), UrlOut) > 0);
		check(UrlOut.Port.Get(Url) == "123");
	}

	FAnsiStringView Url = "http://abc:123@bc.com:999/";
	check(ParseUrl(Url, UrlOut) > 0);
	check(UrlOut.SchemeLength == 4);
	check(UrlOut.UserInfo.Get(Url) == "abc:123");
	check(UrlOut.HostName.Get(Url) == "bc.com");
	check(UrlOut.Port.Get(Url) == "999");
	check(UrlOut.Path == 25);
#undef CRLF

	static const char* OutcomeMsg = "\x4d\x52";
	check(FOutcome::Error(OutcomeMsg, -5).IsOk() == false);
	check(FOutcome::Error(OutcomeMsg, -5).IsWaiting() == false);
	check(FOutcome::Error(OutcomeMsg, -5).IsError());
	check(FOutcome::Error(OutcomeMsg, -5).GetErrorCode() == -5);
	check(FOutcome::Error(OutcomeMsg,  5).GetErrorCode() ==  5);
	check(FOutcome::Error(OutcomeMsg, -5).GetMessage() == OutcomeMsg);

	check(FOutcome::Ok(  0).IsOk());
	check(FOutcome::Ok(-13).IsWaiting() == false);
	check(FOutcome::Ok( 13).IsError() == false);

	check(FOutcome::Waiting().IsOk() == false);
	check(FOutcome::Waiting().IsWaiting());
	check(FOutcome::Waiting().IsError() == false);
}

////////////////////////////////////////////////////////////////////////////////
static void ThrottleTest(FAnsiStringView TestUrl)
{
	enum { TheMax = 0x7fff'fffful };

	check(FThrottler().GetAllowance() >= TheMax);

	FThrottler Throttler;
	uint64 OneSecond = Throttler.CycleFreq;

	for (uint32 TargetElapsed : { 1, 5, 7 })
	{
		uint64 CycleTest = FPlatformTime::Cycles64();
		for (uint32 i = 0; i < TargetElapsed; ++i)
		{
			FPlatformProcess::Sleep(1.0f);
		}
		CycleTest = FPlatformTime::Cycles64() - CycleTest;
		check((CycleTest + (OneSecond / 8)) / OneSecond == TargetElapsed);
	}


	Throttler.SetLimit(0);
	check(Throttler.GetAllowance(0) >= TheMax);
	check(Throttler.GetAllowance()  >= TheMax);
	check(Throttler.GetAllowance()  >= TheMax);

	for (uint32 i : { 10, 63, 100 })
	{
		Throttler.SetLimit(i);
		check(Throttler.GetAllowance( OneSecond          ) == (i << 10));
		check(Throttler.GetAllowance((OneSecond + 1) >> 1) == (i << 9));
		check(Throttler.GetAllowance((OneSecond + 3) >> 2) == (i << 8));
	}

	uint32 Limit = 17 << 10;
	Throttler = FThrottler();
	Throttler.SetLimit(Limit >> 10);
	Throttler.GetAllowance(OneSecond);
	Throttler.ReturnUnused(Limit >> 1);
	check(Throttler.GetAllowance(OneSecond) == (Limit + (Limit >> 1)));

	// runaway
	check(Throttler.GetAllowance(OneSecond * 3) == Limit * 3);
	check(Throttler.GetAllowance(OneSecond * 4) == Limit * 4);
	check(Throttler.GetAllowance(OneSecond * 5) == Limit * 4);

	// idle
	check(Throttler.GetAllowance(OneSecond * 64) == Limit);

	// threshold
	Limit = FThrottler::THRESHOLD;
	Throttler = FThrottler();
	Throttler.SetLimit(Limit >> 10);
	for (int64 Counter = OneSecond;;)
	{
		int64 Delta = (OneSecond + 15) >> 4;
		Counter -= Delta;
		int32 Allowance = Throttler.GetAllowance(Delta);
		if (Allowance > 0)
		{
			check(Allowance == Limit);
			check(Counter < 10);
			break;
		}
	};

	// overflow(ish)
	Throttler = FThrottler();
	Throttler.SetLimit((512 << 10) - 1);
	Throttler.GetAllowance((OneSecond * 790) / 100);

	// timing test
	FIoBuffer RecvData;
	for (uint32 SizeKiB : { 10, 25, 60 })
	{
		const uint32 ThrottleKiB = 5;

		TAnsiStringBuilder<128> Url;
		Url << TestUrl;
		Url << (SizeKiB << 10);

		FEventLoop Loop;
		Loop.Throttle(ThrottleKiB);

		FRequest Request = Loop.Request("GET", Url).Accept("*/*");
		Loop.Send(MoveTemp(Request), [&] (const FTicketStatus& Status) {
			check(Status.GetId() != FTicketStatus::EId::Error);
			if (Status.GetId() == FTicketStatus::EId::Response)
			{
				Status.GetResponse().SetDestination(&RecvData);
			}
		});

		int32 Timeout = -1;
		if (SizeKiB < 25) Timeout = 123;
		if (SizeKiB > 25) Timeout = 4567;

		uint64 Time = FPlatformTime::Cycles64();
		while (Loop.Tick(Timeout));
		Time = FPlatformTime::Cycles64() - Time;
		Time /= OneSecond;

		// It's dangerous stuff testing elapsed time you know. The +1 is because
		// throttling assumes one second has already passed when initialised.
#if PLATFORM_WINDOWS
		check(Time + 1 == (SizeKiB / ThrottleKiB));
#endif

		RecvData = FIoBuffer();
	}
}

////////////////////////////////////////////////////////////////////////////////
static void RedirectTest(const ANSICHAR* TestHost)
{
	FEventLoop Loop;

	auto WaitForLoopIdle = [&] {
		for (; Loop.Tick(-1); FPlatformProcess::SleepNoStats(0.02f));
	};

	FEventLoop::FRequestParams RequestParams = {
		.bAutoRedirect = true,
	};

	enum ReTyp { ReAbs, ReRel };
	enum { RecvDataSize = 48 };

	TAnsiStringBuilder<64> Builder;
	auto BuildUrl = [&] (ReTyp Typ, uint32 Code) -> const auto&
	{
		Builder.Reset();
		Builder << "https://";
		Builder << TestHost;
		Builder << ":9493";
		Builder << "/redirect";
		Builder << ((Typ < ReRel) ? "/abs/" : "/rel/");
		Builder << Code;
		Builder << "/data/" << uint32(RecvDataSize);
		return Builder;
	};

	UPTRINT SinkParam = 0xaa'493'493'493'493'bbull;

	uint32 RecvCount;
	auto OkSink = [Dest=FIoBuffer(), SinkParam, &RecvCount] (const FTicketStatus& Status) mutable {
		check(Status.GetParam() == SinkParam);
		check(Status.GetId() != FTicketStatus::EId::Error);
		if (Status.GetId() == FTicketStatus::EId::Response)
		{
			FResponse& Response = Status.GetResponse();
			check(Response.GetStatusCode() == 200);
			Response.SetDestination(&Dest);
			return;
		}
		check(Status.GetId() == FTicketStatus::EId::Content);
		RecvCount += uint32(Dest.GetSize());
	};

	uint32 TestCodes[] = { 301, 302, 307, 308 };

	for (auto ReTest : { ReAbs, ReRel })
	{
		RecvCount = 0;
		for (uint32 Code : TestCodes)
		{
			FRequest Request = Loop.Get(BuildUrl(ReTest, Code), &RequestParams);
			if (Code > TestCodes[1])
			{
				Request.Header("TestCodeHeader", "Header-Of-Test-Codes");
			}
			Loop.Send(MoveTemp(Request), OkSink, SinkParam);
		}
		WaitForLoopIdle();
		check(RecvCount == RecvDataSize * UE_ARRAY_COUNT(TestCodes));
	}

	RequestParams = FEventLoop::FRequestParams();
	RequestParams.bAutoRedirect = true;

	FConnectionPool::FParams Params;
	Params.SetHostFromUrl(BuildUrl(ReAbs, 0));
	Params.ConnectionCount = 4;
	FConnectionPool Pool(Params);

	RecvCount = 0;
	Loop.Send(Loop.Get("/redirect/abs/307/data/55", Pool, &RequestParams), OkSink, SinkParam);
	WaitForLoopIdle();
	check(RecvCount == 55);
}

////////////////////////////////////////////////////////////////////////////////
static void HttpTest(const ANSICHAR* TestHost)
{
	TAnsiStringBuilder<64> Ret;
	auto BuildUrl = [&] (const ANSICHAR* Suffix=nullptr, uint32 Port=9493) -> const auto&
	{
		Ret.Reset();
		Ret << "http://";
		Ret << TestHost;
		Ret << ":" << Port;
		return (Suffix != nullptr) ? (Ret << Suffix) : Ret;
	};

	struct
	{
		FIoBuffer Dest;
		uint64 Hash = 0;
	} Content[64];

	auto HashSink = [&] (const FTicketStatus& Status) -> FIoBuffer*
	{
		check(Status.GetId() != FTicketStatus::EId::Error);

		uint32 Index = Status.GetIndex();

		if (Status.GetId() == FTicketStatus::EId::Response)
		{
			FResponse& Response = Status.GetResponse();
			check(Response.GetStatus() == EStatusCodeClass::Successful);
			check(Response.GetStatusCode() == 200);
			check(Response.GetContentLength() == Status.GetContentLength());

			FAnsiStringView HashView = Response.GetHeader("X-TestServer-Hash");
			Content[Index].Hash = CrudeToInt(HashView);
			check(int64(Content[Index].Hash) > 0);

			Content[Index].Dest = FIoBuffer();
			Response.SetDestination(&(Content[Index].Dest));
			return nullptr;
		}

		uint32 ReceivedHash = 0x493;
		FMemoryView ContentView = Content[Index].Dest.GetView();
		check(ContentView.GetSize() == Status.GetContentLength());
		for (uint32 i = 0; i < Status.GetContentLength(); ++i)
		{
			uint8 c = ((const uint8*)(ContentView.GetData()))[i];
			ReceivedHash = (ReceivedHash + c) * 0x493;
		}
		check(Content[Index].Hash == ReceivedHash);
		Content[Index].Hash = 0;

		return nullptr;
	};

	auto NullSink = [] (const FTicketStatus&) {};

	auto NoErrorSink = [&] (const FTicketStatus& Status)
	{
		check(Status.GetId() != FTicketStatus::EId::Error);
		if (Status.GetId() != FTicketStatus::EId::Response)
		{
			return;
		}

		uint32 Index = Status.GetIndex();

		FResponse& Response = Status.GetResponse();
		Content[Index].Dest = FIoBuffer();
		Response.SetDestination(&(Content[Index].Dest));
	};

	FEventLoop Loop;
	volatile bool LoopStop = false;
	volatile bool LoopTickDelay = false;
	auto LoopTask = UE::Tasks::Launch(TEXT("IasHttpTest.Loop"), [&] () {
		uint32 DelaySeed = 493;
		while (!LoopStop)
		{
			while (Loop.Tick())
			{
				if (!LoopTickDelay)
				{
					continue;
				}

				float DelayFloat = float(DelaySeed % 75) / 1000.0f;
				FPlatformProcess::SleepNoStats(DelayFloat);
				DelaySeed *= 0xa93;
			}

			FPlatformProcess::SleepNoStats(0.005f);
		}
	});

	auto WaitForLoopIdle = [&Loop] ()
	{
		FPlatformProcess::SleepNoStats(0.25f);
		while (!Loop.IsIdle())
		{
			FPlatformProcess::SleepNoStats(0.03f);
		}
	};

	// unused request
	{
		FRequest Request = Loop.Request("GET", BuildUrl("/data"));
	}

	// foundational
	{
		FRequest Request = Loop.Request("GET", BuildUrl("/seed/493"));
		Request.Accept(EMimeType::Json);

		FTicket Ticket = Loop.Send(MoveTemp(Request), NullSink);

		WaitForLoopIdle();
	}

	// convenience
	{
		FRequest Request = Loop.Get(BuildUrl("/data")).Accept(EMimeType::Json);

		FTicket Tickets[] = {
			Loop.Send(MoveTemp(Request), HashSink),
			Loop.Send(Loop.Get(BuildUrl("/data")).Accept(EMimeType::Json), HashSink),
			Loop.Send(Loop.Get("http://httpbin.org/get"), NoErrorSink),
		};
		WaitForLoopIdle();
	}

	// convenience
	{
		FRequest Request = Loop.Get(BuildUrl("/data")).Accept(EMimeType::Json);
		FTicket Ticket = Loop.Send(MoveTemp(Request), HashSink);
		WaitForLoopIdle();
	}

	// pool
	for (uint16 i = 1; i < 64; ++i)
	{
		FConnectionPool::FParams Params;
		Params.SetHostFromUrl(BuildUrl());
		Params.ConnectionCount = (i % 2) + 1;
		FConnectionPool Pool(Params);
		for (int32 j = 0; j < i; ++j)
		{
			FRequest Request = Loop.Get("/data", Pool);
			Loop.Send(MoveTemp(Request), HashSink);
		}
		WaitForLoopIdle();
	}

	// fatal timeout
	for (int32 i = 0; i < 14; ++i)
	{
		bool bExpectFailTimeout = !!(i & 1);
		auto Sink = [bExpectFailTimeout, Dest=FIoBuffer()] (const FTicketStatus& Status) mutable
		{
			if (Status.GetId() == FTicketStatus::EId::Response)
			{
				FResponse& Response = Status.GetResponse();
				Response.SetDestination(&Dest);
				return;
			}

			check(Status.GetId() == FTicketStatus::EId::Error);

			const char* Reason = Status.GetErrorReason();
			bool IsFailTimeout = (FCStringAnsi::Strstr(Reason, "FailTimeout") != nullptr);
			check(IsFailTimeout == bExpectFailTimeout);
		};

		auto ErrorSink = [] (const FTicketStatus& Status)
		{
			check(Status.GetId() == FTicketStatus::EId::Error);
		};

		FConnectionPool::FParams Params;
		Params.SetHostFromUrl(BuildUrl());
		FConnectionPool Pool(Params);

		FEventLoop Loop2;
		Loop2.Send(Loop2.Get("/data?stall=1", Pool), Sink);

		// Requests are pipelined. The second one will get went during the stall so
		// we expect it to fail. The subsequent ones are expected to succeed.
		Loop2.Send(Loop2.Get("/data", Pool), ErrorSink);
		Loop2.Send(Loop2.Get("/data", Pool), HashSink);
		Loop2.Send(Loop2.Get("/data", Pool), HashSink);

		int32 PollTimeoutMs = -1;
		if (bExpectFailTimeout)
		{
			Loop2.SetFailTimeout(1000);

			if ((i & 3) == 1)
			{
				PollTimeoutMs = 1000;
			}
		}
		while (Loop2.Tick(PollTimeoutMs));

		Loop2.Send(Loop2.Get("/data/23", Pool), NoErrorSink);
		while (Loop2.Tick(PollTimeoutMs));
	}

	// no connect
	{
		FRequest Requests[] = {
			Loop.Request("GET", BuildUrl(nullptr, 10930)),
			Loop.Request("GET", "http://thisdoesnotexistihope/"),
		};
		Loop.Send(MoveTemp(Requests[0]), NullSink);
		Loop.Send(MoveTemp(Requests[1]), NullSink);
		WaitForLoopIdle();
	}

	// head and large requests
	{
		auto MixTh = [Th=uint32(0)] () mutable { return (Th = (Th * 75) + 74) & 255; };

		char AsciiData[257];
		for (char& c : AsciiData)
		{
			int32 i = int32(ptrdiff_t(&c - AsciiData));
			c = 0x41 + (MixTh() % 26);
			c += (MixTh() & 2) ? 0x20 : 0;
		}

		for (int32 i = 0; (i += 69493) < 2 << 20;)
		{
			FRequest Request = Loop.Request("HEAD", BuildUrl("/data"));
			for (int32 j = i; j > 0;)
			{
				FAnsiStringView Name(AsciiData, MixTh() + 1);
				FAnsiStringView Value(AsciiData, MixTh() + 1);
				Request.Header(Name, Value);
				j -= Name.Len() + Value.Len();

			}

			Loop.Send(MoveTemp(Request), [] (const FTicketStatus& Status) {
				if (Status.GetId() == FTicketStatus::EId::Response)
				{
					FResponse& Response = Status.GetResponse();
					check(Response.GetStatusCode() == 431); // "too many headers"
				}
			});

			WaitForLoopIdle();
		}
	}

	// stress 1
	{
		const uint32 StressLoad = 32;

		struct {
			const ANSICHAR* Uri;
			bool Disconnect;
		} StressUrls[] = {
			{ "/data?slowly=1",		false },
			{ "/data?disconnect=1",	true },
		};

		uint64 Errors = 0;
		auto ErrorSink = [&] (const FTicketStatus& Status)
		{
			FTicket Ticket = Status.GetTicket();
			uint32 Index = uint32(63 - FMath::CountLeadingZeros64(uint64(Ticket)));

			if (Status.GetId() == FTicketStatus::EId::Error)
			{
				Errors |= 1ull << Index;
				return;
			}

			else if (Status.GetId() == FTicketStatus::EId::Response)
			{
				FResponse& Response = Status.GetResponse();
				Content[Index].Dest = FIoBuffer();
				Response.SetDestination(&(Content[Index].Dest));
				return;
			}

			check(false);
		};

		for (const auto& [StressUri, ExpectDisconnect] : StressUrls)
		{
			FTicketSink Sink = ExpectDisconnect ? FTicketSink(ErrorSink) : FTicketSink(HashSink);

			const auto& StressUrl = BuildUrl(StressUri);
			for (bool AddDelay : {false, true})
			{
				FTicket Tickets[StressLoad];
				for (FTicket& Ticket : Tickets)
				{
					Ticket = Loop.Send(Loop.Get(StressUrl).Header("Accept", "*/*"), Sink);
				}

				LoopTickDelay = AddDelay;

				WaitForLoopIdle();
			}

			LoopTickDelay = false;
		}
	}

	// stress 2
	{
		const uint32 StressLoad = 3;
		const uint32 StressTaskCount = 7;
		static_assert(StressLoad * StressTaskCount <= 32);

		FAnsiStringView Url = BuildUrl("/data");

		auto StressTaskEntry = [&] {
			for (uint32 i = 0; i < StressLoad; ++i)
			{
				FTicket Ticket = Loop.Send(Loop.Get(Url), HashSink);
				if (!Ticket)
				{
					FPlatformProcess::SleepNoStats(0.01f);
					--i;
				}
			}
		};

		UE::Tasks::FTask StressTasks[StressTaskCount];
		for (auto& StressTask : StressTasks)
		{
			StressTask = UE::Tasks::Launch(TEXT("StressTask"), [&] { StressTaskEntry(); });
		}
		for (auto& StressTask : StressTasks)
		{
			StressTask.Wait();
		}

		WaitForLoopIdle();
	}

	// tamper
	for (int32 i = 1; i <= 100; ++i)
	{
		TAnsiStringBuilder<32> TamperUrl;
		TamperUrl << "/data?tamper=" << i;
		FAnsiStringView Url = BuildUrl(TamperUrl.ToString());

		for (int j = 0; j < 48; ++j)
		{
			FRequest Request = Loop.Request("GET", Url);
			Loop.Send(MoveTemp(Request), NullSink);
		}

		WaitForLoopIdle();
	}

	LoopStop = true;
	LoopTask.Wait();

	check(Loop.IsIdle());

#if IS_PROGRAM
	ThrottleTest(BuildUrl("/data/"));
#endif

	// pre-generated headers
	// request-with-body
	// proxy
	// chunked transfer encoding
	// gzip / deflate
	// redirects
	// loop multi-req.
	// tls
	// url auth credentials
	// transfer-file / splice / sendfile
	// (header field parser)
	// (form-data)
	// (cookies)
	// (cache)
	// (websocket)
	// (ipv6)
	// (utf-8 host names)
}

////////////////////////////////////////////////////////////////////////////////
IOSTOREHTTPCLIENT_API void IasHttpTest(const ANSICHAR* TestHost="localhost")
{
#if PLATFORM_WINDOWS
	WSADATA WsaData;
	if (WSAStartup(MAKEWORD(2, 2), &WsaData) == 0x0a9e0493)
		return;
	ON_SCOPE_EXIT { WSACleanup(); };
#endif

	MiscTest();
	HttpTest(TestHost);
	RedirectTest(TestHost);
}

#endif // !SHIP|TEST
// }}}

} // namespace UE::IoStore::HTTP

