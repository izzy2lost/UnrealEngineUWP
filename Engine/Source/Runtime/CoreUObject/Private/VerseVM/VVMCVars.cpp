// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_VERSE_VM
#include "VerseVM/VVMCVars.h"

namespace Verse
{
TAutoConsoleVariable<bool> CVarTraceExecution(TEXT("verse.TraceExecution"), false, TEXT("When true, print a trace of Verse instructions executed to the log.\n"), ECVF_Default);
TAutoConsoleVariable<bool> CVarDumpBytecode(TEXT("verse.DumpBytecode"), false, TEXT("When true, dump bytecode of all functions.\n"), ECVF_Default);
TAutoConsoleVariable<bool> CVarForceBytecode(TEXT("verse.ForceBytecode"), false, TEXT("When true, generate bytecode instead of BP code.\n"), ECVF_Default);
} // namespace Verse
#endif // WITH_VERSE_VM