// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include  "PlainPropsTypes.h"
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

///// Maps declared / built ids to write ids, the subset that's used by the partial schemas
//class FUsedIds
//{
//public:
//	virtual ~FUsedIds() {}
//	virtual bool Uses(FNameId BuiltId) const = 0;
//	virtual FOptionalStructSchemaId GetWriteId(FStructSchemaId BuiltId) const = 0;
//};
//
//PP_API TUniquePtr<FUsedIds>	DropUnusedIds(const FBuiltSchemas& AllSchemas, int32 NumDeclaredNames);
//PP_API void					WriteSchemas(TArray64<uint8>& Out, const FBuiltSchemas& AllSchemas, const FUsedIds& WriteIds);
//PP_API void					WriteMembers(TArray64<uint8>& Out, const FBuiltSchemas& AllSchemas, const FUsedIds& WriteIds, FStructSchemaId Schema, const FBuiltStruct& Struct, const FDebugIds& Debug);

//struct FDeclaredIds
//{
//	int32 NumNames;
//	const FNestedScopeIndexer& NestedScopes;
//	const FParametricTypeIndexer& ParametricTypes;
//};

class FWriter
{
public:
	PP_API FWriter(const FIdIndexerBase& DeclaredIds, const FBuiltSchemas& InSchemas);
	PP_API ~FWriter();
	
	PP_API bool								Uses(FNameId BuiltId) const;
	PP_API TConstArrayView<FNestedScope>		GetUsedScopes() const;
	PP_API TConstArrayView<FParametricType>	GetUsedParametrics() const;
	PP_API FOptionalStructSchemaId			GetWriteId(FStructSchemaId BuiltId) const;

	PP_API void								WriteSchemas(TArray64<uint8>& Out) const;
	PP_API void								WriteMembers(TArray64<uint8>& Out, FStructSchemaId BuiltId, const FBuiltStruct& Struct) const;

private:
	const FBuiltSchemas& Schemas;
	const FDebugIds& Debug;
	TUniquePtr<FWriteIds> NewIds;
};

} // namespace PlainProps