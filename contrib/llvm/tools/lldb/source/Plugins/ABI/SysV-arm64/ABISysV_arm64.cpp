//===-- ABISysV_arm64.cpp -------------------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "ABISysV_arm64.h"

#include "lldb/Core/ConstString.h"
#include "lldb/Core/Error.h"
#include "lldb/Core/Log.h"
#include "lldb/Core/Module.h"
#include "lldb/Core/PluginManager.h"
#include "lldb/Core/RegisterValue.h"
#include "lldb/Core/Scalar.h"
#include "lldb/Core/Value.h"
#include "lldb/Core/ValueObjectConstResult.h"
#include "lldb/Symbol/UnwindPlan.h"
#include "lldb/Target/Process.h"
#include "lldb/Target/RegisterContext.h"
#include "lldb/Target/Target.h"
#include "lldb/Target/Thread.h"

#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/Triple.h"

#include "Utility/ARM64_DWARF_Registers.h"

#include <vector>

using namespace lldb;
using namespace lldb_private;

static RegisterInfo g_register_infos[] =
{
    //  NAME       ALT       SZ OFF ENCODING          FORMAT                   EH_FRAME             DWARF                  GENERIC                     PROCESS PLUGIN          LLDB NATIVE
    //  ========== =======   == === =============     ===================      ===================  ====================== =========================== ======================= ======================
    {   "x0",      NULL,      8, 0, eEncodingUint   , eFormatHex           , { LLDB_INVALID_REGNUM, arm64_dwarf::x0,       LLDB_REGNUM_GENERIC_ARG1,   LLDB_INVALID_REGNUM,    LLDB_INVALID_REGNUM }, NULL, NULL },
    {   "x1",      NULL,      8, 0, eEncodingUint   , eFormatHex           , { LLDB_INVALID_REGNUM, arm64_dwarf::x1,       LLDB_REGNUM_GENERIC_ARG2,   LLDB_INVALID_REGNUM,    LLDB_INVALID_REGNUM }, NULL, NULL },
    {   "x2",      NULL,      8, 0, eEncodingUint   , eFormatHex           , { LLDB_INVALID_REGNUM, arm64_dwarf::x2,       LLDB_REGNUM_GENERIC_ARG3,   LLDB_INVALID_REGNUM,    LLDB_INVALID_REGNUM }, NULL, NULL },
    {   "x3",      NULL,      8, 0, eEncodingUint   , eFormatHex           , { LLDB_INVALID_REGNUM, arm64_dwarf::x3,       LLDB_REGNUM_GENERIC_ARG4,   LLDB_INVALID_REGNUM,    LLDB_INVALID_REGNUM }, NULL, NULL },
    {   "x4",      NULL,      8, 0, eEncodingUint   , eFormatHex           , { LLDB_INVALID_REGNUM, arm64_dwarf::x4,       LLDB_REGNUM_GENERIC_ARG5,   LLDB_INVALID_REGNUM,    LLDB_INVALID_REGNUM }, NULL, NULL },
    {   "x5",      NULL,      8, 0, eEncodingUint   , eFormatHex           , { LLDB_INVALID_REGNUM, arm64_dwarf::x5,       LLDB_REGNUM_GENERIC_ARG6,   LLDB_INVALID_REGNUM,    LLDB_INVALID_REGNUM }, NULL, NULL },
    {   "x6",      NULL,      8, 0, eEncodingUint   , eFormatHex           , { LLDB_INVALID_REGNUM, arm64_dwarf::x6,       LLDB_REGNUM_GENERIC_ARG7,   LLDB_INVALID_REGNUM,    LLDB_INVALID_REGNUM }, NULL, NULL },
    {   "x7",      NULL,      8, 0, eEncodingUint   , eFormatHex           , { LLDB_INVALID_REGNUM, arm64_dwarf::x7,       LLDB_REGNUM_GENERIC_ARG8,   LLDB_INVALID_REGNUM,    LLDB_INVALID_REGNUM }, NULL, NULL },
    {   "x8",      NULL,      8, 0, eEncodingUint   , eFormatHex           , { LLDB_INVALID_REGNUM, arm64_dwarf::x8,       LLDB_INVALID_REGNUM,        LLDB_INVALID_REGNUM,    LLDB_INVALID_REGNUM }, NULL, NULL },
    {   "x9",      NULL,      8, 0, eEncodingUint   , eFormatHex           , { LLDB_INVALID_REGNUM, arm64_dwarf::x9,       LLDB_INVALID_REGNUM,        LLDB_INVALID_REGNUM,    LLDB_INVALID_REGNUM }, NULL, NULL },
    {   "x10",     NULL,      8, 0, eEncodingUint   , eFormatHex           , { LLDB_INVALID_REGNUM, arm64_dwarf::x10,      LLDB_INVALID_REGNUM,        LLDB_INVALID_REGNUM,    LLDB_INVALID_REGNUM }, NULL, NULL },
    {   "x11",     NULL,      8, 0, eEncodingUint   , eFormatHex           , { LLDB_INVALID_REGNUM, arm64_dwarf::x11,      LLDB_INVALID_REGNUM,        LLDB_INVALID_REGNUM,    LLDB_INVALID_REGNUM }, NULL, NULL },
    {   "x12",     NULL,      8, 0, eEncodingUint   , eFormatHex           , { LLDB_INVALID_REGNUM, arm64_dwarf::x12,      LLDB_INVALID_REGNUM,        LLDB_INVALID_REGNUM,    LLDB_INVALID_REGNUM }, NULL, NULL },
    {   "x13",     NULL,      8, 0, eEncodingUint   , eFormatHex           , { LLDB_INVALID_REGNUM, arm64_dwarf::x13,      LLDB_INVALID_REGNUM,        LLDB_INVALID_REGNUM,    LLDB_INVALID_REGNUM }, NULL, NULL },
    {   "x14",     NULL,      8, 0, eEncodingUint   , eFormatHex           , { LLDB_INVALID_REGNUM, arm64_dwarf::x14,      LLDB_INVALID_REGNUM,        LLDB_INVALID_REGNUM,    LLDB_INVALID_REGNUM }, NULL, NULL },
    {   "x15",     NULL,      8, 0, eEncodingUint   , eFormatHex           , { LLDB_INVALID_REGNUM, arm64_dwarf::x15,      LLDB_INVALID_REGNUM,        LLDB_INVALID_REGNUM,    LLDB_INVALID_REGNUM }, NULL, NULL },
    {   "x16",     NULL,      8, 0, eEncodingUint   , eFormatHex           , { LLDB_INVALID_REGNUM, arm64_dwarf::x16,      LLDB_INVALID_REGNUM,        LLDB_INVALID_REGNUM,    LLDB_INVALID_REGNUM }, NULL, NULL },
    {   "x17",     NULL,      8, 0, eEncodingUint   , eFormatHex           , { LLDB_INVALID_REGNUM, arm64_dwarf::x17,      LLDB_INVALID_REGNUM,        LLDB_INVALID_REGNUM,    LLDB_INVALID_REGNUM }, NULL, NULL },
    {   "x18",     NULL,      8, 0, eEncodingUint   , eFormatHex           , { LLDB_INVALID_REGNUM, arm64_dwarf::x18,      LLDB_INVALID_REGNUM,        LLDB_INVALID_REGNUM,    LLDB_INVALID_REGNUM }, NULL, NULL },
    {   "x19",     NULL,      8, 0, eEncodingUint   , eFormatHex           , { LLDB_INVALID_REGNUM, arm64_dwarf::x19,      LLDB_INVALID_REGNUM,        LLDB_INVALID_REGNUM,    LLDB_INVALID_REGNUM }, NULL, NULL },
    {   "x20",     NULL,      8, 0, eEncodingUint   , eFormatHex           , { LLDB_INVALID_REGNUM, arm64_dwarf::x20,      LLDB_INVALID_REGNUM,        LLDB_INVALID_REGNUM,    LLDB_INVALID_REGNUM }, NULL, NULL },
    {   "x21",     NULL,      8, 0, eEncodingUint   , eFormatHex           , { LLDB_INVALID_REGNUM, arm64_dwarf::x21,      LLDB_INVALID_REGNUM,        LLDB_INVALID_REGNUM,    LLDB_INVALID_REGNUM }, NULL, NULL },
    {   "x22",     NULL,      8, 0, eEncodingUint   , eFormatHex           , { LLDB_INVALID_REGNUM, arm64_dwarf::x22,      LLDB_INVALID_REGNUM,        LLDB_INVALID_REGNUM,    LLDB_INVALID_REGNUM }, NULL, NULL },
    {   "x23",     NULL,      8, 0, eEncodingUint   , eFormatHex           , { LLDB_INVALID_REGNUM, arm64_dwarf::x23,      LLDB_INVALID_REGNUM,        LLDB_INVALID_REGNUM,    LLDB_INVALID_REGNUM }, NULL, NULL },
    {   "x24",     NULL,      8, 0, eEncodingUint   , eFormatHex           , { LLDB_INVALID_REGNUM, arm64_dwarf::x24,      LLDB_INVALID_REGNUM,        LLDB_INVALID_REGNUM,    LLDB_INVALID_REGNUM }, NULL, NULL },
    {   "x25",     NULL,      8, 0, eEncodingUint   , eFormatHex           , { LLDB_INVALID_REGNUM, arm64_dwarf::x25,      LLDB_INVALID_REGNUM,        LLDB_INVALID_REGNUM,    LLDB_INVALID_REGNUM }, NULL, NULL },
    {   "x26",     NULL,      8, 0, eEncodingUint   , eFormatHex           , { LLDB_INVALID_REGNUM, arm64_dwarf::x26,      LLDB_INVALID_REGNUM,        LLDB_INVALID_REGNUM,    LLDB_INVALID_REGNUM }, NULL, NULL },
    {   "x27",     NULL,      8, 0, eEncodingUint   , eFormatHex           , { LLDB_INVALID_REGNUM, arm64_dwarf::x27,      LLDB_INVALID_REGNUM,        LLDB_INVALID_REGNUM,    LLDB_INVALID_REGNUM }, NULL, NULL },
    {   "x28",     NULL,      8, 0, eEncodingUint   , eFormatHex           , { LLDB_INVALID_REGNUM, arm64_dwarf::x28,      LLDB_INVALID_REGNUM,        LLDB_INVALID_REGNUM,    LLDB_INVALID_REGNUM }, NULL, NULL },
    {   "fp",      "x29",     8, 0, eEncodingUint   , eFormatHex           , { LLDB_INVALID_REGNUM, arm64_dwarf::x29,      LLDB_REGNUM_GENERIC_FP,     LLDB_INVALID_REGNUM,    LLDB_INVALID_REGNUM }, NULL, NULL },
    {   "lr",      "x30",     8, 0, eEncodingUint   , eFormatHex           , { LLDB_INVALID_REGNUM, arm64_dwarf::x30,      LLDB_REGNUM_GENERIC_RA,     LLDB_INVALID_REGNUM,    LLDB_INVALID_REGNUM }, NULL, NULL },
    {   "sp",      "x31",     8, 0, eEncodingUint   , eFormatHex           , { LLDB_INVALID_REGNUM, arm64_dwarf::x31,      LLDB_REGNUM_GENERIC_SP,     LLDB_INVALID_REGNUM,    LLDB_INVALID_REGNUM }, NULL, NULL },
    {   "pc",      NULL,      8, 0, eEncodingUint   , eFormatHex           , { LLDB_INVALID_REGNUM, arm64_dwarf::pc,       LLDB_REGNUM_GENERIC_PC,     LLDB_INVALID_REGNUM,    LLDB_INVALID_REGNUM }, NULL, NULL },
    {   "cpsr",    "psr",     4, 0, eEncodingUint   , eFormatHex           , { LLDB_INVALID_REGNUM, arm64_dwarf::cpsr,     LLDB_REGNUM_GENERIC_FLAGS,  LLDB_INVALID_REGNUM,    LLDB_INVALID_REGNUM }, NULL, NULL },

    {   "v0",      NULL,     16, 0, eEncodingVector , eFormatVectorOfUInt8,  { LLDB_INVALID_REGNUM, arm64_dwarf::v0,       LLDB_INVALID_REGNUM,        LLDB_INVALID_REGNUM,    LLDB_INVALID_REGNUM }, NULL, NULL },
    {   "v1",      NULL,     16, 0, eEncodingVector , eFormatVectorOfUInt8,  { LLDB_INVALID_REGNUM, arm64_dwarf::v1,       LLDB_INVALID_REGNUM,        LLDB_INVALID_REGNUM,    LLDB_INVALID_REGNUM }, NULL, NULL },
    {   "v2",      NULL,     16, 0, eEncodingVector , eFormatVectorOfUInt8,  { LLDB_INVALID_REGNUM, arm64_dwarf::v2,       LLDB_INVALID_REGNUM,        LLDB_INVALID_REGNUM,    LLDB_INVALID_REGNUM }, NULL, NULL },
    {   "v3",      NULL,     16, 0, eEncodingVector , eFormatVectorOfUInt8,  { LLDB_INVALID_REGNUM, arm64_dwarf::v3,       LLDB_INVALID_REGNUM,        LLDB_INVALID_REGNUM,    LLDB_INVALID_REGNUM }, NULL, NULL },
    {   "v4",      NULL,     16, 0, eEncodingVector , eFormatVectorOfUInt8,  { LLDB_INVALID_REGNUM, arm64_dwarf::v4,       LLDB_INVALID_REGNUM,        LLDB_INVALID_REGNUM,    LLDB_INVALID_REGNUM }, NULL, NULL },
    {   "v5",      NULL,     16, 0, eEncodingVector , eFormatVectorOfUInt8,  { LLDB_INVALID_REGNUM, arm64_dwarf::v5,       LLDB_INVALID_REGNUM,        LLDB_INVALID_REGNUM,    LLDB_INVALID_REGNUM }, NULL, NULL },
    {   "v6",      NULL,     16, 0, eEncodingVector , eFormatVectorOfUInt8,  { LLDB_INVALID_REGNUM, arm64_dwarf::v6,       LLDB_INVALID_REGNUM,        LLDB_INVALID_REGNUM,    LLDB_INVALID_REGNUM }, NULL, NULL },
    {   "v7",      NULL,     16, 0, eEncodingVector , eFormatVectorOfUInt8,  { LLDB_INVALID_REGNUM, arm64_dwarf::v7,       LLDB_INVALID_REGNUM,        LLDB_INVALID_REGNUM,    LLDB_INVALID_REGNUM }, NULL, NULL },
    {   "v8",      NULL,     16, 0, eEncodingVector , eFormatVectorOfUInt8,  { LLDB_INVALID_REGNUM, arm64_dwarf::v8,       LLDB_INVALID_REGNUM,        LLDB_INVALID_REGNUM,    LLDB_INVALID_REGNUM }, NULL, NULL },
    {   "v9",      NULL,     16, 0, eEncodingVector , eFormatVectorOfUInt8,  { LLDB_INVALID_REGNUM, arm64_dwarf::v9,       LLDB_INVALID_REGNUM,        LLDB_INVALID_REGNUM,    LLDB_INVALID_REGNUM }, NULL, NULL },
    {   "v10",     NULL,     16, 0, eEncodingVector , eFormatVectorOfUInt8,  { LLDB_INVALID_REGNUM, arm64_dwarf::v10,      LLDB_INVALID_REGNUM,        LLDB_INVALID_REGNUM,    LLDB_INVALID_REGNUM }, NULL, NULL },
    {   "v11",     NULL,     16, 0, eEncodingVector , eFormatVectorOfUInt8,  { LLDB_INVALID_REGNUM, arm64_dwarf::v11,      LLDB_INVALID_REGNUM,        LLDB_INVALID_REGNUM,    LLDB_INVALID_REGNUM }, NULL, NULL },
    {   "v12",     NULL,     16, 0, eEncodingVector , eFormatVectorOfUInt8,  { LLDB_INVALID_REGNUM, arm64_dwarf::v12,      LLDB_INVALID_REGNUM,        LLDB_INVALID_REGNUM,    LLDB_INVALID_REGNUM }, NULL, NULL },
    {   "v13",     NULL,     16, 0, eEncodingVector , eFormatVectorOfUInt8,  { LLDB_INVALID_REGNUM, arm64_dwarf::v13,      LLDB_INVALID_REGNUM,        LLDB_INVALID_REGNUM,    LLDB_INVALID_REGNUM }, NULL, NULL },
    {   "v14",     NULL,     16, 0, eEncodingVector , eFormatVectorOfUInt8,  { LLDB_INVALID_REGNUM, arm64_dwarf::v14,      LLDB_INVALID_REGNUM,        LLDB_INVALID_REGNUM,    LLDB_INVALID_REGNUM }, NULL, NULL },
    {   "v15",     NULL,     16, 0, eEncodingVector , eFormatVectorOfUInt8,  { LLDB_INVALID_REGNUM, arm64_dwarf::v15,      LLDB_INVALID_REGNUM,        LLDB_INVALID_REGNUM,    LLDB_INVALID_REGNUM }, NULL, NULL },
    {   "v16",     NULL,     16, 0, eEncodingVector , eFormatVectorOfUInt8,  { LLDB_INVALID_REGNUM, arm64_dwarf::v16,      LLDB_INVALID_REGNUM,        LLDB_INVALID_REGNUM,    LLDB_INVALID_REGNUM }, NULL, NULL },
    {   "v17",     NULL,     16, 0, eEncodingVector , eFormatVectorOfUInt8,  { LLDB_INVALID_REGNUM, arm64_dwarf::v17,      LLDB_INVALID_REGNUM,        LLDB_INVALID_REGNUM,    LLDB_INVALID_REGNUM }, NULL, NULL },
    {   "v18",     NULL,     16, 0, eEncodingVector , eFormatVectorOfUInt8,  { LLDB_INVALID_REGNUM, arm64_dwarf::v18,      LLDB_INVALID_REGNUM,        LLDB_INVALID_REGNUM,    LLDB_INVALID_REGNUM }, NULL, NULL },
    {   "v19",     NULL,     16, 0, eEncodingVector , eFormatVectorOfUInt8,  { LLDB_INVALID_REGNUM, arm64_dwarf::v19,      LLDB_INVALID_REGNUM,        LLDB_INVALID_REGNUM,    LLDB_INVALID_REGNUM }, NULL, NULL },
    {   "v20",     NULL,     16, 0, eEncodingVector , eFormatVectorOfUInt8,  { LLDB_INVALID_REGNUM, arm64_dwarf::v20,      LLDB_INVALID_REGNUM,        LLDB_INVALID_REGNUM,    LLDB_INVALID_REGNUM }, NULL, NULL },
    {   "v21",     NULL,     16, 0, eEncodingVector , eFormatVectorOfUInt8,  { LLDB_INVALID_REGNUM, arm64_dwarf::v21,      LLDB_INVALID_REGNUM,        LLDB_INVALID_REGNUM,    LLDB_INVALID_REGNUM }, NULL, NULL },
    {   "v22",     NULL,     16, 0, eEncodingVector , eFormatVectorOfUInt8,  { LLDB_INVALID_REGNUM, arm64_dwarf::v22,      LLDB_INVALID_REGNUM,        LLDB_INVALID_REGNUM,    LLDB_INVALID_REGNUM }, NULL, NULL },
    {   "v23",     NULL,     16, 0, eEncodingVector , eFormatVectorOfUInt8,  { LLDB_INVALID_REGNUM, arm64_dwarf::v23,      LLDB_INVALID_REGNUM,        LLDB_INVALID_REGNUM,    LLDB_INVALID_REGNUM }, NULL, NULL },
    {   "v24",     NULL,     16, 0, eEncodingVector , eFormatVectorOfUInt8,  { LLDB_INVALID_REGNUM, arm64_dwarf::v24,      LLDB_INVALID_REGNUM,        LLDB_INVALID_REGNUM,    LLDB_INVALID_REGNUM }, NULL, NULL },
    {   "v25",     NULL,     16, 0, eEncodingVector , eFormatVectorOfUInt8,  { LLDB_INVALID_REGNUM, arm64_dwarf::v25,      LLDB_INVALID_REGNUM,        LLDB_INVALID_REGNUM,    LLDB_INVALID_REGNUM }, NULL, NULL },
    {   "v26",     NULL,     16, 0, eEncodingVector , eFormatVectorOfUInt8,  { LLDB_INVALID_REGNUM, arm64_dwarf::v26,      LLDB_INVALID_REGNUM,        LLDB_INVALID_REGNUM,    LLDB_INVALID_REGNUM }, NULL, NULL },
    {   "v27",     NULL,     16, 0, eEncodingVector , eFormatVectorOfUInt8,  { LLDB_INVALID_REGNUM, arm64_dwarf::v27,      LLDB_INVALID_REGNUM,        LLDB_INVALID_REGNUM,    LLDB_INVALID_REGNUM }, NULL, NULL },
    {   "v28",     NULL,     16, 0, eEncodingVector , eFormatVectorOfUInt8,  { LLDB_INVALID_REGNUM, arm64_dwarf::v28,      LLDB_INVALID_REGNUM,        LLDB_INVALID_REGNUM,    LLDB_INVALID_REGNUM }, NULL, NULL },
    {   "v29",     NULL,     16, 0, eEncodingVector , eFormatVectorOfUInt8,  { LLDB_INVALID_REGNUM, arm64_dwarf::v29,      LLDB_INVALID_REGNUM,        LLDB_INVALID_REGNUM,    LLDB_INVALID_REGNUM }, NULL, NULL },
    {   "v30",     NULL,     16, 0, eEncodingVector , eFormatVectorOfUInt8,  { LLDB_INVALID_REGNUM, arm64_dwarf::v30,      LLDB_INVALID_REGNUM,        LLDB_INVALID_REGNUM,    LLDB_INVALID_REGNUM }, NULL, NULL },
    {   "v31",     NULL,     16, 0, eEncodingVector , eFormatVectorOfUInt8,  { LLDB_INVALID_REGNUM, arm64_dwarf::v31,      LLDB_INVALID_REGNUM,        LLDB_INVALID_REGNUM,    LLDB_INVALID_REGNUM }, NULL, NULL },

    {   "fpsr",    NULL,      4, 0, eEncodingUint   , eFormatHex           , { LLDB_INVALID_REGNUM, LLDB_INVALID_REGNUM,   LLDB_INVALID_REGNUM,        LLDB_INVALID_REGNUM,    LLDB_INVALID_REGNUM }, NULL, NULL },
    {   "fpcr",    NULL,      4, 0, eEncodingUint   , eFormatHex           , { LLDB_INVALID_REGNUM, LLDB_INVALID_REGNUM,   LLDB_INVALID_REGNUM,        LLDB_INVALID_REGNUM,    LLDB_INVALID_REGNUM }, NULL, NULL },

    {   "s0",      NULL,     4, 0, eEncodingIEEE754 , eFormatFloat         , { LLDB_INVALID_REGNUM, LLDB_INVALID_REGNUM,    LLDB_INVALID_REGNUM,        LLDB_INVALID_REGNUM,    LLDB_INVALID_REGNUM }, NULL, NULL },
    {   "s1",      NULL,     4, 0, eEncodingIEEE754 , eFormatFloat         , { LLDB_INVALID_REGNUM, LLDB_INVALID_REGNUM,    LLDB_INVALID_REGNUM,        LLDB_INVALID_REGNUM,    LLDB_INVALID_REGNUM }, NULL, NULL },
    {   "s2",      NULL,     4, 0, eEncodingIEEE754 , eFormatFloat         , { LLDB_INVALID_REGNUM, LLDB_INVALID_REGNUM,    LLDB_INVALID_REGNUM,        LLDB_INVALID_REGNUM,    LLDB_INVALID_REGNUM }, NULL, NULL },
    {   "s3",      NULL,     4, 0, eEncodingIEEE754 , eFormatFloat         , { LLDB_INVALID_REGNUM, LLDB_INVALID_REGNUM,    LLDB_INVALID_REGNUM,        LLDB_INVALID_REGNUM,    LLDB_INVALID_REGNUM }, NULL, NULL },
    {   "s4",      NULL,     4, 0, eEncodingIEEE754 , eFormatFloat         , { LLDB_INVALID_REGNUM, LLDB_INVALID_REGNUM,    LLDB_INVALID_REGNUM,        LLDB_INVALID_REGNUM,    LLDB_INVALID_REGNUM }, NULL, NULL },
    {   "s5",      NULL,     4, 0, eEncodingIEEE754 , eFormatFloat         , { LLDB_INVALID_REGNUM, LLDB_INVALID_REGNUM,    LLDB_INVALID_REGNUM,        LLDB_INVALID_REGNUM,    LLDB_INVALID_REGNUM }, NULL, NULL },
    {   "s6",      NULL,     4, 0, eEncodingIEEE754 , eFormatFloat         , { LLDB_INVALID_REGNUM, LLDB_INVALID_REGNUM,    LLDB_INVALID_REGNUM,        LLDB_INVALID_REGNUM,    LLDB_INVALID_REGNUM }, NULL, NULL },
    {   "s7",      NULL,     4, 0, eEncodingIEEE754 , eFormatFloat         , { LLDB_INVALID_REGNUM, LLDB_INVALID_REGNUM,    LLDB_INVALID_REGNUM,        LLDB_INVALID_REGNUM,    LLDB_INVALID_REGNUM }, NULL, NULL },
    {   "s8",      NULL,     4, 0, eEncodingIEEE754 , eFormatFloat         , { LLDB_INVALID_REGNUM, LLDB_INVALID_REGNUM,    LLDB_INVALID_REGNUM,        LLDB_INVALID_REGNUM,    LLDB_INVALID_REGNUM }, NULL, NULL },
    {   "s9",      NULL,     4, 0, eEncodingIEEE754 , eFormatFloat         , { LLDB_INVALID_REGNUM, LLDB_INVALID_REGNUM,    LLDB_INVALID_REGNUM,        LLDB_INVALID_REGNUM,    LLDB_INVALID_REGNUM }, NULL, NULL },
    {   "s10",     NULL,     4, 0, eEncodingIEEE754 , eFormatFloat         , { LLDB_INVALID_REGNUM, LLDB_INVALID_REGNUM,    LLDB_INVALID_REGNUM,        LLDB_INVALID_REGNUM,    LLDB_INVALID_REGNUM }, NULL, NULL },
    {   "s11",     NULL,     4, 0, eEncodingIEEE754 , eFormatFloat         , { LLDB_INVALID_REGNUM, LLDB_INVALID_REGNUM,    LLDB_INVALID_REGNUM,        LLDB_INVALID_REGNUM,    LLDB_INVALID_REGNUM }, NULL, NULL },
    {   "s12",     NULL,     4, 0, eEncodingIEEE754 , eFormatFloat         , { LLDB_INVALID_REGNUM, LLDB_INVALID_REGNUM,    LLDB_INVALID_REGNUM,        LLDB_INVALID_REGNUM,    LLDB_INVALID_REGNUM }, NULL, NULL },
    {   "s13",     NULL,     4, 0, eEncodingIEEE754 , eFormatFloat         , { LLDB_INVALID_REGNUM, LLDB_INVALID_REGNUM,    LLDB_INVALID_REGNUM,        LLDB_INVALID_REGNUM,    LLDB_INVALID_REGNUM }, NULL, NULL },
    {   "s14",     NULL,     4, 0, eEncodingIEEE754 , eFormatFloat         , { LLDB_INVALID_REGNUM, LLDB_INVALID_REGNUM,    LLDB_INVALID_REGNUM,        LLDB_INVALID_REGNUM,    LLDB_INVALID_REGNUM }, NULL, NULL },
    {   "s15",     NULL,     4, 0, eEncodingIEEE754 , eFormatFloat         , { LLDB_INVALID_REGNUM, LLDB_INVALID_REGNUM,    LLDB_INVALID_REGNUM,        LLDB_INVALID_REGNUM,    LLDB_INVALID_REGNUM }, NULL, NULL },
    {   "s16",     NULL,     4, 0, eEncodingIEEE754 , eFormatFloat         , { LLDB_INVALID_REGNUM, LLDB_INVALID_REGNUM,    LLDB_INVALID_REGNUM,        LLDB_INVALID_REGNUM,    LLDB_INVALID_REGNUM }, NULL, NULL },
    {   "s17",     NULL,     4, 0, eEncodingIEEE754 , eFormatFloat         , { LLDB_INVALID_REGNUM, LLDB_INVALID_REGNUM,    LLDB_INVALID_REGNUM,        LLDB_INVALID_REGNUM,    LLDB_INVALID_REGNUM }, NULL, NULL },
    {   "s18",     NULL,     4, 0, eEncodingIEEE754 , eFormatFloat         , { LLDB_INVALID_REGNUM, LLDB_INVALID_REGNUM,    LLDB_INVALID_REGNUM,        LLDB_INVALID_REGNUM,    LLDB_INVALID_REGNUM }, NULL, NULL },
    {   "s19",     NULL,     4, 0, eEncodingIEEE754 , eFormatFloat         , { LLDB_INVALID_REGNUM, LLDB_INVALID_REGNUM,    LLDB_INVALID_REGNUM,        LLDB_INVALID_REGNUM,    LLDB_INVALID_REGNUM }, NULL, NULL },
    {   "s20",     NULL,     4, 0, eEncodingIEEE754 , eFormatFloat         , { LLDB_INVALID_REGNUM, LLDB_INVALID_REGNUM,    LLDB_INVALID_REGNUM,        LLDB_INVALID_REGNUM,    LLDB_INVALID_REGNUM }, NULL, NULL },
    {   "s21",     NULL,     4, 0, eEncodingIEEE754 , eFormatFloat         , { LLDB_INVALID_REGNUM, LLDB_INVALID_REGNUM,    LLDB_INVALID_REGNUM,        LLDB_INVALID_REGNUM,    LLDB_INVALID_REGNUM }, NULL, NULL },
    {   "s22",     NULL,     4, 0, eEncodingIEEE754 , eFormatFloat         , { LLDB_INVALID_REGNUM, LLDB_INVALID_REGNUM,    LLDB_INVALID_REGNUM,        LLDB_INVALID_REGNUM,    LLDB_INVALID_REGNUM }, NULL, NULL },
    {   "s23",     NULL,     4, 0, eEncodingIEEE754 , eFormatFloat         , { LLDB_INVALID_REGNUM, LLDB_INVALID_REGNUM,    LLDB_INVALID_REGNUM,        LLDB_INVALID_REGNUM,    LLDB_INVALID_REGNUM }, NULL, NULL },
    {   "s24",     NULL,     4, 0, eEncodingIEEE754 , eFormatFloat         , { LLDB_INVALID_REGNUM, LLDB_INVALID_REGNUM,    LLDB_INVALID_REGNUM,        LLDB_INVALID_REGNUM,    LLDB_INVALID_REGNUM }, NULL, NULL },
    {   "s25",     NULL,     4, 0, eEncodingIEEE754 , eFormatFloat         , { LLDB_INVALID_REGNUM, LLDB_INVALID_REGNUM,    LLDB_INVALID_REGNUM,        LLDB_INVALID_REGNUM,    LLDB_INVALID_REGNUM }, NULL, NULL },
    {   "s26",     NULL,     4, 0, eEncodingIEEE754 , eFormatFloat         , { LLDB_INVALID_REGNUM, LLDB_INVALID_REGNUM,    LLDB_INVALID_REGNUM,        LLDB_INVALID_REGNUM,    LLDB_INVALID_REGNUM }, NULL, NULL },
    {   "s27",     NULL,     4, 0, eEncodingIEEE754 , eFormatFloat         , { LLDB_INVALID_REGNUM, LLDB_INVALID_REGNUM,    LLDB_INVALID_REGNUM,        LLDB_INVALID_REGNUM,    LLDB_INVALID_REGNUM }, NULL, NULL },
    {   "s28",     NULL,     4, 0, eEncodingIEEE754 , eFormatFloat         , { LLDB_INVALID_REGNUM, LLDB_INVALID_REGNUM,    LLDB_INVALID_REGNUM,        LLDB_INVALID_REGNUM,    LLDB_INVALID_REGNUM }, NULL, NULL },
    {   "s29",     NULL,     4, 0, eEncodingIEEE754 , eFormatFloat         , { LLDB_INVALID_REGNUM, LLDB_INVALID_REGNUM,    LLDB_INVALID_REGNUM,        LLDB_INVALID_REGNUM,    LLDB_INVALID_REGNUM }, NULL, NULL },
    {   "s30",     NULL,     4, 0, eEncodingIEEE754 , eFormatFloat         , { LLDB_INVALID_REGNUM, LLDB_INVALID_REGNUM,    LLDB_INVALID_REGNUM,        LLDB_INVALID_REGNUM,    LLDB_INVALID_REGNUM }, NULL, NULL },
    {   "s31",     NULL,     4, 0, eEncodingIEEE754 , eFormatFloat         , { LLDB_INVALID_REGNUM, LLDB_INVALID_REGNUM,    LLDB_INVALID_REGNUM,        LLDB_INVALID_REGNUM,    LLDB_INVALID_REGNUM }, NULL, NULL },

    {   "d0",      NULL,     8, 0, eEncodingIEEE754 , eFormatFloat         , { LLDB_INVALID_REGNUM, LLDB_INVALID_REGNUM,    LLDB_INVALID_REGNUM,        LLDB_INVALID_REGNUM,    LLDB_INVALID_REGNUM }, NULL, NULL },
    {   "d1",      NULL,     8, 0, eEncodingIEEE754 , eFormatFloat         , { LLDB_INVALID_REGNUM, LLDB_INVALID_REGNUM,    LLDB_INVALID_REGNUM,        LLDB_INVALID_REGNUM,    LLDB_INVALID_REGNUM }, NULL, NULL },
    {   "d2",      NULL,     8, 0, eEncodingIEEE754 , eFormatFloat         , { LLDB_INVALID_REGNUM, LLDB_INVALID_REGNUM,    LLDB_INVALID_REGNUM,        LLDB_INVALID_REGNUM,    LLDB_INVALID_REGNUM }, NULL, NULL },
    {   "d3",      NULL,     8, 0, eEncodingIEEE754 , eFormatFloat         , { LLDB_INVALID_REGNUM, LLDB_INVALID_REGNUM,    LLDB_INVALID_REGNUM,        LLDB_INVALID_REGNUM,    LLDB_INVALID_REGNUM }, NULL, NULL },
    {   "d4",      NULL,     8, 0, eEncodingIEEE754 , eFormatFloat         , { LLDB_INVALID_REGNUM, LLDB_INVALID_REGNUM,    LLDB_INVALID_REGNUM,        LLDB_INVALID_REGNUM,    LLDB_INVALID_REGNUM }, NULL, NULL },
    {   "d5",      NULL,     8, 0, eEncodingIEEE754 , eFormatFloat         , { LLDB_INVALID_REGNUM, LLDB_INVALID_REGNUM,    LLDB_INVALID_REGNUM,        LLDB_INVALID_REGNUM,    LLDB_INVALID_REGNUM }, NULL, NULL },
    {   "d6",      NULL,     8, 0, eEncodingIEEE754 , eFormatFloat         , { LLDB_INVALID_REGNUM, LLDB_INVALID_REGNUM,    LLDB_INVALID_REGNUM,        LLDB_INVALID_REGNUM,    LLDB_INVALID_REGNUM }, NULL, NULL },
    {   "d7",      NULL,     8, 0, eEncodingIEEE754 , eFormatFloat         , { LLDB_INVALID_REGNUM, LLDB_INVALID_REGNUM,    LLDB_INVALID_REGNUM,        LLDB_INVALID_REGNUM,    LLDB_INVALID_REGNUM }, NULL, NULL },
    {   "d8",      NULL,     8, 0, eEncodingIEEE754 , eFormatFloat         , { LLDB_INVALID_REGNUM, LLDB_INVALID_REGNUM,    LLDB_INVALID_REGNUM,        LLDB_INVALID_REGNUM,    LLDB_INVALID_REGNUM }, NULL, NULL },
    {   "d9",      NULL,     8, 0, eEncodingIEEE754 , eFormatFloat         , { LLDB_INVALID_REGNUM, LLDB_INVALID_REGNUM,    LLDB_INVALID_REGNUM,        LLDB_INVALID_REGNUM,    LLDB_INVALID_REGNUM }, NULL, NULL },
    {   "d10",     NULL,     8, 0, eEncodingIEEE754 , eFormatFloat         , { LLDB_INVALID_REGNUM, LLDB_INVALID_REGNUM,    LLDB_INVALID_REGNUM,        LLDB_INVALID_REGNUM,    LLDB_INVALID_REGNUM }, NULL, NULL },
    {   "d11",     NULL,     8, 0, eEncodingIEEE754 , eFormatFloat         , { LLDB_INVALID_REGNUM, LLDB_INVALID_REGNUM,    LLDB_INVALID_REGNUM,        LLDB_INVALID_REGNUM,    LLDB_INVALID_REGNUM }, NULL, NULL },
    {   "d12",     NULL,     8, 0, eEncodingIEEE754 , eFormatFloat         , { LLDB_INVALID_REGNUM, LLDB_INVALID_REGNUM,    LLDB_INVALID_REGNUM,        LLDB_INVALID_REGNUM,    LLDB_INVALID_REGNUM }, NULL, NULL },
    {   "d13",     NULL,     8, 0, eEncodingIEEE754 , eFormatFloat         , { LLDB_INVALID_REGNUM, LLDB_INVALID_REGNUM,    LLDB_INVALID_REGNUM,        LLDB_INVALID_REGNUM,    LLDB_INVALID_REGNUM }, NULL, NULL },
    {   "d14",     NULL,     8, 0, eEncodingIEEE754 , eFormatFloat         , { LLDB_INVALID_REGNUM, LLDB_INVALID_REGNUM,    LLDB_INVALID_REGNUM,        LLDB_INVALID_REGNUM,    LLDB_INVALID_REGNUM }, NULL, NULL },
    {   "d15",     NULL,     8, 0, eEncodingIEEE754 , eFormatFloat         , { LLDB_INVALID_REGNUM, LLDB_INVALID_REGNUM,    LLDB_INVALID_REGNUM,        LLDB_INVALID_REGNUM,    LLDB_INVALID_REGNUM }, NULL, NULL },
    {   "d16",     NULL,     8, 0, eEncodingIEEE754 , eFormatFloat         , { LLDB_INVALID_REGNUM, LLDB_INVALID_REGNUM,    LLDB_INVALID_REGNUM,        LLDB_INVALID_REGNUM,    LLDB_INVALID_REGNUM }, NULL, NULL },
    {   "d17",     NULL,     8, 0, eEncodingIEEE754 , eFormatFloat         , { LLDB_INVALID_REGNUM, LLDB_INVALID_REGNUM,    LLDB_INVALID_REGNUM,        LLDB_INVALID_REGNUM,    LLDB_INVALID_REGNUM }, NULL, NULL },
    {   "d18",     NULL,     8, 0, eEncodingIEEE754 , eFormatFloat         , { LLDB_INVALID_REGNUM, LLDB_INVALID_REGNUM,    LLDB_INVALID_REGNUM,        LLDB_INVALID_REGNUM,    LLDB_INVALID_REGNUM }, NULL, NULL },
    {   "d19",     NULL,     8, 0, eEncodingIEEE754 , eFormatFloat         , { LLDB_INVALID_REGNUM, LLDB_INVALID_REGNUM,    LLDB_INVALID_REGNUM,        LLDB_INVALID_REGNUM,    LLDB_INVALID_REGNUM }, NULL, NULL },
    {   "d20",     NULL,     8, 0, eEncodingIEEE754 , eFormatFloat         , { LLDB_INVALID_REGNUM, LLDB_INVALID_REGNUM,    LLDB_INVALID_REGNUM,        LLDB_INVALID_REGNUM,    LLDB_INVALID_REGNUM }, NULL, NULL },
    {   "d21",     NULL,     8, 0, eEncodingIEEE754 , eFormatFloat         , { LLDB_INVALID_REGNUM, LLDB_INVALID_REGNUM,    LLDB_INVALID_REGNUM,        LLDB_INVALID_REGNUM,    LLDB_INVALID_REGNUM }, NULL, NULL },
    {   "d22",     NULL,     8, 0, eEncodingIEEE754 , eFormatFloat         , { LLDB_INVALID_REGNUM, LLDB_INVALID_REGNUM,    LLDB_INVALID_REGNUM,        LLDB_INVALID_REGNUM,    LLDB_INVALID_REGNUM }, NULL, NULL },
    {   "d23",     NULL,     8, 0, eEncodingIEEE754 , eFormatFloat         , { LLDB_INVALID_REGNUM, LLDB_INVALID_REGNUM,    LLDB_INVALID_REGNUM,        LLDB_INVALID_REGNUM,    LLDB_INVALID_REGNUM }, NULL, NULL },
    {   "d24",     NULL,     8, 0, eEncodingIEEE754 , eFormatFloat         , { LLDB_INVALID_REGNUM, LLDB_INVALID_REGNUM,    LLDB_INVALID_REGNUM,        LLDB_INVALID_REGNUM,    LLDB_INVALID_REGNUM }, NULL, NULL },
    {   "d25",     NULL,     8, 0, eEncodingIEEE754 , eFormatFloat         , { LLDB_INVALID_REGNUM, LLDB_INVALID_REGNUM,    LLDB_INVALID_REGNUM,        LLDB_INVALID_REGNUM,    LLDB_INVALID_REGNUM }, NULL, NULL },
    {   "d26",     NULL,     8, 0, eEncodingIEEE754 , eFormatFloat         , { LLDB_INVALID_REGNUM, LLDB_INVALID_REGNUM,    LLDB_INVALID_REGNUM,        LLDB_INVALID_REGNUM,    LLDB_INVALID_REGNUM }, NULL, NULL },
    {   "d27",     NULL,     8, 0, eEncodingIEEE754 , eFormatFloat         , { LLDB_INVALID_REGNUM, LLDB_INVALID_REGNUM,    LLDB_INVALID_REGNUM,        LLDB_INVALID_REGNUM,    LLDB_INVALID_REGNUM }, NULL, NULL },
    {   "d28",     NULL,     8, 0, eEncodingIEEE754 , eFormatFloat         , { LLDB_INVALID_REGNUM, LLDB_INVALID_REGNUM,    LLDB_INVALID_REGNUM,        LLDB_INVALID_REGNUM,    LLDB_INVALID_REGNUM }, NULL, NULL },
    {   "d29",     NULL,     8, 0, eEncodingIEEE754 , eFormatFloat         , { LLDB_INVALID_REGNUM, LLDB_INVALID_REGNUM,    LLDB_INVALID_REGNUM,        LLDB_INVALID_REGNUM,    LLDB_INVALID_REGNUM }, NULL, NULL },
    {   "d30",     NULL,     8, 0, eEncodingIEEE754 , eFormatFloat         , { LLDB_INVALID_REGNUM, LLDB_INVALID_REGNUM,    LLDB_INVALID_REGNUM,        LLDB_INVALID_REGNUM,    LLDB_INVALID_REGNUM }, NULL, NULL },
    {   "d31",     NULL,     8, 0, eEncodingIEEE754 , eFormatFloat         , { LLDB_INVALID_REGNUM, LLDB_INVALID_REGNUM,    LLDB_INVALID_REGNUM,        LLDB_INVALID_REGNUM,    LLDB_INVALID_REGNUM }, NULL, NULL }
};

static const uint32_t k_num_register_infos = llvm::array_lengthof(g_register_infos);
static bool g_register_info_names_constified = false;

const lldb_private::RegisterInfo *
ABISysV_arm64::GetRegisterInfoArray (uint32_t &count)
{
    // Make the C-string names and alt_names for the register infos into const 
    // C-string values by having the ConstString unique the names in the global
    // constant C-string pool.
    if (!g_register_info_names_constified)
    {
        g_register_info_names_constified = true;
        for (uint32_t i=0; i<k_num_register_infos; ++i)
        {
            if (g_register_infos[i].name)
                g_register_infos[i].name = ConstString(g_register_infos[i].name).GetCString();
            if (g_register_infos[i].alt_name)
                g_register_infos[i].alt_name = ConstString(g_register_infos[i].alt_name).GetCString();
        }
    }
    count = k_num_register_infos;
    return g_register_infos;
}

size_t
ABISysV_arm64::GetRedZoneSize () const
{
    return 128;
}

//------------------------------------------------------------------
// Static Functions
//------------------------------------------------------------------
ABISP
ABISysV_arm64::CreateInstance (const ArchSpec &arch)
{
    static ABISP g_abi_sp;
    const llvm::Triple::ArchType arch_type = arch.GetTriple().getArch();
    const llvm::Triple::VendorType vendor_type = arch.GetTriple().getVendor();

    if (vendor_type != llvm::Triple::Apple)
    {
	    if (arch_type == llvm::Triple::aarch64)
        {
            if (!g_abi_sp)
                g_abi_sp.reset (new ABISysV_arm64);
            return g_abi_sp;
        }
    }

    return ABISP();
}

bool
ABISysV_arm64::PrepareTrivialCall (Thread &thread,
                                   addr_t sp, 
                                   addr_t func_addr, 
                                   addr_t return_addr, 
                                   llvm::ArrayRef<addr_t> args) const
{
    RegisterContext *reg_ctx = thread.GetRegisterContext().get();
    if (!reg_ctx)
        return false;

    Log *log(lldb_private::GetLogIfAllCategoriesSet (LIBLLDB_LOG_EXPRESSIONS));

    if (log)
    {
        StreamString s;
        s.Printf("ABISysV_x86_64::PrepareTrivialCall (tid = 0x%" PRIx64 ", sp = 0x%" PRIx64 ", func_addr = 0x%" PRIx64 ", return_addr = 0x%" PRIx64,
                 thread.GetID(),
                 (uint64_t)sp,
                 (uint64_t)func_addr,
                 (uint64_t)return_addr);

        for (size_t i = 0; i < args.size(); ++i)
            s.Printf (", arg%d = 0x%" PRIx64, static_cast<int>(i + 1), args[i]);
        s.PutCString (")");
        log->PutCString(s.GetString().c_str());
    }

    // x0 - x7 contain first 8 simple args
    if (args.size() > 8)
        return false;

    for (size_t i = 0; i < args.size(); ++i)
    {
        const RegisterInfo *reg_info = reg_ctx->GetRegisterInfo(eRegisterKindGeneric, LLDB_REGNUM_GENERIC_ARG1 + i);
        if (log)
            log->Printf("About to write arg%d (0x%" PRIx64 ") into %s",
                        static_cast<int>(i + 1), args[i], reg_info->name);
        if (!reg_ctx->WriteRegisterFromUnsigned(reg_info, args[i]))
            return false;
    }

    // Set "lr" to the return address
    if (!reg_ctx->WriteRegisterFromUnsigned (reg_ctx->GetRegisterInfo(eRegisterKindGeneric, LLDB_REGNUM_GENERIC_RA), return_addr))
        return false;

    // Set "sp" to the requested value
    if (!reg_ctx->WriteRegisterFromUnsigned (reg_ctx->GetRegisterInfo(eRegisterKindGeneric, LLDB_REGNUM_GENERIC_SP), sp))
        return false;

    // Set "pc" to the address requested
    if (!reg_ctx->WriteRegisterFromUnsigned (reg_ctx->GetRegisterInfo(eRegisterKindGeneric, LLDB_REGNUM_GENERIC_PC), func_addr))
        return false;

    return true;
}

//TODO: We dont support fp/SIMD arguments in v0-v7
bool
ABISysV_arm64::GetArgumentValues (Thread &thread, ValueList &values) const
{
    uint32_t num_values = values.GetSize();
    
    ExecutionContext exe_ctx (thread.shared_from_this());
    
    // Extract the register context so we can read arguments from registers
    
    RegisterContext *reg_ctx = thread.GetRegisterContext().get();
    
    if (!reg_ctx)
        return false;

    addr_t sp = 0;

    for (uint32_t value_idx = 0; value_idx < num_values; ++value_idx)
    {
        // We currently only support extracting values with Clang QualTypes.
        // Do we care about others?
        Value *value = values.GetValueAtIndex(value_idx);
        
        if (!value)
            return false;
        
        CompilerType value_type = value->GetCompilerType();
        if (value_type)
        {
            bool is_signed = false;
            size_t bit_width = 0;
            if (value_type.IsIntegerType (is_signed))
            {
                bit_width = value_type.GetBitSize(&thread);
            }
            else if (value_type.IsPointerOrReferenceType ())
            {
                bit_width = value_type.GetBitSize(&thread);
            }
            else
            {
                // We only handle integer, pointer and reference types currently...
                return false;
            }
            
            if (bit_width <= (exe_ctx.GetProcessRef().GetAddressByteSize() * 8))
            {
                if (value_idx < 8)
                {
                    // Arguments 1-8 are in x0-x7...
                    const RegisterInfo *reg_info = NULL;
                    reg_info= reg_ctx->GetRegisterInfo (eRegisterKindGeneric, LLDB_REGNUM_GENERIC_ARG1 + value_idx);
                    
                    if (reg_info)
                    {
                        RegisterValue reg_value;
                        
                        if (reg_ctx->ReadRegister(reg_info, reg_value))
                        {
                            if (is_signed)
                                reg_value.SignExtend(bit_width);
                            if (!reg_value.GetScalarValue(value->GetScalar()))
                                return false;
                            continue;
                        }
                    }
                    return false;
                }
                else
                {
                    //TODO: Verify for stack layout for SysV
                    if (sp == 0)
                    {
                        // Read the stack pointer if we already haven't read it
                        sp = reg_ctx->GetSP(0);
                        if (sp == 0)
                            return false;
                    }

                    // Arguments 5 on up are on the stack
                    const uint32_t arg_byte_size = (bit_width + (8-1)) / 8;
                    Error error;
                    if (!exe_ctx.GetProcessRef().ReadScalarIntegerFromMemory(sp, arg_byte_size, is_signed, value->GetScalar(), error))
                        return false;
                    
                    sp += arg_byte_size;
                    // Align up to the next 8 byte boundary if needed
                    if (sp % 8)
                    {
                        sp >>= 3;
                        sp += 1;
                        sp <<= 3;
                    }
                }
            }
        }
    }
    return true;
}

Error
ABISysV_arm64::SetReturnValueObject(lldb::StackFrameSP &frame_sp, lldb::ValueObjectSP &new_value_sp)
{
    Error error;
    if (!new_value_sp)
    {
        error.SetErrorString("Empty value object for return value.");
        return error;
    }
    
    CompilerType return_value_type = new_value_sp->GetCompilerType();
    if (!return_value_type)
    {
        error.SetErrorString ("Null clang type for return value.");
        return error;
    }

    Thread *thread = frame_sp->GetThread().get();
    
    RegisterContext *reg_ctx = thread->GetRegisterContext().get();
    
    if (reg_ctx)
    {
        DataExtractor data;
        Error data_error;
        const uint64_t byte_size = new_value_sp->GetData(data, data_error);
        if (data_error.Fail())
        {
            error.SetErrorStringWithFormat("Couldn't convert return value to raw data: %s", data_error.AsCString());
            return error;
        }

        const uint32_t type_flags = return_value_type.GetTypeInfo (NULL);
        if (type_flags & eTypeIsScalar ||
            type_flags & eTypeIsPointer)
        {
            if (type_flags & eTypeIsInteger ||
                type_flags & eTypeIsPointer )
            {
                // Extract the register context so we can read arguments from registers
                lldb::offset_t offset = 0;
                if (byte_size <= 16)
                {
                    const RegisterInfo *x0_info = reg_ctx->GetRegisterInfo (eRegisterKindGeneric, LLDB_REGNUM_GENERIC_ARG1);
                    if (byte_size <= 8)
                    {
                        uint64_t raw_value = data.GetMaxU64(&offset, byte_size);
                        
                        if (!reg_ctx->WriteRegisterFromUnsigned (x0_info, raw_value))
                            error.SetErrorString ("failed to write register x0");
                    }
                    else
                    {
                        uint64_t raw_value = data.GetMaxU64(&offset, 8);
                        
                        if (reg_ctx->WriteRegisterFromUnsigned (x0_info, raw_value))
                        {
                            const RegisterInfo *x1_info = reg_ctx->GetRegisterInfo (eRegisterKindGeneric, LLDB_REGNUM_GENERIC_ARG2);
                            raw_value = data.GetMaxU64(&offset, byte_size - offset);
                            
                            if (!reg_ctx->WriteRegisterFromUnsigned (x1_info, raw_value))
                                error.SetErrorString ("failed to write register x1");
                        }
                    }
                }
                else
                {
                    error.SetErrorString("We don't support returning longer than 128 bit integer values at present.");
                }
            }
            else if (type_flags & eTypeIsFloat)
            {
                if (type_flags & eTypeIsComplex)
                {
                    // Don't handle complex yet.
                    error.SetErrorString ("returning complex float values are not supported");
                }
                else
                {
                    const RegisterInfo *v0_info = reg_ctx->GetRegisterInfoByName("v0", 0);

                    if (v0_info)
                    {
                        if (byte_size <= 16)
                        {
                            if (byte_size <= RegisterValue::GetMaxByteSize())
                            {
                                RegisterValue reg_value;
                                error = reg_value.SetValueFromData (v0_info, data, 0, true);
                                if (error.Success())
                                {
                                    if (!reg_ctx->WriteRegister (v0_info, reg_value))
                                        error.SetErrorString ("failed to write register v0");
                                }
                            }
                            else
                            {
                                error.SetErrorStringWithFormat ("returning float values with a byte size of %" PRIu64 " are not supported", byte_size);
                            }
                        }
                        else
                        {
                            error.SetErrorString("returning float values longer than 128 bits are not supported");            
                        }
                    }
                    else
                    {
                        error.SetErrorString("v0 register is not available on this target");
                    }
                }
            }
        }
        else if (type_flags & eTypeIsVector)
        {
            if (byte_size > 0)
            {
                const RegisterInfo *v0_info = reg_ctx->GetRegisterInfoByName("v0", 0);
                
                if (v0_info)
                {
                    if (byte_size <= v0_info->byte_size)
                    {
                        RegisterValue reg_value;
                        error = reg_value.SetValueFromData (v0_info, data, 0, true);
                        if (error.Success())
                        {
                            if (!reg_ctx->WriteRegister (v0_info, reg_value))
                                error.SetErrorString ("failed to write register v0");
                        }
                    }
                }
            }
        }
    }
    else
    {
        error.SetErrorString("no registers are available");        
    }
    
    return error;
}

bool
ABISysV_arm64::CreateFunctionEntryUnwindPlan (UnwindPlan &unwind_plan)
{
    unwind_plan.Clear();
    unwind_plan.SetRegisterKind (eRegisterKindDWARF);
    
    uint32_t lr_reg_num = arm64_dwarf::lr;
    uint32_t sp_reg_num = arm64_dwarf::sp;
    uint32_t pc_reg_num = arm64_dwarf::pc;
    
    UnwindPlan::RowSP row(new UnwindPlan::Row);
    
    // Our previous Call Frame Address is the stack pointer
    row->GetCFAValue().SetIsRegisterPlusOffset (sp_reg_num, 0);
    
    // Our previous PC is in the LR
    row->SetRegisterLocationToRegister(pc_reg_num, lr_reg_num, true);
    
    unwind_plan.AppendRow (row);
    
    // All other registers are the same.
    
    unwind_plan.SetSourceName ("arm64 at-func-entry default");
    unwind_plan.SetSourcedFromCompiler (eLazyBoolNo);

    return true;
}

bool
ABISysV_arm64::CreateDefaultUnwindPlan (UnwindPlan &unwind_plan)
{
    unwind_plan.Clear();
    unwind_plan.SetRegisterKind (eRegisterKindDWARF);
    
    uint32_t fp_reg_num = arm64_dwarf::fp;
    uint32_t pc_reg_num = arm64_dwarf::pc;

    UnwindPlan::RowSP row(new UnwindPlan::Row);    
    const int32_t ptr_size = 8;
    
    row->GetCFAValue().SetIsRegisterPlusOffset (fp_reg_num, 2 * ptr_size);
    row->SetOffset (0);
    
    row->SetRegisterLocationToAtCFAPlusOffset(fp_reg_num, ptr_size * -2, true);
    row->SetRegisterLocationToAtCFAPlusOffset(pc_reg_num, ptr_size * -1, true);
    
    unwind_plan.AppendRow (row);
    unwind_plan.SetSourceName ("arm64 default unwind plan");
    unwind_plan.SetSourcedFromCompiler (eLazyBoolNo);
    unwind_plan.SetUnwindPlanValidAtAllInstructions (eLazyBoolNo);

    return true;
}

// AAPCS64 (Procedure Call Standard for the ARM 64-bit Architecture) says 
// registers x19 through x28 and sp are callee preserved.  
// v8-v15 are non-volatile (and specifically only the lower 8 bytes of these regs),
// the rest of the fp/SIMD registers are volatile.

// We treat x29 as callee preserved also, else the unwinder won't try to
// retrieve fp saves.

bool
ABISysV_arm64::RegisterIsVolatile (const RegisterInfo *reg_info)
{
    if (reg_info)
    {
        const char *name = reg_info->name;

        // Sometimes we'll be called with the "alternate" name for these registers;
        // recognize them as non-volatile.

        if (name[0] == 'p' && name[1] == 'c')        // pc
            return false;
        if (name[0] == 'f' && name[1] == 'p')        // fp
            return false;
        if (name[0] == 's' && name[1] == 'p')        // sp
            return false;
        if (name[0] == 'l' && name[1] == 'r')        // lr
            return false;

        if (name[0] == 'x')
        {
            // Volatile registers: x0-x18
            // Although documentation says only x19-28 + sp are callee saved
            // We ll also have to treat x30 as non-volatile.
            // Each dwarf frame has its own value of lr.
            // Return false for the non-volatile gpr regs, true for everything else
            switch (name[1])
            {
                case '1':
                    switch (name[2])
                    {
                        case '9': 
                            return false;             // x19 is non-volatile
                        default:
                          return true;
                    }
                        break;
                case '2':
                    switch (name[2])
                    {
                        case '0':
                        case '1': 
                        case '2':
                        case '3':
                        case '4':
                        case '5':
                        case '6':
                        case '7':
                        case '8': 
                            return false;             // x20 - 28 are non-volatile
                        case '9': 
                            return false;             // x29 aka fp treat as non-volatile
                        default:
                            return true;
                    }
                case '3':                             // x30 (lr) and x31 (sp) treat as non-volatile
                    if (name[2] == '0' || name[2] == '1')
                      return false;
                default:
                    return true;                      // all volatile cases not handled above fall here.
            }
        }
        else if (name[0] == 'v' || name[0] == 's' || name[0] == 'd')
        {
            // Volatile registers: v0-7, v16-v31
            // Return false for non-volatile fp/SIMD regs, true for everything else
            switch (name[1])
            {
                case '8':
                case '9':
                    return false; // v8-v9 are non-volatile
                case '1':
                    switch (name[2])
                    {
                        case '0':
                        case '1':
                        case '2':
                        case '3':
                        case '4':
                        case '5':
                            return false; // v10-v15 are non-volatile
                        default:
                            return true;
                    }
                default:
                    return true;
            }
        }
    }
    return true;
}

static bool
LoadValueFromConsecutiveGPRRegisters (ExecutionContext &exe_ctx,
                                      RegisterContext *reg_ctx,
                                      const CompilerType &value_type,
                                      bool is_return_value, // false => parameter, true => return value
                                      uint32_t &NGRN,       // NGRN (see ABI documentation)
                                      uint32_t &NSRN,       // NSRN (see ABI documentation)
                                      DataExtractor &data)
{
    const size_t byte_size = value_type.GetByteSize(nullptr);
    
    if (byte_size == 0)
        return false;

    std::unique_ptr<DataBufferHeap> heap_data_ap (new DataBufferHeap(byte_size, 0));
    const ByteOrder byte_order = exe_ctx.GetProcessRef().GetByteOrder();
    Error error;

    CompilerType base_type;
    const uint32_t homogeneous_count = value_type.IsHomogeneousAggregate (&base_type);
    if (homogeneous_count > 0 && homogeneous_count <= 8)
    {
        // Make sure we have enough registers
        if (NSRN < 8 && (8-NSRN) >= homogeneous_count)
        {
            if (!base_type)
                return false;
            const size_t base_byte_size = base_type.GetByteSize(nullptr);
            uint32_t data_offset = 0;

            for (uint32_t i=0; i<homogeneous_count; ++i)
            {
                char v_name[8];
                ::snprintf (v_name, sizeof(v_name), "v%u", NSRN);
                const RegisterInfo *reg_info = reg_ctx->GetRegisterInfoByName(v_name, 0);
                if (reg_info == NULL)
                    return false;
                
                if (base_byte_size > reg_info->byte_size)
                    return false;
                
                RegisterValue reg_value;
                
                if (!reg_ctx->ReadRegister(reg_info, reg_value))
                    return false;
                
                // Make sure we have enough room in "heap_data_ap"
                if ((data_offset + base_byte_size) <= heap_data_ap->GetByteSize())
                {
                    const size_t bytes_copied = reg_value.GetAsMemoryData (reg_info,
                                                                           heap_data_ap->GetBytes()+data_offset,
                                                                           base_byte_size,
                                                                           byte_order,
                                                                           error);
                    if (bytes_copied != base_byte_size)
                        return false;
                    data_offset += bytes_copied;
                    ++NSRN;
                }
                else
                    return false;
            }
            data.SetByteOrder(byte_order);
            data.SetAddressByteSize(exe_ctx.GetProcessRef().GetAddressByteSize());
            data.SetData(DataBufferSP (heap_data_ap.release()));
            return true;
        }
    }

    const size_t max_reg_byte_size = 16;
    if (byte_size <= max_reg_byte_size)
    {
        size_t bytes_left = byte_size;
        uint32_t data_offset = 0;
        while (data_offset < byte_size)
        {
            if (NGRN >= 8)
                return false;

            const RegisterInfo *reg_info = reg_ctx->GetRegisterInfo (eRegisterKindGeneric, LLDB_REGNUM_GENERIC_ARG1 + NGRN);
            if (reg_info == NULL)
                return false;

            RegisterValue reg_value;
            
            if (!reg_ctx->ReadRegister(reg_info, reg_value))
                return false;
            
            const size_t curr_byte_size = std::min<size_t>(8,bytes_left);
            const size_t bytes_copied = reg_value.GetAsMemoryData (reg_info, heap_data_ap->GetBytes()+data_offset, curr_byte_size, byte_order, error);
            if (bytes_copied == 0)
                return false;
            if (bytes_copied >= bytes_left)
                break;
            data_offset += bytes_copied;
            bytes_left -= bytes_copied;
            ++NGRN;
        }
    }
    else
    {        
        const RegisterInfo *reg_info = NULL;
        if (is_return_value)
        {
            // We are assuming we are decoding this immediately after returning
            // from a function call and that the address of the structure is in x8
            reg_info = reg_ctx->GetRegisterInfoByName("x8", 0);
        }
        else
        {
            // We are assuming we are stopped at the first instruction in a function
            // and that the ABI is being respected so all parameters appear where they
            // should be (functions with no external linkage can legally violate the ABI).
            if (NGRN >= 8)
                return false;
            
            reg_info = reg_ctx->GetRegisterInfo (eRegisterKindGeneric, LLDB_REGNUM_GENERIC_ARG1 + NGRN);
            if (reg_info == NULL)
                return false;
            ++NGRN;
        }

        if (reg_info == NULL)
            return false;

        const lldb::addr_t value_addr = reg_ctx->ReadRegisterAsUnsigned(reg_info, LLDB_INVALID_ADDRESS);

        if (value_addr == LLDB_INVALID_ADDRESS)
            return false;

        if (exe_ctx.GetProcessRef().ReadMemory (value_addr,
                                                heap_data_ap->GetBytes(),
                                                heap_data_ap->GetByteSize(),
                                                error) != heap_data_ap->GetByteSize())
        {
            return false;
        }
    }

    data.SetByteOrder(byte_order);
    data.SetAddressByteSize(exe_ctx.GetProcessRef().GetAddressByteSize());
    data.SetData(DataBufferSP (heap_data_ap.release()));
    return true;
}

ValueObjectSP
ABISysV_arm64::GetReturnValueObjectImpl (Thread &thread, CompilerType &return_compiler_type) const
{
    ValueObjectSP return_valobj_sp;
    Value value;
    
    ExecutionContext exe_ctx (thread.shared_from_this());
    if (exe_ctx.GetTargetPtr() == NULL || exe_ctx.GetProcessPtr() == NULL)
        return return_valobj_sp;

    //value.SetContext (Value::eContextTypeClangType, return_compiler_type);
    value.SetCompilerType(return_compiler_type);
    
    RegisterContext *reg_ctx = thread.GetRegisterContext().get();
    if (!reg_ctx)
        return return_valobj_sp;
    
    const size_t byte_size = return_compiler_type.GetByteSize(nullptr);

    const uint32_t type_flags = return_compiler_type.GetTypeInfo (NULL);
    if (type_flags & eTypeIsScalar ||
        type_flags & eTypeIsPointer)
    {
        value.SetValueType(Value::eValueTypeScalar);
        
        bool success = false;
        if (type_flags & eTypeIsInteger ||
            type_flags & eTypeIsPointer )
        {
            // Extract the register context so we can read arguments from registers
            if (byte_size <= 8)
            {
                const RegisterInfo *x0_reg_info = NULL;
                x0_reg_info = reg_ctx->GetRegisterInfo (eRegisterKindGeneric, LLDB_REGNUM_GENERIC_ARG1);
                if (x0_reg_info)
                {
                    uint64_t raw_value = thread.GetRegisterContext()->ReadRegisterAsUnsigned(x0_reg_info, 0);
                    const bool is_signed = (type_flags & eTypeIsSigned) != 0;
                    switch (byte_size)
                    {
                        default:
                            break;
                        case 16: // uint128_t
                            // In register x0 and x1
                            {
                                const RegisterInfo *x1_reg_info = NULL;
                                x1_reg_info = reg_ctx->GetRegisterInfo (eRegisterKindGeneric, LLDB_REGNUM_GENERIC_ARG2);

                                if (x1_reg_info)
                                {
                                    if (byte_size <= x0_reg_info->byte_size + x1_reg_info->byte_size)
                                    {
                                        std::unique_ptr<DataBufferHeap> heap_data_ap (new DataBufferHeap(byte_size, 0));
                                        const ByteOrder byte_order = exe_ctx.GetProcessRef().GetByteOrder();
                                        RegisterValue x0_reg_value;
                                        RegisterValue x1_reg_value;
                                        if (reg_ctx->ReadRegister(x0_reg_info, x0_reg_value) &&
                                            reg_ctx->ReadRegister(x1_reg_info, x1_reg_value))
                                        {
                                            Error error;
                                            if (x0_reg_value.GetAsMemoryData (x0_reg_info, heap_data_ap->GetBytes()+0, 8, byte_order, error) &&
                                                x1_reg_value.GetAsMemoryData (x1_reg_info, heap_data_ap->GetBytes()+8, 8, byte_order, error))
                                            {
                                                DataExtractor data (DataBufferSP (heap_data_ap.release()),
                                                                    byte_order,
                                                                    exe_ctx.GetProcessRef().GetAddressByteSize());
                                                
                                                return_valobj_sp = ValueObjectConstResult::Create (&thread,
                                                                                                   return_compiler_type,
                                                                                                   ConstString(""),
                                                                                                   data);
                                                return return_valobj_sp;
                                            }
                                        }
                                    }
                                }
                            }
                            break;
                        case sizeof(uint64_t):
                            if (is_signed)
                                value.GetScalar() = (int64_t)(raw_value);
                            else
                                value.GetScalar() = (uint64_t)(raw_value);
                            success = true;
                            break;
                            
                        case sizeof(uint32_t):
                            if (is_signed)
                                value.GetScalar() = (int32_t)(raw_value & UINT32_MAX);
                            else
                                value.GetScalar() = (uint32_t)(raw_value & UINT32_MAX);
                            success = true;
                            break;
                            
                        case sizeof(uint16_t):
                            if (is_signed)
                                value.GetScalar() = (int16_t)(raw_value & UINT16_MAX);
                            else
                                value.GetScalar() = (uint16_t)(raw_value & UINT16_MAX);
                            success = true;
                            break;
                            
                        case sizeof(uint8_t):
                            if (is_signed)
                                value.GetScalar() = (int8_t)(raw_value & UINT8_MAX);
                            else
                                value.GetScalar() = (uint8_t)(raw_value & UINT8_MAX);
                            success = true;
                            break;
                    }
                }
            }
        }
        else if (type_flags & eTypeIsFloat)
        {
            if (type_flags & eTypeIsComplex)
            {
                // Don't handle complex yet.
            }
            else
            {
                if (byte_size <= sizeof(long double))
                {
                    const RegisterInfo *v0_reg_info = reg_ctx->GetRegisterInfoByName("v0", 0);
                    RegisterValue v0_value;
                    if (reg_ctx->ReadRegister (v0_reg_info, v0_value))
                    {
                        DataExtractor data;
                        if (v0_value.GetData(data))
                        {
                            lldb::offset_t offset = 0;
                            if (byte_size == sizeof(float))
                            {
                                value.GetScalar() = data.GetFloat(&offset);
                                success = true;
                            }
                            else if (byte_size == sizeof(double))
                            {
                                value.GetScalar() = data.GetDouble(&offset);
                                success = true;
                            }
                            else if (byte_size == sizeof(long double))
                            {
                                value.GetScalar() = data.GetLongDouble(&offset);
                                success = true;
                            }
                        }
                    }
                }
            }
        }
        
        if (success)
            return_valobj_sp = ValueObjectConstResult::Create (thread.GetStackFrameAtIndex(0).get(),
                                                               value,
                                                               ConstString(""));
        
    }
    else if (type_flags & eTypeIsVector)
    {
        if (byte_size > 0)
        {
            
            const RegisterInfo *v0_info = reg_ctx->GetRegisterInfoByName("v0", 0);
            
            if (v0_info)
            {
                if (byte_size <= v0_info->byte_size)
                {
                    std::unique_ptr<DataBufferHeap> heap_data_ap (new DataBufferHeap(byte_size, 0));
                    const ByteOrder byte_order = exe_ctx.GetProcessRef().GetByteOrder();
                    RegisterValue reg_value;
                    if (reg_ctx->ReadRegister(v0_info, reg_value))
                    {
                        Error error;
                        if (reg_value.GetAsMemoryData (v0_info,
                                                       heap_data_ap->GetBytes(),
                                                       heap_data_ap->GetByteSize(),
                                                       byte_order,
                                                       error))
                        {
                            DataExtractor data (DataBufferSP (heap_data_ap.release()),
                                                byte_order,
                                                exe_ctx.GetProcessRef().GetAddressByteSize());
                            return_valobj_sp = ValueObjectConstResult::Create (&thread,
                                                                               return_compiler_type,
                                                                               ConstString(""),
                                                                               data);
                        }
                    }
                }
            }
        }
    }
    else if (type_flags & eTypeIsStructUnion ||
             type_flags & eTypeIsClass)
    {
        DataExtractor data;
        
        uint32_t NGRN = 0;  // Search ABI docs for NGRN
        uint32_t NSRN = 0;  // Search ABI docs for NSRN
        const bool is_return_value = true;
        if (LoadValueFromConsecutiveGPRRegisters (exe_ctx, reg_ctx, return_compiler_type, is_return_value, NGRN, NSRN, data))
        {
            return_valobj_sp = ValueObjectConstResult::Create (&thread,
                                                               return_compiler_type,
                                                               ConstString(""),
                                                               data);            
        }
    }
    return return_valobj_sp;
}

void
ABISysV_arm64::Initialize()
{
    PluginManager::RegisterPlugin (GetPluginNameStatic(),
                                   "SysV ABI for AArch64 targets",
                                   CreateInstance);    
}

void
ABISysV_arm64::Terminate()
{
    PluginManager::UnregisterPlugin (CreateInstance);
}

lldb_private::ConstString
ABISysV_arm64::GetPluginNameStatic()
{
    static ConstString g_name("SysV-arm64");
    return g_name;
}

//------------------------------------------------------------------
// PluginInterface protocol
//------------------------------------------------------------------
ConstString
ABISysV_arm64::GetPluginName()
{
    return GetPluginNameStatic();
}

uint32_t
ABISysV_arm64::GetPluginVersion()
{
    return 1;
}

