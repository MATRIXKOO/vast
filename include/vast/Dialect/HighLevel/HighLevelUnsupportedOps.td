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

def HighLevel_UnsupportedStmtOp : HighLevel_Op<"unsupportedStmt", [NoSideEffect]> {
  let summary = "VAST unsupported operation";
  let description = [{ VAST unsupported operation }];
  
  let arguments = (ins
    StrAttr:$name,
    Variadic<AnyType>:$elements
  );
  
  let results = (outs Variadic<AnyType>);
  let assemblyFormat = "$elements attr-dict `:` functional-type($elements, results)";
}

def HighLevel_UnsupportedStmtReturnTyOp : HighLevel_Op<"unsupportedStmtReturnTy", [NoSideEffect]>
  , Arguments<(ins StrAttr:$name)> {
  let summary = "VAST unsupported operation";
  let description = [{ VAST unsupported operation }];
  let assemblyFormat = "attr-dict";
}

def HighLevel_UnsupportedStmtTermOp : HighLevel_Op<"unsupportedStmtTerminate", [Terminator, NoSideEffect]>
  , Arguments<(ins StrAttr:$name)> {
  let summary = "VAST unsupported operation";
  let description = [{ VAST unsupported operation }];
  let assemblyFormat = "attr-dict";
}

def HighLevel_UnsupportedInitExpr : HighLevel_Op< "unsupportedInitExpr",
  [NoSideEffect]
> {
  let summary = "VAST initializer list expression";
  let description = [{ VAST initializer list expression }];

  let arguments = (ins Variadic<AnyType>:$elements);
  let results = (outs Variadic<AnyType>);

  let assemblyFormat = "$elements attr-dict `:` functional-type($elements, results)";
}

def HighLevel_UnsupportedExprOp
  : HighLevel_Op< "unsupportedExpr", [SingleBlock]>
  , Arguments<(ins StrAttr:$name)>{
  let summary = "VAST expression";
  let description = [{ VAST expression }];

  let regions = (region SizedRegion<1>:$body);
  let results = (outs AnyType:$result);

  let builders =  [
    OpBuilder<(ins "::mlir::StringRef":$name, "::mlir::Type":$result, "::mlir::Block *":$block),
    [{
      $_state.addAttribute(nameAttrName($_state.name), $_builder.getStringAttr(name));
      if (block != nullptr) {
      	auto *region = $_state.addRegion();
      	region->push_back(block);
      }
      if (result == nullptr) {
      	$_state.addTypes(result);
      }
    }] >];

  let assemblyFormat = [{
     $name attr-dict `:` $body type($result)
  }];
}



#endif // VAST_DIALECT_HIGHLEVEL_IR_HIGHLEVELUNSUPPORTEDOPS