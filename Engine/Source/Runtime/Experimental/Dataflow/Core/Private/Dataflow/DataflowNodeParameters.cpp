// Copyright Epic Games, Inc. All Rights Reserved.

#include "Dataflow/DataflowNodeParameters.h"

#include "Dataflow/DataflowArchive.h"
#include "Dataflow/DataflowContextCachingFactory.h"
#include "Dataflow/DataflowInputOutput.h"
#include "Dataflow/DataflowNode.h"
#include "Serialization/Archive.h"
#include "GeometryCollection/ManagedArrayCollection.h"

namespace Dataflow
{
	uint64 FTimestamp::Invalid = 0;
	uint64 FTimestamp::Current() { return FPlatformTime::Cycles64(); }

	void FContext::PushToCallstack(const FDataflowConnection* Connection)
	{
#if DATAFLOW_EDITOR_EVALUATION
		Callstack.Push(Connection);
#endif
	}

	void FContext::PopFromCallstack(const FDataflowConnection* Connection)
	{
#if DATAFLOW_EDITOR_EVALUATION
		ensure(Connection == Callstack.Top());
		Callstack.Pop();
#endif
	}

	bool FContext::IsInCallstack(const FDataflowConnection* Connection) const
	{
#if DATAFLOW_EDITOR_EVALUATION
		return Callstack.Contains(Connection);
#else
		return false;
#endif
	}

	FContextScopedCallstack::FContextScopedCallstack(FContext& InContext, const FDataflowConnection* InConnection)
		: Context(InContext)
		, Connection(InConnection)
	{
		bLoopDetected = Context.IsInCallstack(Connection);
		Context.PushToCallstack(Connection);
	}

	FContextScopedCallstack::~FContextScopedCallstack()
	{
		Context.PopFromCallstack(Connection);
	}

	void BeginContextEvaluation(FContext& Context, const FDataflowNode* Node, const FDataflowOutput* Output)
	{
		if (Node != nullptr)
		{
			Context.Timestamp = FTimestamp(FPlatformTime::Cycles64());
			if (Node->NumOutputs())
			{
				if (Output)
				{
					Node->Evaluate(Context, Output);
				}
				else
				{
					for (FDataflowOutput* NodeOutput : Node->GetOutputs())
					{
						Node->Evaluate(Context, NodeOutput);
					}
				}
			}
			else
			{
				Node->Evaluate(Context, nullptr);
			}
		}
	}

	void FContextSingle::Evaluate(const FDataflowNode* Node, const FDataflowOutput* Output)
	{
		BeginContextEvaluation(*this, Node, Output);
	}

	bool FContextSingle::Evaluate(const FDataflowOutput& Connection)
	{
		return Connection.EvaluateImpl(*this);
	}



	void FContextThreaded::Evaluate(const FDataflowNode* Node, const FDataflowOutput* Output)
	{
		BeginContextEvaluation(*this, Node, Output);
	}

	bool FContextThreaded::Evaluate(const FDataflowOutput& Connection)
	{
		Connection.OutputLock->Lock(); ON_SCOPE_EXIT{ Connection.OutputLock->Unlock(); };
		return Connection.EvaluateImpl(*this);
	}


	void FContextCache::Serialize(FArchive& Ar)
	{

		if (Ar.IsSaving())
		{

			const int64 NumElementsSavedPosition = Ar.Tell();
			int64 NumElementsWritten = 0;
			Ar << NumElementsWritten;

			for (TPair<FContextCacheKey, TUniquePtr<FContextCacheElementBase>>& Elem : Pairs)
			{
				// note : we only serialize typed cache element and ignore the reference ones ( since they don't hold data per say )
				if (Elem.Value && Elem.Value->Property && Elem.Value->Type == FContextCacheElementBase::EType::CacheElementTyped)
				{
					FProperty* Property = (FProperty*)Elem.Value->Property;
					FString ExtendedType;
					const FString CPPType = Property->GetCPPType(&ExtendedType);
					FName TypeName(CPPType + ExtendedType);
					FGuid NodeGuid = Elem.Value->NodeGuid;
					uint32 NodeHash = Elem.Value->NodeHash;

					if (FContextCachingFactory::GetInstance()->Contains(TypeName))
					{
						Ar << TypeName << Elem.Key << NodeGuid << NodeHash << Elem.Value->Timestamp;

						DATAFLOW_OPTIONAL_BLOCK_WRITE_BEGIN()
						{
							FContextCachingFactory::GetInstance()->Serialize(Ar, {TypeName, NodeGuid, Elem.Value.Get(), NodeHash, Elem.Value->Timestamp});
						}
						DATAFLOW_OPTIONAL_BLOCK_WRITE_END();

						NumElementsWritten++;
					}
				}
			}


			if (NumElementsWritten)
			{
				const int64 FinalPosition = Ar.Tell();
				Ar.Seek(NumElementsSavedPosition);
				Ar << NumElementsWritten;
				Ar.Seek(FinalPosition);
			}
		}
		else if (Ar.IsLoading())
		{
			int64 NumElementsWritten = 0;
			Ar << NumElementsWritten;
			for (int i = NumElementsWritten; i > 0; i--)
			{
				FName TypeName;
				FGuid NodeGuid;
				uint32 NodeHash;
				FContextCacheKey InKey;
				FTimestamp Timestamp = FTimestamp::Invalid;

				Ar << TypeName << InKey << NodeGuid << NodeHash << Timestamp;

				DATAFLOW_OPTIONAL_BLOCK_READ_BEGIN(FContextCachingFactory::GetInstance()->Contains(TypeName))
				{
					FContextCacheElementBase* NewElement = FContextCachingFactory::GetInstance()->Serialize(Ar, { TypeName, NodeGuid, nullptr, NodeHash, Timestamp });
					check(NewElement);
					NewElement->NodeGuid = NodeGuid;
					NewElement->NodeHash = NodeHash;
					NewElement->Timestamp = Timestamp;
					this->Add(InKey, TUniquePtr<FContextCacheElementBase>(NewElement));
				}
				DATAFLOW_OPTIONAL_BLOCK_READ_ELSE()
				{
				}
				DATAFLOW_OPTIONAL_BLOCK_READ_END();
			}
		}
	}
};

