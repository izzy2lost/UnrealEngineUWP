// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include  "PlainPropsTypes.h"
#include "Containers/Array.h"
#include "Templates/UniquePtr.h"

namespace PlainProps
{
struct FBuiltSchemas;
struct FBuiltStruct;
class FDebugIds;
class FIdIndexerBase;
class FNestedScopeIndexer;
class FParametricTypeIndexer;
struct FWriteIds;

class FWriter
{
public:
	PP_API FWriter(const FIdIndexerBase& DeclaredIds, const FBuiltSchemas& InSchemas);
	PP_API ~FWriter();
	
	PP_API bool								Uses(FNameId BuiltId) const;
	PP_API TConstArrayView<FNestedScope>	GetUsedScopes() const;
	PP_API TConstArrayView<FParametricType>	GetUsedParametrics() const;
	PP_API FOptionalStructSchemaId			GetWriteId(FStructSchemaId BuiltId) const;

	PP_API void								WriteSchemas(TArray64<uint8>& Out) const;
	PP_API void								WriteMembers(TArray64<uint8>& Out, FStructSchemaId BuiltId, const FBuiltStruct& Struct) const;

private:
	const FBuiltSchemas&					Schemas;
	const FDebugIds&						Debug;
	TUniquePtr<FWriteIds>					NewIds;
};

} // namespace PlainProps