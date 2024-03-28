// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_VERSE_VM || defined(__INTELLISENSE__)
#include "VerseVM/VVMVerse.h"
#include "AutoRTFM/AutoRTFM.h"
#include "UObject/VerseValueProperty.h"
#include "VerseVM/VVMEmergentTypeCreator.h"
#include "VerseVM/VVMFalse.h"
#include "VerseVM/VVMGlobalProgram.h"
#include "VerseVM/VVMHeap.h"

namespace Verse
{

namespace Private
{
IEngineEnvironment* GEngineEnvironment = nullptr;
}

void VerseVM::Startup()
{
	Verse::FHeap::Initialize();
	Verse::VEmergentTypeCreator::Initialize();

	// We initialize the global True/False ptr's at module startup to avoid checking if they are initialized elsewhere
	Verse::VFalse::InitializeGlobals();

	// VerseVM requires RTFM enabled
#if UE_AUTORTFM || defined(__INTELLISENSE__)
	AutoRTFM::ForTheRuntime::SetAutoRTFMRuntime(AutoRTFM::ForTheRuntime::EAutoRTFMEnabledState::AutoRTFM_Enabled);
#endif

	// Register our property types
	FVValueProperty::StaticClass();
	FVRestValueProperty::StaticClass();

	if (!Verse::GlobalProgram)
	{
		FRunningContext::Create([](FRunningContext Context) {
			GlobalProgram.Set(Context, &VProgram::New(Context, 32));
		});
	}
}

void VerseVM::Shutdown()
{
}

IEngineEnvironment* VerseVM::GetEngineEnvironment()
{
	return Private::GEngineEnvironment;
}

void VerseVM::SetEngineEnvironment(IEngineEnvironment* Environment)
{
	ensure(Environment == nullptr || Private::GEngineEnvironment == nullptr);
	Private::GEngineEnvironment = Environment;
}

} // namespace Verse
#endif // WITH_VERSE_VM || defined(__INTELLISENSE__)