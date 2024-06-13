// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/StringFwd.h"

namespace UE::DerivedData
{

template <typename CharType> using TSharedString = UE::TSharedString<CharType>;

using FSharedString = UE::FSharedString;
using FAnsiSharedString = UE::FAnsiSharedString;
using FWideSharedString = UE::FWideSharedString;
using FUtf8SharedString = UE::FUtf8SharedString;

} // UE::DerivedData
