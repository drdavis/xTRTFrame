//
// Copyright(c) 2016 Gabi Melman.
// Distributed under the MIT License (http://opensource.org/licenses/MIT)
//

#pragma once

// include external or bundled copy of fmtlib's ostream support
//
#if !defined(SPDLOG_FMT_EXTERNAL)
#include "xTRTFrame/spdlog/fmt/fmt.h"
#include "xTRTFrame/spdlog/fmt/bundled/ostream.h"
#else
#include <fmt/ostream.h>
#endif


