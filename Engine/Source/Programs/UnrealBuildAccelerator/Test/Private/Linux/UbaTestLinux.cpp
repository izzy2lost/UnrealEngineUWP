// Copyright Epic Games, Inc. All Rights Reserved.

#include "UbaTestAll.h"

void segfault_sigaction(int signal, siginfo_t* si, void* arg)
{
	uba::UbaAssert("Segmentation fault", "", 0, "", -1);
}

int main()
{
	struct sigaction sa;

	memset(&sa, 0, sizeof(struct sigaction));
	sigemptyset(&sa.sa_mask);
	sa.sa_sigaction = segfault_sigaction;
	sa.sa_flags = SA_SIGINFO;

	sigaction(SIGSEGV, &sa, NULL);

	using namespace uba;
	if (!RunAllTests())
		return -1;
	return 0;
}
