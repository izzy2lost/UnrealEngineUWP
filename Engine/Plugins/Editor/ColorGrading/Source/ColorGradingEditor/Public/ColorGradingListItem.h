// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/Brush.h"
#include "GameFramework/Actor.h"
#include "ScopedTransaction.h"
#include "Templates/SharedPointer.h"

typedef TSharedPtr<class IColorGradingListItemGenerator> FColorGradingListItemGeneratorRef;
typedef TSharedPtr<struct FColorGradingListItem> FColorGradingListItemRef;

DECLARE_DELEGATE_TwoParams(FOnColorGradingItemEnabledChanged, FColorGradingListItemRef, bool);

/** Interface that allows color grading list items to be generated for specific UObjects */
class COLORGRADINGEDITOR_API IColorGradingListItemGenerator : public TSharedFromThis<IColorGradingListItemGenerator>
{
public:
	virtual ~IColorGradingListItemGenerator() {}

	/** Returns a list of actor classes for which this can generate color grading list items */
	virtual TArray<TSubclassOf<AActor>> GetActorClassesForListItems() const = 0;

	/** Appends FColorGradingListItems for each color gradable object associated with the given actor */
	virtual void GenerateColorGradingListItems(AActor* InActor, TArray<FColorGradingListItemRef>& OutList) const = 0;
};

// Macros to create the lambdas used for a specified UObject's color grading enabled property in the color grading object list
#define CREATE_IS_ENABLED_LAMBDA(Object, IsEnabledProperty) TAttribute<bool>::CreateLambda([Object]() { return (bool)IsEnabledProperty; })
#define CREATE_ON_ENABLED_CHANGED_LAMBDA(Object, IsEnabledProperty) FOnColorGradingItemEnabledChanged::CreateLambda([Object](FColorGradingListItemRef ListItem, bool bIsEnabled) \
	{ \
		FScopedTransaction Transaction(NSLOCTEXT("ColorGradingEditor", "ColorGradingToggledTransaction", "Color Grading Toggled")); \
		Object->Modify(!Object->IsA<ABrush>()); \
		IsEnabledProperty = bIsEnabled; \
	})

/** A structure to store references to color gradable actors and components */
struct COLORGRADINGEDITOR_API FColorGradingListItem
{
public:
	/** The actor that is color gradable */
	TWeakObjectPtr<AActor> Actor;

	/** The component that is color gradable */
	TWeakObjectPtr<UActorComponent> Component;

	/** Attribute that retrieves whether color grading is enabled on the color gradable item */
	TAttribute<bool> IsItemEnabled;

	/** Delegate raised when the enabled state of the color gradable item has been changed */
	FOnColorGradingItemEnabledChanged OnItemEnabledChanged;

	FColorGradingListItem(AActor* InActor, UActorComponent* InComponent = nullptr)
		: Actor(InActor)
		, Component(InComponent)
		, IsItemEnabled(false)
	{ }

	/** Less than operator overload that compares list items alphabetically by their display names */
	bool operator<(const FColorGradingListItem& Other) const;

public:
	/** Returns the set of actor classes for which the registered data model generators can generate color grading list items */
	static const TSet<TSubclassOf<AActor>>& GetActorClassesWithListItemGenerators() { return ActorClassesWithListItemGenerators; }

	/** Registers a new list item generator used to populate a color grading item list */
	template<class T>
	static void RegisterColorGradingListItemGenerator()
	{
		TSharedRef<IColorGradingListItemGenerator> Generator = MakeShareable(new T());
		RegisteredListItemGenerators.Add(Generator);
		ActorClassesWithListItemGenerators.Append(Generator->GetActorClassesForListItems());
	}

	/** Returns a list of FColorGradingListItems for each color gradable object associated with the given actor */
	static TArray<FColorGradingListItemRef> GenerateColorGradingListItems(AActor* InActor);

private:
	/** A list of list item generators that have been registered */
	static TArray<FColorGradingListItemGeneratorRef> RegisteredListItemGenerators;

	/** The set of actor classes for which the registered data model generators can generate color grading list items */
	static TSet<TSubclassOf<AActor>> ActorClassesWithListItemGenerators;
};