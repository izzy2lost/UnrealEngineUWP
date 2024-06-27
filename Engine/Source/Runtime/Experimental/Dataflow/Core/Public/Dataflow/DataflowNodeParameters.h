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
	class FContext;

	typedef uint32 FContextCacheKey;

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
		FRenderingParameter(FString InRenderName, FName InTypeName, const TArray<FName>& InOutputs)
			: Name(InRenderName), Type(InTypeName), Outputs(InOutputs) {}
		FRenderingParameter(FString InRenderName, FName InTypeName, TArray<FName>&& InOutputs)
			: Name(InRenderName), Type(InTypeName), Outputs(InOutputs) {}
		
		bool operator==(const FRenderingParameter& Other) const = default;

		FString Name = FString("");
		FName Type = FName("");
		TArray<FName> Outputs;
	};

	struct FContextCacheElementBase 
	{
		enum EType
		{
			CacheElementTyped,
			CacheElementReference
		};

		FContextCacheElementBase(EType CacheElementType, FGuid InNodeGuid = FGuid(), const FProperty* InProperty = nullptr, uint32 InNodeHash = 0, FTimestamp InTimestamp = FTimestamp::Invalid)
			: Type(CacheElementType)
			, NodeGuid(InNodeGuid)
			, Property(InProperty)
			, NodeHash(InNodeHash)
			, Timestamp(InTimestamp)
		{}
		virtual ~FContextCacheElementBase() {}

		// InReferenceDataKey is the key of the cache element this function is called on 
		virtual TUniquePtr<FContextCacheElementBase> CreateReference(FContextCacheKey InReferenceDataKey) const = 0;

		template<typename T>
		const T& GetTypedData(FContext& Context, const FProperty* PropertyIn, const T& Default) const;
		
		EType GetType() const {	return Type; }

		const FProperty* GetProperty() const { return Property; }
		const FTimestamp& GetTimestamp() const { return Timestamp; }
		void SetTimestamp(const FTimestamp& InTimestamp) { Timestamp = InTimestamp; }

		const FGuid& GetNodeGuid() const { return NodeGuid; }
		const uint32 GetNodeHash() const { return NodeHash; }

		// use this with caution: setting the property of a wrong type may cause problems
		void SetProperty(const FProperty* NewProperty) { Property = NewProperty; }

	private:
		friend struct FContextCache;

		EType Type;
		FGuid NodeGuid;
		const FProperty* Property = nullptr;
		uint32 NodeHash = 0;
		FTimestamp Timestamp = FTimestamp::Invalid;
	};

	template<class T>
	struct FContextCacheElement : public FContextCacheElementBase 
	{
		FContextCacheElement(FGuid InNodeGuid, const FProperty* InProperty, T&& InData, uint32 InNodeHash, FTimestamp Timestamp)
			: FContextCacheElementBase(EType::CacheElementTyped, InNodeGuid, InProperty, InNodeHash, Timestamp)
			, Data(Forward<T>(InData))
		{}
		
		const T& GetData(FContext& Context, const FProperty* PropertyIn, const T& Default) const;

		const T& GetDataDirect() const { return Data; }

		virtual TUniquePtr<FContextCacheElementBase> CreateReference(FContextCacheKey InReferenceDataKey) const override;

	private:
		typedef typename TDecay<T>::Type FDataType;  // Using universal references here means T could be either const& or an rvalue reference
		const FDataType Data;                        // Decaying T removes any reference and gets the correct underlying storage data type
	};

	template<class T>
	struct FContextCacheElementReference : public FContextCacheElementBase
	{
		FContextCacheElementReference(FGuid InNodeGuid, const FProperty* InProperty, FContextCacheKey InDataKey, uint32 InNodeHash, FTimestamp Timestamp)
			: FContextCacheElementBase(EType::CacheElementReference, InNodeGuid, InProperty, InNodeHash, Timestamp)
			, DataKey(InDataKey)
		{}

		const T& GetData(FContext& Context, const FProperty* PropertyIn, const T& Default) const;

		virtual TUniquePtr<FContextCacheElementBase> CreateReference(FContextCacheKey InReferenceDataKey) const override;

	private:
		const FContextCacheKey DataKey; // this is a key to another cache element
	};

	// cache element method implementation 
	template<class T>
	const T& FContextCacheElementBase::GetTypedData(FContext& Context, const FProperty* PropertyIn, const T& Default) const
	{
		// check(PropertyIn->IsA<T>()); // @todo(dataflow) compile error for non-class T; find alternatives
		if (Type == EType::CacheElementTyped)
		{
			return static_cast<const FContextCacheElement<T>&>(*this).GetData(Context, PropertyIn, Default);
		}
		if (Type == EType::CacheElementReference)
		{
			return static_cast<const FContextCacheElementReference<T>&>(*this).GetData(Context, PropertyIn, Default);
		}
		check(false); // should never happen
		return Default;
	}

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
		void SetData(FContextCacheKey InKey, const FProperty* InProperty, T&& InValue, const FGuid& InNodeGuid, uint32 InNodeHash, const FTimestamp& InTimestamp)
		{
			TUniquePtr<FContextCacheElement<T>> DataStoreEntry = MakeUnique<FContextCacheElement<T>>(InNodeGuid, InProperty, Forward<T>(InValue), InNodeHash, InTimestamp);

			SetDataImpl(InKey, MoveTemp(DataStoreEntry));
		}

		void SetDataReference(FContextCacheKey Key, const FProperty* Property, FContextCacheKey ReferenceKey)
		{
			// find the reference key to get 
			if (TUniquePtr<FContextCacheElementBase>* CacheElement = GetDataImpl(ReferenceKey))
			{
				TUniquePtr<FContextCacheElementBase> CacheReferenceElement = (*CacheElement)->CreateReference(ReferenceKey);
				SetDataImpl(Key, MoveTemp(CacheReferenceElement));
			}
			else
			{
				ensure(false); // could not find the original cache element 
			}
		}

		virtual TUniquePtr<FContextCacheElementBase>* GetDataImpl(FContextCacheKey Key) = 0;

		template<class T>
		const T& GetData(FContextCacheKey Key, const FProperty* InProperty, const T& Default = T())
		{
			if (TUniquePtr<FContextCacheElementBase>* Cache = GetDataImpl(Key))
			{
				return (*Cache)->GetTypedData<T>(*this, InProperty, Default);
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
			return DataStore.Contains(Key) && DataStore[Key]->GetTimestamp() >= InTimestamp;
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
			

			// Threaded evaluation can only set an output once per context evaluation. Otherwise
			// downstream nodes that are extracting the data will get currupted store entries. 
			TUniquePtr<FContextCacheElementBase>* CurrentData = DataStore.Find(Key);
			if (!CurrentData || !(*CurrentData) || (*CurrentData)->GetTimestamp() < GetTimestamp())
			{
				DataStore.Emplace(Key, MoveTemp(DataStoreEntry));
			}
		}

		virtual TUniquePtr<FContextCacheElementBase>* GetDataImpl(FContextCacheKey Key) override
		{
			CacheLock->Lock(); ON_SCOPE_EXIT { CacheLock->Unlock(); };
			
			return DataStore.Find(Key);
		}

		virtual bool HasDataImpl(FContextCacheKey Key, FTimestamp InTimestamp = FTimestamp::Invalid) override
		{
			CacheLock->Lock(); ON_SCOPE_EXIT { CacheLock->Unlock(); };
			
			return DataStore.Contains(Key) && DataStore[Key]->GetTimestamp() >= InTimestamp;
		}

		virtual bool IsEmptyImpl() const override
		{
			return DataStore.IsEmpty();
		}

		DATAFLOWCORE_API virtual void Evaluate(const FDataflowNode* Node, const FDataflowOutput* Output) override;
		DATAFLOWCORE_API virtual bool Evaluate(const FDataflowOutput& Connection) override;

	};

	// cache classes implemetation 
	// this needs to be after the FContext definition because they access its methods

	template<class T>
	const T& FContextCacheElement<T>::GetData(FContext& Context, const FProperty* PropertyIn, const T& Default) const
	{
		return Data;
	}

	template<class T>
	TUniquePtr<FContextCacheElementBase> FContextCacheElement<T>::CreateReference(FContextCacheKey InReferenceDataKey) const
	{
		return MakeUnique<FContextCacheElementReference<T>>(GetNodeGuid(), GetProperty(), InReferenceDataKey, GetNodeHash(), GetTimestamp());
	}

	template<class T>
	const T& FContextCacheElementReference<T>::GetData(FContext& Context, const FProperty* PropertyIn, const T& Default) const
	{
		return Context.GetData(DataKey, PropertyIn, Default);
	}

	template<class T>
	TUniquePtr<FContextCacheElementBase> FContextCacheElementReference<T>::CreateReference(FContextCacheKey InReferenceDataKey) const
	{
		return MakeUnique<FContextCacheElementReference<T>>(GetNodeGuid(), GetProperty(), InReferenceDataKey, GetNodeHash(), GetTimestamp());
	}
}
