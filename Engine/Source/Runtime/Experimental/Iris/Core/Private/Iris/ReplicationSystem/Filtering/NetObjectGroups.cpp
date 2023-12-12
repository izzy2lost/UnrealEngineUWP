// Copyright Epic Games, Inc. All Rights Reserved.

#include "NetObjectGroups.h"
#include "Math/UnrealMathUtility.h"
#include "Iris/Core/IrisLog.h"
#include "Iris/ReplicationSystem/Filtering/NetObjectFilter.h"
#include "Iris/ReplicationSystem/NetRefHandleManager.h" // for InvalidInternalIndex
#include "Containers/ArrayView.h"

namespace UE::Net::Private
{

FNetObjectGroups::FNetObjectGroups()
: MaxGroupCount(0U)
, CurrentEpoch(++NextEpoch)
{
}

FNetObjectGroups::~FNetObjectGroups()
{
}

bool FNetObjectGroups::IsMemberOf(const FNetObjectGroupMembership& Target, FNetObjectGroupHandle Group)
{
	for (uint32 It = 0; It < FNetObjectGroupMembership::MaxAssignedGroupCount; ++It)
	{
		if (Target.Groups[It] == Group)
		{
			return true;
		}
	}

	return false;
}

bool FNetObjectGroups::AddGroupMembership(FNetObjectGroupMembership& Target, FNetObjectGroupHandle Group)
{
	for (uint32 It = 0; It < FNetObjectGroupMembership::MaxAssignedGroupCount; ++It)
	{
		if (Target.Groups[It] == Group)
		{
			return false;
		}
		else if (!Target.Groups[It].IsValid())
		{
			Target.Groups[It] = Group;
			return true;
		}
	}

	return false;
}

void FNetObjectGroups::RemoveGroupMembership(FNetObjectGroupMembership& Target, FNetObjectGroupHandle Group)
{
	const uint32 LastIndex = FNetObjectGroupMembership::MaxAssignedGroupCount - 1U;
	for (uint32 It = 0U; It < FNetObjectGroupMembership::MaxAssignedGroupCount; ++It)
	{
		if (Target.Groups[It] == Group)
		{
			FPlatformMemory::Memmove(&Target.Groups[It], &Target.Groups[It + 1], (LastIndex - It) * sizeof(FNetObjectGroupHandle));
			Target.Groups[LastIndex] = FNetObjectGroupHandle();

			return;
		}
	}
}

void FNetObjectGroups::ResetGroupMembership(FNetObjectGroupMembership& Target)
{
	FMemory::Memzero(Target);
}

void FNetObjectGroups::Init(const FNetObjectGroupInitParams& Params)
{
	ensureMsgf(Params.MaxGroupCount < std::numeric_limits<FNetObjectGroupHandle::FGroupIndexType>::max(), TEXT("MaxGroupCount cannot exceed %u"), std::numeric_limits<FNetObjectGroupHandle::FGroupIndexType>::max());
	MaxGroupCount = FMath::Clamp<uint32>(Params.MaxGroupCount, 0U, std::numeric_limits<FNetObjectGroupHandle::FGroupIndexType>::max());

	// Reserve first as invalid group
	Groups.Add(FNetObjectGroup());

	GroupMemberships.SetNumZeroed(Params.MaxObjectCount);
	GroupFilteredOutObjects.Init(Params.MaxObjectCount);
}

FNetObjectGroupHandle FNetObjectGroups::CreateGroup()
{
	if (ensure((uint32)Groups.Num() < MaxGroupCount))
	{
		const uint32 Index = static_cast<uint32>(Groups.Add(FNetObjectGroup()));
		FNetObjectGroupHandle GroupHandle;
		GroupHandle.Index = static_cast<FNetObjectGroupHandle::FGroupIndexType>(Index);
		GroupHandle.Epoch = CurrentEpoch;
		return GroupHandle;
	}
	else
	{
		return FNetObjectGroupHandle();
	}
}

void FNetObjectGroups::DestroyGroup(FNetObjectGroupHandle GroupHandle)
{
	if (IsValidGroup(GroupHandle))
	{
		ClearGroup(GroupHandle);

		FNetObjectGroup& Group = Groups[GroupHandle.GetGroupIndex()];

		if (EnumHasAnyFlags(Group.Traits, ENetObjectGroupTraits::IsFindableByName))
		{
			NamedGroups.FindAndRemoveChecked(Group.GroupName);
		}

		Groups.RemoveAt(GroupHandle.GetGroupIndex());
	}
}

void FNetObjectGroups::SetGroupName(FNetObjectGroupHandle GroupHandle, FName GroupName)
{
	if (FNetObjectGroup* Group = GetGroup(GroupHandle))
	{
		if (ensureMsgf(EnumHasAnyFlags(Group->Traits, ENetObjectGroupTraits::IsFindableByName), TEXT("FNetObjectGroups::SetGroupName Cannot SetGroupName for grouphandle %u as it is FindableByName"), GroupHandle.GetGroupIndex()))
		{
			Group->GroupName = GroupName;
		}
	}
}

FName FNetObjectGroups::GetGroupName(FNetObjectGroupHandle GroupHandle) const
{
	if (const FNetObjectGroup* Group = GetGroup(GroupHandle))
	{
		return Group->GroupName;
	}

	return FName();
}

FNetObjectGroupHandle FNetObjectGroups::CreateNamedGroup(FName GroupName)
{
	if (NamedGroups.Contains(GroupName))
	{
		ensureMsgf(false, TEXT("FNetObjectGroups, trying to create named group %s that already exists"), *GroupName.ToString());
		return FNetObjectGroupHandle();
	}

	FNetObjectGroupHandle GroupHandle = CreateGroup();
	if (FNetObjectGroup* Group = GetGroup(GroupHandle))
	{
		Group->GroupName = GroupName;
		Group->Traits |= ENetObjectGroupTraits::IsFindableByName;
		NamedGroups.Add(GroupName, GroupHandle);
	}

	return GroupHandle;
}

FNetObjectGroupHandle FNetObjectGroups::GetNamedGroupHandle(FName GroupName)
{
	if (FNetObjectGroupHandle* Group = NamedGroups.Find(GroupName))
	{
		return *Group;
	}

	return FNetObjectGroupHandle();
}

void FNetObjectGroups::DestroyNamedGroup(FName GroupName)
{
	DestroyGroup(GetNamedGroupHandle(GroupName));
}

void FNetObjectGroups::ClearGroup(FNetObjectGroupHandle GroupHandle)
{
	if (IsValidGroup(GroupHandle))
	{
		FNetObjectGroup& Group = Groups[GroupHandle.GetGroupIndex()];

		for (FInternalNetRefIndex InternalIndex : Group.Members)
		{
			checkSlow(IsMemberOf(GroupMemberships[InternalIndex], GroupHandle));
			RemoveGroupMembership(GroupMemberships[InternalIndex], GroupHandle);
		}

		Group.Members.Empty();
	}
}

const FNetObjectGroup* FNetObjectGroups::GetGroup(FNetObjectGroupHandle GroupHandle) const
{
	return IsValidGroup(GroupHandle) ? &Groups[GroupHandle.GetGroupIndex()] : nullptr;
}

FNetObjectGroup* FNetObjectGroups::GetGroup(FNetObjectGroupHandle GroupHandle)
{
	return IsValidGroup(GroupHandle) ? &Groups[GroupHandle.GetGroupIndex()] : nullptr;
}

const FNetObjectGroup* FNetObjectGroups::GetGroupByIndex(FNetObjectGroupHandle::FGroupIndexType GroupIndex) const
{
	return (GroupIndex != FNetObjectGroupHandle::InvalidNetObjectGroupIndex && Groups.IsValidIndex(GroupIndex)) ? &Groups[GroupIndex] : nullptr;
}

FNetObjectGroup* FNetObjectGroups::GetGroupByIndex(FNetObjectGroupHandle::FGroupIndexType GroupIndex)
{
	return (GroupIndex != FNetObjectGroupHandle::InvalidNetObjectGroupIndex && Groups.IsValidIndex(GroupIndex)) ? &Groups[GroupIndex] : nullptr;
}

bool FNetObjectGroups::Contains(FNetObjectGroupHandle GroupHandle, FInternalNetRefIndex InternalIndex) const
{
	// If the group does not exist we cannot be in it..
	const FNetObjectGroup* Group = GetGroup(GroupHandle);
	if (!Group)
	{
		return false;
	}

	checkSlow(InternalIndex == 0U || (Group->Members.Contains(InternalIndex) == IsMemberOf(GroupMemberships[InternalIndex], GroupHandle)));

	return InternalIndex ? IsMemberOf(GroupMemberships[InternalIndex], GroupHandle) : false;
}

void FNetObjectGroups::AddToGroup(FNetObjectGroupHandle GroupHandle, FInternalNetRefIndex InternalIndex)
{
	FNetObjectGroup* Group = GetGroup(GroupHandle);
	if (InternalIndex != FNetRefHandleManager::InvalidInternalIndex && Group)
	{
		if (AddGroupMembership(GroupMemberships[InternalIndex], GroupHandle))
		{
			Group->Members.AddUnique(InternalIndex);

			if (IsFilterGroup(*Group))
			{
				GroupFilteredOutObjects.SetBit(InternalIndex);
			}
		}
		else
		{
			UE_LOG(LogIris, Error, TEXT("FNetObjectGroups::AddToGroup, Failed to add ( InternalIndex: %u ) to Group %s (GroupIndex: %u) A NetObject can only be a member of %u groups."), InternalIndex, *GetGroupName(GroupHandle).ToString(), GroupHandle.GetRawValue(), FNetObjectGroupMembership::MaxAssignedGroupCount);
			ensure(false);
		}
	}
}

void FNetObjectGroups::RemoveFromGroup(FNetObjectGroupHandle GroupHandle, FInternalNetRefIndex InternalIndex)
{
	FNetObjectGroup* Group = GetGroup(GroupHandle);
	if (InternalIndex != FNetRefHandleManager::InvalidInternalIndex && Group)
	{
		FNetObjectGroupMembership& GroupMembership = GroupMemberships[InternalIndex];
		checkSlow(IsMemberOf(GroupMembership, GroupHandle));

		RemoveGroupMembership(GroupMembership, GroupHandle);
		Group->Members.RemoveSingle(InternalIndex);

		// Check to see if the object is still part of a filter group
		if (!IsInAnyFilterGroup(GroupMembership))
		{
			GroupFilteredOutObjects.ClearBit(InternalIndex);
		}
	}
}

void FNetObjectGroups::AddExclusionFilterTrait(FNetObjectGroupHandle GroupHandle)
{
	if (FNetObjectGroup* Group = GetGroup(GroupHandle))
	{
		if (!IsFilterGroup(*Group))
		{
			Group->Traits |= ENetObjectGroupTraits::IsExclusionFiltering;

			// Flag all current members of this group that they are now filterable
			for (FInternalNetRefIndex MemberIndex : Group->Members)
			{
				GroupFilteredOutObjects.SetBit(MemberIndex);
			}
		}
	}
}

void FNetObjectGroups::RemoveExclusionFilterTrait(FNetObjectGroupHandle GroupHandle)
{
	FNetObjectGroup* Group = GetGroup(GroupHandle);
	if (Group == nullptr)
	{
		return;
	}

	if (!IsExclusionFilterGroup(*Group))
	{
		return;
	}

	Group->Traits &= ~(ENetObjectGroupTraits::IsExclusionFiltering);

	for (FInternalNetRefIndex MemberIndex : Group->Members)
	{
		// Check to see if the object is still part of a filter group
		if (!IsInAnyFilterGroup(GroupMemberships[MemberIndex]))
		{
			GroupFilteredOutObjects.ClearBit(MemberIndex);
		}
	}
}

void FNetObjectGroups::AddInclusionFilterTrait(FNetObjectGroupHandle GroupHandle)
{
	if (FNetObjectGroup* Group = GetGroup(GroupHandle))
	{
		// Can't be both inclusion and exclusion so let's do nothing if the group has any sort of filter trait.
		if (!IsFilterGroup(*Group))
		{
			Group->Traits |= ENetObjectGroupTraits::IsInclusionFiltering;
		}
	}
}

void FNetObjectGroups::RemoveInclusionFilterTrait(FNetObjectGroupHandle GroupHandle)
{
	FNetObjectGroup* Group = GetGroup(GroupHandle);
	if (Group == nullptr)
	{
		return;
	}

	// Simply remove the trait.
	Group->Traits &= ~(ENetObjectGroupTraits::IsInclusionFiltering);
}

bool FNetObjectGroups::IsFilterGroup(FNetObjectGroupHandle GroupHandle) const
{
	if (const FNetObjectGroup* Group = GetGroup(GroupHandle))
	{
		return EnumHasAnyFlags(Group->Traits, ENetObjectGroupTraits::IsExclusionFiltering | ENetObjectGroupTraits::IsInclusionFiltering);
	}

	return false;
}

bool FNetObjectGroups::IsInAnyFilterGroup(const FNetObjectGroupMembership& GroupMembership) const
{
	for (FNetObjectGroupHandle AssignedGroup : GroupMembership.Groups)
	{
		if (!AssignedGroup.IsValid())
		{
			// Note: An invalid group means we found the end of the array
			return false;
		}
		else if (IsFilterGroup(Groups[AssignedGroup.GetGroupIndex()]))
		{
			return true;
		}
	}

	return false;
}

uint32 FNetObjectGroups::GetNumGroupMemberships(FInternalNetRefIndex InternalIndex) const
{
	if (InternalIndex >= (uint32)GroupMemberships.Num())
	{
		return 0U;
	}

	uint32 Count = 0U;
	for (uint32 It=0; It < FNetObjectGroupMembership::MaxAssignedGroupCount; ++It)
	{
		Count += GroupMemberships[InternalIndex].Groups[It].IsValid() ? 1U : 0U;
	}
	
	return Count;
}

const FNetObjectGroupHandle* FNetObjectGroups::GetGroupMemberships(FInternalNetRefIndex InternalIndex, uint32& GroupCount) const
{
	GroupCount = GetNumGroupMemberships(InternalIndex);
	if (GroupCount)
	{
		return GroupMemberships[InternalIndex].Groups;
	}
	else
	{
		return nullptr;
	}
}

}
