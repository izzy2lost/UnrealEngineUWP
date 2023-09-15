// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Serialization/Archive.h"

namespace UE::AnimNext
{
	/**
	 * Latent Property Handle
	 * Latent property handles represent a 1-based index into the RigVM memory handles array during execution.
	 *
	 * @see FExecutionContext
	 */
	struct FLatentPropertyHandle final
	{
		// Creates an invalid latent property handle
		constexpr FLatentPropertyHandle() noexcept
			: HandleValue(INVALID_HANDLE_VALUE)
		{}

		// Returns true if this latent property handle is valid, false otherwise
		constexpr bool IsValid() const noexcept { return HandleValue != INVALID_HANDLE_VALUE; }

		// Returns the latent property index represented by this handle if valid, -1 otherwise
		constexpr int32 GetLatentPropertyIndex() const noexcept { return IsValid() ? (HandleValue - 1) : INDEX_NONE; }

		// The first valid latent handle
		static constexpr FLatentPropertyHandle GetFirstHandle() noexcept { return FLatentPropertyHandle(1); }

		// Returns the next latent handle
		// If the current handle is invalid, the next is invalid as well
		// Note that this function does not handle wrapping if more than 65536 handles are used
		// If wrapping occurs, the next handle returned will be invalid and all subsequent handles will be invalid
		constexpr FLatentPropertyHandle GetNextHandle() const noexcept { return IsValid() ? FLatentPropertyHandle(HandleValue + 1) : FLatentPropertyHandle(); }

	private:
		// Constructs a FLatentPropertyHandle instance from a raw value
		explicit constexpr FLatentPropertyHandle(uint16 InHandleValue) noexcept : HandleValue(InHandleValue) {}

		// Latent property handles are a 1-based index, 0 is invalid
		static constexpr uint16 INVALID_HANDLE_VALUE = 0;

		// The raw 1-based latent property index or 0 if invalid
		uint16		HandleValue;

		friend FArchive& operator<<(FArchive& Ar, FLatentPropertyHandle& Handle);
	};

	// Serializes a latent handle value
	inline FArchive& operator<<(FArchive& Ar, FLatentPropertyHandle& Handle)
	{
		Ar << Handle.HandleValue;
		return Ar;
	}
}
