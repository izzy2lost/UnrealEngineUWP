// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/ArrayView.h"
#include "Containers/StringFwd.h"

namespace UE::IO::IAS::HTTP
{

void LatencyTest(FStringView InUrl, FStringView InPath, TArrayView<int32> OutResults);

} // namespace UE::IO::IAS::HTTP
