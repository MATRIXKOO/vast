// Copyright (c) 2022-present, Trail of Bits, Inc.

#include "vast/Translation/CodeGenDriver.hpp"

VAST_RELAX_WARNINGS
#include <clang/AST/GlobalDecl.h>
#include <clang/Basic/TargetInfo.h>
VAST_UNRELAX_WARNINGS

#include "vast/Translation/Error.hpp"

// FIXME: get rid of dependency from upper layer
#include "vast/CodeGen/TypeInfo.hpp"

namespace vast::cg
{
    defer_handle_of_top_level_decl::defer_handle_of_top_level_decl(
        codegen_driver &codegen, bool emit_deferred
    )
        : codegen(codegen), emit_deferred(emit_deferred)
    {
        ++codegen.deferred_top_level_decls;
    }

    defer_handle_of_top_level_decl::~defer_handle_of_top_level_decl() {
        unsigned level = --codegen.deferred_top_level_decls;
        if (level == 0 && emit_deferred) {
            codegen.build_deferred_decls();
        }
    }

    x86_avx_abi_level avx_level(const clang::TargetInfo &target) {
        const auto &abi = target.getABI();

        if (abi == "avx512")
            return x86_avx_abi_level::avx512;
        if (abi == "avx")
            return x86_avx_abi_level::avx;
        return x86_avx_abi_level::none;
    }


    namespace detail {
        target_info_ptr initialize_target_info(
            const clang::TargetInfo &target, const type_info_t &type_info
        ) {
            const auto &triple = target.getTriple();
            const auto &abi = target.getABI();

            auto aarch64_info = [&] {
                if (abi == "aapcs" || abi == "darwinpcs") {
                    throw cg::unimplemented("Only Darwin supported for aarch64");
                }

                auto abi_kind = aarch64_abi_info::abi_kind::darwin_pcs;
                return target_info_ptr(new aarch64_target_info(type_info, abi_kind));
            };

            auto x86_64_info = [&] {
                auto os = triple.getOS();

                if (os == llvm::Triple::Win32) {
                    throw cg::unimplemented( "target info for Win32" );
                } else {
                    return target_info_ptr(
                        new x86_64_target_info(type_info, avx_level(target))
                    );
                }

                throw cg::unimplemented(std::string("Unsupported x86_64 OS type: ") + triple.getOSName().str());
            };

            switch (triple.getArch()) {
                case llvm::Triple::aarch64: return aarch64_info();
                case llvm::Triple::x86_64:  return x86_64_info();
                default: throw cg::unimplemented("Target not yet supported.");
            }
        }
    } // namespace detail

    void codegen_driver::finalize() {
        build_deferred();
        // TODO: buildVTablesOpportunistically();
        // TODO: applyGlobalValReplacements();
        apply_replacements();
        // TODO: checkAliases();
        // TODO: buildMultiVersionFunctions();
        // TODO: buildCXXGlobalInitFunc();
        // TODO: buildCXXGlobalCleanUpFunc();
        // TODO: registerGlobalDtorsWithAtExit();
        // TODO: buildCXXThreadLocalInitFunc();
        // TODO: ObjCRuntime
        if (lang().CUDA) {
            throw cg::unimplemented("cuda module release");
        }
        // TODO: OpenMPRuntime
        // TODO: PGOReader
        // TODO: buildCtorList(GlobalCtors);
        // TODO: builtCtorList(GlobalDtors);
        // TODO: buildGlobalAnnotations();
        // TODO: buildDeferredUnusedCoverageMappings();
        // TODO: CIRGenPGO
        // TODO: CoverageMapping
        // TODO: if (codegen_opts.SanitizeCfiCrossDso) {
        // TODO: buildAtAvailableLinkGuard();
        const auto &target_triplet = actx.getTargetInfo().getTriple();
        if (target_triplet.isWasm() && !target_triplet.isOSEmscripten()) {
            throw cg::unimplemented("WASM module release");
        }

        // Emit reference of __amdgpu_device_library_preserve_asan_functions to
        // preserve ASAN functions in bitcode libraries.
        if (lang().Sanitize.has(clang::SanitizerKind::Address)) {
            throw cg::unimplemented("AddressSanitizer module release");
        }

        // TODO: buildLLVMUsed();// TODO: SanStats

        // TODO: if (codegen_opts.Autolink) {
            // TODO: buildModuleLinkOptions
        // }

        // TODO: FINISH THE REST OF THIS
    }

    bool codegen_driver::verify_module() const {
        return codegen.verify_module();
    }

    void codegen_driver::build_deferred_decls() {
        if (deferred_inline_member_func_defs.empty())
            return;

        // Emit any deferred inline method definitions. Note that more deferred
        // methods may be added during this loop, since ASTConsumer callbacks can be
        // invoked if AST inspection results in declarations being added.
        auto deferred = deferred_inline_member_func_defs;
        deferred_inline_member_func_defs.clear();

        defer_handle_of_top_level_decl defer(*this);
        for (auto decl : deferred) {
            handle_top_level_decl(decl);
        }

        // Recurse to handle additional deferred inline method definitions.
        build_deferred_decls();
    }

    void codegen_driver::handle_translation_unit(acontext_t &/* acontext */) {
        finalize();
    }

    void codegen_driver::handle_top_level_decl(clang::DeclGroupRef decls) {
        defer_handle_of_top_level_decl defer(*this);

        for (auto decl : decls) {
            handle_top_level_decl(decl);
        }
    }

    void codegen_driver::handle_top_level_decl(clang::Decl *decl) {
        // Ignore dependent declarations
        if (decl->isTemplated())
            return;

        // Consteval function shouldn't be emitted.
        if (auto *fn = llvm::dyn_cast< clang::FunctionDecl >(decl)) {
            if (fn->isConsteval()) {
                return;
            }
        }

        switch (decl->getKind()) {
            case clang::Decl::Var:
            case clang::Decl::Decomposition:
            case clang::Decl::VarTemplateSpecialization: {
                build_global(llvm::cast< clang::VarDecl >(decl));
                if (llvm::isa< clang::DecompositionDecl >(decl)) {
                    throw cg::unimplemented("codegen for DecompositionDecl");
                }
                break;
            }
            case clang::Decl::CXXMethod:
            case clang::Decl::Function: {
                build_global(llvm::cast< clang::FunctionDecl >(decl));
                if (options.coverage_mapping) {
                    throw cg::unimplemented("coverage mapping not supported");
                }
                break;
            }
            // case clang::Decl::Namespace:
            // case clang::Decl::ClassTemplateSpecialization:
            // case clang::Decl::CXXRecord:
            // case clang::Decl::UsingShadow:
            // case clang::Decl::ClassTemplate:
            // case clang::Decl::VarTemplate:
            // case clang::Decl::Concept:
            // case clang::Decl::VarTemplatePartialSpecialization:
            // case clang::Decl::FunctionTemplate:
            // case clang::Decl::TypeAliasTemplate:
            // case clang::Decl::Block:
            // case clang::Decl::Empty:
            // case clang::Decl::Binding:
            // case clang::Decl::Using:
            // case clang::Decl::UsingEnum:
            // case clang::Decl::NamespaceAlias:
            // case clang::Decl::UsingDirective:
            // case clang::Decl::CXXConstructor:
            // case clang::Decl::StaticAssert:
            case clang::Decl::Typedef:
            case clang::Decl::TypeAlias:
            case clang::Decl::Record:
            case clang::Decl::Enum:
                return codegen.append_to_module(decl);
            default: throw cg::unimplemented(std::string("codegen for: ") + decl->getDeclKindName());
        }
    }

    function_processing_lock codegen_driver::make_lock(const function_info_t *fninfo) {
        return function_processing_lock(type_conv, fninfo);
    }

    void codegen_driver::update_completed_type(const clang::TagDecl *tag) {
        type_conv.update_completed_type(tag);
    }

    owning_module_ref codegen_driver::freeze() { return codegen.freeze(); }

    operation codegen_driver::build_global_definition(clang::GlobalDecl glob) {
        const auto *decl = llvm::cast< clang::ValueDecl >(glob.getDecl());

        if (const auto *fn = llvm::dyn_cast< clang::FunctionDecl >(decl)) {
            // At -O0, don't generate vast for functions with available_externally linkage.
            if (!should_emit_function(glob))
                return nullptr;

            if (fn->isMultiVersion()) {
                throw cg::unimplemented("codegen for multi version function");
            }

            if (const auto *method = llvm::dyn_cast< clang::CXXMethodDecl >(decl)) {
                throw cg::unimplemented("cxx methods");
            }

            return build_global_function_definition(glob);
        }

        if (const auto *var = llvm::dyn_cast< clang::VarDecl >(decl))
            return build_global_var_definition(var, !var->hasDefinition());

        llvm_unreachable("Invalid argument to buildGlobalDefinition()");

    }

    operation codegen_driver::build_global_function_definition(clang::GlobalDecl decl) {
        auto const *function_decl = llvm::cast< clang::FunctionDecl >(decl.getDecl());

        // Compute the function info and vast type.
        const auto &fty_info = type_info->arrange_global_decl(decl, get_target_info());
        auto ty = type_conv.get_function_type(fty_info);

        assert(!lang().CUDA && "NYI");
        auto op = codegen.build_function_prototype(decl, ty);

        auto fn = mlir::cast< vast::hl::FuncOp >(op);
        // Already emitted.
        if (!fn.isDeclaration()) {
            return op;
        }

        // TODO setGVProperties
        // TODO MaubeHandleStaticInExternC
        // TODO maybeSetTrivialComdat
        // TODO setLLVMFunctionFEnvAttributes

        fn = build_function_body(fn, decl, fty_info);

        // TODO: setNonAliasAttributes
        // TODO: SetLLVMFunctionAttributesForDeclaration

        if (function_decl->getAttr< clang::ConstructorAttr >()) {
            throw cg::unimplemented("ctor emition");
        }

        if (function_decl->getAttr< clang::DestructorAttr >()) {
            throw cg::unimplemented("dtor emition");
        }

        if (function_decl->getAttr< clang::AnnotateAttr >()) {
            throw cg::unimplemented("annotated emition");
        }

        return op;
    }

    CodeGenContext::VarTable & codegen_driver::variables_symbol_table() {
        return codegen.variables_symbol_table();
    }


    bool codegen_driver::should_emit_function(clang::GlobalDecl /* decl */) {
        // TODO: implement this -- requires defining linkage for vast
        return true;
    }

    operation codegen_driver::build_global_var_definition(const clang::VarDecl */* decl */, bool /* tentative */) {
        throw cg::unimplemented("build_global_var_definition");
    }

    operation codegen_driver::build_global_decl(const clang::GlobalDecl &/* decl */) {
        throw cg::unimplemented("build_global_decl");
    }

    mangled_name_ref codegen_driver::get_mangled_name(clang::GlobalDecl decl) {
        return codegen.get_mangled_name(decl);
    }

    operation codegen_driver::build_global(clang::GlobalDecl decl) {
        const auto *glob = llvm::cast< clang::ValueDecl >(decl.getDecl());

        assert(!glob->hasAttr< clang::WeakRefAttr >() && "NYI");
        assert(!glob->hasAttr< clang::AliasAttr >() && "NYI");
        assert(!glob->hasAttr< clang::IFuncAttr >() && "NYI");
        assert(!glob->hasAttr< clang::CPUDispatchAttr >() && "NYI");

        assert(!lang().CUDA && "NYI");
        assert(!lang().OpenMP && "NYI");

        // Ignore declarations, they will be emitted on their first use.
        if (const auto *fn = llvm::dyn_cast< clang::FunctionDecl >(glob)) {
            // Forward declarations are emitted lazily on first use.
            if (!fn->doesThisDeclarationHaveABody()) {
                if (!fn->doesDeclarationForceExternallyVisibleDefinition())
                    return nullptr;
                throw cg::unimplemented("build_global FunctionDecl");
                // auto mangled_name = getMangledName(decl);

                // Compute the function info and vast type.
                // const auto &FI = getTypes().arrangeGlobalDeclaration(decl);
                // mlir_type Ty = getTypes().GetFunctionType(FI);

                // GetOrCreateFunction(MangledName, Ty, decl, /*ForVTable=*/false,
                //                         /*DontDefer=*/false);
                // return;
            }
        } else {
            const auto *var = llvm::cast< clang::VarDecl >(glob);
            assert(var->isFileVarDecl() && "Cannot emit local var decl as global.");
            if (var->isThisDeclarationADefinition() != clang::VarDecl::Definition &&
                !acontext().isMSStaticDataMemberInlineDefinition(var)
            ) {
                assert(!lang().OpenMP && "NYI");
                // If this declaration may have caused an inline variable definition
                // to change linkage, make sure that it's emitted.
                // TODO probably use GetAddrOfGlobalVar(var) below?
                assert((acontext().getInlineVariableDefinitionKind(var) !=
                    clang::ASTContext::InlineVariableDefinitionKind::Strong
                ) && "NYI");

                return {};
            }
        }

        // Defer code generation to first use when possible, e.g. if this is an inline
        // function. If the global mjust always be emitted, do it eagerly if possible
        // to benefit from cache locality.
        if (must_be_emitted(glob) && may_be_emitted_eagerly(glob)) {
            // Emit the definition if it can't be deferred.
            return build_global_definition(glob);
        }

        // If we're deferring emission of a C++ variable with an initializer, remember
        // the order in which it appeared on the file.
        if (lang().CPlusPlus && clang::isa< clang::VarDecl >(glob) &&
            clang::cast< clang::VarDecl >(glob)->hasInit()
        ) {
            throw cg::unimplemented("build_global CXX GlobalVar");
            // DelayedCXXInitPosition[glob] = CXXGlobalInits.size();
            // CXXGlobalInits.push_back(nullptr);
        }

        auto mangled_name = get_mangled_name(decl);
        if (get_global_value(mangled_name) != nullptr) {
            // The value has already been used and should therefore be emitted.
            codegen.add_deferred_decl_to_emit(decl);
        } else if (must_be_emitted(glob)) {
            // The value must be emitted, but cannot be emitted eagerly.
            assert(!may_be_emitted_eagerly(glob));
            codegen.add_deferred_decl_to_emit(decl);
        } else {
            // Otherwise, remember that we saw a deferred decl with this name. The first
            // use of the mangled name will cause it to move into DeferredDeclsToEmit.
            codegen.set_deferred_decl(mangled_name, decl);
        }

        return {};
    }

    bool codegen_driver::must_be_emitted(const clang::ValueDecl *glob) {
        // Never defer when EmitAllDecls is specified.
        assert(!lang().EmitAllDecls && "EmitAllDecls NYI");
        assert(!options.keep_static_consts && "KeepStaticConsts NYI");

        return actx.DeclMustBeEmitted(glob);
    }

    bool codegen_driver::may_be_emitted_eagerly(const clang::ValueDecl *glob) {
        assert(!lang().OpenMP && "not supported");

        if (const auto *fn = llvm::dyn_cast< clang::FunctionDecl >(glob)) {
            // Implicit template instantiations may change linkage if they are later
            // explicitly instantiated, so they should not be emitted eagerly.
            constexpr auto implicit = clang::TSK_ImplicitInstantiation;
            if (fn->getTemplateSpecializationKind() == implicit) {
                throw cg::unimplemented("implicit template specialization emition");
            }
            assert(!fn->isTemplated() && "templates NYI");
            return true;
        }


        if (const auto *vr = llvm::dyn_cast< clang::VarDecl >(glob)) {
            // A definition of an inline constexpr static data member may change
            // linkage later if it's redeclared outside the class.
            constexpr auto weak_unknown = clang::ASTContext::InlineVariableDefinitionKind::WeakUnknown;
            if (actx.getInlineVariableDefinitionKind(vr) == weak_unknown) {
                throw cg::unimplemented("inline variable definitions");
            }
            return true;
        }

        throw cg::unimplemented("unsupported value decl");
    }

    operation codegen_driver::get_global_value(mangled_name_ref name) {
        return codegen.get_global_value(name);
    }

    mlir_value codegen_driver::get_global_value(const clang::Decl *decl) {
        return codegen.get_global_value(decl);
    }

    const std::vector< clang::GlobalDecl >& codegen_driver::default_methods_to_emit() const {
        return codegen.default_methods_to_emit();
    }

    const std::vector< clang::GlobalDecl >& codegen_driver::deferred_decls_to_emit() const {
        return codegen.deferred_decls_to_emit();
    }

    const std::vector< const clang::CXXRecordDecl * >& codegen_driver::deferred_vtables() const {
        return codegen.deferred_vtables();
    }

    const std::map< mangled_name_ref, clang::GlobalDecl >& codegen_driver::deferred_decls() const {
        return codegen.deferred_decls();
    }

    std::vector< clang::GlobalDecl >&& codegen_driver::receive_deferred_decls_to_emit() {
        return codegen.receive_deferred_decls_to_emit();
    }

    void codegen_driver::build_default_methods() {
        // Differently from deferred_decls_to_emit, there's no recurrent use of
        // deferred_decls_to_emit, so use it directly for emission.
        for (const auto &decl : default_methods_to_emit()) {
            build_global_decl(decl);
        }
    }

    void codegen_driver::build_deferred() {
        // Emit deferred declare target declarations
        if (lang().OpenMP && !lang().OpenMPSimd) {
            throw cg::unimplemented("build_deferred for openmp");
        }

        // Emit code for any potentially referenced deferred decls. Since a previously
        // unused static decl may become used during the generation of code for a
        // static function, iterate until no changes are made.
        if (!deferred_vtables().empty()) {
            throw cg::unimplemented("build_deferred for vtables");
        }

        // Emit CUDA/HIP static device variables referenced by host code only. Note we
        // should not clear CUDADeviceVarODRUsedByHost since it is still needed for
        // further handling.
        if (lang().CUDA && lang().CUDAIsDevice) {
            throw cg::unimplemented("build_deferred for cuda");
        }

        // Stop if we're out of both deferred vtables and deferred declarations.
        if (deferred_decls_to_emit().empty())
            return;

        // Grab the list of decls to emit. If build_global_definition schedules more
        // work, it will not interfere with this.
        auto curr_decls_to_emit = receive_deferred_decls_to_emit();
        for (auto &decl : curr_decls_to_emit) {
            build_global_decl(decl);

            // FIXME: rework to worklist?
            // If we found out that we need to emit more decls, do that recursively.
            // This has the advantage that the decls are emitted in a DFS and related
            // ones are close together, which is convenient for testing.
            if (!deferred_vtables().empty() || !deferred_decls_to_emit().empty()) {
                build_deferred();
                assert(deferred_vtables().empty() && deferred_decls_to_emit().empty());
            }
        }
    }

    void codegen_driver::add_replacement(string_ref name, mlir::Operation *op) {
        replacements[name] = op;
    }

    void codegen_driver::apply_replacements() {
        if (!replacements.empty()) {
            throw cg::unimplemented(" function replacement in module release");
        }
    }

} // namespace vast::cg