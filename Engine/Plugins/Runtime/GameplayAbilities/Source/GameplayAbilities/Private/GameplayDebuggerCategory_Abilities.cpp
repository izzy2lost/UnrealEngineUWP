// Copyright Epic Games, Inc. All Rights Reserved.

#include "GameplayDebuggerCategory_Abilities.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"

#if WITH_GAMEPLAY_DEBUGGER_MENU

#include "GameplayTagContainer.h"
#include "GameplayAbilitySpec.h"
#include "GameplayEffect.h"
#include "AbilitySystemGlobals.h"
#include "AbilitySystemComponent.h"
#include "Engine/Canvas.h"
#include "Engine/NetConnection.h"
#include "GameFramework/PlayerController.h"


FGameplayDebuggerCategory_Abilities::FGameplayDebuggerCategory_Abilities()
{
	SetDataPackReplication<FRepData>(&DataPack);

	// Hard coding these to avoid needing to import InputCore just for EKeys::GetFName().
	const FName KeyNameOne{ "One" };
	const FName KeyNameTwo{ "Two" };
	const FName KeyNameThree{ "Three" };

	BindKeyPress(KeyNameOne, FGameplayDebuggerInputModifier::Shift, this, &FGameplayDebuggerCategory_Abilities::OnShowGameplayTagsToggle, EGameplayDebuggerInputMode::Local);
	BindKeyPress(KeyNameTwo, FGameplayDebuggerInputModifier::Shift, this, &FGameplayDebuggerCategory_Abilities::OnShowGameplayAbilitiesToggle, EGameplayDebuggerInputMode::Local);
	BindKeyPress(KeyNameThree, FGameplayDebuggerInputModifier::Shift, this, &FGameplayDebuggerCategory_Abilities::OnShowGameplayEffectsToggle, EGameplayDebuggerInputMode::Local);
}

TSharedRef<FGameplayDebuggerCategory> FGameplayDebuggerCategory_Abilities::MakeInstance()
{
	return MakeShareable(new FGameplayDebuggerCategory_Abilities());
}

void FGameplayDebuggerCategory_Abilities::OnShowGameplayTagsToggle()
{
	bShowGameplayTags = !bShowGameplayTags;
}

void FGameplayDebuggerCategory_Abilities::OnShowGameplayAbilitiesToggle()
{
	bShowGameplayAbilities = !bShowGameplayAbilities;
}

void FGameplayDebuggerCategory_Abilities::OnShowGameplayEffectsToggle()
{
	bShowGameplayEffects = !bShowGameplayEffects;
}

void FGameplayDebuggerCategory_Abilities::FRepData::Serialize(FArchive& Ar)
{
	bool bSuccess;
	OwnedTags.NetSerialize(Ar, ClientPackageMap.Get(), bSuccess);

	int32 NumAbilities = Abilities.Num();
	Ar << NumAbilities;
	if (Ar.IsLoading())
	{
		Abilities.SetNum(NumAbilities);
	}

	for (int32 Idx = 0; Idx < NumAbilities; Idx++)
	{
		Ar << Abilities[Idx].Ability;
		Ar << Abilities[Idx].Source;
		Ar << Abilities[Idx].Level;
		Ar << Abilities[Idx].bIsActive;
	}

	int32 NumGE = GameplayEffects.Num();
	Ar << NumGE;
	if (Ar.IsLoading())
	{
		GameplayEffects.SetNum(NumGE);
	}

	for (int32 Idx = 0; Idx < NumGE; Idx++)
	{
		Ar << GameplayEffects[Idx].Effect;
		Ar << GameplayEffects[Idx].Context;
		Ar << GameplayEffects[Idx].Duration;
		Ar << GameplayEffects[Idx].Period;
		Ar << GameplayEffects[Idx].Stacks;
		Ar << GameplayEffects[Idx].Level;
	}
}

void FGameplayDebuggerCategory_Abilities::CollectData(APlayerController* OwnerPC, AActor* DebugActor)
{
	UAbilitySystemComponent* AbilityComp = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(DebugActor);
	if (AbilityComp)
	{
		// Save off the package map for serialization over the network
		UNetConnection* NetConnection = OwnerPC->GetNetConnection();
		DataPack.ClientPackageMap = NetConnection ? NetConnection->PackageMap : nullptr;

		AbilityComp->GetOwnedGameplayTags(DataPack.OwnedTags);

		TArray<FGameplayEffectSpec> ActiveEffectSpecs;
		AbilityComp->GetAllActiveGameplayEffectSpecs(ActiveEffectSpecs);
		for (int32 Idx = 0; Idx < ActiveEffectSpecs.Num(); Idx++)
		{
			const FGameplayEffectSpec& EffectSpec = ActiveEffectSpecs[Idx];
			FRepData::FGameplayEffectDebug ItemData;

			ItemData.Effect = EffectSpec.ToSimpleString();
			ItemData.Effect.RemoveFromStart(DEFAULT_OBJECT_PREFIX);
			ItemData.Effect.RemoveFromEnd(TEXT("_C"));

			ItemData.Context = EffectSpec.GetContext().ToString();
			ItemData.Duration = EffectSpec.GetDuration();
			ItemData.Period = EffectSpec.GetPeriod();
			ItemData.Stacks = EffectSpec.GetStackCount();
			ItemData.Level = EffectSpec.GetLevel();

			DataPack.GameplayEffects.Add(ItemData);
		}

		const TArray<FGameplayAbilitySpec>& AbilitySpecs = AbilityComp->GetActivatableAbilities();
		for (int32 Idx = 0; Idx < AbilitySpecs.Num(); Idx++)
		{
			const FGameplayAbilitySpec& AbilitySpec = AbilitySpecs[Idx];
			FRepData::FGameplayAbilityDebug ItemData;

			ItemData.Ability = GetNameSafe(AbilitySpec.Ability);
			ItemData.Ability.RemoveFromStart(DEFAULT_OBJECT_PREFIX);
			ItemData.Ability.RemoveFromEnd(TEXT("_C"));

			ItemData.Source = GetNameSafe(AbilitySpec.SourceObject.Get());
			ItemData.Source.RemoveFromStart(DEFAULT_OBJECT_PREFIX);

			ItemData.Level = AbilitySpec.Level;
			ItemData.bIsActive = AbilitySpec.IsActive();

			DataPack.Abilities.Add(ItemData);
		}
	}
}

bool FGameplayDebuggerCategory_Abilities::WrapStringAccordingToViewport(const FString& StrIn, FString& StrOut, FGameplayDebuggerCanvasContext& CanvasContext, float ViewportWitdh) const
{
	if (!StrIn.IsEmpty())
	{
		// Clamp the Width
		ViewportWitdh = FMath::Max(ViewportWitdh, 10.0f);

		float StrWidth = 0.0f, StrHeight = 0.0f;
		// Calculate the length(in pixel) of the tags
		CanvasContext.MeasureString(StrIn, StrWidth, StrHeight);

		int32 SubDivision = FMath::CeilToInt(StrWidth / ViewportWitdh);
		if (SubDivision > 1)
		{
			// Copy the string
			StrOut = StrIn;
			const int32 Step = StrOut.Len() / SubDivision;
			// Start sub divide if needed
			for (int32 i = SubDivision - 1; i > 0; --i)
			{
				// Insert Line Feed
				StrOut.InsertAt(i * Step - 1, '\n');
			}
			return true;
		}
		else
		{
			StrOut = StrIn;
		}
	}
	// No need to wrap the text 
	return false;
}

void FGameplayDebuggerCategory_Abilities::DrawData(APlayerController* OwnerPC, FGameplayDebuggerCanvasContext& CanvasContext)
{
	// Draw the sub-category bindings inline with the category header
	{
		CanvasContext.CursorX += 250.0f;
		CanvasContext.CursorY -= CanvasContext.GetLineHeight();
		const TCHAR* Active = TEXT("{green}");
		const TCHAR* Inactive = TEXT("{yellow}");
		CanvasContext.Printf(TEXT("Tags [%s%s{white}]\tAbilities [%s%s{white}]\tEffects [%s%s{white}]"), bShowGameplayTags ? Active : Inactive, *GetInputHandlerDescription(0), bShowGameplayAbilities ? Active : Inactive, *GetInputHandlerDescription(1), bShowGameplayEffects ? Active : Inactive, *GetInputHandlerDescription(2));
	}

	static float LastDrawDataEndSize = CanvasContext.Canvas->SizeY - CanvasContext.CursorY - CanvasContext.CursorX;
	float ThisDrawDataStartPos = CanvasContext.CursorY;

	const FLinearColor BackgroundColor(0.1f, 0.1f, 0.1f, 0.8f);
	const FVector2D BackgroundPos{ CanvasContext.CursorX, CanvasContext.CursorY };
	const FVector2D BackgroundSize(CanvasContext.Canvas->SizeX - (2.0f * CanvasContext.CursorX), LastDrawDataEndSize);

	// Draw a transparent dark background so that the text is easier to look at
	FCanvasTileItem Background(FVector2D(0.0f), BackgroundSize, BackgroundColor);
	Background.BlendMode = SE_BLEND_Translucent;

	CanvasContext.DrawItem(Background, BackgroundPos.X, BackgroundPos.Y);

	if (bShowGameplayTags)
	{
		DrawGameplayTags(CanvasContext, OwnerPC);
	}

	if (bShowGameplayAbilities)
	{
		DrawGameplayAbilities(CanvasContext, OwnerPC);
	}

	if (bShowGameplayEffects)
	{
		DrawGameplayEffects(CanvasContext, OwnerPC);
	}

	LastDrawDataEndSize = CanvasContext.CursorY - ThisDrawDataStartPos;
}

void FGameplayDebuggerCategory_Abilities::DrawGameplayTags(FGameplayDebuggerCanvasContext& CanvasContext, const APlayerController* OwnerPC) const
{
	const float CanvasWidth = CanvasContext.Canvas->SizeX;

	AActor* LocalDebugActor = FindLocalDebugActor();
	if (UAbilitySystemComponent* AbilityComp = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(LocalDebugActor))
	{
		FGameplayTagContainer ServerOnlyTags = DataPack.OwnedTags;

		// If we're not the authority, we should represent the tags in such a way that the user can see the difference between agreed upon server/client tags
		// and ones where they disagree.
		if (!AbilityComp->IsOwnerActorAuthoritative())
		{
			static FGameplayTagContainer LocalOnlyTags;
			AbilityComp->GetOwnedGameplayTags(LocalOnlyTags);

			FGameplayTagContainer MatchingTags = ServerOnlyTags.FilterExact(LocalOnlyTags);
			ServerOnlyTags.RemoveTags(MatchingTags);
			LocalOnlyTags.RemoveTags(MatchingTags);

			// Wrap the strings to the viewport
			FString MatchingTagsStr, ServerOnlyTagsStr, LocalOnlyTagsStr;
			WrapStringAccordingToViewport(MatchingTags.ToStringSimple(), MatchingTagsStr, CanvasContext, CanvasWidth);
			WrapStringAccordingToViewport(ServerOnlyTags.ToStringSimple(), ServerOnlyTagsStr, CanvasContext, CanvasWidth);
			WrapStringAccordingToViewport(LocalOnlyTags.ToStringSimple(), LocalOnlyTagsStr, CanvasContext, CanvasWidth);

			MatchingTagsStr = MatchingTagsStr.Len() > 0 ? FString::Printf(TEXT("{cyan}%s  "), *MatchingTagsStr) : FString{};
			ServerOnlyTagsStr = ServerOnlyTagsStr.Len() > 0 ? FString::Printf(TEXT("{yellow}%s  "), *ServerOnlyTagsStr) : FString{};
			LocalOnlyTagsStr = LocalOnlyTagsStr.Len() > 0 ? FString::Printf(TEXT("{green}%s  "), *LocalOnlyTagsStr) : FString{};

			CanvasContext.Printf(TEXT("Owned Tags Legend:  {cyan}Both  {yellow}Server  {green}Local \n%s%s%s"), *MatchingTagsStr, *ServerOnlyTagsStr, *LocalOnlyTagsStr);
		}
		else
		{
			FString ServerOnlyTagsStr;
			WrapStringAccordingToViewport(ServerOnlyTags.ToStringSimple(), ServerOnlyTagsStr, CanvasContext, CanvasWidth);
			CanvasContext.Printf(TEXT("Owned Tags: \n{cyan}%s"), *ServerOnlyTagsStr);
		}
	}

	// End with a newline to separate from the other categories
	CanvasContext.Print(TEXT(""));
}

void FGameplayDebuggerCategory_Abilities::DrawGameplayEffects(FGameplayDebuggerCanvasContext& CanvasContext, const APlayerController* OwnerPC) const
{
	CanvasContext.Printf(TEXT("Gameplay Effects: {yellow}%d"), DataPack.GameplayEffects.Num());
	for (int32 Idx = 0; Idx < DataPack.GameplayEffects.Num(); Idx++)
	{
		const FRepData::FGameplayEffectDebug& ItemData = DataPack.GameplayEffects[Idx];

		FStringBuilderBase Desc;
		Desc.Appendf(TEXT("\t{yellow}%s {grey}source:{white}%s {grey}duration:{white}"), *ItemData.Effect, *ItemData.Context);
		if (ItemData.Duration > 0.0f)
		{
			Desc.Appendf(TEXT("%.3f"), ItemData.Duration);
		}
		else
		{
			Desc.Appendf(TEXT("INF"));
		}

		if (ItemData.Period > 0.0f)
		{
			Desc.Appendf(TEXT(" {grey}period:{white}%.3f"), ItemData.Period);
		}

		if (ItemData.Stacks > 1)
		{
			Desc.Appendf(TEXT(" {grey}stacks:{white}%d"), ItemData.Stacks);
		}

		if (ItemData.Level > 1.0f)
		{
			Desc.Appendf(TEXT(" {grey}level:{white}%.2f"), ItemData.Level);
		}

		CanvasContext.Print(Desc.ToString());
	}
	
	// End with a newline to separate from the other categories
	CanvasContext.MoveToNewLine();
}

void FGameplayDebuggerCategory_Abilities::DrawGameplayAbilities(FGameplayDebuggerCanvasContext& CanvasContext, const APlayerController* OwnerPC) const
{
	const float CanvasWidth = CanvasContext.Canvas->SizeX;

	// Let's do something semi-smart and stable to resize the columns to readable yet wide enough
	int32 NumActive = 0;
	FString LongestLengthObjectName;
	for (const FRepData::FGameplayAbilityDebug& ItemData : DataPack.Abilities)
	{
		if (ItemData.Ability.Len() > LongestLengthObjectName.Len())
		{
			LongestLengthObjectName = ItemData.Ability;
		}

		if (ItemData.Source.Len() > LongestLengthObjectName.Len())
		{
			LongestLengthObjectName = ItemData.Source;
		}

		NumActive += ItemData.bIsActive;
	}

	// Measure the individual string sizes, so that we can size the columns properly
	constexpr float Padding = 10.0f;
	static float ObjNameSize = 150.0f, SourceNameSize = 100.0f, LevelNameSize = 100.0f, TempSizeY = 0.0f;
	static int32 CachedLen = 0;
	if (LongestLengthObjectName.Len() > CachedLen)
	{
		CanvasContext.MeasureString(LongestLengthObjectName, ObjNameSize, TempSizeY);
		CanvasContext.MeasureString(TEXT("source: "), SourceNameSize, TempSizeY);
		CanvasContext.MeasureString(TEXT("level: 00"), LevelNameSize, TempSizeY);
		ObjNameSize += Padding;
	}
	const float ColumnWidth = ObjNameSize * 2 + SourceNameSize + LevelNameSize;
	const int NumColumns = FMath::Max(1, FMath::FloorToInt(CanvasWidth / ColumnWidth));

	CanvasContext.Printf(TEXT("Gameplay Abilities: \t{yellow}Granted[%d] \t{cyan}Active[%d]"), DataPack.Abilities.Num(), NumActive);
	for (const FRepData::FGameplayAbilityDebug& ItemData : DataPack.Abilities)
	{
		float CursorX = CanvasContext.CursorX;
		float CursorY = CanvasContext.CursorY;

		// Print positions manually to align them properly
		CanvasContext.PrintAt(CursorX + ObjNameSize * 0, CursorY, ItemData.bIsActive ? FColor::Cyan : FColor::Yellow, ItemData.Ability);
		CanvasContext.PrintAt(CursorX + ObjNameSize * 1, CursorY, FString::Printf(TEXT("{grey}source:{white}%s"), *ItemData.Source));
		CanvasContext.PrintAt(CursorX + ObjNameSize * 2 + SourceNameSize, CursorY, FString::Printf(TEXT("{grey}level:{white}%02d"), ItemData.Level));

		// PrintAt would have reset these values, restore them.
		CanvasContext.CursorX = CursorX + (CanvasWidth / NumColumns);
		CanvasContext.CursorY = CursorY;

		// If we're going to overflow, go to the next line...
		if (CanvasContext.CursorX + ColumnWidth >= CanvasWidth)
		{
			CanvasContext.MoveToNewLine();
		}
	}

	// End the row with a newline
	if (CanvasContext.CursorX != CanvasContext.DefaultX)
	{
		CanvasContext.MoveToNewLine();
	}

	// End the category with a newline to separate from the other categories
	CanvasContext.MoveToNewLine();
}

#endif // WITH_GAMEPLAY_DEBUGGER_MENU
