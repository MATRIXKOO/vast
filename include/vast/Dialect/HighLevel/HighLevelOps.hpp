// Copyright (c) 2021-present, Trail of Bits, Inc.

#pragma once


#include "vast/Dialect/HighLevel/HighLevelDialect.hpp"
#include "vast/Dialect/HighLevel/HighLevelTypes.hpp"

#include <mlir/Interfaces/InferTypeOpInterface.h>

#define GET_OP_CLASSES
#include "vast/Dialect/HighLevel/HighLevel.h.inc"

namespace vast::hl
{
    static constexpr llvm::StringRef external_storage = "external";
    static constexpr llvm::StringRef static_storage   = "static";
}
