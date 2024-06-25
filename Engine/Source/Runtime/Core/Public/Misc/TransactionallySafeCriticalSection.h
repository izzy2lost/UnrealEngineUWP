// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/CriticalSection.h"
#include "AutoRTFM/AutoRTFM.h"

// A transactionally safe critical section that works in the following novel ways:
// - In the open (non-transactional):
//   - Take the lock like before. Simple!
//   - Free the lock like before too.
// - In the closed (transactional):
//   - During locking we query `TransactionalLockCount`:
//     - 0 means we haven't taken the lock within our transaction nest and need to acquire the lock.
//     - Otherwise we already have the lock (and are preventing non-transactional code seeing any
//       modifications we've made while holding the lock), so just bump `TransactionalLockCount`.
//     - We also register an on-abort handler to release the lock should we abort (but we need to
//       query `TransactionalLockCount` even there because we could be aborting an inner transaction
//       and the parent transaction still wants to have the lock held!).
//   - During unlocking we defer doing the unlock until the transaction commits.
//
// Thus with this approach we will hold this lock for the *entirety* of the transactional nest should
// we take the lock during the transaction, thus preventing non-transactional code from seeing any
// modifications we should make.
struct FTransactionallySafeCriticalSectionDefinition final
{
	void Lock()
	{
		if (AutoRTFM::IsTransactional() || AutoRTFM::IsCommittingOrAborting())
		{
			AutoRTFM::Open([&]
				{
					// The transactional system which can increment TransactionalLockCount
					// is always single-threaded, thus this is safe to check without atomicity.
					if (0 == TransactionalLockCount)
					{
						CriticalSection.Lock();
					}

					TransactionalLockCount += 1;
				});

			AutoRTFM::OnAbort([this]
				{
					ensure(0 != TransactionalLockCount);
					TransactionalLockCount -= 1;

					if (0 == TransactionalLockCount)
					{
						CriticalSection.Unlock();
					}
				});
		}
		else
		{
			CriticalSection.Lock();
			ensure(0 == TransactionalLockCount);
		}
	}

	void Unlock()
	{
		if (AutoRTFM::IsTransactional() || AutoRTFM::IsCommittingOrAborting())
		{
			AutoRTFM::OnCommit([this]
				{
					ensure(0 != TransactionalLockCount);
					TransactionalLockCount -= 1;

					if (0 == TransactionalLockCount)
					{
						CriticalSection.Unlock();
					}
				});
		}
		else
		{
			ensure(0 == TransactionalLockCount);
			CriticalSection.Unlock();
		}
	}

private:
	FCriticalSection CriticalSection;
	uint32 TransactionalLockCount = 0;
};

#if UE_AUTORTFM
using FTransactionallySafeCriticalSection = FTransactionallySafeCriticalSectionDefinition;
#else
using FTransactionallySafeCriticalSection = FCriticalSection;
#endif
