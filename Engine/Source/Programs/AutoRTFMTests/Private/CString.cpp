// Copyright Epic Games, Inc. All Rights Reserved.

#include "Catch2Includes.h"
#include <AutoRTFM/AutoRTFM.h>
#include <cstring>

#ifdef __clang__
#define CLANG_BEGIN_DISABLE_OPTIMIZATIONS  _Pragma("clang optimize off")
#define CLANG_END_DISABLE_OPTIMIZATIONS    _Pragma("clang optimize on")
#else
#define CLANG_BEGIN_DISABLE_OPTIMIZATIONS
#define CLANG_END_DISABLE_OPTIMIZATIONS
#endif

#ifdef _MSC_VER
#define MSVC_BEGIN_DISABLE_WARN_UNSAFE_FUNCTION \
		__pragma(warning(push)) \
		__pragma(warning(disable : 4996))
#define MSVC_END_DISABLE_UNSAFE_FN  __pragma(warning(pop))
#else
#define MSVC_BEGIN_DISABLE_WARN_UNSAFE_FUNCTION
#define MSVC_END_DISABLE_UNSAFE_FN
#endif

TEST_CASE("CString.memcpy")
{
	const char* const From = "Kittie says meow";
	char To[] = "Doggie says woof";

	SECTION("With Abort")
	{
		CLANG_BEGIN_DISABLE_OPTIMIZATIONS
		AutoRTFM::ETransactionResult Result = AutoRTFM::Transact([&]
		{
			memcpy(To, From, 6);
			AutoRTFM::AbortTransaction();
		});
		CLANG_END_DISABLE_OPTIMIZATIONS

		REQUIRE(AutoRTFM::ETransactionResult::AbortedByRequest == Result);
		REQUIRE("Doggie says woof" == std::string_view(To));
	}

	SECTION("With Commit")
	{
		CLANG_BEGIN_DISABLE_OPTIMIZATIONS
		AutoRTFM::Commit([&] { memcpy(To, From, 6); });
		CLANG_END_DISABLE_OPTIMIZATIONS

		REQUIRE("Kittie says woof" == std::string_view(To));
	}
}

TEST_CASE("CString.memmove")
{
	char To[] = "Hello, world!";

	SECTION("With Abort")
	{
		CLANG_BEGIN_DISABLE_OPTIMIZATIONS
		AutoRTFM::ETransactionResult Result = AutoRTFM::Transact([&]
		{
			memmove(To + 7, To, 5);
			AutoRTFM::AbortTransaction();
		});
		CLANG_END_DISABLE_OPTIMIZATIONS

		REQUIRE(AutoRTFM::ETransactionResult::AbortedByRequest == Result);
		REQUIRE("Hello, world!" == std::string_view(To));
	}

	SECTION("With Commit")
	{
		CLANG_BEGIN_DISABLE_OPTIMIZATIONS
		AutoRTFM::Commit([&] { memmove(To + 7, To, 5); });
		CLANG_END_DISABLE_OPTIMIZATIONS

		REQUIRE("Hello, Hello!" == std::string_view(To));
	}
}

TEST_CASE("CString.strcpy")
{
	MSVC_BEGIN_DISABLE_WARN_UNSAFE_FUNCTION

	const char From[] = "Kittie says meow";
	char To[] = "Doggie says woof";
	static_assert(sizeof(From) == sizeof(To));

	SECTION("With Abort")
	{
		CLANG_BEGIN_DISABLE_OPTIMIZATIONS
		AutoRTFM::ETransactionResult Result = AutoRTFM::Transact([&]
		{
			strcpy(To, From);
			AutoRTFM::AbortTransaction();
		});
		CLANG_END_DISABLE_OPTIMIZATIONS

		REQUIRE(AutoRTFM::ETransactionResult::AbortedByRequest == Result);
		REQUIRE("Doggie says woof" == std::string_view(To));
	}

	SECTION("With Commit")
	{
		CLANG_BEGIN_DISABLE_OPTIMIZATIONS
		AutoRTFM::Commit([&] { strcpy(To, From); });
		CLANG_END_DISABLE_OPTIMIZATIONS

		REQUIRE("Kittie says meow" == std::string_view(To));
	}

	MSVC_END_DISABLE_UNSAFE_FN
}

TEST_CASE("CString.strncpy")
{
	MSVC_BEGIN_DISABLE_WARN_UNSAFE_FUNCTION

	const char* const From = "Kittie says meow";
	char To[] = "Doggie says woof";

	SECTION("With Abort")
	{
		CLANG_BEGIN_DISABLE_OPTIMIZATIONS
		AutoRTFM::ETransactionResult Result = AutoRTFM::Transact([&]
		{
			strncpy(To, From, 6);
			AutoRTFM::AbortTransaction();
		});
		CLANG_END_DISABLE_OPTIMIZATIONS

		REQUIRE(AutoRTFM::ETransactionResult::AbortedByRequest == Result);
		REQUIRE("Doggie says woof" == std::string_view(To));
	}

	SECTION("With Commit")
	{
		CLANG_BEGIN_DISABLE_OPTIMIZATIONS
		AutoRTFM::Commit([&] { strncpy(To, From, 6); });
		CLANG_END_DISABLE_OPTIMIZATIONS

		REQUIRE("Kittie says woof" == std::string_view(To));
	}

	MSVC_END_DISABLE_UNSAFE_FN
}

TEST_CASE("CString.strcat")
{
	MSVC_BEGIN_DISABLE_WARN_UNSAFE_FUNCTION

	constexpr unsigned Size = 128;
	char To[Size] = "Hello";

	SECTION("With Abort")
	{
		CLANG_BEGIN_DISABLE_OPTIMIZATIONS
		AutoRTFM::ETransactionResult Result = AutoRTFM::Transact([&]
		{
			strcat(To, ", world!");
			AutoRTFM::AbortTransaction();
		});
		CLANG_END_DISABLE_OPTIMIZATIONS

		REQUIRE(AutoRTFM::ETransactionResult::AbortedByRequest == Result);
		REQUIRE("Hello" == std::string_view(To));
	}

	SECTION("With Commit")
	{
		CLANG_BEGIN_DISABLE_OPTIMIZATIONS
		AutoRTFM::Commit([&] { strcat(To, ", world!"); });
		CLANG_END_DISABLE_OPTIMIZATIONS

		REQUIRE("Hello, world!" == std::string_view(To));
	}

	MSVC_END_DISABLE_UNSAFE_FN
}

TEST_CASE("CString.strncat")
{
	MSVC_BEGIN_DISABLE_WARN_UNSAFE_FUNCTION

	constexpr unsigned Size = 128;
	char To[Size] = "Hello";

	SECTION("With Abort")
	{
		CLANG_BEGIN_DISABLE_OPTIMIZATIONS
		AutoRTFM::ETransactionResult Result = AutoRTFM::Transact([&]
		{
			strncat(To, ", world! Not this!", 8);
			AutoRTFM::AbortTransaction();
		});
		CLANG_END_DISABLE_OPTIMIZATIONS

		REQUIRE(AutoRTFM::ETransactionResult::AbortedByRequest == Result);
		REQUIRE("Hello" == std::string_view(To));
	}

	SECTION("With Commit")
	{
		CLANG_BEGIN_DISABLE_OPTIMIZATIONS
		AutoRTFM::Commit([&] { strncat(To, ", world! Not this!", 8); });
		CLANG_END_DISABLE_OPTIMIZATIONS

		REQUIRE("Hello, world!" == std::string_view(To));
	}

	MSVC_END_DISABLE_UNSAFE_FN
}

TEST_CASE("CString.memcmp")
{
	constexpr unsigned Size = 128;
	char A[Size] = "This";

	SECTION("With Abort")
	{
		int Compare = 0;

		CLANG_BEGIN_DISABLE_OPTIMIZATIONS
		AutoRTFM::ETransactionResult Result = AutoRTFM::Transact([&]
		{
			Compare = memcmp(A, "That", 4);
			AutoRTFM::AbortTransaction();
		});
		CLANG_END_DISABLE_OPTIMIZATIONS

		REQUIRE(0 == Compare);
	}

	SECTION("With Commit")
	{
		int Compare = 0;

		CLANG_BEGIN_DISABLE_OPTIMIZATIONS
		AutoRTFM::Commit([&] { Compare = memcmp(A, "That", 4); });
		CLANG_END_DISABLE_OPTIMIZATIONS

		REQUIRE(0 < Compare);
	}
}

TEST_CASE("CString.strcmp")
{
	const char* A = "This";

	SECTION("With Abort")
	{
		int Compare = 0;

		CLANG_BEGIN_DISABLE_OPTIMIZATIONS
		AutoRTFM::ETransactionResult Result = AutoRTFM::Transact([&]
		{
			Compare = strcmp(A, "That");
			AutoRTFM::AbortTransaction();
		});
		CLANG_END_DISABLE_OPTIMIZATIONS

		REQUIRE(0 == Compare);
	}

	SECTION("With Commit")
	{
		int Compare = 0;

		CLANG_BEGIN_DISABLE_OPTIMIZATIONS
		AutoRTFM::Commit([&] { Compare = strcmp(A, "That"); });
		CLANG_END_DISABLE_OPTIMIZATIONS

		REQUIRE(0 < Compare);
	}
}

TEST_CASE("CString.strncmp")
{
	const char* A = "This";

	SECTION("With Abort")
	{
		int Compare = 0;

		CLANG_BEGIN_DISABLE_OPTIMIZATIONS
		AutoRTFM::ETransactionResult Result = AutoRTFM::Transact([&]
		{
			Compare = strncmp(A, "That", 3);
			AutoRTFM::AbortTransaction();
		});
		CLANG_END_DISABLE_OPTIMIZATIONS

		REQUIRE(0 == Compare);
	}

	SECTION("With Commit")
	{
		int Compare = 0;

		CLANG_BEGIN_DISABLE_OPTIMIZATIONS
		AutoRTFM::Commit([&] { Compare = strncmp(A, "That", 3); });
		CLANG_END_DISABLE_OPTIMIZATIONS

		REQUIRE(0 < Compare);
	}
}

TEST_CASE("CString.strchr")
{
	const char* A = "Thinking";

	SECTION("With Abort")
	{
		const char* Value = nullptr;

		CLANG_BEGIN_DISABLE_OPTIMIZATIONS
		AutoRTFM::ETransactionResult Result = AutoRTFM::Transact([&]
		{
			Value = strchr(A, 'i');
			AutoRTFM::AbortTransaction();
		});
		CLANG_END_DISABLE_OPTIMIZATIONS

		REQUIRE(nullptr == Value);
	}

	SECTION("With Commit")
	{
		const char* Value = nullptr;

		CLANG_BEGIN_DISABLE_OPTIMIZATIONS
		AutoRTFM::Commit([&] { Value = strchr(A, 'i'); });
		CLANG_END_DISABLE_OPTIMIZATIONS

		REQUIRE((A + 2) == Value);
	}
}

TEST_CASE("CString.strrchr")
{
	const char* A = "Thinking";

	SECTION("With Abort")
	{
		const char* Value = nullptr;

		CLANG_BEGIN_DISABLE_OPTIMIZATIONS
		AutoRTFM::ETransactionResult Result = AutoRTFM::Transact([&]
		{
			Value = strrchr(A, 'i');
			AutoRTFM::AbortTransaction();
		});
		CLANG_END_DISABLE_OPTIMIZATIONS

		REQUIRE(nullptr == Value);
	}

	SECTION("With Commit")
	{
		const char* Value = nullptr;

		CLANG_BEGIN_DISABLE_OPTIMIZATIONS
		AutoRTFM::Commit([&] { Value = strrchr(A, 'i'); });
		CLANG_END_DISABLE_OPTIMIZATIONS

		REQUIRE((A + 5) == Value);
	}
}

TEST_CASE("CString.strstr")
{
	const char* A = "This";

	SECTION("With Abort")
	{
		const char* Value = nullptr;

		CLANG_BEGIN_DISABLE_OPTIMIZATIONS
		AutoRTFM::ETransactionResult Result = AutoRTFM::Transact([&]
		{
			Value = strstr(A, "is");
			AutoRTFM::AbortTransaction();
		});
		CLANG_END_DISABLE_OPTIMIZATIONS

		REQUIRE(nullptr == Value);
	}

	SECTION("With Commit")
	{
		const char* Value = nullptr;

		CLANG_BEGIN_DISABLE_OPTIMIZATIONS
		AutoRTFM::Commit([&] { Value = strstr(A, "is"); });
		CLANG_END_DISABLE_OPTIMIZATIONS

		REQUIRE((A + 2) == Value);
	}
}

TEST_CASE("CString.strlen")
{
	const char* A = "This";

	SECTION("With Abort")
	{
		size_t Value = 0;

		CLANG_BEGIN_DISABLE_OPTIMIZATIONS
		AutoRTFM::ETransactionResult Result = AutoRTFM::Transact([&]
		{
			Value = strlen(A);
			AutoRTFM::AbortTransaction();
		});
		CLANG_END_DISABLE_OPTIMIZATIONS

		REQUIRE(0 == Value);
	}

	SECTION("With Commit")
	{
		size_t Value = 0;

		CLANG_BEGIN_DISABLE_OPTIMIZATIONS
		AutoRTFM::Commit([&] { Value = strlen(A); });
		CLANG_END_DISABLE_OPTIMIZATIONS

		REQUIRE(4 == Value);
	}
}

#undef CLANG_BEGIN_DISABLE_OPTIMIZATIONS
#undef CLANG_END_DISABLE_OPTIMIZATIONS
#undef MSVC_BEGIN_DISABLE_WARN_UNSAFE_FUNCTION
#undef MSVC_END_DISABLE_UNSAFE_FN
