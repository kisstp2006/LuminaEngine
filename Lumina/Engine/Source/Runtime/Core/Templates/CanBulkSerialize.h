#pragma once

template <typename T>
struct TCanBulkSerialize
{
	static constexpr bool Value = std::is_trivially_constructible_v<T> && std::is_standard_layout_v<T>;
};

