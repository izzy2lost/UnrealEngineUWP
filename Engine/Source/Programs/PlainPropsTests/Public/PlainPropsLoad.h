// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/ArrayView.h"
#include  "PlainPropsRead.h"
#include "Memory/MemoryFwd.h"

namespace PlainProps 
{
	
class FCustomBindings;
class FDeclarations;
struct FLoadBatch;
class FRangeBinding;
class FSchemaBindings;
struct FTypedRange;


PP_API FLoadBatch* CreateLoadPlans(FReadBatchId ReadId, const FDeclarations& Declarations, const FCustomBindings& Customs, const FSchemaBindings& Schemas, TConstArrayView<FStructSchemaId> RuntimeIds);
PP_API void DestroyLoadPlans(FLoadBatch* Batch);
PP_API void LoadStruct(uint8* Dst, FByteReader Src, FStructSchemaId Id, const FLoadBatch& Batch);
PP_API void LoadStruct(uint8* Dst, FStructView Src, const FLoadBatch& Batch);
PP_API void ConstructAndLoadStruct(uint8* Dst, FByteReader Src, FStructSchemaId Id, const FLoadBatch& Batch);
PP_API void ConstructAndLoadStruct(uint8* Dst, FStructView Src, const FLoadBatch& Batch);
PP_API void LoadRange(uint8* Dst, FRangeView Src, ERangeSizeType MaxType, TConstArrayView<FRangeBinding> Bindings, const FLoadBatch& Batch);

//using FItemConstructor = void (*)(FLoadRangeContext& Ctx);

//struct FLoadRangeBinding
//{
//	FItemConstructor	MakeItems;
//	const void*			UserContext;
//};

// Possible optimization - opt-in leaf range load method to avoid copying and repeated calls when target representation is not a range
//template<typename T> using TDirectLeafLoader = void (*)(TRangeView<T> Items);
//using FDirectBoolLoader = void (*)(FBoolRangeView Bits);

} // namespace PlainProps

