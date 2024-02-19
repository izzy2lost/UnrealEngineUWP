// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/ScopeExit.h"
#include "UObject/UnrealType.h"
#include "Templates/UniquePtr.h"
#include "GenericPlatform/GenericPlatformCriticalSection.h"
#include "Serialization/Archive.h"

class  UDataflow;
struct FDataflowNode;
struct FDataflowOutput;
struct FDataflowConnection;

#define DATAFLOW_EDITOR_EVALUATION WITH_EDITOR

namespace Dataflow
{
	struct FTimestamp
	{
		typedef uint64 Type;
		Type Value = Type(0);

		FTimestamp(Type InValue) : Value(InValue) {}
		bool operator>=(const FTimestamp& InTimestamp) const { return Value >= InTimestamp.Value; }
		bool operator<(const FTimestamp& InTimestamp) const { return Value < InTimestamp.Value; }
		bool IsInvalid() { return Value == Invalid; }

		static DATAFLOWCORE_API Type Current();
		static DATAFLOWCORE_API Type Invalid; // 0
	};

	struct FRenderingParameter {
		FRenderingParameter() {}
		FRenderingParameter(FName InTypeName, const TArray<FName>& InOutputs)
			: Type(InTypeName), Outputs(InOutputs) {}
		FRenderingParameter(FName InTypeName, TArray<FName>&& InOutputs)
			: Type(InTypeName), Outputs(InOutputs) {}

		FName Type = FName("");
		TArray<FName> Outputs;
	};

	struct FContextCacheElementBase 
	{
		FContextCacheElementBase(FGuid InNodeGuid = FGuid(), const FProperty* InProperty = nullptr, uint32 InNodeHash = 0, FTimestamp InTimestamp = FTimestamp::Invalid)
			: NodeGuid(InNodeGuid)
			, Property(InProperty)
			, NodeHash(InNodeHash)
			, Timestamp(InTimestamp)
		{}
		virtual ~FContextCacheElementBase() {}

		template<typename T>
		const T& GetTypedData(const FProperty* PropertyIn) const;
		
		FGuid NodeGuid;
		const FProperty* Property = nullptr;
		uint32 NodeHash = 0;
		FTimestamp Timestamp = FTimestamp::Invalid;
	};

	template<class T>
	struct FContextCacheElement : public FContextCacheElementBase 
	{
		FContextCacheElement(FGuid InNodeGuid, const FProperty* InProperty, T&& InData, uint32 InNodeHash, FTimestamp Timestamp)
			: FContextCacheElementBase(InNodeGuid, InProperty, InNodeHash, Timestamp)
			, Data(Forward<T>(InData))
		{}
		
		typedef typename TDecay<T>::Type FDataType;  // Using universal references here means T could be either const& or an rvalue reference
		const FDataType Data;                        // Decaying T removes any reference and gets the correct underlying storage data type
	};

	template<class T>
	const T& FContextCacheElementBase::GetTypedData(const FProperty* InProperty) const
	{
		check(InProperty);
		check(Property->SameType(InProperty));
		return static_cast<const FContextCacheElement<T>&>(*this).Data;
	}

	typedef uint32 FContextCacheKey;
	

	struct FContextCache : public TMap<FContextCacheKey, TUniquePtr<FContextCacheElementBase>>
	{
		DATAFLOWCORE_API void Serialize(FArchive& Ar);
	};
};

inline FArchive& operator<<(FArchive& Ar, Dataflow::FTimestamp& ValueIn)
{
	Ar << ValueIn.Value;
	Ar << ValueIn.Invalid;
	return Ar;
}

inline FArchive& operator<<(FArchive& Ar, Dataflow::FContextCache& ValueIn)
{
	ValueIn.Serialize(Ar);
	return Ar;
}


namespace Dataflow
{

	class FContext
	{
	protected:
		FContext(FContext&&) = default;
		FContext& operator=(FContext&&) = default;
		
		FContext(const FContext&) = delete;
		FContext& operator=(const FContext&) = delete;

		FContextCache DataStore;

	public:
		FContext(FTimestamp InTimestamp)
			: Timestamp(InTimestamp)
		{}

		virtual ~FContext() {}
		
		FTimestamp Timestamp = FTimestamp::Invalid;

		static FName StaticType() { return FName("FContext"); }

		virtual bool IsA(FName InType) const { return InType==StaticType(); }

		virtual FName GetType() const { return FContext::StaticType(); }

		virtual int32 GetKeys(TSet<FContextCacheKey>& InKeys) const { return DataStore.GetKeys(InKeys); }

		template<class T>
		const T* AsType() const
		{
			if (IsA(T::StaticType()))
			{
				return (T*)this;
			}
			return nullptr;
		}

		virtual void SetDataImpl(FContextCacheKey Key, TUniquePtr<FContextCacheElementBase>&& DataStoreEntry) = 0;
		
		template<typename T>
		void SetData(FContextCacheKey Key, FContextCacheElementBase&& Data, T&& Value)
		{
			FContextCacheKey IntKey = (FContextCacheKey)Key;
			TUniquePtr<FContextCacheElement<T>> DataStoreEntry = MakeUnique<FContextCacheElement<T>>(Data.NodeGuid, Data.Property, Forward<T>(Value), Data.NodeHash, Data.Timestamp);

			SetDataImpl(IntKey, MoveTemp(DataStoreEntry));
		}

		virtual TUniquePtr<FContextCacheElementBase>* GetDataImpl(FContextCacheKey Key) = 0;

		template<class T>
		const T& GetData(FContextCacheKey Key, const FProperty* InProperty, const T& Default = T())
		{
			if (TUniquePtr<FContextCacheElementBase>* Cache = GetDataImpl(Key))
			{
				return (*Cache)->GetTypedData<T>(InProperty);
			}
			return Default;
		}

		
		virtual bool HasDataImpl(FContextCacheKey Key, FTimestamp InTimestamp = FTimestamp::Invalid) = 0;
		
		bool HasData(FContextCacheKey Key, FTimestamp InTimestamp = FTimestamp::Invalid)
		{
			FContextCacheKey IntKey = (FContextCacheKey)Key;
			return HasDataImpl(Key, InTimestamp);
		}

		virtual bool IsEmptyImpl() const = 0;

		bool IsEmpty() const
		{
			return IsEmptyImpl();
		}

		virtual void Serialize(FArchive& Ar)
		{
			Ar << Timestamp;
			Ar << DataStore;
		}


		FTimestamp GetTimestamp() const { return Timestamp; }
		virtual void Evaluate(const FDataflowNode* Node, const FDataflowOutput* Output) = 0;
		virtual bool Evaluate(const FDataflowOutput& Connection) = 0;

		DATAFLOWCORE_API void PushToCallstack(const FDataflowConnection* Connection);
		DATAFLOWCORE_API void PopFromCallstack(const FDataflowConnection* Connection);
		DATAFLOWCORE_API bool IsInCallstack(const FDataflowConnection* Connection) const;


	private:
#if DATAFLOW_EDITOR_EVALUATION
		TArray<const FDataflowConnection*> Callstack;
#endif
	};

	struct FContextScopedCallstack
	{
	public:
		DATAFLOWCORE_API FContextScopedCallstack(FContext& InContext, const FDataflowConnection* InConnection);
		DATAFLOWCORE_API ~FContextScopedCallstack();

		bool IsLoopDetected() const { return bLoopDetected; }

	private:
		bool bLoopDetected;
		FContext& Context;
		const FDataflowConnection* Connection;
	};

#define DATAFLOW_CONTEXT_INTERNAL(PARENTTYPE, TYPENAME)														\
	typedef PARENTTYPE Super;																				\
	static FName StaticType() { return FName(#TYPENAME); }													\
	virtual bool IsA(FName InType) const override { return InType==StaticType() || Super::IsA(InType); }	\
	virtual FName GetType() const override { return StaticType(); }

	class FContextSingle : public FContext
	{

	public:
		DATAFLOW_CONTEXT_INTERNAL(FContext, FContextSingle);

		FContextSingle(FTimestamp InTime)
			: FContext(InTime)
		{}

		virtual void SetDataImpl(FContextCacheKey Key, TUniquePtr<FContextCacheElementBase>&& DataStoreEntry) override
		{
			DataStore.Emplace(Key, MoveTemp(DataStoreEntry));
		}

		virtual TUniquePtr<FContextCacheElementBase>* GetDataImpl(FContextCacheKey Key) override
		{
			return DataStore.Find(Key);
		}

		virtual bool HasDataImpl(FContextCacheKey Key, FTimestamp InTimestamp = FTimestamp::Invalid) override
		{
			return DataStore.Contains(Key) && DataStore[Key]->Timestamp >= InTimestamp;
		}

		virtual bool IsEmptyImpl() const override
		{
			return DataStore.IsEmpty();
		}

		DATAFLOWCORE_API virtual void Evaluate(const FDataflowNode* Node, const FDataflowOutput* Output) override;
		DATAFLOWCORE_API virtual bool Evaluate(const FDataflowOutput& Connection) override;

	};
	
	class FContextThreaded : public FContext
	{
		TSharedPtr<FCriticalSection> CacheLock;

	public:
		DATAFLOW_CONTEXT_INTERNAL(FContext, FContextThreaded);


		FContextThreaded(FTimestamp InTime)
			: FContext(InTime)
		{
			CacheLock = MakeShared<FCriticalSection>();
		}

		virtual void SetDataImpl(FContextCacheKey Key, TUniquePtr<FContextCacheElementBase>&& DataStoreEntry) override
		{
			CacheLock->Lock(); ON_SCOPE_EXIT { CacheLock->Unlock(); };
			
			DataStore.Emplace(Key, MoveTemp(DataStoreEntry));
		}

		virtual TUniquePtr<FContextCacheElementBase>* GetDataImpl(FContextCacheKey Key) override
		{
			CacheLock->Lock(); ON_SCOPE_EXIT { CacheLock->Unlock(); };
			
			return DataStore.Find(Key);
		}

		virtual bool HasDataImpl(FContextCacheKey Key, FTimestamp InTimestamp = FTimestamp::Invalid) override
		{
			CacheLock->Lock(); ON_SCOPE_EXIT { CacheLock->Unlock(); };
			
			return DataStore.Contains(Key) && DataStore[Key]->Timestamp >= InTimestamp;
		}

		virtual bool IsEmptyImpl() const override
		{
			return DataStore.IsEmpty();
		}

		DATAFLOWCORE_API virtual void Evaluate(const FDataflowNode* Node, const FDataflowOutput* Output) override;
		DATAFLOWCORE_API virtual bool Evaluate(const FDataflowOutput& Connection) override;

	};

}
