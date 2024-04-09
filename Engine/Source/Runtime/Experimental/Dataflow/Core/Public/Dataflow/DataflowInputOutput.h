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
	struct FInputParameters {
		FInputParameters(FName InType = FName(""), FName InName = FName(""), FDataflowNode* InOwner = nullptr, const FProperty* InProperty = nullptr)
			: Type(InType)
			, Name(InName)
			, Owner(InOwner)
			, Property(InProperty){}
		FName Type;
		FName Name;
		FDataflowNode* Owner = nullptr;
		const FProperty* Property = nullptr;
	};
}

USTRUCT()
struct FDataflowInput : public FDataflowConnection
{
	GENERATED_USTRUCT_BODY()

	static FDataflowInput NoOpInput;

	friend struct FDataflowConnection;

	FDataflowOutput* Connection;
public:
	FDataflowInput(const Dataflow::FInputParameters& Param = {}, FGuid InGuid = FGuid::NewGuid());

	virtual bool AddConnection(FDataflowConnection* InOutput) override;
	virtual bool RemoveConnection(FDataflowConnection* InOutput) override;

	FDataflowOutput* GetConnection() { return Connection; }
	const FDataflowOutput* GetConnection() const { return Connection; }

	virtual TArray< FDataflowOutput* > GetConnectedOutputs();
	virtual const TArray< const FDataflowOutput* > GetConnectedOutputs() const;

	/** 
	* Get the value of this input by evaluating the value of the connected output 
	* @return the typed value of the input 
	*/
	template<class T>
	const T& GetValue(Dataflow::FContext& Context, const T& Default) const;

	/**
	* pull the value from the upstream connections
	* the upstream graph is evaluated if necessary and values are cached along the way 
	*/
	void PullValue(Dataflow::FContext& Context) const;

	template<class T>
	TFuture<const T&> GetValueParallel(Dataflow::FContext& Context, const T& Default) const;

	virtual void Invalidate(const Dataflow::FTimestamp& ModifiedTimestamp = Dataflow::FTimestamp::Current()) override;
};

//
// Output
//
namespace Dataflow
{
	struct FOutputParameters
	{
		FOutputParameters(FName InType = FName(""), FName InName = FName(""), FDataflowNode* InOwner = nullptr, const FProperty* InProperty = nullptr)
			: Type(InType)
			, Name(InName)
			, Owner(InOwner)
			, Property(InProperty) {}

		FName Type;
		FName Name;
		FDataflowNode* Owner = nullptr;
		const FProperty* Property = nullptr;
	};
}
USTRUCT()
struct FDataflowOutput : public FDataflowConnection
{
	GENERATED_USTRUCT_BODY()

	friend struct FDataflowConnection;
	
	TArray< FDataflowInput* > Connections;

	uint32 PassthroughOffset = INDEX_NONE;

public:
	static DATAFLOWCORE_API FDataflowOutput NoOpOutput;
	
	mutable TSharedPtr<FCriticalSection> OutputLock;
	
	DATAFLOWCORE_API FDataflowOutput(const Dataflow::FOutputParameters& Param = {}, FGuid InGuid = FGuid::NewGuid());

	DATAFLOWCORE_API TArray<FDataflowInput*>& GetConnections();
	DATAFLOWCORE_API const TArray<FDataflowInput*>& GetConnections() const;

	DATAFLOWCORE_API virtual TArray<FDataflowInput*> GetConnectedInputs();
	DATAFLOWCORE_API virtual const TArray<const FDataflowInput*> GetConnectedInputs() const;

	DATAFLOWCORE_API virtual bool AddConnection(FDataflowConnection* InOutput) override;

	DATAFLOWCORE_API virtual bool RemoveConnection(FDataflowConnection* InInput) override;

	virtual FORCEINLINE void SetPassthroughOffset(const uint32 InPassthroughOffset)
	{
		PassthroughOffset = InPassthroughOffset;
	}

	virtual FORCEINLINE void* GetPassthroughRealAddress() const
	{
		if(PassthroughOffset != INDEX_NONE)
		{
			return (void*)((size_t)OwningNode + (size_t)PassthroughOffset);
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
	DATAFLOWCORE_API void ForwardInput(const void* InputReference, Dataflow::FContext& Context) const;

	DATAFLOWCORE_API bool EvaluateImpl(Dataflow::FContext& Context) const;
	
	DATAFLOWCORE_API bool Evaluate(Dataflow::FContext& Context) const;

	DATAFLOWCORE_API TFuture<bool> EvaluateParallel(Dataflow::FContext& Context) const;

	DATAFLOWCORE_API virtual void Invalidate(const Dataflow::FTimestamp& ModifiedTimestamp = Dataflow::FTimestamp::Current()) override;

private:
	DATAFLOWCORE_API const FDataflowInput* GetPassthroughInput() const;
};
 
template<class T>
const T& FDataflowInput::GetValue(Dataflow::FContext& Context, const T& Default) const
{
	if (GetConnectedOutputs().Num())
	{
		ensure(GetConnectedOutputs().Num() == 1);
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
	}
	return Default;
}

template<class T>
TFuture<const T&> FDataflowInput::GetValueParallel(Dataflow::FContext& Context, const T& Default) const
{
	return Async(EAsyncExecution::TaskGraph, [&]() -> const T& { return this->GetValue<T>(Context, Default); });
}

