// Copyright (c) 2021-present, Trail of Bits, Inc.

#ifndef VAST_DIALECT_HIGHLEVEL_IR_HIGHLEVELUNSUPPORTEDOPS
#define VAST_DIALECT_HIGHLEVEL_IR_HIGHLEVELUNSUPPORTEDOPS

def HighLevel_UnsupportedDeclOp : HighLevel_Op<"unsupportedDecl", [NoSideEffect]> {
  let summary = "VAST unsupported operation";
  let description = [{ VAST unsupported operation }];
 
  
  let arguments = (ins
    StrAttr:$name,
    Variadic<AnyType>:$elements
  );
  
}

def HighLevel_UnsupportedInitExpr : HighLevel_Op< "unsupportedInitExpr",
  [Terminator, NoSideEffect]
> {
  let summary = "VAST initializer list expression";
  let description = [{ VAST initializer list expression }];

  let arguments = (ins 
      StrAttr:$name,
      Variadic<AnyType>:$elements
  );
  let results = (outs Variadic<AnyType>);

  let assemblyFormat = "$elements attr-dict `:` functional-type($elements, results)";
}

def HighLevel_UnsupportedExprOp
  : HighLevel_Op< "unsupportedExpr", [RecursiveSideEffects, SingleBlock] >
  , Arguments<(ins StrAttr:$name)>
  , Results<(outs AnyType:$result)>
{
  let summary = "VAST expression";
  let description = [{ VAST expression }];

  let regions = (region SizedRegion<1>:$subexpr);

  let skipDefaultBuilders = 1;
  let builders = [ OpBuilder<(ins "::mlir::StringRef":$name, "::mlir::Type":$rty, "std::unique_ptr<::mlir::Region> &&":$region)> ];

  let assemblyFormat = [{
     attr-dict `:` type($result) $subexpr
  }];
}


#endif // VAST_DIALECT_HIGHLEVEL_IR_HIGHLEVELUNSUPPORTEDOPS