// Copyright Epic Games, Inc. All Rights Reserved.
#include "OSCAddress.h"

#include "Audio/AudioAddressPattern.h"
#include "OSCLog.h"


namespace UE::OSC
{
	const FString BundleTag = TEXT("#bundle");
	const FString PathSeparator = TEXT("/");
} // namespace UE::OSC


FOSCAddress::FOSCAddress()
	: bIsValidPattern(false)
	, bIsValidPath(false)
	, Hash(GetTypeHash(GetFullPath()))
{
}

FOSCAddress::FOSCAddress(const FString& InValue)
	: bIsValidPattern(false)
	, bIsValidPath(false)
{
	InValue.ParseIntoArray(Containers, *UE::OSC::PathSeparator, true);
	if (Containers.Num() > 0)
	{
		Method = Containers.Pop();
	}

	CacheAggregates();
}

void FOSCAddress::CacheAggregates()
{
	const bool bInvalidateSeparator = false;
	const FString FullPath = GetFullPath();

	Hash = GetTypeHash(FullPath);
	bIsValidPath = FAudioAddressPattern::IsValidPath(FullPath, bInvalidateSeparator);
	bIsValidPattern = FAudioAddressPattern::IsValidPattern(Containers, Method);
}

bool FOSCAddress::Matches(const FOSCAddress& InAddress) const
{
	if (IsValidPattern() && InAddress.IsValidPath())
	{
		return FAudioAddressPattern::PartsMatch(GetFullPath(), InAddress.GetFullPath());
	}

	return false;
}

bool FOSCAddress::IsValidPattern() const
{
	return bIsValidPattern;
}

bool FOSCAddress::IsValidPath() const
{
	return bIsValidPath;
}

bool FOSCAddress::PushContainer(const FString& Container)
{
	return PushContainers({ Container });
}

bool FOSCAddress::PushContainer(FString&& Container)
{
	return PushContainers({ MoveTemp(Container) });
}

bool FOSCAddress::PushContainers(const TArray<FString>& InContainers)
{
	TArray<FString> NewContainers = InContainers;
	const bool bPushed = PushContainersInternal(MoveTemp(NewContainers));
	if (bPushed)
	{
		CacheAggregates();
	}

	return bPushed;
}

bool FOSCAddress::PushContainers(TArray<FString>&& InContainers)
{
	const bool bPushed = PushContainersInternal(MoveTemp(InContainers));
	if (bPushed)
	{
		CacheAggregates();
	}

	return bPushed;
}

bool FOSCAddress::PushContainersInternal(TArray<FString>&& InContainers)
{
	if (InContainers.IsEmpty())
	{
		return false;
	}

	for (const FString& Container : InContainers)
	{
		if (Container.Contains(UE::OSC::PathSeparator))
		{
			UE_LOG(LogOSC, Warning, TEXT("Failed to push containers on OSCAddress. "
				"Cannot contain OSC path separator '%s'."), *UE::OSC::PathSeparator);
			return false;
		}
	}

	Containers.Append(MoveTemp(InContainers));
	for (const FString& Container : InContainers)
	{
		Containers.Push(Container);
	}

	return true;
}

FString FOSCAddress::PopContainer(bool* bOutDidPop)
{
	FString Popped;
	if (Containers.Num() > 0)
	{
		Popped = Containers.Pop(EAllowShrinking::No);
		Hash = GetTypeHash(GetFullPath());

		if (bOutDidPop)
		{
			*bOutDidPop = true;
		}
	}
	else
	{
		if (bOutDidPop)
		{
			*bOutDidPop = false;
		}
	}

	return Popped;
}

TArray<FString> FOSCAddress::PopContainers(int32 InNumContainers, bool* bOutDidPop)
{
	TArray<FString> Popped;
	if (InNumContainers <= 0 || Containers.Num() == 0)
	{
		if (bOutDidPop)
		{
			*bOutDidPop = false;
		}
		return Popped;
	}

	int32 Removed = 0;
	for (int32 i = Containers.Num() - 1; i >= 0; --i)
	{
		if (Removed > InNumContainers)
		{
			break;
		}
		++Removed;
		Popped.Add(Containers.Pop(EAllowShrinking::No));
	}

	Hash = GetTypeHash(GetFullPath());
	if (bOutDidPop)
	{
		*bOutDidPop = true;
	}
	return Popped;
}

bool FOSCAddress::RemoveContainers(int32 InIndex, int32 InCount)
{
	if (InIndex >= 0 && InCount > 0)
	{
		if (InIndex + InCount < Containers.Num())
		{
			Containers.RemoveAt(InIndex, InCount);
			return true;
		}
	}

	return false;
}

void FOSCAddress::ClearContainers()
{
	Containers.Reset();
	CacheAggregates();
}

const FString& FOSCAddress::GetMethod() const
{
	return Method;
}

int32 FOSCAddress::GetNumericPrefix(bool* bIsNumeric) const
{
	int32 Value = 0;
	int32 Dec = 1;
	const TArray<TCHAR>& CharArray = Method.GetCharArray();
	for (
		int32 Index = 0;
		Index < CharArray.Num() && FChar::IsDigit(CharArray[Index]);
		++Index)
	{
		Value = (Value * Dec) + FChar::ConvertCharDigitToInt(CharArray[Index]);
		Dec *= 10;
	}

	if (bIsNumeric)
	{
		*bIsNumeric = Dec == 1;
	}
	return Value;
}

int32 FOSCAddress::GetNumericSuffix(bool* bIsNumeric) const
{
	int32 Value = 0;
	int32 Dec = 1;
	const TArray<TCHAR>& CharArray = Method.GetCharArray();
	for (
		int32 Index = CharArray.Num() - 2 /* less term char */;
		Index >= 0 && FChar::IsDigit(CharArray[Index]);
		--Index)
	{
		Value += FChar::ConvertCharDigitToInt(CharArray[Index]) * Dec;
		Dec *= 10;
	}

	if (bIsNumeric)
	{
		*bIsNumeric = Dec == 1;
	}
	return Value;
}

bool FOSCAddress::Set(const TArray<FString>& InContainers, const FString& InMethod)
{
	TArray<FString> NewContainers = InContainers;
	FString NewMethod = InMethod;
	return Set(MoveTemp(NewContainers), MoveTemp(NewMethod));
}

bool FOSCAddress::Set(TArray<FString>&& InContainers, FString&& InMethod)
{
	Containers = { };
	Method = { };

	const bool bPushContainers = PushContainersInternal(MoveTemp(InContainers));
	if (bPushContainers)
	{
		const bool bSetMethod = SetMethod(MoveTemp(InMethod)); // Calls aggregate internally, so no need to call again on success
		if (bSetMethod)
		{
			return true;
		}
		else
		{
			Containers = { };
		}
	}

	CacheAggregates();
	return false;
}

bool FOSCAddress::SetMethod(const FString& InMethod)
{
	FString NewMethod = InMethod;
	return SetMethod(MoveTemp(NewMethod));
}

bool FOSCAddress::SetMethod(FString&& InMethod)
{
	if (InMethod.IsEmpty())
	{
		UE_LOG(LogOSC, Warning, TEXT("Failed to set OSCAddress method. "
			"'InMethod' cannot be empty string."));
		return false;
	}

	if (InMethod.Contains(UE::OSC::PathSeparator))
	{
		UE_LOG(LogOSC, Warning, TEXT("Failed to set OSCAddress method. "
			"Cannot contain OSC path separator '%s'."), *UE::OSC::PathSeparator);
		return false;
	}

	Method = MoveTemp(InMethod);

	CacheAggregates();
	return true;
}

FString FOSCAddress::GetContainerPath() const
{
	return UE::OSC::PathSeparator + FString::Join(Containers, *UE::OSC::PathSeparator);
}

FString FOSCAddress::GetContainer(int32 Index) const
{
	if (Index >= 0 && Index < Containers.Num())
	{
		return Containers[Index];
	}

	return FString();
}

void FOSCAddress::GetContainers(TArray<FString>& OutContainers) const
{
	OutContainers = Containers;
}

FString FOSCAddress::GetFullPath() const
{
	if (Containers.Num() == 0)
	{
		return UE::OSC::PathSeparator + Method;
	}

	return GetContainerPath() + UE::OSC::PathSeparator + Method;
}
