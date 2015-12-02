//===-- UnwindAssemblyInstEmulation.cpp --------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "UnwindAssemblyInstEmulation.h"

#include "lldb/Core/Address.h"
#include "lldb/Core/ArchSpec.h"
#include "lldb/Core/DataBufferHeap.h"
#include "lldb/Core/DataExtractor.h"
#include "lldb/Core/Disassembler.h"
#include "lldb/Core/Error.h"
#include "lldb/Core/FormatEntity.h"
#include "lldb/Core/Log.h"
#include "lldb/Core/PluginManager.h"
#include "lldb/Core/StreamString.h"
#include "lldb/Target/ExecutionContext.h"
#include "lldb/Target/Process.h"
#include "lldb/Target/Thread.h"
#include "lldb/Target/Target.h"

using namespace lldb;
using namespace lldb_private;



//-----------------------------------------------------------------------------------------------
//  UnwindAssemblyInstEmulation method definitions 
//-----------------------------------------------------------------------------------------------

bool
UnwindAssemblyInstEmulation::GetNonCallSiteUnwindPlanFromAssembly (AddressRange& range, 
                                                                   Thread& thread, 
                                                                   UnwindPlan& unwind_plan)
{
    if (range.GetByteSize() > 0 && 
        range.GetBaseAddress().IsValid() &&
        m_inst_emulator_ap.get())
    {
     
        // The instruction emulation subclass setup the unwind plan for the
        // first instruction.
        m_inst_emulator_ap->CreateFunctionEntryUnwind (unwind_plan);

        // CreateFunctionEntryUnwind should have created the first row. If it
        // doesn't, then we are done.
        if (unwind_plan.GetRowCount() == 0)
            return false;
        
        ExecutionContext exe_ctx;
        thread.CalculateExecutionContext(exe_ctx);
        const bool prefer_file_cache = true;
        DisassemblerSP disasm_sp (Disassembler::DisassembleRange (m_arch,
                                                                  NULL,
                                                                  NULL,
                                                                  exe_ctx,
                                                                  range,
                                                                  prefer_file_cache));
        
        Log *log(GetLogIfAllCategoriesSet (LIBLLDB_LOG_UNWIND));

        if (disasm_sp)
        {
            
            m_range_ptr = &range;
            m_thread_ptr = &thread;
            m_unwind_plan_ptr = &unwind_plan;

            const uint32_t addr_byte_size = m_arch.GetAddressByteSize();
            const bool show_address = true;
            const bool show_bytes = true;
            m_inst_emulator_ap->GetRegisterInfo (unwind_plan.GetRegisterKind(), 
                                                 unwind_plan.GetInitialCFARegister(), 
                                                 m_cfa_reg_info);
            
            m_fp_is_cfa = false;
            m_register_values.clear();
            m_pushed_regs.clear();

            // Initialize the CFA with a known value. In the 32 bit case
            // it will be 0x80000000, and in the 64 bit case 0x8000000000000000.
            // We use the address byte size to be safe for any future address sizes
            m_initial_sp = (1ull << ((addr_byte_size * 8) - 1));
            RegisterValue cfa_reg_value;
            cfa_reg_value.SetUInt (m_initial_sp, m_cfa_reg_info.byte_size);
            SetRegisterValue (m_cfa_reg_info, cfa_reg_value);

            const InstructionList &inst_list = disasm_sp->GetInstructionList ();
            const size_t num_instructions = inst_list.GetSize();

            if (num_instructions > 0)
            {
                Instruction *inst = inst_list.GetInstructionAtIndex (0).get();
                const lldb::addr_t base_addr = inst->GetAddress().GetFileAddress();

                // Map for storing the unwind plan row and the value of the registers at a given offset.
                // When we see a forward branch we add a new entry to this map with the actual unwind plan
                // row and register context for the target address of the branch as the current data have
                // to be valid for the target address of the branch too if we are in the same function.
                std::map<lldb::addr_t, std::pair<UnwindPlan::RowSP, RegisterValueMap>> saved_unwind_states;

                // Make a copy of the current instruction Row and save it in m_curr_row
                // so we can add updates as we process the instructions.  
                UnwindPlan::RowSP last_row = unwind_plan.GetLastRow();
                UnwindPlan::Row *newrow = new UnwindPlan::Row;
                if (last_row.get())
                    *newrow = *last_row.get();
                m_curr_row.reset(newrow);

                // Add the initial state to the save list with offset 0.
                saved_unwind_states.insert({0, {last_row, m_register_values}});

                // cache the pc register number (in whatever register numbering this UnwindPlan uses) for
                // quick reference during instruction parsing.
                uint32_t pc_reg_num = LLDB_INVALID_REGNUM;
                RegisterInfo pc_reg_info;
                if (m_inst_emulator_ap->GetRegisterInfo (eRegisterKindGeneric, LLDB_REGNUM_GENERIC_PC, pc_reg_info))
                    pc_reg_num = pc_reg_info.kinds[unwind_plan.GetRegisterKind()];
                else
                    pc_reg_num = LLDB_INVALID_REGNUM;

                // cache the return address register number (in whatever register numbering this UnwindPlan uses) for
                // quick reference during instruction parsing.
                uint32_t ra_reg_num = LLDB_INVALID_REGNUM;
                RegisterInfo ra_reg_info;
                if (m_inst_emulator_ap->GetRegisterInfo (eRegisterKindGeneric, LLDB_REGNUM_GENERIC_RA, ra_reg_info))
                    ra_reg_num = ra_reg_info.kinds[unwind_plan.GetRegisterKind()];
                else
                    ra_reg_num = LLDB_INVALID_REGNUM;

                for (size_t idx=0; idx<num_instructions; ++idx)
                {
                    m_curr_row_modified = false;
                    m_forward_branch_offset = 0;

                    inst = inst_list.GetInstructionAtIndex (idx).get();
                    if (inst)
                    {
                        lldb::addr_t current_offset = inst->GetAddress().GetFileAddress() - base_addr;
                        auto it = saved_unwind_states.upper_bound(current_offset);
                        assert(it != saved_unwind_states.begin() && "Unwind row for the function entry missing");
                        --it; // Move it to the row corresponding to the current offset

                        // If the offset of m_curr_row don't match with the offset we see in saved_unwind_states
                        // then we have to update m_curr_row and m_register_values based on the saved values. It
                        // is happenning after we processed an epilogue and a return to caller instruction.
                        if (it->second.first->GetOffset() != m_curr_row->GetOffset())
                        {
                            UnwindPlan::Row *newrow = new UnwindPlan::Row;
                            *newrow = *it->second.first;
                            m_curr_row.reset(newrow);
                            m_register_values = it->second.second;;
                        }

                        if (log && log->GetVerbose ())
                        {
                            StreamString strm;
                            lldb_private::FormatEntity::Entry format;
                            FormatEntity::Parse("${frame.pc}: ", format);
                            inst->Dump(&strm, inst_list.GetMaxOpcocdeByteSize (), show_address, show_bytes, NULL, NULL, NULL, &format, 0);
                            log->PutCString (strm.GetData());
                        }

                        m_inst_emulator_ap->SetInstruction (inst->GetOpcode(), 
                                                            inst->GetAddress(), 
                                                            exe_ctx.GetTargetPtr());

                        m_inst_emulator_ap->EvaluateInstruction (eEmulateInstructionOptionIgnoreConditions);

                        // If the current instruction is a branch forward then save the current CFI information
                        // for the offset where we are branching.
                        if (m_forward_branch_offset != 0 && range.ContainsFileAddress(inst->GetAddress().GetFileAddress() + m_forward_branch_offset))
                        {
                            auto newrow = std::make_shared<UnwindPlan::Row>(*m_curr_row.get());
                            newrow->SetOffset(current_offset + m_forward_branch_offset);
                            saved_unwind_states.insert({current_offset + m_forward_branch_offset, {newrow, m_register_values}});
                            unwind_plan.InsertRow(newrow);
                        }

                        // Were there any changes to the CFI while evaluating this instruction?
                        if (m_curr_row_modified)
                        {
                            // Save the modified row if we don't already have a CFI row in the currennt address
                            if (saved_unwind_states.count(current_offset + inst->GetOpcode().GetByteSize()) == 0)
                            {
                                m_curr_row->SetOffset (current_offset + inst->GetOpcode().GetByteSize());
                                unwind_plan.InsertRow (m_curr_row);
                                saved_unwind_states.insert({current_offset + inst->GetOpcode().GetByteSize(), {m_curr_row, m_register_values}});

                                // Allocate a new Row for m_curr_row, copy the current state into it
                                UnwindPlan::Row *newrow = new UnwindPlan::Row;
                                *newrow = *m_curr_row.get();
                                m_curr_row.reset(newrow);
                            }
                        }
                    }
                }
            }
            // FIXME: The DisassemblerLLVMC has a reference cycle and won't go away if it has any active instructions.
            // I'll fix that but for now, just clear the list and it will go away nicely.
            disasm_sp->GetInstructionList().Clear();
        }
        
        if (log && log->GetVerbose ())
        {
            StreamString strm;
            lldb::addr_t base_addr = range.GetBaseAddress().GetLoadAddress(thread.CalculateTarget().get());
            strm.Printf ("Resulting unwind rows for [0x%" PRIx64 " - 0x%" PRIx64 "):", base_addr, base_addr + range.GetByteSize());
            unwind_plan.Dump(strm, &thread, base_addr);
            log->PutCString (strm.GetData());
        }
        return unwind_plan.GetRowCount() > 0;
    }
    return false;
}

bool
UnwindAssemblyInstEmulation::AugmentUnwindPlanFromCallSite (AddressRange& func,
                                                            Thread& thread,
                                                            UnwindPlan& unwind_plan)
{
    return false;
}

bool
UnwindAssemblyInstEmulation::GetFastUnwindPlan (AddressRange& func, 
                                                Thread& thread, 
                                                UnwindPlan &unwind_plan)
{
    return false;
}

bool
UnwindAssemblyInstEmulation::FirstNonPrologueInsn (AddressRange& func, 
                                                   const ExecutionContext &exe_ctx, 
                                                   Address& first_non_prologue_insn)
{
    return false;
}

UnwindAssembly *
UnwindAssemblyInstEmulation::CreateInstance (const ArchSpec &arch)
{
    std::unique_ptr<EmulateInstruction> inst_emulator_ap (EmulateInstruction::FindPlugin (arch, eInstructionTypePrologueEpilogue, NULL));
    // Make sure that all prologue instructions are handled
    if (inst_emulator_ap.get())
        return new UnwindAssemblyInstEmulation (arch, inst_emulator_ap.release());
    return NULL;
}


//------------------------------------------------------------------
// PluginInterface protocol in UnwindAssemblyParser_x86
//------------------------------------------------------------------
ConstString
UnwindAssemblyInstEmulation::GetPluginName()
{
    return GetPluginNameStatic();
}

uint32_t
UnwindAssemblyInstEmulation::GetPluginVersion()
{
    return 1;
}

void
UnwindAssemblyInstEmulation::Initialize()
{
    PluginManager::RegisterPlugin (GetPluginNameStatic(),
                                   GetPluginDescriptionStatic(),
                                   CreateInstance);
}

void
UnwindAssemblyInstEmulation::Terminate()
{
    PluginManager::UnregisterPlugin (CreateInstance);
}


ConstString
UnwindAssemblyInstEmulation::GetPluginNameStatic()
{
    static ConstString g_name("inst-emulation");
    return g_name;
}

const char *
UnwindAssemblyInstEmulation::GetPluginDescriptionStatic()
{
    return "Instruction emulation based unwind information.";
}


uint64_t 
UnwindAssemblyInstEmulation::MakeRegisterKindValuePair (const RegisterInfo &reg_info)
{
    lldb::RegisterKind reg_kind;
    uint32_t reg_num;
    if (EmulateInstruction::GetBestRegisterKindAndNumber (&reg_info, reg_kind, reg_num))
        return (uint64_t)reg_kind << 24 | reg_num;
    return 0ull;
}

void
UnwindAssemblyInstEmulation::SetRegisterValue (const RegisterInfo &reg_info, const RegisterValue &reg_value)
{
    m_register_values[MakeRegisterKindValuePair (reg_info)] = reg_value;
}

bool
UnwindAssemblyInstEmulation::GetRegisterValue (const RegisterInfo &reg_info, RegisterValue &reg_value)
{
    const uint64_t reg_id = MakeRegisterKindValuePair (reg_info);
    RegisterValueMap::const_iterator pos = m_register_values.find(reg_id);
    if (pos != m_register_values.end())
    {
        reg_value = pos->second;
        return true; // We had a real value that comes from an opcode that wrote
                     // to it...
    }
    // We are making up a value that is recognizable...
    reg_value.SetUInt(reg_id, reg_info.byte_size);
    return false;
}


size_t
UnwindAssemblyInstEmulation::ReadMemory (EmulateInstruction *instruction,
                                         void *baton,
                                         const EmulateInstruction::Context &context, 
                                         lldb::addr_t addr, 
                                         void *dst,
                                         size_t dst_len)
{
    Log *log(GetLogIfAllCategoriesSet (LIBLLDB_LOG_UNWIND));

    if (log && log->GetVerbose ())
    {
        StreamString strm;
        strm.Printf ("UnwindAssemblyInstEmulation::ReadMemory    (addr = 0x%16.16" PRIx64 ", dst = %p, dst_len = %" PRIu64 ", context = ",
                     addr,
                     dst,
                     (uint64_t)dst_len);
        context.Dump(strm, instruction);
        log->PutCString (strm.GetData ());
    }
    memset (dst, 0, dst_len);
    return dst_len;
}

size_t
UnwindAssemblyInstEmulation::WriteMemory (EmulateInstruction *instruction,
                                          void *baton,
                                          const EmulateInstruction::Context &context, 
                                          lldb::addr_t addr, 
                                          const void *dst,
                                          size_t dst_len)
{
    if (baton && dst && dst_len)
        return ((UnwindAssemblyInstEmulation *)baton)->WriteMemory (instruction, context, addr, dst, dst_len);
    return 0;
}

size_t
UnwindAssemblyInstEmulation::WriteMemory (EmulateInstruction *instruction,
                                          const EmulateInstruction::Context &context, 
                                          lldb::addr_t addr, 
                                          const void *dst,
                                          size_t dst_len)
{
    DataExtractor data (dst, 
                        dst_len, 
                        instruction->GetArchitecture ().GetByteOrder(), 
                        instruction->GetArchitecture ().GetAddressByteSize());

    Log *log(GetLogIfAllCategoriesSet (LIBLLDB_LOG_UNWIND));

    if (log && log->GetVerbose ())
    {
        StreamString strm;

        strm.PutCString ("UnwindAssemblyInstEmulation::WriteMemory   (");
        data.Dump(&strm, 0, eFormatBytes, 1, dst_len, UINT32_MAX, addr, 0, 0);
        strm.PutCString (", context = ");
        context.Dump(strm, instruction);
        log->PutCString (strm.GetData());
    }

    const bool cant_replace = false;

    switch (context.type)
    {
        default:
        case EmulateInstruction::eContextInvalid:
        case EmulateInstruction::eContextReadOpcode:
        case EmulateInstruction::eContextImmediate:
        case EmulateInstruction::eContextAdjustBaseRegister:
        case EmulateInstruction::eContextRegisterPlusOffset:
        case EmulateInstruction::eContextAdjustPC:
        case EmulateInstruction::eContextRegisterStore:
        case EmulateInstruction::eContextRegisterLoad:  
        case EmulateInstruction::eContextRelativeBranchImmediate:
        case EmulateInstruction::eContextAbsoluteBranchRegister:
        case EmulateInstruction::eContextSupervisorCall:
        case EmulateInstruction::eContextTableBranchReadMemory:
        case EmulateInstruction::eContextWriteRegisterRandomBits:
        case EmulateInstruction::eContextWriteMemoryRandomBits:
        case EmulateInstruction::eContextArithmetic:
        case EmulateInstruction::eContextAdvancePC:    
        case EmulateInstruction::eContextReturnFromException:
        case EmulateInstruction::eContextPopRegisterOffStack:
        case EmulateInstruction::eContextAdjustStackPointer:
            break;
            
        case EmulateInstruction::eContextPushRegisterOnStack:
            {
                uint32_t reg_num = LLDB_INVALID_REGNUM;
                uint32_t generic_regnum = LLDB_INVALID_REGNUM;
                if (context.info_type == EmulateInstruction::eInfoTypeRegisterToRegisterPlusOffset)
                {
                    const uint32_t unwind_reg_kind = m_unwind_plan_ptr->GetRegisterKind();
                    reg_num = context.info.RegisterToRegisterPlusOffset.data_reg.kinds[unwind_reg_kind];
                    generic_regnum = context.info.RegisterToRegisterPlusOffset.data_reg.kinds[eRegisterKindGeneric];
                }
                else
                    assert (!"unhandled case, add code to handle this!");

                if (reg_num != LLDB_INVALID_REGNUM && generic_regnum != LLDB_REGNUM_GENERIC_SP)
                {
                    if (m_pushed_regs.find (reg_num) == m_pushed_regs.end())
                    {
                        m_pushed_regs[reg_num] = addr;
                        const int32_t offset = addr - m_initial_sp;
                        m_curr_row->SetRegisterLocationToAtCFAPlusOffset (reg_num, offset, cant_replace);
                        m_curr_row_modified = true;
                    }
                }
            }
            break;
            
    }

    return dst_len;
}

bool
UnwindAssemblyInstEmulation::ReadRegister (EmulateInstruction *instruction,
                                           void *baton,
                                           const RegisterInfo *reg_info,
                                           RegisterValue &reg_value)
{
    
    if (baton && reg_info)
        return ((UnwindAssemblyInstEmulation *)baton)->ReadRegister (instruction, reg_info, reg_value);
    return false;
}
bool
UnwindAssemblyInstEmulation::ReadRegister (EmulateInstruction *instruction,
                                           const RegisterInfo *reg_info,
                                           RegisterValue &reg_value)
{
    bool synthetic = GetRegisterValue (*reg_info, reg_value);

    Log *log(GetLogIfAllCategoriesSet (LIBLLDB_LOG_UNWIND));
    
    if (log && log->GetVerbose ())
    {
        
        StreamString strm;
        strm.Printf ("UnwindAssemblyInstEmulation::ReadRegister  (name = \"%s\") => synthetic_value = %i, value = ", reg_info->name, synthetic);
        reg_value.Dump(&strm, reg_info, false, false, eFormatDefault);
        log->PutCString(strm.GetData());
    }
    return true;
}

bool
UnwindAssemblyInstEmulation::WriteRegister (EmulateInstruction *instruction,
                                            void *baton,
                                            const EmulateInstruction::Context &context, 
                                            const RegisterInfo *reg_info,
                                            const RegisterValue &reg_value)
{
    if (baton && reg_info)
        return ((UnwindAssemblyInstEmulation *)baton)->WriteRegister (instruction, context, reg_info, reg_value);
    return false;
}
bool
UnwindAssemblyInstEmulation::WriteRegister (EmulateInstruction *instruction,
                                            const EmulateInstruction::Context &context, 
                                            const RegisterInfo *reg_info,
                                            const RegisterValue &reg_value)
{
    Log *log(GetLogIfAllCategoriesSet (LIBLLDB_LOG_UNWIND));

    if (log && log->GetVerbose ())
    {
        
        StreamString strm;
        strm.Printf ("UnwindAssemblyInstEmulation::WriteRegister (name = \"%s\", value = ", reg_info->name);
        reg_value.Dump(&strm, reg_info, false, false, eFormatDefault);
        strm.PutCString (", context = ");
        context.Dump(strm, instruction);
        log->PutCString(strm.GetData());
    }

    SetRegisterValue (*reg_info, reg_value);

    switch (context.type)
    {
        case EmulateInstruction::eContextInvalid:
        case EmulateInstruction::eContextReadOpcode:
        case EmulateInstruction::eContextImmediate:
        case EmulateInstruction::eContextAdjustBaseRegister:
        case EmulateInstruction::eContextRegisterPlusOffset:
        case EmulateInstruction::eContextAdjustPC:
        case EmulateInstruction::eContextRegisterStore:
        case EmulateInstruction::eContextSupervisorCall:
        case EmulateInstruction::eContextTableBranchReadMemory:
        case EmulateInstruction::eContextWriteRegisterRandomBits:
        case EmulateInstruction::eContextWriteMemoryRandomBits:
        case EmulateInstruction::eContextArithmetic:
        case EmulateInstruction::eContextAdvancePC:    
        case EmulateInstruction::eContextReturnFromException:
        case EmulateInstruction::eContextPushRegisterOnStack:
        case EmulateInstruction::eContextRegisterLoad:
//            {
//                const uint32_t reg_num = reg_info->kinds[m_unwind_plan_ptr->GetRegisterKind()];
//                if (reg_num != LLDB_INVALID_REGNUM)
//                {
//                    const bool can_replace_only_if_unspecified = true;
//
//                    m_curr_row.SetRegisterLocationToUndefined (reg_num, 
//                                                               can_replace_only_if_unspecified, 
//                                                               can_replace_only_if_unspecified);
//                    m_curr_row_modified = true;
//                }
//            }
            break;

        case EmulateInstruction::eContextAbsoluteBranchRegister:
        case EmulateInstruction::eContextRelativeBranchImmediate:
            {
                if (context.info_type == EmulateInstruction::eInfoTypeISAAndImmediate &&
                    context.info.ISAAndImmediate.unsigned_data32 > 0)
                {
                    m_forward_branch_offset = context.info.ISAAndImmediateSigned.signed_data32;
                }
                else if (context.info_type == EmulateInstruction::eInfoTypeISAAndImmediateSigned &&
                         context.info.ISAAndImmediateSigned.signed_data32 > 0)
                {
                    m_forward_branch_offset = context.info.ISAAndImmediate.unsigned_data32;
                }
                else if (context.info_type == EmulateInstruction::eInfoTypeImmediate &&
                         context.info.unsigned_immediate > 0)
                {
                    m_forward_branch_offset = context.info.unsigned_immediate;
                }
                else if (context.info_type == EmulateInstruction::eInfoTypeImmediateSigned &&
                         context.info.signed_immediate > 0)
                {
                    m_forward_branch_offset = context.info.signed_immediate;
                }
            }
            break;

        case EmulateInstruction::eContextPopRegisterOffStack:
            {
                const uint32_t reg_num = reg_info->kinds[m_unwind_plan_ptr->GetRegisterKind()];
                const uint32_t generic_regnum = reg_info->kinds[eRegisterKindGeneric];
                if (reg_num != LLDB_INVALID_REGNUM && generic_regnum != LLDB_REGNUM_GENERIC_SP)
                {
                    m_curr_row->SetRegisterLocationToSame (reg_num, /*must_replace*/ false);
                    m_curr_row_modified = true;
                }
            }
            break;

        case EmulateInstruction::eContextSetFramePointer:
            if (!m_fp_is_cfa)
            {
                m_fp_is_cfa = true;
                m_cfa_reg_info = *reg_info;
                const uint32_t cfa_reg_num = reg_info->kinds[m_unwind_plan_ptr->GetRegisterKind()];
                assert (cfa_reg_num != LLDB_INVALID_REGNUM);
                m_curr_row->GetCFAValue().SetIsRegisterPlusOffset(cfa_reg_num, m_initial_sp -
                        reg_value.GetAsUInt64());
                m_curr_row_modified = true;
            }
            break;

        case EmulateInstruction::eContextAdjustStackPointer:
            // If we have created a frame using the frame pointer, don't follow
            // subsequent adjustments to the stack pointer.
            if (!m_fp_is_cfa)
            {
                m_curr_row->GetCFAValue().SetIsRegisterPlusOffset(
                        m_curr_row->GetCFAValue().GetRegisterNumber(),
                        m_initial_sp - reg_value.GetAsUInt64());
                m_curr_row_modified = true;
            }
            break;
    }
    return true;
}


