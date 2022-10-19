// Copyright (c) 2021-present, Trail of Bits, Inc.

#include "vast/Translation/DataLayout.hpp"

namespace vast::hl
{
    void emit_data_layout(MContext &ctx, OwningModuleRef &mod, const dl::DataLayoutBlueprint &dl) {
        std::vector< mlir::DataLayoutEntryInterface > entries;
        for (const auto &[t, e] : dl.entries) {
          if(t) {
            entries.push_back(e.wrap(ctx));
          }
        }

        mod.get()->setAttr(
            mlir::DLTIDialect::kDataLayoutAttrName, mlir::DataLayoutSpecAttr::get(&ctx, entries)
        );
    }

} // namespace vast::hl
