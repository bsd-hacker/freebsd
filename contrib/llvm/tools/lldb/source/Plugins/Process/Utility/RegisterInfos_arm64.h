//===-- RegisterInfos_arm64.h -----------------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifdef DECLARE_REGISTER_INFOS_ARM64_STRUCT

// C Includes
#include <stddef.h>

// C++ Includes
// Other libraries and framework includes
// Project includes
#include "lldb/lldb-private.h"
#include "lldb/lldb-defines.h"
#include "lldb/lldb-enumerations.h"

#include "Utility/ARM64_DWARF_Registers.h"
#include "Utility/ARM64_ehframe_Registers.h"

#ifndef GPR_OFFSET
#error GPR_OFFSET must be defined before including this header file
#endif

#ifndef GPR_OFFSET_NAME
#error GPR_OFFSET_NAME must be defined before including this header file
#endif

#ifndef FPU_OFFSET
#error FPU_OFFSET must be defined before including this header file
#endif

#ifndef FPU_OFFSET_NAME
#error FPU_OFFSET_NAME must be defined before including this header file
#endif

#ifndef EXC_OFFSET_NAME
#error EXC_OFFSET_NAME must be defined before including this header file
#endif

#ifndef DBG_OFFSET_NAME
#error DBG_OFFSET_NAME must be defined before including this header file
#endif

#ifndef DEFINE_DBG
#error DEFINE_DBG must be defined before including this header file
#endif

enum
{
    gpr_x0 = 0,
    gpr_x1,
    gpr_x2,
    gpr_x3,
    gpr_x4,
    gpr_x5,
    gpr_x6,
    gpr_x7,
    gpr_x8,
    gpr_x9,
    gpr_x10,
    gpr_x11,
    gpr_x12,
    gpr_x13,
    gpr_x14,
    gpr_x15,
    gpr_x16,
    gpr_x17,
    gpr_x18,
    gpr_x19,
    gpr_x20,
    gpr_x21,
    gpr_x22,
    gpr_x23,
    gpr_x24,
    gpr_x25,
    gpr_x26,
    gpr_x27,
    gpr_x28,
    gpr_x29 = 29,  gpr_fp = gpr_x29,
    gpr_x30 = 30,  gpr_lr = gpr_x30,  gpr_ra = gpr_x30,
    gpr_x31 = 31,  gpr_sp = gpr_x31,
    gpr_pc = 32,
    gpr_cpsr,

    fpu_v0,
    fpu_v1,
    fpu_v2,
    fpu_v3,
    fpu_v4,
    fpu_v5,
    fpu_v6,
    fpu_v7,
    fpu_v8,
    fpu_v9,
    fpu_v10,
    fpu_v11,
    fpu_v12,
    fpu_v13,
    fpu_v14,
    fpu_v15,
    fpu_v16,
    fpu_v17,
    fpu_v18,
    fpu_v19,
    fpu_v20,
    fpu_v21,
    fpu_v22,
    fpu_v23,
    fpu_v24,
    fpu_v25,
    fpu_v26,
    fpu_v27,
    fpu_v28,
    fpu_v29,
    fpu_v30,
    fpu_v31,

    fpu_fpsr,
    fpu_fpcr,

    exc_far,
    exc_esr,
    exc_exception,

    dbg_bvr0,
    dbg_bvr1,
    dbg_bvr2,
    dbg_bvr3,
    dbg_bvr4,
    dbg_bvr5,
    dbg_bvr6,
    dbg_bvr7,
    dbg_bvr8,
    dbg_bvr9,
    dbg_bvr10,
    dbg_bvr11,
    dbg_bvr12,
    dbg_bvr13,
    dbg_bvr14,
    dbg_bvr15,

    dbg_bcr0,
    dbg_bcr1,
    dbg_bcr2,
    dbg_bcr3,
    dbg_bcr4,
    dbg_bcr5,
    dbg_bcr6,
    dbg_bcr7,
    dbg_bcr8,
    dbg_bcr9,
    dbg_bcr10,
    dbg_bcr11,
    dbg_bcr12,
    dbg_bcr13,
    dbg_bcr14,
    dbg_bcr15,

    dbg_wvr0,
    dbg_wvr1,
    dbg_wvr2,
    dbg_wvr3,
    dbg_wvr4,
    dbg_wvr5,
    dbg_wvr6,
    dbg_wvr7,
    dbg_wvr8,
    dbg_wvr9,
    dbg_wvr10,
    dbg_wvr11,
    dbg_wvr12,
    dbg_wvr13,
    dbg_wvr14,
    dbg_wvr15,

    dbg_wcr0,
    dbg_wcr1,
    dbg_wcr2,
    dbg_wcr3,
    dbg_wcr4,
    dbg_wcr5,
    dbg_wcr6,
    dbg_wcr7,
    dbg_wcr8,
    dbg_wcr9,
    dbg_wcr10,
    dbg_wcr11,
    dbg_wcr12,
    dbg_wcr13,
    dbg_wcr14,
    dbg_wcr15,

    k_num_registers
};

static lldb_private::RegisterInfo g_register_infos_arm64[] = {
// General purpose registers
//  NAME        ALT     SZ  OFFSET              ENCODING                FORMAT                 EH_FRAME                     DWARF                      GENERIC                     PROCESS PLUGIN          LLDB NATIVE   VALUE REGS    INVALIDATE REGS
//  ======      ======= ==  =============       =============         ============            ===============              ===============            =========================   =====================   ============= ==========    ===============
{   "x0",       nullptr,  8,  GPR_OFFSET(0),      lldb::eEncodingUint,  lldb::eFormatHex,     { arm64_ehframe::x0,           arm64_dwarf::x0,           LLDB_REGNUM_GENERIC_ARG1,   LLDB_INVALID_REGNUM,       gpr_x0      },      nullptr,        nullptr, nullptr, 0},
{   "x1",       nullptr,  8,  GPR_OFFSET(1),      lldb::eEncodingUint,  lldb::eFormatHex,     { arm64_ehframe::x1,           arm64_dwarf::x1,           LLDB_REGNUM_GENERIC_ARG2,   LLDB_INVALID_REGNUM,       gpr_x1      },      nullptr,        nullptr, nullptr, 0},
{   "x2",       nullptr,  8,  GPR_OFFSET(2),      lldb::eEncodingUint,  lldb::eFormatHex,     { arm64_ehframe::x2,           arm64_dwarf::x2,           LLDB_REGNUM_GENERIC_ARG3,   LLDB_INVALID_REGNUM,       gpr_x2      },      nullptr,        nullptr, nullptr, 0},
{   "x3",       nullptr,  8,  GPR_OFFSET(3),      lldb::eEncodingUint,  lldb::eFormatHex,     { arm64_ehframe::x3,           arm64_dwarf::x3,           LLDB_REGNUM_GENERIC_ARG4,   LLDB_INVALID_REGNUM,       gpr_x3      },      nullptr,        nullptr, nullptr, 0},
{   "x4",       nullptr,  8,  GPR_OFFSET(4),      lldb::eEncodingUint,  lldb::eFormatHex,     { arm64_ehframe::x4,           arm64_dwarf::x4,           LLDB_REGNUM_GENERIC_ARG5,   LLDB_INVALID_REGNUM,       gpr_x4      },      nullptr,        nullptr, nullptr, 0},
{   "x5",       nullptr,  8,  GPR_OFFSET(5),      lldb::eEncodingUint,  lldb::eFormatHex,     { arm64_ehframe::x5,           arm64_dwarf::x5,           LLDB_REGNUM_GENERIC_ARG6,   LLDB_INVALID_REGNUM,       gpr_x5      },      nullptr,        nullptr, nullptr, 0},
{   "x6",       nullptr,  8,  GPR_OFFSET(6),      lldb::eEncodingUint,  lldb::eFormatHex,     { arm64_ehframe::x6,           arm64_dwarf::x6,           LLDB_REGNUM_GENERIC_ARG7,   LLDB_INVALID_REGNUM,       gpr_x6      },      nullptr,        nullptr, nullptr, 0},
{   "x7",       nullptr,  8,  GPR_OFFSET(7),      lldb::eEncodingUint,  lldb::eFormatHex,     { arm64_ehframe::x7,           arm64_dwarf::x7,           LLDB_REGNUM_GENERIC_ARG8,   LLDB_INVALID_REGNUM,       gpr_x7      },      nullptr,        nullptr, nullptr, 0},
{   "x8",       nullptr,  8,  GPR_OFFSET(8),      lldb::eEncodingUint,  lldb::eFormatHex,     { arm64_ehframe::x8,           arm64_dwarf::x8,           LLDB_INVALID_REGNUM,        LLDB_INVALID_REGNUM,       gpr_x8      },      nullptr,        nullptr, nullptr, 0},
{   "x9",       nullptr,  8,  GPR_OFFSET(9),      lldb::eEncodingUint,  lldb::eFormatHex,     { arm64_ehframe::x9,           arm64_dwarf::x9,           LLDB_INVALID_REGNUM,        LLDB_INVALID_REGNUM,       gpr_x9      },      nullptr,        nullptr, nullptr, 0},
{   "x10",      nullptr,  8,  GPR_OFFSET(10),     lldb::eEncodingUint,  lldb::eFormatHex,     { arm64_ehframe::x10,          arm64_dwarf::x10,          LLDB_INVALID_REGNUM,        LLDB_INVALID_REGNUM,       gpr_x10     },      nullptr,        nullptr, nullptr, 0},
{   "x11",      nullptr,  8,  GPR_OFFSET(11),     lldb::eEncodingUint,  lldb::eFormatHex,     { arm64_ehframe::x11,          arm64_dwarf::x11,          LLDB_INVALID_REGNUM,        LLDB_INVALID_REGNUM,       gpr_x11     },      nullptr,        nullptr, nullptr, 0},
{   "x12",      nullptr,  8,  GPR_OFFSET(12),     lldb::eEncodingUint,  lldb::eFormatHex,     { arm64_ehframe::x12,          arm64_dwarf::x12,          LLDB_INVALID_REGNUM,        LLDB_INVALID_REGNUM,       gpr_x12     },      nullptr,        nullptr, nullptr, 0},
{   "x13",      nullptr,  8,  GPR_OFFSET(13),     lldb::eEncodingUint,  lldb::eFormatHex,     { arm64_ehframe::x13,          arm64_dwarf::x13,          LLDB_INVALID_REGNUM,        LLDB_INVALID_REGNUM,       gpr_x13     },      nullptr,        nullptr, nullptr, 0},
{   "x14",      nullptr,  8,  GPR_OFFSET(14),     lldb::eEncodingUint,  lldb::eFormatHex,     { arm64_ehframe::x14,          arm64_dwarf::x14,          LLDB_INVALID_REGNUM,        LLDB_INVALID_REGNUM,       gpr_x14     },      nullptr,        nullptr, nullptr, 0},
{   "x15",      nullptr,  8,  GPR_OFFSET(15),     lldb::eEncodingUint,  lldb::eFormatHex,     { arm64_ehframe::x15,          arm64_dwarf::x15,          LLDB_INVALID_REGNUM,        LLDB_INVALID_REGNUM,       gpr_x15     },      nullptr,        nullptr, nullptr, 0},
{   "x16",      nullptr,  8,  GPR_OFFSET(16),     lldb::eEncodingUint,  lldb::eFormatHex,     { arm64_ehframe::x16,          arm64_dwarf::x16,          LLDB_INVALID_REGNUM,        LLDB_INVALID_REGNUM,       gpr_x16     },      nullptr,        nullptr, nullptr, 0},
{   "x17",      nullptr,  8,  GPR_OFFSET(17),     lldb::eEncodingUint,  lldb::eFormatHex,     { arm64_ehframe::x17,          arm64_dwarf::x17,          LLDB_INVALID_REGNUM,        LLDB_INVALID_REGNUM,       gpr_x17     },      nullptr,        nullptr, nullptr, 0},
{   "x18",      nullptr,  8,  GPR_OFFSET(18),     lldb::eEncodingUint,  lldb::eFormatHex,     { arm64_ehframe::x18,          arm64_dwarf::x18,          LLDB_INVALID_REGNUM,        LLDB_INVALID_REGNUM,       gpr_x18     },      nullptr,        nullptr, nullptr, 0},
{   "x19",      nullptr,  8,  GPR_OFFSET(19),     lldb::eEncodingUint,  lldb::eFormatHex,     { arm64_ehframe::x19,          arm64_dwarf::x19,          LLDB_INVALID_REGNUM,        LLDB_INVALID_REGNUM,       gpr_x19     },      nullptr,        nullptr, nullptr, 0},
{   "x20",      nullptr,  8,  GPR_OFFSET(20),     lldb::eEncodingUint,  lldb::eFormatHex,     { arm64_ehframe::x20,          arm64_dwarf::x20,          LLDB_INVALID_REGNUM,        LLDB_INVALID_REGNUM,       gpr_x20     },      nullptr,        nullptr, nullptr, 0},
{   "x21",      nullptr,  8,  GPR_OFFSET(21),     lldb::eEncodingUint,  lldb::eFormatHex,     { arm64_ehframe::x21,          arm64_dwarf::x21,          LLDB_INVALID_REGNUM,        LLDB_INVALID_REGNUM,       gpr_x21     },      nullptr,        nullptr, nullptr, 0},
{   "x22",      nullptr,  8,  GPR_OFFSET(22),     lldb::eEncodingUint,  lldb::eFormatHex,     { arm64_ehframe::x22,          arm64_dwarf::x22,          LLDB_INVALID_REGNUM,        LLDB_INVALID_REGNUM,       gpr_x22     },      nullptr,        nullptr, nullptr, 0},
{   "x23",      nullptr,  8,  GPR_OFFSET(23),     lldb::eEncodingUint,  lldb::eFormatHex,     { arm64_ehframe::x23,          arm64_dwarf::x23,          LLDB_INVALID_REGNUM,        LLDB_INVALID_REGNUM,       gpr_x23     },      nullptr,        nullptr, nullptr, 0},
{   "x24",      nullptr,  8,  GPR_OFFSET(24),     lldb::eEncodingUint,  lldb::eFormatHex,     { arm64_ehframe::x24,          arm64_dwarf::x24,          LLDB_INVALID_REGNUM,        LLDB_INVALID_REGNUM,       gpr_x24     },      nullptr,        nullptr, nullptr, 0},
{   "x25",      nullptr,  8,  GPR_OFFSET(25),     lldb::eEncodingUint,  lldb::eFormatHex,     { arm64_ehframe::x25,          arm64_dwarf::x25,          LLDB_INVALID_REGNUM,        LLDB_INVALID_REGNUM,       gpr_x25     },      nullptr,        nullptr, nullptr, 0},
{   "x26",      nullptr,  8,  GPR_OFFSET(26),     lldb::eEncodingUint,  lldb::eFormatHex,     { arm64_ehframe::x26,          arm64_dwarf::x26,          LLDB_INVALID_REGNUM,        LLDB_INVALID_REGNUM,       gpr_x26     },      nullptr,        nullptr, nullptr, 0},
{   "x27",      nullptr,  8,  GPR_OFFSET(27),     lldb::eEncodingUint,  lldb::eFormatHex,     { arm64_ehframe::x27,          arm64_dwarf::x27,          LLDB_INVALID_REGNUM,        LLDB_INVALID_REGNUM,       gpr_x27     },      nullptr,        nullptr, nullptr, 0},
{   "x28",      nullptr,  8,  GPR_OFFSET(28),     lldb::eEncodingUint,  lldb::eFormatHex,     { arm64_ehframe::x28,          arm64_dwarf::x28,          LLDB_INVALID_REGNUM,        LLDB_INVALID_REGNUM,       gpr_x28     },      nullptr,        nullptr, nullptr, 0},

{   "fp",       "x29",    8,  GPR_OFFSET(29),     lldb::eEncodingUint,  lldb::eFormatHex,     { arm64_ehframe::fp,           arm64_dwarf::fp,           LLDB_REGNUM_GENERIC_FP,     LLDB_INVALID_REGNUM,       gpr_fp      },      nullptr,        nullptr, nullptr, 0},
{   "lr",       "x30",    8,  GPR_OFFSET(30),     lldb::eEncodingUint,  lldb::eFormatHex,     { arm64_ehframe::lr,           arm64_dwarf::lr,           LLDB_REGNUM_GENERIC_RA,     LLDB_INVALID_REGNUM,       gpr_lr      },      nullptr,        nullptr, nullptr, 0},
{   "sp",       "x31",    8,  GPR_OFFSET(31),     lldb::eEncodingUint,  lldb::eFormatHex,     { arm64_ehframe::sp,           arm64_dwarf::sp,           LLDB_REGNUM_GENERIC_SP,     LLDB_INVALID_REGNUM,       gpr_sp      },      nullptr,        nullptr, nullptr, 0},
{   "pc",       nullptr,  8,  GPR_OFFSET(32),     lldb::eEncodingUint,  lldb::eFormatHex,     { arm64_ehframe::pc,           arm64_dwarf::pc,           LLDB_REGNUM_GENERIC_PC,     LLDB_INVALID_REGNUM,       gpr_pc      },      nullptr,        nullptr, nullptr, 0},

{   "cpsr",     nullptr,  4,  GPR_OFFSET_NAME(cpsr), lldb::eEncodingUint,  lldb::eFormatHex,  { arm64_ehframe::cpsr,         arm64_dwarf::cpsr,         LLDB_REGNUM_GENERIC_FLAGS,  LLDB_INVALID_REGNUM,       gpr_cpsr    },      nullptr,        nullptr, nullptr, 0},

{   "v0",       nullptr, 16,  FPU_OFFSET(0),      lldb::eEncodingVector, lldb::eFormatVectorOfUInt8,  { LLDB_INVALID_REGNUM,  arm64_dwarf::v0,           LLDB_INVALID_REGNUM,        LLDB_INVALID_REGNUM,       fpu_v0      },      nullptr,        nullptr, nullptr, 0},
{   "v1",       nullptr, 16,  FPU_OFFSET(1),      lldb::eEncodingVector, lldb::eFormatVectorOfUInt8,  { LLDB_INVALID_REGNUM,  arm64_dwarf::v1,           LLDB_INVALID_REGNUM,        LLDB_INVALID_REGNUM,       fpu_v1      },      nullptr,        nullptr, nullptr, 0},
{   "v2",       nullptr, 16,  FPU_OFFSET(2),      lldb::eEncodingVector, lldb::eFormatVectorOfUInt8,  { LLDB_INVALID_REGNUM,  arm64_dwarf::v2,           LLDB_INVALID_REGNUM,        LLDB_INVALID_REGNUM,       fpu_v2      },      nullptr,        nullptr, nullptr, 0},
{   "v3",       nullptr, 16,  FPU_OFFSET(3),      lldb::eEncodingVector, lldb::eFormatVectorOfUInt8,  { LLDB_INVALID_REGNUM,  arm64_dwarf::v3,           LLDB_INVALID_REGNUM,        LLDB_INVALID_REGNUM,       fpu_v3      },      nullptr,        nullptr, nullptr, 0},
{   "v4",       nullptr, 16,  FPU_OFFSET(4),      lldb::eEncodingVector, lldb::eFormatVectorOfUInt8,  { LLDB_INVALID_REGNUM,  arm64_dwarf::v4,           LLDB_INVALID_REGNUM,        LLDB_INVALID_REGNUM,       fpu_v4      },      nullptr,        nullptr, nullptr, 0},
{   "v5",       nullptr, 16,  FPU_OFFSET(5),      lldb::eEncodingVector, lldb::eFormatVectorOfUInt8,  { LLDB_INVALID_REGNUM,  arm64_dwarf::v5,           LLDB_INVALID_REGNUM,        LLDB_INVALID_REGNUM,       fpu_v5      },      nullptr,        nullptr, nullptr, 0},
{   "v6",       nullptr, 16,  FPU_OFFSET(6),      lldb::eEncodingVector, lldb::eFormatVectorOfUInt8,  { LLDB_INVALID_REGNUM,  arm64_dwarf::v6,           LLDB_INVALID_REGNUM,        LLDB_INVALID_REGNUM,       fpu_v6      },      nullptr,        nullptr, nullptr, 0},
{   "v7",       nullptr, 16,  FPU_OFFSET(7),      lldb::eEncodingVector, lldb::eFormatVectorOfUInt8,  { LLDB_INVALID_REGNUM,  arm64_dwarf::v7,           LLDB_INVALID_REGNUM,        LLDB_INVALID_REGNUM,       fpu_v7      },      nullptr,        nullptr, nullptr, 0},
{   "v8",       nullptr, 16,  FPU_OFFSET(8),      lldb::eEncodingVector, lldb::eFormatVectorOfUInt8,  { LLDB_INVALID_REGNUM,  arm64_dwarf::v8,           LLDB_INVALID_REGNUM,        LLDB_INVALID_REGNUM,       fpu_v8      },      nullptr,        nullptr, nullptr, 0},
{   "v9",       nullptr, 16,  FPU_OFFSET(9),      lldb::eEncodingVector, lldb::eFormatVectorOfUInt8,  { LLDB_INVALID_REGNUM,  arm64_dwarf::v9,           LLDB_INVALID_REGNUM,        LLDB_INVALID_REGNUM,       fpu_v9      },      nullptr,        nullptr, nullptr, 0},
{   "v10",      nullptr, 16,  FPU_OFFSET(10),     lldb::eEncodingVector, lldb::eFormatVectorOfUInt8,  { LLDB_INVALID_REGNUM,  arm64_dwarf::v10,          LLDB_INVALID_REGNUM,        LLDB_INVALID_REGNUM,       fpu_v10     },      nullptr,        nullptr, nullptr, 0},
{   "v11",      nullptr, 16,  FPU_OFFSET(11),     lldb::eEncodingVector, lldb::eFormatVectorOfUInt8,  { LLDB_INVALID_REGNUM,  arm64_dwarf::v11,          LLDB_INVALID_REGNUM,        LLDB_INVALID_REGNUM,       fpu_v11     },      nullptr,        nullptr, nullptr, 0},
{   "v12",      nullptr, 16,  FPU_OFFSET(12),     lldb::eEncodingVector, lldb::eFormatVectorOfUInt8,  { LLDB_INVALID_REGNUM,  arm64_dwarf::v12,          LLDB_INVALID_REGNUM,        LLDB_INVALID_REGNUM,       fpu_v12     },      nullptr,        nullptr, nullptr, 0},
{   "v13",      nullptr, 16,  FPU_OFFSET(13),     lldb::eEncodingVector, lldb::eFormatVectorOfUInt8,  { LLDB_INVALID_REGNUM,  arm64_dwarf::v13,          LLDB_INVALID_REGNUM,        LLDB_INVALID_REGNUM,       fpu_v13     },      nullptr,        nullptr, nullptr, 0},
{   "v14",      nullptr, 16,  FPU_OFFSET(14),     lldb::eEncodingVector, lldb::eFormatVectorOfUInt8,  { LLDB_INVALID_REGNUM,  arm64_dwarf::v14,          LLDB_INVALID_REGNUM,        LLDB_INVALID_REGNUM,       fpu_v14     },      nullptr,        nullptr, nullptr, 0},
{   "v15",      nullptr, 16,  FPU_OFFSET(15),     lldb::eEncodingVector, lldb::eFormatVectorOfUInt8,  { LLDB_INVALID_REGNUM,  arm64_dwarf::v15,          LLDB_INVALID_REGNUM,        LLDB_INVALID_REGNUM,       fpu_v15     },      nullptr,        nullptr, nullptr, 0},
{   "v16",      nullptr, 16,  FPU_OFFSET(16),     lldb::eEncodingVector, lldb::eFormatVectorOfUInt8,  { LLDB_INVALID_REGNUM,  arm64_dwarf::v16,          LLDB_INVALID_REGNUM,        LLDB_INVALID_REGNUM,       fpu_v16     },      nullptr,        nullptr, nullptr, 0},
{   "v17",      nullptr, 16,  FPU_OFFSET(17),     lldb::eEncodingVector, lldb::eFormatVectorOfUInt8,  { LLDB_INVALID_REGNUM,  arm64_dwarf::v17,          LLDB_INVALID_REGNUM,        LLDB_INVALID_REGNUM,       fpu_v17     },      nullptr,        nullptr, nullptr, 0},
{   "v18",      nullptr, 16,  FPU_OFFSET(18),     lldb::eEncodingVector, lldb::eFormatVectorOfUInt8,  { LLDB_INVALID_REGNUM,  arm64_dwarf::v18,          LLDB_INVALID_REGNUM,        LLDB_INVALID_REGNUM,       fpu_v18     },      nullptr,        nullptr, nullptr, 0},
{   "v19",      nullptr, 16,  FPU_OFFSET(19),     lldb::eEncodingVector, lldb::eFormatVectorOfUInt8,  { LLDB_INVALID_REGNUM,  arm64_dwarf::v19,          LLDB_INVALID_REGNUM,        LLDB_INVALID_REGNUM,       fpu_v19     },      nullptr,        nullptr, nullptr, 0},
{   "v20",      nullptr, 16,  FPU_OFFSET(20),     lldb::eEncodingVector, lldb::eFormatVectorOfUInt8,  { LLDB_INVALID_REGNUM,  arm64_dwarf::v20,          LLDB_INVALID_REGNUM,        LLDB_INVALID_REGNUM,       fpu_v20     },      nullptr,        nullptr, nullptr, 0},
{   "v21",      nullptr, 16,  FPU_OFFSET(21),     lldb::eEncodingVector, lldb::eFormatVectorOfUInt8,  { LLDB_INVALID_REGNUM,  arm64_dwarf::v21,          LLDB_INVALID_REGNUM,        LLDB_INVALID_REGNUM,       fpu_v21     },      nullptr,        nullptr, nullptr, 0},
{   "v22",      nullptr, 16,  FPU_OFFSET(22),     lldb::eEncodingVector, lldb::eFormatVectorOfUInt8,  { LLDB_INVALID_REGNUM,  arm64_dwarf::v22,          LLDB_INVALID_REGNUM,        LLDB_INVALID_REGNUM,       fpu_v22     },      nullptr,        nullptr, nullptr, 0},
{   "v23",      nullptr, 16,  FPU_OFFSET(23),     lldb::eEncodingVector, lldb::eFormatVectorOfUInt8,  { LLDB_INVALID_REGNUM,  arm64_dwarf::v23,          LLDB_INVALID_REGNUM,        LLDB_INVALID_REGNUM,       fpu_v23     },      nullptr,        nullptr, nullptr, 0},
{   "v24",      nullptr, 16,  FPU_OFFSET(24),     lldb::eEncodingVector, lldb::eFormatVectorOfUInt8,  { LLDB_INVALID_REGNUM,  arm64_dwarf::v24,          LLDB_INVALID_REGNUM,        LLDB_INVALID_REGNUM,       fpu_v24     },      nullptr,        nullptr, nullptr, 0},
{   "v25",      nullptr, 16,  FPU_OFFSET(25),     lldb::eEncodingVector, lldb::eFormatVectorOfUInt8,  { LLDB_INVALID_REGNUM,  arm64_dwarf::v25,          LLDB_INVALID_REGNUM,        LLDB_INVALID_REGNUM,       fpu_v25     },      nullptr,        nullptr, nullptr, 0},
{   "v26",      nullptr, 16,  FPU_OFFSET(26),     lldb::eEncodingVector, lldb::eFormatVectorOfUInt8,  { LLDB_INVALID_REGNUM,  arm64_dwarf::v26,          LLDB_INVALID_REGNUM,        LLDB_INVALID_REGNUM,       fpu_v26     },      nullptr,        nullptr, nullptr, 0},
{   "v27",      nullptr, 16,  FPU_OFFSET(27),     lldb::eEncodingVector, lldb::eFormatVectorOfUInt8,  { LLDB_INVALID_REGNUM,  arm64_dwarf::v27,          LLDB_INVALID_REGNUM,        LLDB_INVALID_REGNUM,       fpu_v27     },      nullptr,        nullptr, nullptr, 0},
{   "v28",      nullptr, 16,  FPU_OFFSET(28),     lldb::eEncodingVector, lldb::eFormatVectorOfUInt8,  { LLDB_INVALID_REGNUM,  arm64_dwarf::v28,          LLDB_INVALID_REGNUM,        LLDB_INVALID_REGNUM,       fpu_v28     },      nullptr,        nullptr, nullptr, 0},
{   "v29",      nullptr, 16,  FPU_OFFSET(29),     lldb::eEncodingVector, lldb::eFormatVectorOfUInt8,  { LLDB_INVALID_REGNUM,  arm64_dwarf::v29,          LLDB_INVALID_REGNUM,        LLDB_INVALID_REGNUM,       fpu_v29     },      nullptr,        nullptr, nullptr, 0},
{   "v30",      nullptr, 16,  FPU_OFFSET(30),     lldb::eEncodingVector, lldb::eFormatVectorOfUInt8,  { LLDB_INVALID_REGNUM,  arm64_dwarf::v30,          LLDB_INVALID_REGNUM,        LLDB_INVALID_REGNUM,       fpu_v30     },      nullptr,        nullptr, nullptr, 0},
{   "v31",      nullptr, 16,  FPU_OFFSET(31),     lldb::eEncodingVector, lldb::eFormatVectorOfUInt8,  { LLDB_INVALID_REGNUM,  arm64_dwarf::v31,          LLDB_INVALID_REGNUM,        LLDB_INVALID_REGNUM,       fpu_v31     },      nullptr,        nullptr, nullptr, 0},

{   "fpsr",     nullptr,  4,  FPU_OFFSET_NAME(fpsr),     lldb::eEncodingUint,  lldb::eFormatHex,     { LLDB_INVALID_REGNUM,  LLDB_INVALID_REGNUM, LLDB_INVALID_REGNUM,        LLDB_INVALID_REGNUM,  fpu_fpsr   },      nullptr,        nullptr, nullptr, 0},
{   "fpcr",     nullptr,  4,  FPU_OFFSET_NAME(fpcr),     lldb::eEncodingUint,  lldb::eFormatHex,     { LLDB_INVALID_REGNUM,  LLDB_INVALID_REGNUM, LLDB_INVALID_REGNUM,        LLDB_INVALID_REGNUM,  fpu_fpcr   },      nullptr,        nullptr, nullptr, 0},

{   "far",      nullptr,  8,  EXC_OFFSET_NAME(far),       lldb::eEncodingUint,  lldb::eFormatHex,     { LLDB_INVALID_REGNUM,  LLDB_INVALID_REGNUM, LLDB_INVALID_REGNUM,        LLDB_INVALID_REGNUM,    exc_far       },    nullptr,        nullptr, nullptr, 0},
{   "esr",      nullptr,  4,  EXC_OFFSET_NAME(esr),       lldb::eEncodingUint,  lldb::eFormatHex,     { LLDB_INVALID_REGNUM,  LLDB_INVALID_REGNUM, LLDB_INVALID_REGNUM,        LLDB_INVALID_REGNUM,    exc_esr       },    nullptr,        nullptr, nullptr, 0},
{   "exception",nullptr,  4,  EXC_OFFSET_NAME(exception), lldb::eEncodingUint,  lldb::eFormatHex,     { LLDB_INVALID_REGNUM,  LLDB_INVALID_REGNUM, LLDB_INVALID_REGNUM,        LLDB_INVALID_REGNUM,    exc_exception },    nullptr,        nullptr, nullptr, 0},

{   DEFINE_DBG (bvr, 0) },
{   DEFINE_DBG (bvr, 1) },
{   DEFINE_DBG (bvr, 2) },
{   DEFINE_DBG (bvr, 3) },
{   DEFINE_DBG (bvr, 4) },
{   DEFINE_DBG (bvr, 5) },
{   DEFINE_DBG (bvr, 6) },
{   DEFINE_DBG (bvr, 7) },
{   DEFINE_DBG (bvr, 8) },
{   DEFINE_DBG (bvr, 9) },
{   DEFINE_DBG (bvr, 10) },
{   DEFINE_DBG (bvr, 11) },
{   DEFINE_DBG (bvr, 12) },
{   DEFINE_DBG (bvr, 13) },
{   DEFINE_DBG (bvr, 14) },
{   DEFINE_DBG (bvr, 15) },

{   DEFINE_DBG (bcr, 0) },
{   DEFINE_DBG (bcr, 1) },
{   DEFINE_DBG (bcr, 2) },
{   DEFINE_DBG (bcr, 3) },
{   DEFINE_DBG (bcr, 4) },
{   DEFINE_DBG (bcr, 5) },
{   DEFINE_DBG (bcr, 6) },
{   DEFINE_DBG (bcr, 7) },
{   DEFINE_DBG (bcr, 8) },
{   DEFINE_DBG (bcr, 9) },
{   DEFINE_DBG (bcr, 10) },
{   DEFINE_DBG (bcr, 11) },
{   DEFINE_DBG (bcr, 12) },
{   DEFINE_DBG (bcr, 13) },
{   DEFINE_DBG (bcr, 14) },
{   DEFINE_DBG (bcr, 15) },

{   DEFINE_DBG (wvr, 0) },
{   DEFINE_DBG (wvr, 1) },
{   DEFINE_DBG (wvr, 2) },
{   DEFINE_DBG (wvr, 3) },
{   DEFINE_DBG (wvr, 4) },
{   DEFINE_DBG (wvr, 5) },
{   DEFINE_DBG (wvr, 6) },
{   DEFINE_DBG (wvr, 7) },
{   DEFINE_DBG (wvr, 8) },
{   DEFINE_DBG (wvr, 9) },
{   DEFINE_DBG (wvr, 10) },
{   DEFINE_DBG (wvr, 11) },
{   DEFINE_DBG (wvr, 12) },
{   DEFINE_DBG (wvr, 13) },
{   DEFINE_DBG (wvr, 14) },
{   DEFINE_DBG (wvr, 15) },

{   DEFINE_DBG (wcr, 0) },
{   DEFINE_DBG (wcr, 1) },
{   DEFINE_DBG (wcr, 2) },
{   DEFINE_DBG (wcr, 3) },
{   DEFINE_DBG (wcr, 4) },
{   DEFINE_DBG (wcr, 5) },
{   DEFINE_DBG (wcr, 6) },
{   DEFINE_DBG (wcr, 7) },
{   DEFINE_DBG (wcr, 8) },
{   DEFINE_DBG (wcr, 9) },
{   DEFINE_DBG (wcr, 10) },
{   DEFINE_DBG (wcr, 11) },
{   DEFINE_DBG (wcr, 12) },
{   DEFINE_DBG (wcr, 13) },
{   DEFINE_DBG (wcr, 14) },
{   DEFINE_DBG (wcr, 15) }
};

#endif // DECLARE_REGISTER_INFOS_ARM64_STRUCT
