// Copyright Epic Games, Inc. All Rights Reserved.

#include "WorldMetricsConsoleCommands.h"

#if WITH_WORLDMETRICS_DEBUG

#include "HAL/IConsoleManager.h"
#include "String/LexFromString.h"
#include "WorldMetricInterface.h"
#include "WorldMetrics.h"
#include "WorldMetricsLog.h"
#include "WorldMetricsSubsystem.h"

namespace UE::WorldMetrics::Private
{

static FAutoConsoleCommandWithWorldAndArgs CmdWorldMetricsEnable(
	TEXT("WorldMetrics"),
	TEXT("Toggles the World Metrics Subsystem unless a boolean parameter is passed."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda(
		[](const TArray<FString>& Args, UWorld* World)
		{
			if (UWorldMetricsSubsystem* WorldMetrics = GetSubsystem(World))
			{
				bool bEnable = !WorldMetrics->IsEnabled();
				if (Args.Num())
				{
					bEnable = Args[0].ToBool();
				}
				WorldMetrics->Enable(bEnable);
			}
		}),
	ECVF_Default);

static FAutoConsoleCommandWithWorldAndArgs CmdWorldMetricsClear(
	TEXT("WorldMetrics.Clear"),
	TEXT("Clears the World Metrics Subsystem removing all metrics and extensions."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda(
		[](const TArray<FString>& Args, UWorld* World)
		{
			if (UWorldMetricsSubsystem* WorldMetrics = GetSubsystem(World))
			{
				WorldMetrics->Clear();
			}
		}),
	ECVF_Default);

static FAutoConsoleCommandWithWorldAndArgs CmdWorldMetricsSetUpdateRateInSeconds(
	TEXT("WorldMetrics.SetUpdateRateInSeconds"),
	TEXT("Resets the World Metrics Subsystem update rate to its default value or the parameter value if specified."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda(
		[](const TArray<FString>& Args, UWorld* World)
		{
			if (UWorldMetricsSubsystem* WorldMetrics = GetSubsystem(World))
			{
				float UpdateRateInSeconds = 0;
				if (Args.Num() && Args[0].IsNumeric())
				{
					UpdateRateInSeconds = FCString::Atof(*Args[0]);
				}
				WorldMetrics->SetUpdateRateInSeconds(UpdateRateInSeconds);
			}
		}),
	ECVF_Default);

static void WorldMetricToggleCmdHandler(
	const TArray<FString>& Args,
	UWorld* World,
	FOutputDevice& Ar,
	TWeakObjectPtr<UClass> MetricClass)
{
	UClass* MetricClassPtr = MetricClass.Get();
	if (MetricClassPtr == nullptr)
	{
		Ar.Logf(TEXT("[%hs]: Invalid class."), __FUNCTION__);
		return;
	}

	const bool bMetricExists = GetMetric(World, MetricClassPtr) != nullptr;
	const FName ClassName = MetricClassPtr->GetFName();

	bool bEnable = !bMetricExists;
	if (Args.IsValidIndex(0))
	{
		LexFromString(bEnable, Args[0]);
	}

	if (bEnable == bMetricExists)
	{
		Ar.Log(*WriteToString<256>(
			ClassName, ANSITEXTVIEW(" is already "), bEnable ? ANSITEXTVIEW("enabled.") : ANSITEXTVIEW("disabled.")));
		return;
	}

	if (bEnable && AddMetric(World, MetricClassPtr))
	{
		Ar.Log(*WriteToString<128>(ClassName, ANSITEXTVIEW(" added.")));
		return;
	}

	if (!bEnable && RemoveMetric(World, MetricClassPtr))
	{
		Ar.Log(*WriteToString<128>(ClassName, ANSITEXTVIEW(" removed.")));
		return;
	}

	Ar.Log(*WriteToString<256>(
		ClassName, ANSITEXTVIEW(" couldn't be "), bEnable ? ANSITEXTVIEW("enabled.") : ANSITEXTVIEW("disabled.")));
}

static IConsoleObject* RegisterMetricToggleConsoleCommand(IConsoleManager& ConsoleManager, UClass* Class)
{
	auto NameStringBuilder = WriteToString<256>(ANSITEXTVIEW("WorldMetrics."), Class->GetFName());
	return ConsoleManager.RegisterConsoleCommand(
		NameStringBuilder.ToString(), *WriteToString<256>(NameStringBuilder, ANSITEXTVIEW(" [0|1]")),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(
			&WorldMetricToggleCmdHandler, MakeWeakObjectPtr(Class)));
}

static void RegisterCommonConsoleCommandsForMetric(
	IConsoleManager& ConsoleManager,
	UClass* Class,
	TArray<IConsoleObject*>& OutConsoleObjects)
{
	using FConsoleCommandFunc = IConsoleObject* (*)(IConsoleManager& ConsoleManager, UClass* Class);
	constexpr FConsoleCommandFunc Funcs[] = {
		&RegisterMetricToggleConsoleCommand,
	};

	for (FConsoleCommandFunc Func : Funcs)
	{
		IConsoleObject* Command = Invoke(Func, ConsoleManager, Class);
		if (Command != nullptr)
		{
			OutConsoleObjects.Add(Command);
		}
		else
		{
			UE_LOG(
				LogWorldMetrics, Warning, TEXT("%hs: Failed to register command for class '%s'"), __FUNCTION__,
				*Class->GetName());
		}
	}
}

}  // namespace UE::WorldMetrics::Private

void UE::WorldMetrics::RegisterCommonMetricConsoleCommands(
	IConsoleManager& ConsoleManager,
	TArray<IConsoleObject*>& OutConsoleObjects)
{
	TArray<UClass*> WorldMetricClasses;
	GetDerivedClasses(UWorldMetricInterface::StaticClass(), WorldMetricClasses, true);

	OutConsoleObjects.Reserve(OutConsoleObjects.Num() + WorldMetricClasses.Num());

	for (UClass* Class : WorldMetricClasses)
	{
		if (Class == nullptr || Class->HasAnyClassFlags(CLASS_Abstract))
		{
			continue;
		}

		Private::RegisterCommonConsoleCommandsForMetric(ConsoleManager, Class, OutConsoleObjects);
	}
}

#endif	// WITH_WORLDMETRICS_DEBUG
