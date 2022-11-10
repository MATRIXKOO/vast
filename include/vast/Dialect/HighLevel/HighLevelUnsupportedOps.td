// Copyright (c) 2021-present, Trail of Bits, Inc.

#ifndef VAST_DIALECT_HIGHLEVEL_IR_HIGHLEVELUNSUPPORTEDOPS
#define VAST_DIALECT_HIGHLEVEL_IR_HIGHLEVELUNSUPPORTEDOPS

def HighLevel_UnsupportedDeclOp : HighLevel_Op<"unsupportedDecl", [NoSideEffect]> {
  let summary = "VAST unsupported declaration operation";
  let description = [{ VAST unsupported declaration operation }];
  
  let arguments = (ins StrAttr:$name, Variadic<AnyType>:$elements);
  
  let assemblyFormat = [{ $name attr-dict `:` type($elements) $elements}];
}

def HighLevel_UnsupportedExprOp : HighLevel_Op<"unsupportedExpr", [NoSideEffect, SingleBlock] >
  , Arguments<(ins StrAttr:$name)>, Results<(outs AnyType:$result)> {
  let summary = "VAST unsupported expression";
  let description = [{ VAST unsupported expression }];

  let regions = (region SizedRegion<1>:$subexpr);

  let skipDefaultBuilders = 1;
  let builders = [ OpBuilder<(ins "::mlir::StringRef":$name, "::mlir::Type":$rty, "std::unique_ptr<::mlir::Region> &&":$region)> ];

  let assemblyFormat = [{
     $name attr-dict `:` type($result) $subexpr
  }];
}

#endif // VAST_DIALECT_HIGHLEVEL_IR_HIGHLEVELUNSUPPORTEDOPS