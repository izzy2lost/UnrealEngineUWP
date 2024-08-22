// Copyright Epic Games, Inc. All Rights Reserved.

#include "RemoteControlDMXLibraryBuilder.h"

#include "Algo/AnyOf.h"
#include "RemoteControlDMXUserData.h"
#include "RemoteControlPreset.h"
#include "Library/DMXEntityFixturePatch.h"
#include "Library/DMXLibrary.h"
#include "Library/RemoteControlDMXControlledPropertyPatch.h"
#include "Library/RemoteControlDMXLibraryProxy.h"
#include "Library/RemoteControlDMXPatchBuilder.h"

namespace UE::RemoteControl::DMX
{
	void FRemoteControlDMXLibraryBuilder::Register()
	{
		static TSharedRef<FRemoteControlDMXLibraryBuilder> Instance = MakeShared<FRemoteControlDMXLibraryBuilder>();

		URemoteControlDMXLibraryProxy::GetOnPrePropertyPatchesChanged().AddSP(Instance, &FRemoteControlDMXLibraryBuilder::PrePropertyPatchesChanged);
		URemoteControlDMXLibraryProxy::GetOnPostPropertyPatchesChanged().AddSP(Instance, &FRemoteControlDMXLibraryBuilder::PostPropertyPatchesChanged);
	}

	void FRemoteControlDMXLibraryBuilder::PrePropertyPatchesChanged(URemoteControlPreset* InOutPreset)
	{
		// Set the preset to work with 
		Preset = InOutPreset;

		URemoteControlDMXLibraryProxy* DMXLibraryProxy = GetDMXLibraryProxy();
		UDMXLibrary* DMXLibrary = GetDMXLibrary();
		if (DMXLibraryProxy && DMXLibrary)
		{
			// Remember fixture patches and fixture types before property patches changed
			const TArray<TSharedRef<FRemoteControlDMXControlledPropertyPatch>> PreEditChangePropertyPatches = DMXLibraryProxy->GetPropertyPatches();

			PreviousFixturePatches.Reset();
			Algo::TransformIf(PreEditChangePropertyPatches, PreviousFixturePatches,
				[](const TSharedRef<FRemoteControlDMXControlledPropertyPatch>& PropertyPatch)
				{
					return PropertyPatch->GetFixturePatch();
				},
				[](const TSharedRef<FRemoteControlDMXControlledPropertyPatch>& PropertyPatch)
				{
					return PropertyPatch->GetFixturePatch();
				});

			PreviousFixtureTypes.Reset();
			Algo::TransformIf(PreviousFixturePatches, PreviousFixtureTypes,
				[](const UDMXEntityFixturePatch* FixturePatch)
				{
					return FixturePatch->GetFixtureType();
				},
				[](const UDMXEntityFixturePatch* FixturePatch)
				{
					return FixturePatch->GetFixtureType();
				});
		}
	}

	void FRemoteControlDMXLibraryBuilder::PostPropertyPatchesChanged()
	{
		if (URemoteControlDMXLibraryProxy* DMXLibraryProxy = GetDMXLibraryProxy())
		{
			const TArray<TSharedRef<FRemoteControlDMXControlledPropertyPatch>> PostEditChangePropertyPatches = DMXLibraryProxy->GetPropertyPatches();
			FRemoteControlDMXPatchBuilder::BuildFixturePatches(AsShared(), PostEditChangePropertyPatches);

			const URemoteControlDMXUserData* DMXUserData = GetDMXUserData();
			const bool bAutoAssign = DMXUserData && DMXUserData->IsAutoPatch();
			if (bAutoAssign)
			{
				AutoAssignFixturePatches(PostEditChangePropertyPatches);
			}

			RemoveObsoleteFixturesFromDMXLibrary(PostEditChangePropertyPatches);
		}
	}

	URemoteControlDMXUserData* FRemoteControlDMXLibraryBuilder::GetDMXUserData() const
	{
		return URemoteControlDMXUserData::GetOrCreateDMXUserData(Preset);
	}

	URemoteControlDMXLibraryProxy* FRemoteControlDMXLibraryBuilder::GetDMXLibraryProxy() const
	{
		URemoteControlDMXUserData* DMXUserData = GetDMXUserData();
		return DMXUserData ? DMXUserData->GetDMXLibraryProxy() : nullptr;
	}

	UDMXLibrary* FRemoteControlDMXLibraryBuilder::GetDMXLibrary() const
	{
		URemoteControlDMXLibraryProxy* DMXLibraryProxy = GetDMXLibraryProxy();
		return DMXLibraryProxy ? DMXLibraryProxy->GetDMXLibrary() : nullptr;
	}

	void FRemoteControlDMXLibraryBuilder::RemoveObsoleteFixturesFromDMXLibrary(const TArray<TSharedRef<FRemoteControlDMXControlledPropertyPatch>>& PostEditChangePropertyPatches)
	{
		UDMXLibrary* DMXLibrary = GetDMXLibrary();
		if (DMXLibrary)
		{
			for (UDMXEntityFixturePatch* PreviousFixturePatch : PreviousFixturePatches)
			{
				if (!PreviousFixturePatch || !PreviousFixturePatch->GetParentLibrary())
				{
					continue;
				}

				const bool bFixturePatchStillReferenced = Algo::AnyOf(PostEditChangePropertyPatches,
					[PreviousFixturePatch](const TSharedRef<FRemoteControlDMXControlledPropertyPatch>& PropertyPatch)
					{
						if (PropertyPatch->GetFixturePatch() == PreviousFixturePatch)
						{
							return true;
						}

						return false;
					});

				if (!bFixturePatchStillReferenced)
				{
					UDMXEntityFixturePatch::RemoveFixturePatchFromLibrary(PreviousFixturePatch);
				}
			}

			for (UDMXEntityFixtureType* PreviousFixtureType : PreviousFixtureTypes)
			{
				if (!PreviousFixtureType || !PreviousFixtureType->GetParentLibrary())
				{
					continue;
				}

				const TArray<UDMXEntityFixturePatch*> FixturePatchesInLibrary = DMXLibrary->GetEntitiesTypeCast<UDMXEntityFixturePatch>();

				const bool bFixtureTypeStillReferenced = Algo::AnyOf(FixturePatchesInLibrary, [PreviousFixtureType](const UDMXEntityFixturePatch* FixturePatch)
					{
						return FixturePatch && FixturePatch->GetFixtureType() == PreviousFixtureType;
					});

				if (!bFixtureTypeStillReferenced)
				{
					UDMXEntityFixtureType::RemoveFixtureTypeFromLibrary(PreviousFixtureType);
				}
			}
		}
	}

	void FRemoteControlDMXLibraryBuilder::AutoAssignFixturePatches(const TArray<TSharedRef<FRemoteControlDMXControlledPropertyPatch>>& PostEditChangePropertyPatches)
	{
		URemoteControlDMXUserData* DMXUserData = GetDMXUserData();
		UDMXLibrary* DMXLibrary = GetDMXLibrary();
		if (!DMXUserData || !DMXLibrary)
		{
			return;
		}

		TArray<UDMXEntityFixturePatch*> FixturePatches;
		Algo::TransformIf(PostEditChangePropertyPatches, FixturePatches,
			[](const TSharedRef<FRemoteControlDMXControlledPropertyPatch>& PropertyPatch)
			{
				return PropertyPatch->GetFixturePatch() != nullptr;
			},
			[](const TSharedRef<FRemoteControlDMXControlledPropertyPatch>& PropertyPatch)
			{
				return PropertyPatch->GetFixturePatch();
			});

		// Reset patch
		for (UDMXEntityFixturePatch* FixturePatch : FixturePatches)
		{
			FixturePatch->SetStartingChannel(1);
			FixturePatch->SetUniverseID(1);
		}

		// Auto assign
		for (const TSharedRef<FRemoteControlDMXControlledPropertyPatch>& PropertyPatch : PostEditChangePropertyPatches)
		{
			UDMXEntityFixturePatch* FixturePatch = PropertyPatch->GetFixturePatch();
			if (!FixturePatch)
			{
				continue;
			}

			const int32 FixturePatchIndex = FixturePatches.IndexOfByKey(FixturePatch);
			if (!ensureMsgf(FixturePatchIndex != INDEX_NONE, TEXT("Unexpected cannot find fixture patch in DMX Library. Cannot auto assign fixture patch.")))
			{
				continue;
			}

			const int32 AutoAssingFromUniverse = DMXUserData->GetAutoAssignFromUniverse();
			const UDMXEntityFixturePatch* PreviousPatch = FixturePatches.IsValidIndex(FixturePatchIndex - 1) ? FixturePatches[FixturePatchIndex - 1] : nullptr;
			const int64 DesiredAbsoluteStartingChannel = [PreviousPatch, AutoAssingFromUniverse]()
				{
					if (PreviousPatch && PreviousPatch->GetUniverseID() >= AutoAssingFromUniverse)
					{
						return (int64)PreviousPatch->GetUniverseID() * DMX_UNIVERSE_SIZE + PreviousPatch->GetEndingChannel();
					}
					else
					{
						return (int64)AutoAssingFromUniverse * DMX_UNIVERSE_SIZE;
					}
				}();

			const bool bFitsUniverse = DesiredAbsoluteStartingChannel % DMX_UNIVERSE_SIZE + FixturePatch->GetChannelSpan() <= DMX_UNIVERSE_SIZE;
			const int64 AbsoluteStartingChannel = bFitsUniverse ? DesiredAbsoluteStartingChannel : (DesiredAbsoluteStartingChannel / DMX_UNIVERSE_SIZE + 1) * DMX_UNIVERSE_SIZE;

			const int32 Universe = AbsoluteStartingChannel / DMX_UNIVERSE_SIZE;
			const int32 Channel = AbsoluteStartingChannel % DMX_UNIVERSE_SIZE + 1;

			FixturePatch->PreEditChange(nullptr);

			FixturePatch->SetUniverseID(Universe);
			FixturePatch->SetStartingChannel(Channel);

			FixturePatch->PostEditChange();
		}
	}
}
