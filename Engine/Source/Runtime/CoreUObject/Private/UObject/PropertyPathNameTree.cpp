// Copyright Epic Games, Inc. All Rights Reserved.

#include "UObject/PropertyPathNameTree.h"

#include "Templates/TypeHash.h"

namespace UE
{

void FPropertyPathNameTree::Empty()
{
	Nodes.Empty();
}

void FPropertyPathNameTree::Add(const FPropertyPathName& Path, int32 StartIndex)
{
	if (const int32 SegmentCount = Path.GetSegmentCount(); StartIndex < SegmentCount)
	{
		FPropertyPathNameSegment Segment = Path.GetSegment(StartIndex);
		TUniquePtr<FPropertyPathNameTree>& Child = Nodes.FindOrAdd({Segment.Name, Segment.Type});
		if (++StartIndex < SegmentCount)
		{
			if (!Child)
			{
				Child = MakeUnique<FPropertyPathNameTree>();
			}
			Child->Add(Path, StartIndex);
		}
	}
}

bool FPropertyPathNameTree::Find(FPropertyPathNameTree** OutSubTree, const FPropertyPathName& Path, int32 StartIndex)
{
	if (const int32 SegmentCount = Path.GetSegmentCount(); StartIndex < SegmentCount)
	{
		FPropertyPathNameSegment Segment = Path.GetSegment(StartIndex);
		if (TUniquePtr<FPropertyPathNameTree>* Child = Nodes.Find({Segment.Name, Segment.Type}))
		{
			if (++StartIndex >= SegmentCount)
			{
				if (OutSubTree)
				{
					*OutSubTree = Child->Get();
				}
				return true;
			}
			if (*Child)
			{
				return (*Child)->Find(OutSubTree, Path, StartIndex);
			}
		}
	}
	return false;
}

} // UE
