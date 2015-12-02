//===-- CrashReason.cpp -----------------------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "CrashReason.h"

#include <sstream>

namespace {

void
AppendFaultAddr (std::string& str, lldb::addr_t addr)
{
    std::stringstream ss;
    ss << " (fault address: 0x" << std::hex << addr << ")";
    str += ss.str();
}

CrashReason
GetCrashReasonForSIGSEGV(const siginfo_t& info)
{
    assert(info.si_signo == SIGSEGV);

    switch (info.si_code)
    {
    case SI_KERNEL:
        // Linux will occasionally send spurious SI_KERNEL codes.
        // (this is poorly documented in sigaction)
        // One way to get this is via unaligned SIMD loads.
        return CrashReason::eInvalidAddress; // for lack of anything better
    case SEGV_MAPERR:
        return CrashReason::eInvalidAddress;
    case SEGV_ACCERR:
        return CrashReason::ePrivilegedAddress;
    }

    assert(false && "unexpected si_code for SIGSEGV");
    return CrashReason::eInvalidCrashReason;
}

CrashReason
GetCrashReasonForSIGILL(const siginfo_t& info)
{
    assert(info.si_signo == SIGILL);

    switch (info.si_code)
    {
    case ILL_ILLOPC:
        return CrashReason::eIllegalOpcode;
    case ILL_ILLOPN:
        return CrashReason::eIllegalOperand;
    case ILL_ILLADR:
        return CrashReason::eIllegalAddressingMode;
    case ILL_ILLTRP:
        return CrashReason::eIllegalTrap;
    case ILL_PRVOPC:
        return CrashReason::ePrivilegedOpcode;
    case ILL_PRVREG:
        return CrashReason::ePrivilegedRegister;
    case ILL_COPROC:
        return CrashReason::eCoprocessorError;
    case ILL_BADSTK:
        return CrashReason::eInternalStackError;
    }

    assert(false && "unexpected si_code for SIGILL");
    return CrashReason::eInvalidCrashReason;
}

CrashReason
GetCrashReasonForSIGFPE(const siginfo_t& info)
{
    assert(info.si_signo == SIGFPE);

    switch (info.si_code)
    {
    case FPE_INTDIV:
        return CrashReason::eIntegerDivideByZero;
    case FPE_INTOVF:
        return CrashReason::eIntegerOverflow;
    case FPE_FLTDIV:
        return CrashReason::eFloatDivideByZero;
    case FPE_FLTOVF:
        return CrashReason::eFloatOverflow;
    case FPE_FLTUND:
        return CrashReason::eFloatUnderflow;
    case FPE_FLTRES:
        return CrashReason::eFloatInexactResult;
    case FPE_FLTINV:
        return CrashReason::eFloatInvalidOperation;
    case FPE_FLTSUB:
        return CrashReason::eFloatSubscriptRange;
    }

    assert(false && "unexpected si_code for SIGFPE");
    return CrashReason::eInvalidCrashReason;
}

CrashReason
GetCrashReasonForSIGBUS(const siginfo_t& info)
{
    assert(info.si_signo == SIGBUS);

    switch (info.si_code)
    {
    case BUS_ADRALN:
        return CrashReason::eIllegalAlignment;
    case BUS_ADRERR:
        return CrashReason::eIllegalAddress;
    case BUS_OBJERR:
        return CrashReason::eHardwareError;
    }

    assert(false && "unexpected si_code for SIGBUS");
    return CrashReason::eInvalidCrashReason;
}

}

std::string
GetCrashReasonString (CrashReason reason, lldb::addr_t fault_addr)
{
    std::string str;

    switch (reason)
    {
    default:
        assert(false && "invalid CrashReason");
        break;

    case CrashReason::eInvalidAddress:
        str = "signal SIGSEGV: invalid address";
        AppendFaultAddr (str, fault_addr);
        break;
    case CrashReason::ePrivilegedAddress:
        str = "signal SIGSEGV: address access protected";
        AppendFaultAddr (str, fault_addr);
        break;
    case CrashReason::eIllegalOpcode:
        str = "signal SIGILL: illegal instruction";
        break;
    case CrashReason::eIllegalOperand:
        str = "signal SIGILL: illegal instruction operand";
        break;
    case CrashReason::eIllegalAddressingMode:
        str = "signal SIGILL: illegal addressing mode";
        break;
    case CrashReason::eIllegalTrap:
        str = "signal SIGILL: illegal trap";
        break;
    case CrashReason::ePrivilegedOpcode:
        str = "signal SIGILL: privileged instruction";
        break;
    case CrashReason::ePrivilegedRegister:
        str = "signal SIGILL: privileged register";
        break;
    case CrashReason::eCoprocessorError:
        str = "signal SIGILL: coprocessor error";
        break;
    case CrashReason::eInternalStackError:
        str = "signal SIGILL: internal stack error";
        break;
    case CrashReason::eIllegalAlignment:
        str = "signal SIGBUS: illegal alignment";
        break;
    case CrashReason::eIllegalAddress:
        str = "signal SIGBUS: illegal address";
        break;
    case CrashReason::eHardwareError:
        str = "signal SIGBUS: hardware error";
        break;
    case CrashReason::eIntegerDivideByZero:
        str = "signal SIGFPE: integer divide by zero";
        break;
    case CrashReason::eIntegerOverflow:
        str = "signal SIGFPE: integer overflow";
        break;
    case CrashReason::eFloatDivideByZero:
        str = "signal SIGFPE: floating point divide by zero";
        break;
    case CrashReason::eFloatOverflow:
        str = "signal SIGFPE: floating point overflow";
        break;
    case CrashReason::eFloatUnderflow:
        str = "signal SIGFPE: floating point underflow";
        break;
    case CrashReason::eFloatInexactResult:
        str = "signal SIGFPE: inexact floating point result";
        break;
    case CrashReason::eFloatInvalidOperation:
        str = "signal SIGFPE: invalid floating point operation";
        break;
    case CrashReason::eFloatSubscriptRange:
        str = "signal SIGFPE: invalid floating point subscript range";
        break;
    }

    return str;
}

const char *
CrashReasonAsString (CrashReason reason)
{
#ifdef LLDB_CONFIGURATION_BUILDANDINTEGRATION
    // Just return the code in ascii for integration builds.
    chcar str[8];
    sprintf(str, "%d", reason);
#else
    const char *str = nullptr;

    switch (reason)
    {
    case CrashReason::eInvalidCrashReason:
        str = "eInvalidCrashReason";
        break;

    // SIGSEGV crash reasons.
    case CrashReason::eInvalidAddress:
        str = "eInvalidAddress";
        break;
    case CrashReason::ePrivilegedAddress:
        str = "ePrivilegedAddress";
        break;

    // SIGILL crash reasons.
    case CrashReason::eIllegalOpcode:
        str = "eIllegalOpcode";
        break;
    case CrashReason::eIllegalOperand:
        str = "eIllegalOperand";
        break;
    case CrashReason::eIllegalAddressingMode:
        str = "eIllegalAddressingMode";
        break;
    case CrashReason::eIllegalTrap:
        str = "eIllegalTrap";
        break;
    case CrashReason::ePrivilegedOpcode:
        str = "ePrivilegedOpcode";
        break;
    case CrashReason::ePrivilegedRegister:
        str = "ePrivilegedRegister";
        break;
    case CrashReason::eCoprocessorError:
        str = "eCoprocessorError";
        break;
    case CrashReason::eInternalStackError:
        str = "eInternalStackError";
        break;

    // SIGBUS crash reasons:
    case CrashReason::eIllegalAlignment:
        str = "eIllegalAlignment";
        break;
    case CrashReason::eIllegalAddress:
        str = "eIllegalAddress";
        break;
    case CrashReason::eHardwareError:
        str = "eHardwareError";
        break;

    // SIGFPE crash reasons:
    case CrashReason::eIntegerDivideByZero:
        str = "eIntegerDivideByZero";
        break;
    case CrashReason::eIntegerOverflow:
        str = "eIntegerOverflow";
        break;
    case CrashReason::eFloatDivideByZero:
        str = "eFloatDivideByZero";
        break;
    case CrashReason::eFloatOverflow:
        str = "eFloatOverflow";
        break;
    case CrashReason::eFloatUnderflow:
        str = "eFloatUnderflow";
        break;
    case CrashReason::eFloatInexactResult:
        str = "eFloatInexactResult";
        break;
    case CrashReason::eFloatInvalidOperation:
        str = "eFloatInvalidOperation";
        break;
    case CrashReason::eFloatSubscriptRange:
        str = "eFloatSubscriptRange";
        break;
    }
#endif

    return str;
}

CrashReason
GetCrashReason(const siginfo_t& info)
{
    switch(info.si_signo)
    {
    case SIGSEGV:
        return GetCrashReasonForSIGSEGV(info);
    case SIGBUS:
        return GetCrashReasonForSIGBUS(info);
    case SIGFPE:
        return GetCrashReasonForSIGFPE(info);
    case SIGILL:
        return GetCrashReasonForSIGILL(info);
    }

    assert(false && "unexpected signal");
    return CrashReason::eInvalidCrashReason;
}
