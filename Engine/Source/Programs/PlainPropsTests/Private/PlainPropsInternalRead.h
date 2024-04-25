// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include  "PlainPropsTypes.h"
#include  "PlainPropsRead.h"

namespace PlainProps
{

template<typename MemberType, typename IndexType>
TConstArrayView<MemberType> GrabInnerRangeTypes(TConstArrayView<MemberType> InnerRangeTypes, IndexType& InOutIdx)
{
	const int32 StartIdx = InOutIdx;
	int32 Idx = StartIdx;
	while (InnerRangeTypes[Idx++].IsRange());
	
	InOutIdx = static_cast<IndexType>(Idx);
	return MakeArrayView(&InnerRangeTypes[StartIdx], Idx - StartIdx);
}

inline uint64 GrabRangeNum(ERangeSizeType MaxSize, FByteReader& ByteIt, FBitCacheReader& BitIt)
{
	switch(MaxSize)
	{
	case ERangeSizeType::Uni:	return BitIt.GrabNext(/* in-out */ ByteIt) ? 1 : 0;
	case ERangeSizeType::S8:	return IntCastChecked<uint64>(	ByteIt.Grab<int8>());
	case ERangeSizeType::U8:	return							ByteIt.Grab<uint8>();
	case ERangeSizeType::S16:	return IntCastChecked<uint64>(	ByteIt.Grab<int16>());
	case ERangeSizeType::U16:	return							ByteIt.Grab<uint16>();
	case ERangeSizeType::S32:	return IntCastChecked<uint64>(	ByteIt.Grab<int32>());
	case ERangeSizeType::U32:	return							ByteIt.Grab<uint32>();
	case ERangeSizeType::S64:	return IntCastChecked<uint64>(	ByteIt.Grab<int64>());
	case ERangeSizeType::U64:	return							ByteIt.Grab<uint64>();
	}
	check(false);
	return 0;
}

} // namespace PlainProps