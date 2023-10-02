// Copyright Epic Games, Inc. All Rights Reserved.

#include "Param/ParamEntry.h"
#include "Param/ParamHelpers.h"

namespace UE::AnimNext::Private
{

FParamEntry::FParamEntry(const FParamTypeHandle& InTypeHandle, TArrayView<uint8> InData, bool bInIsReference, bool bInIsMutable)
	: Data(nullptr)
	, TypeHandle(InTypeHandle)
	, Size(InData.Num())
	, Flags(EParamFlags::None)
{
	check(TypeHandle.IsValid());
	check(InData.Num() > 0 && InData.Num() < 0xffff);

	// If we can store our data inside of a ptr, we do
	if (!bInIsReference && InData.Num() <= sizeof(void*))
	{
		FParamHelpers::Copy(InTypeHandle, InData, TArrayView<uint8>(reinterpret_cast<uint8*>(&Data), sizeof(void*)));
		Flags |= EParamFlags::Embedded;
	}
	else
	{
		Data = InData.GetData();
	}

	if (bInIsReference)
	{
		Flags |= EParamFlags::Reference;
	}

	if (bInIsMutable)
	{
		Flags |= EParamFlags::Mutable;
	}
}

FParamEntry::~FParamEntry()
{
	if (Size > 0)
	{
		if (IsEmbedded())
		{
			FParamHelpers::Destroy(TypeHandle, TArrayView<uint8>(reinterpret_cast<uint8*>(&Data), sizeof(void*)));
		}
	}
}

FParamEntry::FParamEntry(const FParamEntry& InOtherParam)
	: TypeHandle(InOtherParam.TypeHandle)
	, Size(InOtherParam.Size)
	, Flags(InOtherParam.Flags)
{
	if (IsEmbedded())
	{
		FParamHelpers::Copy(InOtherParam.TypeHandle, TConstArrayView<uint8>(reinterpret_cast<const uint8*>(&InOtherParam.Data), sizeof(void*)), TArrayView<uint8>(reinterpret_cast<uint8*>(&Data), sizeof(void*)));
	}
	else
	{
		Data = InOtherParam.Data;
	}
}

FParamEntry& FParamEntry::operator=(const FParamEntry& InOtherParam)
{
	TypeHandle = InOtherParam.TypeHandle;
	Size = InOtherParam.Size;
	Flags = InOtherParam.Flags;

	if (IsEmbedded())
	{
		FParamHelpers::Copy(InOtherParam.TypeHandle, TConstArrayView<uint8>(reinterpret_cast<const uint8*>(&InOtherParam.Data), sizeof(void*)), TArrayView<uint8>(reinterpret_cast<uint8*>(&Data), sizeof(void*)));
	}
	else
	{
		Data = InOtherParam.Data;
	}

	return *this;
}

FParamEntry::FParamEntry(FParamEntry&& InOtherParam) noexcept
	: TypeHandle(InOtherParam.TypeHandle)
	, Size(InOtherParam.Size)
	, Flags(InOtherParam.Flags)
{
	if (IsEmbedded())
	{
		FParamHelpers::Copy(InOtherParam.TypeHandle, TConstArrayView<uint8>(reinterpret_cast<const uint8*>(&InOtherParam.Data), sizeof(void*)), TArrayView<uint8>(reinterpret_cast<uint8*>(&Data), sizeof(void*)));
	}
	else
	{
		Data = InOtherParam.Data;
	}
}

FParamEntry& FParamEntry::operator=(FParamEntry&& InOtherParam) noexcept
{
	TypeHandle = InOtherParam.TypeHandle;
	Size = InOtherParam.Size;
	Flags = InOtherParam.Flags;

	if (IsEmbedded())
	{
		FParamHelpers::Copy(InOtherParam.TypeHandle, TConstArrayView<uint8>(reinterpret_cast<const uint8*>(&InOtherParam.Data), sizeof(void*)), TArrayView<uint8>(reinterpret_cast<uint8*>(&Data), sizeof(void*)));
	}
	else
	{
		Data = InOtherParam.Data;
	}

	return *this;
}

}