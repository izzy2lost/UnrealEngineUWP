// Copyright Epic Games, Inc. All Rights Reserved.

#include "AdvancedRenamer.h"

#include "AdvancedRenamerModule.h"

FAdvancedRenamer::FAdvancedRenamer(const TSharedRef<IAdvancedRenamerProvider>& InProvider)
	: Provider(InProvider)
{
	int32 Count = Num();
	check(Count > 0);

	for (int32 Index = 0; Index < Count; ++Index)
	{
		if (!CanRename(Index))
		{
			RemoveIndex(Index);
			--Index;
			--Count;
			continue;
		}

		const int32 Hash = GetHash(Index);
		FString OriginalName = GetOriginalName(Index);

		Previews.Add(MakeShared<FAdvancedRenamerPreview>(Hash, OriginalName));
	}
}

const TSharedRef<IAdvancedRenamerProvider>& FAdvancedRenamer::GetProvider() const
{
	return Provider;
}

const TArray<TSharedPtr<FAdvancedRenamerPreview>>& FAdvancedRenamer::GetPreviews()
{
	return Previews;
}

TSharedPtr<FAdvancedRenamerPreview> FAdvancedRenamer::GetPreview(int32 InIndex) const
{
	if (Previews.IsValidIndex(InIndex))
	{
		return Previews[InIndex];
	}

	return nullptr;
}

void FAdvancedRenamer::AddSection(FAdvancedRenamerExecuteSection InSection)
{
	Sections.Add(InSection);
}

bool FAdvancedRenamer::HasRenames() const
{
	return bHasRenames;
}

bool FAdvancedRenamer::IsDirty() const
{
	return bDirty;
}

void FAdvancedRenamer::MarkDirty()
{
	bDirty = true;
}

void FAdvancedRenamer::MarkClean()
{
	bDirty = false;
}

bool FAdvancedRenamer::UpdatePreviews()
{
	bHasRenames = false;
	const int32 Count = Previews.Num();

	BeforeOperationsStartExecute();

	for (int32 Index = 0; Index < Count; ++Index)
	{
		if (!Previews[Index].IsValid() || !IsValidIndex(Index))
		{
			RemoveIndex(Index);
			--Index;
			continue;
		}

		// Force recreation
		Previews[Index]->NewName = ApplyRename(Previews[Index]->OriginalName);

		if (Previews[Index]->NewName.IsEmpty())
		{
			continue;
		}

		if (GetOriginalName(Index).Equals(Previews[Index]->NewName))
		{
			continue;
		}

		bHasRenames = true;
	}

	AfterOperationsEndExecute();

	MarkClean();

	return bHasRenames;
}

bool FAdvancedRenamer::Execute()
{
	if (!HasRenames())
	{
		UpdatePreviews();

		if (!HasRenames())
		{
			return false;
		}
	}

	const int32 Count = Previews.Num();
	bool bAllSuccess = true;

	for (int32 Index = 0; Index < Count; ++Index)
	{
		if (!Previews[Index].IsValid())
		{
			continue;
		}

		if (!IsValidIndex(Index))
		{
			continue;
		}

		if (Previews[Index]->NewName.IsEmpty())
		{
			continue;
		}

		if (!ExecuteRename(Index, Previews[Index]->NewName))
		{
			bAllSuccess = false;
		}
	}

	MarkClean();

	return bAllSuccess;
}

FString FAdvancedRenamer::ApplyRename(const FString& InOriginalName)
{
	FString NewName = InOriginalName;
	for (FAdvancedRenamerExecuteSection& Section : Sections)
	{
		Section.OnOperationExecuted().ExecuteIfBound(NewName);
	}
	return NewName;
}

void FAdvancedRenamer::BeforeOperationsStartExecute()
{
	for (FAdvancedRenamerExecuteSection& Section : Sections)
	{
		Section.OnBeforeOperationExecutionStart().ExecuteIfBound();
	}
}

void FAdvancedRenamer::AfterOperationsEndExecute()
{
	for (FAdvancedRenamerExecuteSection& Section : Sections)
	{
		Section.OnAfterOperationExecutionEnded().ExecuteIfBound();
	}
}

int32 FAdvancedRenamer::Num() const
{
	return Provider->Num();
}

bool FAdvancedRenamer::IsValidIndex(int32 InIndex) const
{
	return Provider->IsValidIndex(InIndex);
}

uint32 FAdvancedRenamer::GetHash(int32 InIndex) const
{
	return Provider->GetHash(InIndex);
}

FString FAdvancedRenamer::GetOriginalName(int32 InIndex) const
{
	return Provider->GetOriginalName(InIndex);
}

bool FAdvancedRenamer::RemoveIndex(int32 InIndex)
{
	// Can fail during construction when indices that aren't renameable are removed from the provider before
	// they are added to ListData.
	if (Previews.IsValidIndex(InIndex))
	{
		Previews.RemoveAt(InIndex);
	}

	return Provider->RemoveIndex(InIndex);
}

bool FAdvancedRenamer::CanRename(int32 InIndex) const
{
	return Provider->CanRename(InIndex);
}

bool FAdvancedRenamer::ExecuteRename(int32 InIndex, const FString& InNewName)
{
	return Provider->ExecuteRename(InIndex, InNewName);
}
