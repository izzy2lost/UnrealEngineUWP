// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include  "PlainPropsTypes.h"
#include "Containers/Set.h"

namespace PlainProps
{

class FNestedScopeIndexer
{
public:
	FNestedScopeId						Index(FNestedScope Scope);
	FNestedScopeId						Index(FScopeId Outer, FFlatScopeId Inner)			{ return Index({Outer, Inner}); }
	FNestedScope						Resolve(FNestedScopeId Id) const;
	int32								Num() const											{ return Scopes.Num(); }

	auto								begin() const										{ return Scopes.begin(); }
	auto								end() const											{ return Scopes.end(); }
private:
	TSet<FNestedScope> Scopes;
};

//////////////////////////////////////////////////////////////////////////

class FParametricTypeIndexer
{
public:
	~FParametricTypeIndexer();

	FParametricTypeId					Index(FParametricTypeView View);
	/// @return View invalidated by calling Index() (switch Parameters to TPagedArray to avoid)
	FParametricTypeView					Resolve(FParametricTypeId Id) const;
	FParametricType						At(int32 Idx) const									{ return Types[Idx]; }
	int32								Num() const											{ return Types.Num(); }
	TConstArrayView<FParametricType>	GetAllTypes() const									{ return Types; }
	TConstArrayView<FTypeId>			GetAllParameters() const							{ return Parameters; }

private:
	uint32								NumSlots = 0;
	uint32*								Slots = nullptr;
	TArray<FParametricType>				Types;
	TArray<FTypeId>						Parameters;
};

//////////////////////////////////////////////////////////////////////////

class FIdIndexerBase : public FDebugIds
{
public:
	FScopeId							NestScope(FScopeId Outer, FFlatScopeId Inner);
	FParametricTypeId					MakeParametricTypeId(FConcreteTypenameId Name, TConstArrayView<FTypeId> Params);
	FTypeId								MakeParametricType(FTypeId Type, TConstArrayView<FTypeId> Params);

	FEnumSchemaId						IndexEnum(FTypeId Type);
	FStructSchemaId						IndexStruct(FTypeId Type);
	
	virtual FNestedScope				Resolve(FNestedScopeId Id) const override final		{ return NestedScopes.Resolve(Id); }
	virtual FParametricTypeView			Resolve(FParametricTypeId Id) const override final	{ return ParametricTypes.Resolve(Id); }
	
	const FNestedScopeIndexer&			GetNestedScopes() const								{ return NestedScopes; }
	const FParametricTypeIndexer&		GetParametricTypes() const							{ return ParametricTypes; }

	virtual uint32						NumNames() const = 0;

protected:
	FNestedScopeIndexer					NestedScopes;
	FParametricTypeIndexer				ParametricTypes;
	TSet<FTypeId>						Enums;
	TSet<FTypeId>						Structs;
};

template<class NameType>
void AppendString(FString& Out, const NameType& Str)
{
	Out.Append(Str);
}

template<class NameType>
class TIdIndexer : public FIdIndexerBase
{
public:
	template<typename T> FNameId		MakeName(T&& Str)									{ return { IntCastChecked<uint32>(Names.Add(NameType(Str)).AsInteger()) }; }
	template<typename T> FMemberId		NameMember(T&& Name)								{ return { MakeName(Name) }; }
	template<typename T> FScopeId		MakeScope(T&& Str)									{ return FScopeId(static_cast<FFlatScopeId>(MakeName(Str))); }
	template<typename T> FScopeId		NestScope(FScopeId Outer, T&& Inner)				{ return FIdIndexerBase::NestScope(Outer, static_cast<FFlatScopeId>(MakeName(Inner))); }
	template<typename T> FTypenameId	MakeTypename(T&& Name)								{ return FTypenameId(FConcreteTypenameId(MakeName(Name))); }
	template<typename T> FTypeId		MakeType(T&& Scope, T&& Name)						{ return { MakeScope(Scope), MakeTypename(Name) }; }

	NameType							ResolveName(FNameId Id)	const						{ return Names.Get(FSetElementId::FromInteger(Id.Idx)); }
	virtual uint32						NumNames() const override final						{ return static_cast<uint32>(Names.Num()); }

protected:
	TSet<NameType>						Names;

	virtual void						AppendDebugString(FString& Out, FNameId Id) const override final
	{
		AppendString(Out, ResolveName(Id));
	}
};


} // namespace PlainProps
