// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Dataflow/DataflowNodeParameters.h"
#include "Dataflow/DataflowConnection.h"
#include "Templates/Function.h"
#include "Async/Async.h"
#include "GenericPlatform/GenericPlatformCriticalSection.h"

#include "DataflowInputOutput.generated.h"


struct FDataflowOutput;

//
//  Input
//
namespace Dataflow
{
	struct FInputParameters : public FConnectionParameters
	{
		FInputParameters(FName InType = NAME_None, FName InName = NAME_None, FDataflowNode* InOwner = nullptr, const FProperty* InProperty = nullptr, uint32 InOffset = INDEX_NONE, FGuid InGuid = FGuid::NewGuid())
			: FConnectionParameters(InType, InName, InOwner, InProperty, InOffset, InGuid)
		{}
	};

	struct FArrayInputParameters : public FInputParameters
	{
		FArrayInputParameters()
			: FInputParameters()
		{}
		const FArrayProperty* ArrayProperty = nullptr;
		uint32 InnerOffset = INDEX_NONE;
	};
}

USTRUCT()
struct FDataflowInput : public FDataflowConnection
{
	GENERATED_USTRUCT_BODY()

	static FDataflowInput NoOpInput;

	friend struct FDataflowConnection;

	FDataflowOutput* Connection;

protected:
	friend struct FDataflowOutput;
	virtual void FixAndPropagateType(FName InType) override;

public:
	UE_DEPRECATED(5.5, "Deprecated constructor : Guid is now passed through FInputParameters")
	DATAFLOWCORE_API FDataflowInput(const Dataflow::FInputParameters& Param, FGuid InGuid);

	DATAFLOWCORE_API FDataflowInput(const Dataflow::FInputParameters& Param = {});

	virtual bool AddConnection(FDataflowConnection* InOutput) override;
	virtual bool RemoveConnection(FDataflowConnection* InOutput) override;

	FDataflowOutput* GetConnection() { return Connection; }
	const FDataflowOutput* GetConnection() const { return Connection; }
	bool HasAnyConnections() const { return Connection != nullptr; }

	virtual TArray< FDataflowOutput* > GetConnectedOutputs();
	virtual const TArray< const FDataflowOutput* > GetConnectedOutputs() const;

	/** 
	* Get the value of this input by evaluating the value of the connected output 
	* @return the typed value of the input 
	*/
	template<class T>
	const T& GetValue(Dataflow::FContext& Context, const T& Default) const;

	template<typename TAnyType>
	typename TAnyType::FStorageType GetValueFromAnyType(Dataflow::FContext& Context, const typename TAnyType::FStorageType& Default) const;

	/**
	* pull the value from the upstream connections
	* the upstream graph is evaluated if necessary and values are cached along the way 
	*/
	DATAFLOWCORE_API void PullValue(Dataflow::FContext& Context) const;

	template<class T>
	TFuture<const T&> GetValueParallel(Dataflow::FContext& Context, const T& Default) const;

	virtual void Invalidate(const Dataflow::FTimestamp& ModifiedTimestamp = Dataflow::FTimestamp::Current()) override;
};

USTRUCT()
struct FDataflowArrayInput : public FDataflowInput
{
	GENERATED_USTRUCT_BODY()

private:
	int32 Index;
	uint32 ElementOffset; // Offset to Property inside an array element
	const FArrayProperty* ArrayProperty = nullptr;
	// uint32 Offset; // On base class. This is the Offset to ArrayProperty from OwningNode.

public:
	explicit FDataflowArrayInput(int32 InIndex = INDEX_NONE, const Dataflow::FArrayInputParameters& Param = {});

	DATAFLOWCORE_API virtual void* RealAddress() const override;
	virtual int32 GetContainerIndex() const override { return Index; }
	virtual uint32 GetContainerElementOffset() const override { return ElementOffset; }
};

//
// Output
//
namespace Dataflow
{
	struct FOutputParameters: public FConnectionParameters
	{
		FOutputParameters(FName InType = NAME_None, FName InName = NAME_None, FDataflowNode* InOwner = nullptr, const FProperty* InProperty = nullptr, uint32 InOffset = INDEX_NONE, FGuid InGuid = FGuid::NewGuid())
			: FConnectionParameters(InType, InName, InOwner, InProperty, InOffset, InGuid)
		{}
	};
}

USTRUCT()
struct FDataflowOutput : public FDataflowConnection
{
	GENERATED_USTRUCT_BODY()

	friend struct FDataflowConnection;
	
	TArray< FDataflowInput* > Connections;

	UE_DEPRECATED(5.5, "Use PassthroughKey instead")
	uint32 PassthroughOffset = INDEX_NONE;

	Dataflow::FConnectionKey PassthroughKey;

protected:
	friend struct FDataflowInput;
	virtual void FixAndPropagateType(FName InType) override;

public:
	static DATAFLOWCORE_API FDataflowOutput NoOpOutput;
	
	mutable TSharedPtr<FCriticalSection> OutputLock;
	
	UE_DEPRECATED(5.5, "Deprecated constructor : Guid is now passed through FOutputParameters")
	DATAFLOWCORE_API FDataflowOutput(const Dataflow::FOutputParameters& Param, FGuid InGuid);

	DATAFLOWCORE_API FDataflowOutput(const Dataflow::FOutputParameters& Param = {});

	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	~FDataflowOutput() = default;
	FDataflowOutput(const FDataflowOutput&) = delete; 
	FDataflowOutput(FDataflowOutput&&) = delete;
	FDataflowOutput& operator=(const FDataflowOutput&) = delete;
	FDataflowOutput& operator=(FDataflowOutput&&) = delete;
	PRAGMA_ENABLE_DEPRECATION_WARNINGS

	DATAFLOWCORE_API TArray<FDataflowInput*>& GetConnections();
	DATAFLOWCORE_API const TArray<FDataflowInput*>& GetConnections() const;
	bool HasAnyConnections() const { return !Connections.IsEmpty(); }

	DATAFLOWCORE_API virtual TArray<FDataflowInput*> GetConnectedInputs();
	DATAFLOWCORE_API virtual const TArray<const FDataflowInput*> GetConnectedInputs() const;

	DATAFLOWCORE_API virtual bool AddConnection(FDataflowConnection* InOutput) override;

	DATAFLOWCORE_API virtual bool RemoveConnection(FDataflowConnection* InInput) override;

	UE_DEPRECATED(5.5, "Use SetPassthroughInput instead")
	virtual void SetPassthroughOffset(const uint32 InPassthroughOffset)
	{
		SetPassthroughInput(Dataflow::FConnectionKey(InPassthroughOffset, INDEX_NONE, INDEX_NONE));
	}

	DATAFLOWCORE_API FDataflowOutput& SetPassthroughInput(const Dataflow::FConnectionReference& Reference);
	DATAFLOWCORE_API FDataflowOutput& SetPassthroughInput(const Dataflow::FConnectionKey& Key);

	DATAFLOWCORE_API const FDataflowInput* GetPassthroughInput() const;

	virtual FORCEINLINE void* GetPassthroughRealAddress() const
	{
		if(const FDataflowInput* const PassthroughInput = GetPassthroughInput())
		{
			return PassthroughInput->RealAddress();
		}
		return nullptr;
	}
 
	template<class T>
	void SetValue(T&& InVal, Dataflow::FContext& Context) const
	{
		if (Property)
		{
			Context.SetData(CacheKey(), Property, Forward<T>(InVal), GetOwningNodeGuid(), GetOwningNodeValueHash(), Dataflow::FTimestamp::Current());
		}
	}

	template<typename TAnyType>
	void SetValueFromAnyType(const typename TAnyType::FStorageType& InVal, Dataflow::FContext& Context) const
	{
		TAnyType::FPolicyType::VisitPolicyByType(GetType(),
			[this, &Context, &InVal](auto SingleTypePolicy)
			{
				using FSingleType = typename decltype(SingleTypePolicy)::FType;
				FSingleType ValueToSet{};
				FDataflowConverter<typename TAnyType::FStorageType>::To(InVal, ValueToSet);
				Context.SetData(CacheKey(), GetProperty(), Forward<FSingleType>(ValueToSet), GetOwningNodeGuid(), GetOwningNodeValueHash(), Dataflow::FTimestamp::Current());
			});
	}

	template<class T>
	const T& GetValue(Dataflow::FContext& Context, const T& Default) const
	{
		if (!this->Evaluate(Context))
		{
			Context.SetData(CacheKey(), Property, Default, GetOwningNodeGuid(), GetOwningNodeValueHash(), Dataflow::FTimestamp::Current());
		}

		if (Context.HasData(CacheKey()))
		{
			return Context.GetData(CacheKey(), Property, Default);
		}

		return Default;
	}

	// there's no need for a templatized version as the parameter will not be used
	// the method do check if the type of the input is the same as the output type though 
	DATAFLOWCORE_API void ForwardInput(const Dataflow::FConnectionReference& InputReference, Dataflow::FContext& Context) const;
	DATAFLOWCORE_API void ForwardInput(const FDataflowInput* Input, Dataflow::FContext& Context) const;


	DATAFLOWCORE_API bool EvaluateImpl(Dataflow::FContext& Context) const;
	
	DATAFLOWCORE_API bool Evaluate(Dataflow::FContext& Context) const;

	DATAFLOWCORE_API TFuture<bool> EvaluateParallel(Dataflow::FContext& Context) const;

	DATAFLOWCORE_API virtual void Invalidate(const Dataflow::FTimestamp& ModifiedTimestamp = Dataflow::FTimestamp::Current()) override;

private:
};

template<>
struct TStructOpsTypeTraits<FDataflowOutput> : public  TStructOpsTypeTraitsBase2<FDataflowOutput>
{
	enum
	{
		WithCopy = false,
	};
};

template<typename T>
const T& FDataflowInput::GetValue(Dataflow::FContext& Context, const T& Default) const
{
	if (const FDataflowOutput* ConnectionOut = GetConnection())
	{
		if (!ConnectionOut->Evaluate(Context))
		{
			Context.SetData(ConnectionOut->CacheKey(), Property, Default, GetOwningNodeGuid(), GetOwningNodeValueHash(), Dataflow::FTimestamp::Current());
		}
		if (Context.HasData(ConnectionOut->CacheKey()))
		{
			const T& data = Context.GetData(ConnectionOut->CacheKey(), Property, Default);
			return data;
		}
	}
	return Default;
}

template<typename TAnyType>
typename TAnyType::FStorageType FDataflowInput::GetValueFromAnyType(Dataflow::FContext& Context, const typename TAnyType::FStorageType& Default) const
{
	typename TAnyType::FStorageType ReturnValue = Default;
	if (const FDataflowOutput* ConnectionOut = GetConnection())
	{
		if (ConnectionOut->Evaluate(Context))
		{
			if (const TUniquePtr<Dataflow::FContextCacheElementBase>* CacheEntry = Context.GetDataImpl(ConnectionOut->CacheKey()))
			{
				if (*CacheEntry)
				{
					TAnyType::FPolicyType::VisitPolicyByType(GetType(),
						[this, &Context, &CacheEntry, &ReturnValue](auto SingleTypePolicy)
						{
							using FSingleType = typename decltype(SingleTypePolicy)::FType;
							FSingleType Default{};
							const FSingleType& CachedValue = (*CacheEntry)->GetTypedData<FSingleType>(Context, nullptr, Default);
							FDataflowConverter<typename TAnyType::FStorageType>::From(CachedValue, ReturnValue);
						});
				}
			}
		}
	}
	return ReturnValue;
}

template<class T>
TFuture<const T&> FDataflowInput::GetValueParallel(Dataflow::FContext& Context, const T& Default) const
{
	return Async(EAsyncExecution::TaskGraph, [&]() -> const T& { return this->GetValue<T>(Context, Default); });
}

