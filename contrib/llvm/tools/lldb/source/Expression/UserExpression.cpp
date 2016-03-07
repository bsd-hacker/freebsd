//===-- UserExpression.cpp ---------------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include <stdio.h>
#if HAVE_SYS_TYPES_H
#  include <sys/types.h>
#endif

#include <cstdlib>
#include <string>
#include <map>

#include "lldb/Core/ConstString.h"
#include "lldb/Core/Log.h"
#include "lldb/Core/Module.h"
#include "lldb/Core/StreamFile.h"
#include "lldb/Core/StreamString.h"
#include "lldb/Core/ValueObjectConstResult.h"
#include "lldb/Expression/ExpressionSourceCode.h"
#include "lldb/Expression/IRExecutionUnit.h"
#include "lldb/Expression/IRInterpreter.h"
#include "lldb/Expression/Materializer.h"
#include "lldb/Expression/UserExpression.h"
#include "Plugins/ExpressionParser/Clang/ClangPersistentVariables.h"
#include "lldb/Host/HostInfo.h"
#include "lldb/Symbol/Block.h"
#include "lldb/Symbol/Function.h"
#include "lldb/Symbol/ObjectFile.h"
#include "lldb/Symbol/SymbolVendor.h"
#include "lldb/Symbol/Type.h"
#include "lldb/Symbol/TypeSystem.h"
#include "lldb/Symbol/VariableList.h"
#include "lldb/Target/ExecutionContext.h"
#include "lldb/Target/Process.h"
#include "lldb/Target/StackFrame.h"
#include "lldb/Target/Target.h"
#include "lldb/Target/ThreadPlan.h"
#include "lldb/Target/ThreadPlanCallUserExpression.h"

using namespace lldb_private;

UserExpression::UserExpression (ExecutionContextScope &exe_scope,
                                const char *expr,
                                const char *expr_prefix,
                                lldb::LanguageType language,
                                ResultType desired_type,
                                const EvaluateExpressionOptions &options) :
      Expression(exe_scope),
      m_expr_text(expr),
      m_expr_prefix(expr_prefix ? expr_prefix : ""),
      m_language(language),
      m_desired_type(desired_type),
      m_options (options)
{
}

UserExpression::~UserExpression ()
{
}

void
UserExpression::InstallContext (ExecutionContext &exe_ctx)
{
    m_jit_process_wp = exe_ctx.GetProcessSP();

    lldb::StackFrameSP frame_sp = exe_ctx.GetFrameSP();

    if (frame_sp)
        m_address = frame_sp->GetFrameCodeAddress();
}

bool
UserExpression::LockAndCheckContext (ExecutionContext &exe_ctx,
                                          lldb::TargetSP &target_sp,
                                          lldb::ProcessSP &process_sp,
                                          lldb::StackFrameSP &frame_sp)
{
    lldb::ProcessSP expected_process_sp = m_jit_process_wp.lock();
    process_sp = exe_ctx.GetProcessSP();

    if (process_sp != expected_process_sp)
        return false;

    process_sp = exe_ctx.GetProcessSP();
    target_sp = exe_ctx.GetTargetSP();
    frame_sp = exe_ctx.GetFrameSP();

    if (m_address.IsValid())
    {
        if (!frame_sp)
            return false;
        else
            return (0 == Address::CompareLoadAddress(m_address, frame_sp->GetFrameCodeAddress(), target_sp.get()));
    }

    return true;
}

bool
UserExpression::MatchesContext (ExecutionContext &exe_ctx)
{
    lldb::TargetSP target_sp;
    lldb::ProcessSP process_sp;
    lldb::StackFrameSP frame_sp;

    return LockAndCheckContext(exe_ctx, target_sp, process_sp, frame_sp);
}

lldb::addr_t
UserExpression::GetObjectPointer (lldb::StackFrameSP frame_sp,
                  ConstString &object_name,
                  Error &err)
{
    err.Clear();

    if (!frame_sp)
    {
        err.SetErrorStringWithFormat("Couldn't load '%s' because the context is incomplete", object_name.AsCString());
        return LLDB_INVALID_ADDRESS;
    }

    lldb::VariableSP var_sp;
    lldb::ValueObjectSP valobj_sp;

    valobj_sp = frame_sp->GetValueForVariableExpressionPath(object_name.AsCString(),
                                                            lldb::eNoDynamicValues,
                                                            StackFrame::eExpressionPathOptionCheckPtrVsMember |
                                                            StackFrame::eExpressionPathOptionsNoFragileObjcIvar |
                                                            StackFrame::eExpressionPathOptionsNoSyntheticChildren |
                                                            StackFrame::eExpressionPathOptionsNoSyntheticArrayRange,
                                                            var_sp,
                                                            err);

    if (!err.Success() || !valobj_sp.get())
        return LLDB_INVALID_ADDRESS;

    lldb::addr_t ret = valobj_sp->GetValueAsUnsigned(LLDB_INVALID_ADDRESS);

    if (ret == LLDB_INVALID_ADDRESS)
    {
        err.SetErrorStringWithFormat("Couldn't load '%s' because its value couldn't be evaluated", object_name.AsCString());
        return LLDB_INVALID_ADDRESS;
    }

    return ret;
}

lldb::ExpressionResults
UserExpression::Evaluate (ExecutionContext &exe_ctx,
                               const EvaluateExpressionOptions& options,
                               const char *expr_cstr,
                               const char *expr_prefix,
                               lldb::ValueObjectSP &result_valobj_sp,
                               Error &error,
                               uint32_t line_offset,
                               lldb::ModuleSP *jit_module_sp_ptr)
{
    Log *log(lldb_private::GetLogIfAnyCategoriesSet (LIBLLDB_LOG_EXPRESSIONS | LIBLLDB_LOG_STEP));

    lldb_private::ExecutionPolicy execution_policy = options.GetExecutionPolicy();
    lldb::LanguageType language = options.GetLanguage();
    const ResultType desired_type = options.DoesCoerceToId() ? UserExpression::eResultTypeId : UserExpression::eResultTypeAny;
    lldb::ExpressionResults execution_results = lldb::eExpressionSetupError;
    
    Target *target = exe_ctx.GetTargetPtr();
    if (!target)
    {
        if (log)
            log->Printf("== [UserExpression::Evaluate] Passed a NULL target, can't run expressions.");
        return lldb::eExpressionSetupError;
    }

    Process *process = exe_ctx.GetProcessPtr();

    if (process == NULL || process->GetState() != lldb::eStateStopped)
    {
        if (execution_policy == eExecutionPolicyAlways)
        {
            if (log)
                log->Printf("== [UserExpression::Evaluate] Expression may not run, but is not constant ==");

            error.SetErrorString ("expression needed to run but couldn't");

            return execution_results;
        }
    }

    if (process == NULL || !process->CanJIT())
        execution_policy = eExecutionPolicyNever;

    const char *full_prefix = NULL;
    const char *option_prefix = options.GetPrefix();
    std::string full_prefix_storage;
    if (expr_prefix && option_prefix)
    {
        full_prefix_storage.assign(expr_prefix);
        full_prefix_storage.append(option_prefix);
        if (!full_prefix_storage.empty())
            full_prefix = full_prefix_storage.c_str();
    }
    else if (expr_prefix)
        full_prefix = expr_prefix;
    else
        full_prefix = option_prefix;

    // If the language was not specified in the expression command,
    // set it to the language in the target's properties if
    // specified, else default to the langage for the frame.
    if (language == lldb::eLanguageTypeUnknown)
    {
        if (target->GetLanguage() != lldb::eLanguageTypeUnknown)
            language = target->GetLanguage();
        else if (StackFrame *frame = exe_ctx.GetFramePtr())
            language = frame->GetLanguage();
    }

    lldb::UserExpressionSP user_expression_sp(target->GetUserExpressionForLanguage (expr_cstr,
                                                                                    full_prefix,
                                                                                    language,
                                                                                    desired_type,
                                                                                    options,
                                                                                    error));
    if (error.Fail())
    {
        if (log)
            log->Printf ("== [UserExpression::Evaluate] Getting expression: %s ==", error.AsCString());
        return lldb::eExpressionSetupError;
    }
 
    StreamString error_stream;

    if (log)
        log->Printf("== [UserExpression::Evaluate] Parsing expression %s ==", expr_cstr);

    const bool keep_expression_in_memory = true;
    const bool generate_debug_info = options.GetGenerateDebugInfo();

    if (options.InvokeCancelCallback (lldb::eExpressionEvaluationParse))
    {
        error.SetErrorString ("expression interrupted by callback before parse");
        result_valobj_sp = ValueObjectConstResult::Create (exe_ctx.GetBestExecutionContextScope(), error);
        return lldb::eExpressionInterrupted;
    }

    if (!user_expression_sp->Parse (error_stream,
                                    exe_ctx,
                                    execution_policy,
                                    keep_expression_in_memory,
                                    generate_debug_info))
    {
        execution_results = lldb::eExpressionParseError;
        if (error_stream.GetString().empty())
            error.SetExpressionError (execution_results, "expression failed to parse, unknown error");
        else
            error.SetExpressionError (execution_results, error_stream.GetString().c_str());
    }
    else
    {
        // If a pointer to a lldb::ModuleSP was passed in, return the JIT'ed module if one was created
        if (jit_module_sp_ptr)
            *jit_module_sp_ptr = user_expression_sp->GetJITModule();

        lldb::ExpressionVariableSP expr_result;

        if (execution_policy == eExecutionPolicyNever &&
            !user_expression_sp->CanInterpret())
        {
            if (log)
                log->Printf("== [UserExpression::Evaluate] Expression may not run, but is not constant ==");

            if (error_stream.GetString().empty())
                error.SetExpressionError (lldb::eExpressionSetupError, "expression needed to run but couldn't");
        }
        else
        {
            if (options.InvokeCancelCallback (lldb::eExpressionEvaluationExecution))
            {
                error.SetExpressionError (lldb::eExpressionInterrupted, "expression interrupted by callback before execution");
                result_valobj_sp = ValueObjectConstResult::Create (exe_ctx.GetBestExecutionContextScope(), error);
                return lldb::eExpressionInterrupted;
            }

            error_stream.GetString().clear();

            if (log)
                log->Printf("== [UserExpression::Evaluate] Executing expression ==");

            execution_results = user_expression_sp->Execute (error_stream,
                                                             exe_ctx,
                                                             options,
                                                             user_expression_sp,
                                                             expr_result);

            if (options.GetResultIsInternal() && expr_result && process)
            {
                process->GetTarget().GetPersistentExpressionStateForLanguage(language)->RemovePersistentVariable (expr_result);
            }

            if (execution_results != lldb::eExpressionCompleted)
            {
                if (log)
                    log->Printf("== [UserExpression::Evaluate] Execution completed abnormally ==");

                if (error_stream.GetString().empty())
                    error.SetExpressionError (execution_results, "expression failed to execute, unknown error");
                else
                    error.SetExpressionError (execution_results, error_stream.GetString().c_str());
            }
            else
            {
                if (expr_result)
                {
                    result_valobj_sp = expr_result->GetValueObject();

                    if (log)
                        log->Printf("== [UserExpression::Evaluate] Execution completed normally with result %s ==",
                                    result_valobj_sp->GetValueAsCString());
                }
                else
                {
                    if (log)
                        log->Printf("== [UserExpression::Evaluate] Execution completed normally with no result ==");

                    error.SetError(UserExpression::kNoResult, lldb::eErrorTypeGeneric);
                }
            }
        }
    }

    if (options.InvokeCancelCallback(lldb::eExpressionEvaluationComplete))
    {
        error.SetExpressionError (lldb::eExpressionInterrupted, "expression interrupted by callback after complete");
        return lldb::eExpressionInterrupted;
    }

    if (result_valobj_sp.get() == NULL)
    {
        result_valobj_sp = ValueObjectConstResult::Create (exe_ctx.GetBestExecutionContextScope(), error);
    }

    return execution_results;
}
