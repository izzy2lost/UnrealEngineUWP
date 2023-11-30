// Copyright Epic Games, Inc. All Rights Reserved.

#include "UbaTestAll.h"

int main(int argc, char* argv[])
{
	using namespace uba;

	if (!RunAllTests())
		return -1;
	return 0;
}
