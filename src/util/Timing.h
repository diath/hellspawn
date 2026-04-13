/*
 * Copyright (c) 2026, Kamil Chojnowski <Y29udGFjdEBkaWF0aC5uZXQ=>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include <chrono>
#include <cstdint>

namespace Timing {

/// Returns the current timestamp in milliseconds from a monotonic clock
/// suitable for measuring elapsed time between two points.
///
/// Usage:
///   int64_t start = Timing::currentMillis();
///   // ... work ...
///   int64_t elapsed = Timing::currentMillis() - start;
inline int64_t currentMillis()
{
	using namespace std::chrono;
	return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}

} // namespace Timing
