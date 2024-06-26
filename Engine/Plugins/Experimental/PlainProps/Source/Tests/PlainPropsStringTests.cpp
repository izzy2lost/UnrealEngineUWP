// Copyright Epic Games, Inc. All Rights Reserved.

#include "PlainPropsStringUtil.h"

namespace PlainProps::Private
{
	using namespace std::literals;

	static constexpr std::string_view Hello = "Hello";
	static constexpr std::string_view Space = " ";
	static constexpr std::string_view World = "World";
	static_assert("Hello World"sv == Private::Concat<Hello, Space, World>);
	
	static_assert("0"sv == Private::HexString<0>);
	static_assert("1"sv == Private::HexString<1>);
	static_assert("9"sv == Private::HexString<9>);
	static_assert("A"sv == Private::HexString<0xA>);
	static_assert("F"sv == Private::HexString<0xF>);
	static_assert("10"sv == Private::HexString<0x10>);
	static_assert("FF"sv == Private::HexString<0xFF>);
	static_assert("100"sv == Private::HexString<0x100>);
	static_assert("FFF"sv == Private::HexString<0xFFF>);
	static_assert("1000"sv == Private::HexString<0x1000>);
	static_assert("FEDCBA9876543210"sv == Private::HexString<0xFEDCBA9876543210>);
}