// RUN: %vast-cc --ccopts -xc --from-source %s | %file-check %s

// CHECK: hl.func @abort
// CHECK-NOT: hl.func @abort
// CHECK: hl.func @main

void abort();
void abort();
int main() {}
