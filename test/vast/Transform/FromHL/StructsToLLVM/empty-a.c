// RUN: %vast-cc --ccopts -xc --from-source %s | %vast-opt --vast-hl-lower-types --vast-hl-structs-to-llvm | %file-check %s

// CHECK: hl.typedef "X" : !llvm.struct<"X", ()>
struct X {};
