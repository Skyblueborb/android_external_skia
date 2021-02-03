/*
 * Copyright 2020 Google LLC
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef SKSL_DSLWRITER
#define SKSL_DSLWRITER

#include "src/sksl/SkSLMangler.h"
#include "src/sksl/dsl/DSLExpression.h"
#include "src/sksl/ir/SkSLExpressionStatement.h"
#include "src/sksl/ir/SkSLProgram.h"
#include "src/sksl/ir/SkSLStatement.h"
#if !defined(SKSL_STANDALONE) && SK_SUPPORT_GPU
#include "src/gpu/glsl/GrGLSLFragmentProcessor.h"
#endif // !defined(SKSL_STANDALONE) && SK_SUPPORT_GPU

#include <stack>

class AutoDSLContext;

namespace SkSL {

class Compiler;
class Context;
class IRGenerator;
class SymbolTable;
class Type;

namespace dsl {

class ErrorHandler;

/**
 * Thread-safe class that tracks per-thread state associated with DSL output. This class is for
 * internal use only.
 */
class DSLWriter {
public:
    DSLWriter(SkSL::Compiler* compiler);

    /**
     * Returns the Compiler used by DSL operations in the current thread.
     */
    static SkSL::Compiler& Compiler() {
        return *Instance().fCompiler;
    }

    /**
     * Returns the IRGenerator used by DSL operations in the current thread.
     */
    static SkSL::IRGenerator& IRGenerator();

    /**
     * Returns the Context used by DSL operations in the current thread.
     */
    static const SkSL::Context& Context();

    /**
     * Returns the collection to which DSL program elements in this thread should be appended.
     */
    static std::vector<std::unique_ptr<SkSL::ProgramElement>>& ProgramElements() {
        return Instance().fProgramElements;
    }

    /**
     * Returns the SymbolTable of the current thread's IRGenerator.
     */
    static const std::shared_ptr<SkSL::SymbolTable>& SymbolTable();

    /**
     * Returns the final pointer to a pooled Modifiers object that should be used to represent the
     * given modifiers.
     */
    static const SkSL::Modifiers* Modifiers(SkSL::Modifiers modifiers);

    /**
     * Returns the SkSL variable corresponding to a DSLVar.
     */
    static const SkSL::Variable& Var(const DSLVar& var);

    /**
     * Returns the (possibly mangled) final name that should be used for an entity with the given
     * raw name.
     */
    static const char* Name(const char* name);

    /**
     * Returns the current function for which we are generating nodes.
     */
    static const SkSL::FunctionDeclaration* CurrentFunction();

    /**
     * Specifies the function for which we are generating nodes.
     */
    static void SetCurrentFunction(const SkSL::FunctionDeclaration* fn);

#if !defined(SKSL_STANDALONE) && SK_SUPPORT_GPU
    /**
     * Returns the fragment processor for which DSL output is being generated for the current
     * thread.
     */
    static GrGLSLFragmentProcessor* CurrentProcessor() {
        SkASSERTF(!Instance().fStack.empty(), "This feature requires a FragmentProcessor");
        return Instance().fStack.top().fProcessor;
    }

    /**
     * Returns the EmitArgs for fragment processor output in the current thread.
     */
    static GrGLSLFragmentProcessor::EmitArgs* CurrentEmitArgs() {
        SkASSERTF(!Instance().fStack.empty(), "This feature requires a FragmentProcessor");
        return Instance().fStack.top().fEmitArgs;
    }

    /**
     * Pushes a new processor / emitArgs pair for the current thread.
     */
    static void StartFragmentProcessor(GrGLSLFragmentProcessor* processor,
                                       GrGLSLFragmentProcessor::EmitArgs* emitArgs);

    /**
     * Pops the processor / emitArgs pair associated with the current thread.
     */
    static void EndFragmentProcessor();
#endif // !defined(SKSL_STANDALONE) && SK_SUPPORT_GPU

    /**
     * Reports an error if the argument is null. Returns its argument unmodified.
     */
    static std::unique_ptr<SkSL::Expression> Check(std::unique_ptr<SkSL::Expression> expr);

    static DSLExpression Coerce(std::unique_ptr<Expression> left, const SkSL::Type& type);

    static DSLExpression Construct(const SkSL::Type& type, std::vector<DSLExpression> rawArgs);

    static DSLExpression ConvertBinary(std::unique_ptr<Expression> left, Token::Kind op,
                                std::unique_ptr<Expression> right);

    static DSLExpression ConvertIndex(std::unique_ptr<Expression> base,
                                      std::unique_ptr<Expression> index);

    static DSLExpression ConvertPostfix(std::unique_ptr<Expression> expr, Token::Kind op);

    static DSLExpression ConvertPrefix(Token::Kind op, std::unique_ptr<Expression> expr);

    /**
     * Sets the ErrorHandler associated with the current thread. This object will be notified when
     * any DSL errors occur. With a null ErrorHandler (the default), any errors will be dumped to
     * stderr and a fatal exception will be generated.
     */
    static void SetErrorHandler(ErrorHandler* errorHandler) {
        Instance().fErrorHandler = errorHandler;
    }

    /**
     * Notifies the current ErrorHandler that a DSL error has occurred. With a null ErrorHandler
     * (the default), any errors will be dumped to stderr and a fatal exception will be generated.
     */
    static void ReportError(const char* msg);

    /**
     * Returns whether name mangling is enabled. This should always be enabled outside of tests.
     */
    static bool ManglingEnabled() {
        return Instance().fMangle;
    }

    static DSLWriter& Instance();

    static void SetInstance(std::unique_ptr<DSLWriter> instance);

private:
    SkSL::Program::Settings fSettings;
    SkSL::Compiler* fCompiler;
    std::vector<std::unique_ptr<SkSL::ProgramElement>> fProgramElements;
    ErrorHandler* fErrorHandler = nullptr;
    bool fMangle = true;
    Mangler fMangler;
#if !defined(SKSL_STANDALONE) && SK_SUPPORT_GPU
    struct StackFrame {
        GrGLSLFragmentProcessor* fProcessor;
        GrGLSLFragmentProcessor::EmitArgs* fEmitArgs;
    };
    std::stack<StackFrame> fStack;
#endif // !defined(SKSL_STANDALONE) && SK_SUPPORT_GPU

    friend class DSLCore;
    friend class ::AutoDSLContext;
};

} // namespace dsl

} // namespace SkSL

#endif
