// Copyright (c) 2021-present, Trail of Bits, Inc.

#include "vast/Dialect/HighLevel/HighLevelAttributes.hpp"
#include "vast/Dialect/HighLevel/HighLevelDialect.hpp"
#include "vast/Dialect/HighLevel/HighLevelTypes.hpp"
#include "vast/Dialect/HighLevel/HighLevelOps.hpp"

#include <mlir/Support/LLVM.h>
#include <mlir/Support/LogicalResult.h>
#include <mlir/IR/Builders.h>
#include <mlir/IR/OperationSupport.h>
#include <mlir/IR/SymbolTable.h>
#include <mlir/IR/OpImplementation.h>

#include <llvm/Support/ErrorHandling.h>

#include <optional>
#include <variant>

namespace vast::hl
{
    namespace detail
    {
        Region* build_region(Builder &bld, State &st, BuilderCallback callback)
        {
            auto reg = st.addRegion();
            if (callback.has_value()) {
                bld.createBlock(reg);
                callback.value()(bld, st.location);
            }
            return reg;
        }
    } // namespace detail

    using FoldResult = mlir::OpFoldResult;

    static void printConstantOp(Printer &printer, auto &op) {
        printer << " ";
        printer.printAttributeWithoutType(op.getValue());
        printer << " : ";
        printer.printType(op.getType());
        printer.printOptionalAttrDict(op->getAttrs(), /*elidedAttrs=*/{"value"});
    }

    FoldResult ConstantIntOp::fold(mlir::ArrayRef<Attribute> operands) {
        VAST_CHECK(operands.empty(), "constant has no operands");
        return getValue();
    }

    static void printConstantIntOp(Printer &printer, ConstantIntOp &op) {
        printConstantOp(printer, op);
    }

    static ParseResult parseConstantIntOp(Parser &parser, State &st) {
        auto loc = parser.getCurrentLocation();
        auto ctx = parser.getBuilder().getContext();

        Attribute attr;
        llvm::APInt value;
        if (succeeded(parser.parseOptionalKeyword("true"))) {
            attr = mlir::BoolAttr::get(ctx, true);
        } else if (succeeded(parser.parseOptionalKeyword("false"))) {
            attr = mlir::BoolAttr::get(ctx, false);
        } else if (failed(parser.parseInteger(value))) {
            return parser.emitError(loc, "expected integer value");
        }

        Type rty;
        if (parser.parseColonType(rty) || parser.parseOptionalAttrDict(st.attributes)) {
            return mlir::failure();
        }
        st.addTypes(rty);

        if (!attr) {
            auto is_signed = [&]() {
                if (auto builtin_int = rty.dyn_cast< mlir::IntegerType >())
                    return builtin_int.isSigned();
                if (isHighLevelType(rty))
                    return isSigned(rty);
                VAST_UNREACHABLE("Cannot decide if {0} is signed or not.", rty);
            }();

            attr = mlir::IntegerAttr::get(ctx, llvm::APSInt(value, is_signed));
        }

        st.addAttribute("value", attr);
        return mlir::success();
    }

    FoldResult ConstantFloatOp::fold(mlir::ArrayRef<Attribute> operands) {
        VAST_CHECK(operands.empty(), "constant has no operands");
        return getValue();
    }

    static void printConstantFloatOp(Printer &printer, ConstantFloatOp &op) {
        printConstantOp(printer, op);
    }

    static ParseResult parseConstantFloatOp(Parser &parser, State &st) {
        auto loc = parser.getCurrentLocation();

        Attribute value;
        auto f64 = parser.getBuilder().getF64Type();
        if (failed(parser.parseAttribute(value, f64))) {
            return parser.emitError(loc, "expected floating-point value");
        }
        st.addAttribute("value", value);

        Type rty;
        if (parser.parseColonType(rty) || parser.parseOptionalAttrDict(st.attributes)) {
            return mlir::failure();
        }
        st.addTypes(rty);

        return mlir::success();
    }

    FoldResult ConstantArrayOp::fold(mlir::ArrayRef<Attribute> operands) {
        VAST_CHECK(operands.empty(), "constant has no operands");
        return getValue();
    }

    static void printConstantArrayOp(Printer &printer, ConstantArrayOp &op) {
        printConstantOp(printer, op);
    }

    static ParseResult parseConstantArrayOp(Parser &parser, State &st) {
        VAST_UNIMPLEMENTED;
    }

    FoldResult ConstantStringOp::fold(mlir::ArrayRef<Attribute> operands) {
        VAST_ASSERT(operands.empty() && "constant has no operands");
        return getValue();
    }

    static void printConstantStringOp(Printer &printer, ConstantStringOp &op) {
        printConstantOp(printer, op);
    }

    static ParseResult parseConstantStringOp(Parser &parser, State &st) {
        auto loc = parser.getCurrentLocation();
        auto ctx = parser.getBuilder().getContext();

        Attribute value;
        if (failed(parser.parseAttribute(value, mlir::NoneType::get(ctx)))) {
            return parser.emitError(loc, "expected string value");
        }
        st.addAttribute("value", value);

        Type rty;
        if (parser.parseColonType(rty) || parser.parseOptionalAttrDict(st.attributes)) {
            return mlir::failure();
        }
        st.addTypes(rty);

        return mlir::success();
    }

    void build_expr_trait(Builder &bld, State &st, Type rty, BuilderCallback expr) {
        assert(expr && "the builder callback for 'expr' block must be present");
        Builder::InsertionGuard guard(bld);
        detail::build_region(bld, st, expr);
        st.addTypes(rty);
    }

    void SizeOfExprOp::build(Builder &bld, State &st, Type rty, BuilderCallback expr) {
        build_expr_trait(bld, st, rty, expr);
    }

    void AlignOfExprOp::build(Builder &bld, State &st, Type rty, BuilderCallback expr) {
        build_expr_trait(bld, st, rty, expr);
    }

    void VarDeclOp::build(Builder &bld, State &st, Type type, llvm::StringRef name, BuilderCallback init, BuilderCallback alloc) {
        st.addAttribute("name", bld.getStringAttr(name));
        Builder::InsertionGuard guard(bld);

        detail::build_region(bld, st, init);
        detail::build_region(bld, st, alloc);

        st.addTypes(type);
    }

    void EnumDeclOp::build(Builder &bld, State &st, llvm::StringRef name, Type type, BuilderCallback constants) {
        st.addAttribute("name", bld.getStringAttr(name));
        st.addAttribute("type", mlir::TypeAttr::get(type));
        Builder::InsertionGuard guard(bld);
        detail::build_region(bld, st, constants);
    }

    void EnumConstantOp::build(Builder &bld, State &st, llvm::StringRef name, llvm::APSInt value, BuilderCallback init) {
        st.addAttribute("name", bld.getStringAttr(name));
        st.addAttribute("value", mlir::IntegerAttr::get(bld.getContext(), value));

        Builder::InsertionGuard guard(bld);
        detail::build_region(bld, st, init);
    }

    namespace detail {
        void build_record_like_decl(Builder &bld, State &st, llvm::StringRef name, BuilderCallback fields) {
            st.addAttribute("name", bld.getStringAttr(name));

            Builder::InsertionGuard guard(bld);
            detail::build_region(bld, st, fields);
        }
    } // anamespace detail

    void StructDeclOp::build(Builder &bld, State &st, llvm::StringRef name, BuilderCallback fields) {
        detail::build_record_like_decl(bld, st, name, fields);
    }

    void UnionDeclOp::build(Builder &bld, State &st, llvm::StringRef name, BuilderCallback fields) {
        detail::build_record_like_decl(bld, st, name, fields);
    }

    mlir::CallInterfaceCallable CallOp::getCallableForCallee()
    {
        return (*this)->getAttrOfType< mlir::SymbolRefAttr >("callee");
    }

    mlir::Operation::operand_range CallOp::getArgOperands() { return operands(); }

    mlir::CallInterfaceCallable IndirectCallOp::getCallableForCallee()
    {
        return (*this)->getOperand(0);
    }

    mlir::Operation::operand_range IndirectCallOp::getArgOperands() { return operands(); }

    void IfOp::build(Builder &bld, State &st, BuilderCallback condBuilder, BuilderCallback thenBuilder, BuilderCallback elseBuilder)
    {
        assert(condBuilder && "the builder callback for 'condition' block must be present");
        assert(thenBuilder && "the builder callback for 'then' block must be present");

        Builder::InsertionGuard guard(bld);

        detail::build_region(bld, st, condBuilder);
        detail::build_region(bld, st, thenBuilder);
        detail::build_region(bld, st, elseBuilder);
    }

    void WhileOp::build(Builder &bld, State &st, BuilderCallback cond, BuilderCallback body)
    {
        assert(cond && "the builder callback for 'condition' block must be present");
        assert(body && "the builder callback for 'body' must be present");

        Builder::InsertionGuard guard(bld);

        detail::build_region(bld, st, cond);
        detail::build_region(bld, st, body);
    }

    void ForOp::build(Builder &bld, State &st, BuilderCallback cond, BuilderCallback incr, BuilderCallback body)
    {
        assert(body && "the builder callback for 'body' must be present");
        Builder::InsertionGuard guard(bld);

        detail::build_region(bld, st, cond);
        detail::build_region(bld, st, incr);
        detail::build_region(bld, st, body);
    }

    void DoOp::build(Builder &bld, State &st, BuilderCallback body, BuilderCallback cond)
    {
        assert(body && "the builder callback for 'body' must be present");
        Builder::InsertionGuard guard(bld);

        detail::build_region(bld, st, body);
        detail::build_region(bld, st, cond);
    }

    void SwitchOp::build(Builder &bld, State &st, BuilderCallback cond, BuilderCallback body)
    {
        assert(cond && "the builder callback for 'condition' block must be present");
        Builder::InsertionGuard guard(bld);

        detail::build_region(bld, st, cond);
        detail::build_region(bld, st, body);
    }

    void CaseOp::build(Builder &bld, State &st, BuilderCallback lhs, BuilderCallback body)
    {
        assert(lhs && "the builder callback for 'case condition' block must be present");
        Builder::InsertionGuard guard(bld);

        detail::build_region(bld, st, lhs);
        detail::build_region(bld, st, body);
    }

    void DefaultOp::build(Builder &bld, State &st, BuilderCallback body)
    {
        assert(body && "the builder callback for 'body' block must be present");
        Builder::InsertionGuard guard(bld);

        detail::build_region(bld, st, body);
    }

    void LabelStmt::build(Builder &bld, State &st, Value label, BuilderCallback substmt)
    {
        st.addOperands(label);

        assert(substmt && "the builder callback for 'substmt' block must be present");
        Builder::InsertionGuard guard(bld);

        detail::build_region(bld, st, substmt);
    }

    mlir::Operation* build_constant(Builder &builder, Attribute value, Type type, Location loc)
    {
        if (type.isa< BoolType >()) {
            return builder.create< ConstantIntOp >(loc, type, value.cast< mlir::BoolAttr >());
        }

        if (util::is_one_of< integer_types >(type)) {
            return builder.create< ConstantIntOp >(loc, type, value.cast< mlir::IntegerAttr >());
        }

        if (util::is_one_of< floating_types >(type)) {
            return builder.create< ConstantFloatOp >(loc, type, value.cast< mlir::FloatAttr >());
        }

        if (type.isa< ArrayType >()) {
            return builder.create< ConstantArrayOp >(loc, type, value.cast< mlir::ArrayAttr >());
        }

        VAST_UNREACHABLE("unknown constant type");
    }

    void ExprOp::build(Builder &bld, State &st, Type rty, std::unique_ptr< Region > &&region) {
        Builder::InsertionGuard guard(bld);
        st.addRegion(std::move(region));
        st.addTypes(rty);
    }

    void UnsupportedExprOp::build(Builder &bld, State &st, mlir::StringRef name, Type rty, std::unique_ptr< Region > &&region) {
        Builder::InsertionGuard guard(bld);
        st.addRegion(std::move(region));
        st.addTypes(rty);
        st.addAttribute(nameAttrName(st.name), bld.getStringAttr(name));
    }

}

//===----------------------------------------------------------------------===//
// TableGen generated logic.
//===----------------------------------------------------------------------===//

#define GET_OP_CLASSES
#include "vast/Dialect/HighLevel/HighLevel.cpp.inc"
