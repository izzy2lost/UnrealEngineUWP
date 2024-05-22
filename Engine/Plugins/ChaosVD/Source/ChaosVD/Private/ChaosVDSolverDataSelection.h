// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "Templates/SharedPointer.h"
#include "UObject/StructOnScope.h"
#include "ChaosVDSolverDataSelection.generated.h"

class FChaosVDSolverDataSelection;

USTRUCT()
struct FChaosVDSelectionContext
{
	GENERATED_BODY()
};

struct FChaosVDSolverDataSelectionHandle : public TSharedFromThis<FChaosVDSolverDataSelectionHandle>
{
	virtual ~FChaosVDSolverDataSelectionHandle() = default;
	
	template<typename DataStructType>
	void SetHandleData(const TSharedPtr<DataStructType>& Data);
	
	template<typename ContextDataStructType>
	void SetHandleContext(ContextDataStructType&& ContextData);

	void SetOwner(const TSharedPtr<FChaosVDSolverDataSelection>& InOwner);

	virtual bool IsSelected();

	bool IsValid() const;

	template<typename DataStructType>
	bool IsA() const;

	template<typename DataStructType>
	DataStructType* GetData() const;

	template<typename DataStructType>
	TSharedPtr<DataStructType> GetDataAsShared() const;

	template<typename ContextDataStructType>
	ContextDataStructType* GetContextData() const;

	TSharedPtr<FStructOnScope> GetDataAsStructScope() { return SelectedDataStruct; }

	bool operator==(const FChaosVDSolverDataSelectionHandle& Other) const
	{
		return DataSharedPtr.Get() == Other.DataSharedPtr.Get();
	}

	bool operator!=(const FChaosVDSolverDataSelectionHandle& Other) const
	{
		return DataSharedPtr.Get() != Other.DataSharedPtr.Get();
	}

private:

	template<typename DataStructType>
	bool IsA_Internal(const TSharedPtr<FStructOnScope>& InStructOnScope) const;

	TSharedPtr<FStructOnScope> SelectedDataStruct;
	TSharedPtr<FStructOnScope> SelectedDataContext;

	TSharedPtr<void> DataSharedPtr;
	TSharedPtr<void> SelectedDataContextSharedPtr;

	TWeakPtr<FChaosVDSolverDataSelection> Owner;
};

template <typename DataStructType>
void FChaosVDSolverDataSelectionHandle::SetHandleData(const TSharedPtr<DataStructType>& Data)
{
	if (Data)
	{
		SelectedDataStruct = MakeShared<FStructOnScope>(DataStructType::StaticStruct(), reinterpret_cast<uint8*>(Data.Get()));
		DataSharedPtr = Data;
	}
	else
	{
		SelectedDataStruct->Reset();
		DataSharedPtr = nullptr;
	}
}

template <typename ContextDataStructType>
void FChaosVDSolverDataSelectionHandle::SetHandleContext(ContextDataStructType&& ContextData)
{
	SelectedDataContextSharedPtr = MakeShared<ContextDataStructType>();
	ContextDataStructType* ContextDataRawPtr = StaticCastSharedPtr<ContextDataStructType>(SelectedDataContextSharedPtr).Get();
	*ContextDataRawPtr = MoveTemp(ContextData);

	SelectedDataContext = MakeShared<FStructOnScope>(ContextDataStructType::StaticStruct(), reinterpret_cast<uint8*>(ContextDataRawPtr));		
}

template <typename DataStructType>
bool FChaosVDSolverDataSelectionHandle::IsA() const
{
	return IsA_Internal<DataStructType>(SelectedDataStruct);
}

template <typename DataStructType>
DataStructType* FChaosVDSolverDataSelectionHandle::GetData() const
{
	if (IsA_Internal<DataStructType>(SelectedDataStruct))
	{
		return reinterpret_cast<DataStructType*>(SelectedDataStruct->GetStructMemory());
	}
	
	return nullptr;
}

template <typename DataStructType>
TSharedPtr<DataStructType> FChaosVDSolverDataSelectionHandle::GetDataAsShared() const
{
	if (IsA_Internal<DataStructType>(SelectedDataStruct))
	{
		return StaticCastSharedPtr<DataStructType>(DataSharedPtr);
	}
	return nullptr;
}

template <typename ContextDataStructType>
ContextDataStructType* FChaosVDSolverDataSelectionHandle::GetContextData() const
{
	if (IsA_Internal<ContextDataStructType>(SelectedDataContext))
	{
		return reinterpret_cast<ContextDataStructType*>(SelectedDataContext->GetStructMemory());
	}

	return nullptr;
}

template <typename DataStructType>
bool FChaosVDSolverDataSelectionHandle::IsA_Internal(const TSharedPtr<FStructOnScope>& InStructOnScope) const
{
	if (InStructOnScope)
	{
		const UStruct* HandleStruct = InStructOnScope->GetStruct();
		return HandleStruct && (DataStructType::StaticStruct() == HandleStruct || HandleStruct->IsChildOf(DataStructType::StaticStruct()));
	}

	return false;
}


DECLARE_MULTICAST_DELEGATE_OneParam(FChaosVDSolverDataSelectionChangedDelegate, const TSharedPtr<FChaosVDSolverDataSelectionHandle>& SelectionHandle)

class FChaosVDSolverDataSelection : public TSharedFromThis<FChaosVDSolverDataSelection>
{
public:

	void SelectData(const TSharedPtr<FChaosVDSolverDataSelectionHandle>& InSelectionHandle);

	template<typename SolverDataType>
	TSharedPtr<FChaosVDSolverDataSelectionHandle> MakeSelectionHandle(const TSharedPtr<SolverDataType>& InSolverData);

	FChaosVDSolverDataSelectionChangedDelegate& GetDataSelectionChangedDelegate() { return SolverDataSelectionChangeDelegate; }
	TSharedPtr<FChaosVDSolverDataSelectionHandle> GetCurrentSelectionHandle() { return CurrentSelectedSolverDataHandle; }

	template <typename SolverDataType>
	bool IsDataSelected(const TSharedPtr<SolverDataType>& InSolverData);
	
	bool IsSelectionHandleSelected(const TSharedPtr<FChaosVDSolverDataSelectionHandle>& InSelectionHandle) const;

private:
	FChaosVDSolverDataSelectionChangedDelegate SolverDataSelectionChangeDelegate;
	TSharedPtr<FChaosVDSolverDataSelectionHandle> CurrentSelectedSolverDataHandle;
};

template <typename SolverDataType>
TSharedPtr<FChaosVDSolverDataSelectionHandle> FChaosVDSolverDataSelection::MakeSelectionHandle(const TSharedPtr<SolverDataType>& InSolverData)
{
	TSharedPtr<FChaosVDSolverDataSelectionHandle> NewSelectionHandle = MakeShared<FChaosVDSolverDataSelectionHandle>();
	NewSelectionHandle->SetHandleData(StaticCastSharedPtr<SolverDataType>(InSolverData));
	NewSelectionHandle->SetOwner(AsShared());

	return NewSelectionHandle;
}

template <typename SolverDataType>
bool FChaosVDSolverDataSelection::IsDataSelected(const TSharedPtr<SolverDataType>& InSolverData)
{
	TSharedPtr<FChaosVDSolverDataSelectionHandle> SelectionHandle = MakeSelectionHandle(InSolverData);
	return SelectionHandle->IsSelected();
}