// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Commandlets/IChunkDataGenerator.h"

TArray<TFunction<TSharedRef<IChunkDataGenerator>(const UE::Cook::ICookInfo&)>> IChunkDataGenerator::GeneratorsDataGeneratorFactories;

void IChunkDataGenerator::AddChunkDataGeneratorFactory(const TFunction<TSharedRef<IChunkDataGenerator>(const UE::Cook::ICookInfo&)>& InChunkGeneratorFactory)
{
	GeneratorsDataGeneratorFactories.Add(InChunkGeneratorFactory);
}

const TArray<TFunction<TSharedRef<IChunkDataGenerator>(const UE::Cook::ICookInfo&)>>& IChunkDataGenerator::GetChunkDataGeneratorFactories()
{
	return GeneratorsDataGeneratorFactories;
}