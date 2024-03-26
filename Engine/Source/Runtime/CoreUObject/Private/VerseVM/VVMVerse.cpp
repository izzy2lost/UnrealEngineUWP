// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_VERSE_VM || defined(__INTELLISENSE__)
#include "VerseVM/VVMVerse.h"
#include "AutoRTFM/AutoRTFM.h"
#include "UObject/CoreRedirects.h"
#include "UObject/VerseValueProperty.h"
#include "VerseVM/VVMEmergentTypeCreator.h"
#include "VerseVM/VVMFalse.h"
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

	TArray<FCoreRedirect> Redirects;
	Redirects.Emplace(ECoreRedirectFlags::Type_Class, TEXT("/Script/Solaris.VerseClass"), TEXT("/Script/CoreUObject.VerseVMClass"));
	FCoreRedirects::AddRedirectList(Redirects, TEXT("VerseVM"));
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