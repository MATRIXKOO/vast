// Copyright (c) 2023, Trail of Bits, Inc.

#include "vast/Util/Warnings.hpp"

VAST_RELAX_WARNINGS
#include <clang/Basic/TargetOptions.h>
#include <clang/Driver/DriverDiagnostic.h>
#include <clang/Driver/Options.h>
#include <clang/Frontend/CompilerInstance.h>
#include <clang/Frontend/FrontendActions.h>
#include <clang/Frontend/FrontendPluginRegistry.h>
#include <llvm/Option/OptTable.h>
VAST_UNRELAX_WARNINGS

#include "vast/Frontend/Common.hpp"
#include "vast/Frontend/CompilerInstance.hpp"
#include "vast/Frontend/GenAction.hpp"

namespace vast::cc
{
    using frontend_action_ptr = std::unique_ptr< clang::FrontendAction >;
    using compiler_instance   = clang::CompilerInstance;

    // static frontend_action_ptr create_frontend_base_action(compiler_instance &/* ci */) {
    //     throw std::runtime_error("create_frontend_base_action not implemented");
    // }

    frontend_action_ptr create_frontend_action(compiler_instance &ci) {
        auto &opts = ci.getFrontendOpts();
        auto act   = opts.ProgramAction;
        using namespace clang::frontend;

        switch (act) {
            case EmitAssembly: return std::make_unique< vast::cc::emit_assembly_action >();
            case EmitLLVM: return std::make_unique< vast::cc::emit_llvm_action >();
            case EmitObj: return std::make_unique< vast::cc::emit_obj_action >();
            default: throw compiler_error("unsupported frontend action");
        }

        if (act != clang::frontend::PluginAction) {
            throw compiler_error("unsupported frontend action");
        }

        for (const auto &plugin : clang::FrontendPluginRegistry::entries()) {
            if (plugin.getName() == opts.ActionName) {
                // TODO emit MLIR
                // TODO emit HL
            }
        }

        throw compiler_error("not implemented frontend action");
    }

    bool execute_compiler_invocation(compiler_instance *ci) {
        auto &opts = ci->getFrontendOpts();

        // Honor -help.
        if (opts.ShowHelp) {
            clang::driver::getDriverOptTable().printHelp(
                llvm::outs(), "vast-front -cc1 [options] file...",
                "VAST Compiler: https://github.com/trailofbits/vast",
                /*Include=*/clang::driver::options::CC1Option,
                /*Exclude=*/0, /*ShowAllAliases=*/false);
            return true;
        }

        // Honor -version.
        //
        // FIXME: Use a better -version message?
        if (opts.ShowVersion) {
            llvm::cl::PrintVersionMessage();
            return true;
        }

        ci->LoadRequestedPlugins();

        // FIXME: Honor -mllvm.

        // FIXME: CLANG_ENABLE_STATIC_ANALYZER

        // If there were errors in processing arguments, don't do anything else.
        if (ci->getDiagnostics().hasErrorOccurred())
            return false;

        // Create and execute the frontend action.
        auto action = create_frontend_action(*ci);
        if (!action)
            return false;

        bool success = ci->ExecuteAction(*action);

        if (opts.DisableFree) {
            llvm::BuryPointer(std::move(action));
        }

        return success;
    }

} // namespace vast::cc