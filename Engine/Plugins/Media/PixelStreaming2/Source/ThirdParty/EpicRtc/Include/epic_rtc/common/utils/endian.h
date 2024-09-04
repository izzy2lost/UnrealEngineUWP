// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include <bit>
#include <concepts>
#include <algorithm>
#include <ranges>

#pragma pack(push, 8)

// This byteswap is included to C++23, but can be implemented based on C++20
// See https://en.cppreference.com/w/cpp/numeric/byteswap
template <std::integral T>
constexpr T byteswap(T value) noexcept
{
    static_assert(std::has_unique_object_representations_v<T>, "T may not have padding bits");
    auto value_representation = std::bit_cast<std::array<std::byte, sizeof(T)>>(value);
    std::reverse(value_representation.begin(), value_representation.end());
    return std::bit_cast<T>(value_representation);
}

template <typename T>
T ToLittleEndian(const T value)
{
    if constexpr (std::endian::native != std::endian::little)
    {
        return byteswap(value);
    }
    return value;
}

template <typename T>
T FromLittleEndian(const T leValue)
{
    if constexpr (std::endian::native != std::endian::little)
    {
        return byteswap(leValue);
    }
    return leValue;
}

#pragma pack(pop)
