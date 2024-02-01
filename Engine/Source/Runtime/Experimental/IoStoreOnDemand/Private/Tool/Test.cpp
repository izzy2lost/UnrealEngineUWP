// Copyright Epic Games, Inc. All Rights Reserved.

#if defined(UE_WITH_IAS_TOOL)

#include "Command.h"

#include <Async/TaskGraphInterfaces.h>
#include <Containers/StringConv.h>
#include <Containers/UnrealString.h>
#include <Misc/CommandLine.h>

namespace UE::IO::IAS {

////////////////////////////////////////////////////////////////////////////////
namespace IasJournaledFileCacheTest { void Tests(const TCHAR*);				}
namespace HTTP						{ void IasHttpTest(const ANSICHAR*);	}

namespace Tool {

////////////////////////////////////////////////////////////////////////////////
void CommandTest();

////////////////////////////////////////////////////////////////////////////////
static void HttpTests(const FContext& Context)
{
	auto TestHost = Context.Get<FStringView>(TEXT("-Host"), TEXT("localhost"));

	auto TestHostAnsi = StringCast<ANSICHAR>(TestHost.GetData());
	HTTP::IasHttpTest(TestHostAnsi.Get());
}

////////////////////////////////////////////////////////////////////////////////
static void CacheTests(const FContext& Context)
{
	FStringView CacheDirStr = Context.Get<FStringView>(TEXT("-Dir"));

	const TCHAR* CacheDir = CacheDirStr.IsEmpty() ? nullptr : CacheDirStr.GetData();
	IasJournaledFileCacheTest::Tests(CacheDir);
}

////////////////////////////////////////////////////////////////////////////////
static int32 TestCommandEntry(const FContext& Context)
{
	CommandTest();
	CacheTests(Context);
	HttpTests(Context);
	return 0;
}

////////////////////////////////////////////////////////////////////////////////
static FCommand TestCommand(
	TestCommandEntry,
	TEXT("Test"),
	TEXT("Run IAS tests"),
	{
		TArgument<FStringView>(TEXT("-Host"), TEXT("Host of the HTTP test server")),
		TArgument<FStringView>(TEXT("-Dir"), TEXT("Primary directory to use for cache tests")),
	}
);

} // namespace Tool
} // namespace UE::IO::IAS

#endif // UE_WITH_IAS_TOOL
