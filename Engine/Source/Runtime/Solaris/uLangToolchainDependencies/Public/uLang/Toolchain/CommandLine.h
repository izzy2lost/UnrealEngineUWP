// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "uLang/Common/Common.h"
#include "uLang/CompilerPasses/CompilerTypes.h" // for SCommandLine

namespace uLang
{
namespace CommandLine
{
    ULANGTOOLCHAINDEPENDENCIES_API void Init(int ArgC, char* ArgV[]);
    ULANGTOOLCHAINDEPENDENCIES_API void Init(const SCommandLine& Rhs);

    ULANGTOOLCHAINDEPENDENCIES_API bool IsSet();
    ULANGTOOLCHAINDEPENDENCIES_API const SCommandLine& Get();
}
} // namespace uLang
