// RUN: vast-cc --from-source %s | FileCheck %s
// RUN: vast-cc --from-source %s > %t && vast-opt %t | diff -B %t -

// CHECK: hl.typedef "ck_rv_t" : !hl.long< unsigned >
typedef unsigned long ck_rv_t;
// CHECK: hl.var "global_lock" sc_static : !hl.lvalue<!hl.ptr<!hl.void>>
static void *global_lock = 0;

// CHECK: hl.typedef "ck_createmutex_t" : !hl.ptr<!hl.paren<(!hl.lvalue<!hl.ptr<!hl.ptr<!hl.void>>>) -> !hl.typedef<"ck_rv_t">>>
typedef ck_rv_t (*ck_createmutex_t) (void **mutex);
// CHECK: hl.typedef "ck_destroymutex_t" : !hl.ptr<!hl.paren<(!hl.lvalue<!hl.ptr<!hl.void>>) -> !hl.typedef<"ck_rv_t">>>
typedef ck_rv_t (*ck_destroymutex_t) (void *mutex);
// CHECK: hl.typedef "ck_lockmutex_t" : !hl.ptr<!hl.paren<(!hl.lvalue<!hl.ptr<!hl.void>>) -> !hl.typedef<"ck_rv_t">>>
typedef ck_rv_t (*ck_lockmutex_t) (void *mutex);
// CHECK: hl.typedef "ck_unlockmutex_t" : !hl.ptr<!hl.paren<(!hl.lvalue<!hl.ptr<!hl.void>>) -> !hl.typedef<"ck_rv_t">>>
typedef ck_rv_t (*ck_unlockmutex_t) (void *mutex);

// CHECK: hl.struct "ck_c_initialize_args"
struct ck_c_initialize_args
{
    // CHECK: hl.field "create_mutex" : !hl.typedef<"ck_createmutex_t">
    ck_createmutex_t create_mutex;
    // CHECK: hl.field "destroy_mutex" : !hl.typedef<"ck_destroymutex_t">
    ck_destroymutex_t destroy_mutex;
    // CHECK: hl.field "lock_mutex" : !hl.typedef<"ck_lockmutex_t">
    ck_lockmutex_t lock_mutex;
    // CHECK: hl.field "unlock_mutex" : !hl.typedef<"ck_unlockmutex_t">
    ck_unlockmutex_t unlock_mutex;
    // CHECK: hl.field "reserved" : !hl.ptr<!hl.void>
    void *reserved;
};

// CHECK: hl.typedef "CK_C_INITIALIZE_ARGS_PTR" : !hl.ptr<!hl.elaborated<!hl.record<"ck_c_initialize_args">>>
typedef struct ck_c_initialize_args *CK_C_INITIALIZE_ARGS_PTR;

// CHECK: hl.var "global_locking" sc_static : !hl.lvalue<!hl.typedef<"CK_C_INITIALIZE_ARGS_PTR">>
static CK_C_INITIALIZE_ARGS_PTR	global_locking;

// CHECK: hl.func external @sc_pkcs11_lock () -> !hl.long
long sc_pkcs11_lock(void)
{
    // CHECK: [[V1:%[0-9]+]] = hl.globref "global_locking" : !hl.lvalue<!hl.typedef<"CK_C_INITIALIZE_ARGS_PTR">>
    // CHECK: [[V2:%[0-9]+]] = hl.implicit_cast [[V1]] LValueToRValue : !hl.lvalue<!hl.typedef<"CK_C_INITIALIZE_ARGS_PTR">> -> !hl.typedef<"CK_C_INITIALIZE_ARGS_PTR">
    // CHECK: [[V3:%[0-9]+]] = hl.implicit_cast [[V2]] PointerToBoolean : !hl.typedef<"CK_C_INITIALIZE_ARGS_PTR"> -> !hl.bool
    // CHECK: hl.cond.yield [[V3]] : !hl.bool
	if (global_locking)  {
        // CHECK: [[M:%[0-9]+]] = hl.member [[X:%[0-9]+]] at "lock_mutex"
        // CHECK: [[C:%[0-9]+]] = hl.implicit_cast [[M]] LValueToRValue : !hl.lvalue<!hl.typedef<"ck_lockmutex_t">> -> !hl.typedef<"ck_lockmutex_t">
        // CHECK: [[G:%[0-9]+]] = hl.globref "global_lock" : !hl.lvalue<!hl.ptr<!hl.void>>
        // CHECK: [[A:%[0-9]+]] = hl.implicit_cast [[G]] LValueToRValue : !hl.lvalue<!hl.ptr<!hl.void>> -> !hl.ptr<!hl.void>
        // CHECK: hl.indirect_call [[C]] : !hl.typedef<"ck_lockmutex_t">([[A]]) : (!hl.ptr<!hl.void>) -> !hl.typedef<"ck_rv_t">
		while (global_locking->lock_mutex(global_lock) != 0);
	}

	return 0;
}