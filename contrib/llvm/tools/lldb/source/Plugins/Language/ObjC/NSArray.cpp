//===-- NSArray.cpp ---------------------------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

// C Includes
// C++ Includes
// Other libraries and framework includes
#include "clang/AST/ASTContext.h"

// Project includes
#include "Cocoa.h"

#include "lldb/Core/DataBufferHeap.h"
#include "lldb/Core/Error.h"
#include "lldb/Core/Stream.h"
#include "lldb/Core/ValueObject.h"
#include "lldb/Core/ValueObjectConstResult.h"
#include "lldb/DataFormatters/FormattersHelpers.h"
#include "lldb/Expression/FunctionCaller.h"
#include "lldb/Host/Endian.h"
#include "lldb/Symbol/ClangASTContext.h"
#include "lldb/Target/Language.h"
#include "lldb/Target/ObjCLanguageRuntime.h"
#include "Plugins/LanguageRuntime/ObjC/AppleObjCRuntime/AppleObjCRuntime.h"
#include "lldb/Target/Target.h"

using namespace lldb;
using namespace lldb_private;
using namespace lldb_private::formatters;

namespace  lldb_private {
    namespace formatters {
        class NSArrayMSyntheticFrontEnd : public SyntheticChildrenFrontEnd
        {
        public:
            NSArrayMSyntheticFrontEnd (lldb::ValueObjectSP valobj_sp);

            ~NSArrayMSyntheticFrontEnd() override = default;

            size_t
            CalculateNumChildren() override;
            
            lldb::ValueObjectSP
            GetChildAtIndex(size_t idx) override;
            
            bool
            Update() override = 0;
            
            bool
            MightHaveChildren() override;
            
            size_t
            GetIndexOfChildWithName(const ConstString &name) override;
            
        protected:
            virtual lldb::addr_t
            GetDataAddress () = 0;
            
            virtual uint64_t
            GetUsedCount () = 0;
            
            virtual uint64_t
            GetOffset () = 0;
            
            virtual uint64_t
            GetSize () = 0;
            
            ExecutionContextRef m_exe_ctx_ref;
            uint8_t m_ptr_size;
            CompilerType m_id_type;
            std::vector<lldb::ValueObjectSP> m_children;
        };
        
        class NSArrayMSyntheticFrontEnd_109 : public NSArrayMSyntheticFrontEnd
        {
        public:
            NSArrayMSyntheticFrontEnd_109 (lldb::ValueObjectSP valobj_sp);

            ~NSArrayMSyntheticFrontEnd_109() override;

            bool
            Update() override;
            
        protected:
            lldb::addr_t
            GetDataAddress() override;
            
            uint64_t
            GetUsedCount() override;
            
            uint64_t
            GetOffset() override;
            
            uint64_t
            GetSize() override;
            
        private:
            struct DataDescriptor_32
            {
                uint32_t _used;
                uint32_t _priv1 : 2 ;
                uint32_t _size : 30;
                uint32_t _priv2 : 2;
                uint32_t _offset : 30;
                uint32_t _priv3;
                uint32_t _data;
            };

            struct DataDescriptor_64
            {
                uint64_t _used;
                uint64_t _priv1 : 2 ;
                uint64_t _size : 62;
                uint64_t _priv2 : 2;
                uint64_t _offset : 62;
                uint32_t _priv3;
                uint64_t _data;
            };

            DataDescriptor_32 *m_data_32;
            DataDescriptor_64 *m_data_64;
        };
        
        class NSArrayMSyntheticFrontEnd_1010 : public NSArrayMSyntheticFrontEnd
        {
        public:
            NSArrayMSyntheticFrontEnd_1010 (lldb::ValueObjectSP valobj_sp);

            ~NSArrayMSyntheticFrontEnd_1010() override;

            bool
            Update() override;
            
        protected:
            lldb::addr_t
            GetDataAddress() override;
            
            uint64_t
            GetUsedCount() override;
            
            uint64_t
            GetOffset() override;
            
            uint64_t
            GetSize() override;
            
        private:
            struct DataDescriptor_32
            {
                uint32_t _used;
                uint32_t _offset;
                uint32_t _size : 28;
                uint64_t _priv1 : 4;
                uint32_t _priv2;
                uint32_t _data;
            };

            struct DataDescriptor_64
            {
                uint64_t _used;
                uint64_t _offset;
                uint64_t _size : 60;
                uint64_t _priv1 : 4;
                uint32_t _priv2;
                uint64_t _data;
            };

            DataDescriptor_32 *m_data_32;
            DataDescriptor_64 *m_data_64;
        };
        
        class NSArrayISyntheticFrontEnd : public SyntheticChildrenFrontEnd
        {
        public:
            NSArrayISyntheticFrontEnd (lldb::ValueObjectSP valobj_sp);

            ~NSArrayISyntheticFrontEnd() override = default;

            size_t
            CalculateNumChildren() override;
            
            lldb::ValueObjectSP
            GetChildAtIndex(size_t idx) override;
            
            bool
            Update() override;
            
            bool
            MightHaveChildren() override;
            
            size_t
            GetIndexOfChildWithName(const ConstString &name) override;

        private:
            ExecutionContextRef m_exe_ctx_ref;
            uint8_t m_ptr_size;
            uint64_t m_items;
            lldb::addr_t m_data_ptr;
            CompilerType m_id_type;
            std::vector<lldb::ValueObjectSP> m_children;
        };
        
        class NSArray0SyntheticFrontEnd : public SyntheticChildrenFrontEnd
        {
        public:
            NSArray0SyntheticFrontEnd (lldb::ValueObjectSP valobj_sp);

            ~NSArray0SyntheticFrontEnd() override = default;

            size_t
            CalculateNumChildren() override;
            
            lldb::ValueObjectSP
            GetChildAtIndex(size_t idx) override;
            
            bool
            Update() override;
            
            bool
            MightHaveChildren() override;
            
            size_t
            GetIndexOfChildWithName(const ConstString &name) override;
        };
        
        class NSArrayCodeRunningSyntheticFrontEnd : public SyntheticChildrenFrontEnd
        {
        public:
            NSArrayCodeRunningSyntheticFrontEnd (lldb::ValueObjectSP valobj_sp);

            ~NSArrayCodeRunningSyntheticFrontEnd() override = default;

            size_t
            CalculateNumChildren() override;
            
            lldb::ValueObjectSP
            GetChildAtIndex(size_t idx) override;
            
            bool
            Update() override;
            
            bool
            MightHaveChildren() override;
            
            size_t
            GetIndexOfChildWithName(const ConstString &name) override;
        };
    } // namespace formatters
} // namespace lldb_private

bool
lldb_private::formatters::NSArraySummaryProvider (ValueObject& valobj, Stream& stream, const TypeSummaryOptions& options)
{
    static ConstString g_TypeHint("NSArray");
    
    ProcessSP process_sp = valobj.GetProcessSP();
    if (!process_sp)
        return false;
    
    ObjCLanguageRuntime* runtime = (ObjCLanguageRuntime*)process_sp->GetLanguageRuntime(lldb::eLanguageTypeObjC);
    
    if (!runtime)
        return false;
    
    ObjCLanguageRuntime::ClassDescriptorSP descriptor(runtime->GetClassDescriptor(valobj));
    
    if (!descriptor.get() || !descriptor->IsValid())
        return false;
    
    uint32_t ptr_size = process_sp->GetAddressByteSize();
    
    lldb::addr_t valobj_addr = valobj.GetValueAsUnsigned(0);
    
    if (!valobj_addr)
        return false;
    
    uint64_t value = 0;
    
    const char* class_name = descriptor->GetClassName().GetCString();
    
    if (!class_name || !*class_name)
        return false;
    
    if (!strcmp(class_name,"__NSArrayI"))
    {
        Error error;
        value = process_sp->ReadUnsignedIntegerFromMemory(valobj_addr + ptr_size, ptr_size, 0, error);
        if (error.Fail())
            return false;
    }
    else if (!strcmp(class_name,"__NSArrayM"))
    {
        Error error;
        value = process_sp->ReadUnsignedIntegerFromMemory(valobj_addr + ptr_size, ptr_size, 0, error);
        if (error.Fail())
            return false;
    }
    else if (!strcmp(class_name,"__NSArray0"))
    {
        value = 0;
    }
    else if (!strcmp(class_name,"__NSCFArray"))
    {
        Error error;
        value = process_sp->ReadUnsignedIntegerFromMemory(valobj_addr + 2 * ptr_size, ptr_size, 0, error);
        if (error.Fail())
            return false;
    }
    else
    {
        if (!ExtractValueFromObjCExpression(valobj, "int", "count", value))
            return false;
    }
    
    std::string prefix,suffix;
    if (Language* language = Language::FindPlugin(options.GetLanguage()))
    {
        if (!language->GetFormatterPrefixSuffix(valobj, g_TypeHint, prefix, suffix))
        {
            prefix.clear();
            suffix.clear();
        }
    }
    
    stream.Printf("%s%" PRIu64 " %s%s%s",
                  prefix.c_str(),
                  value,
                  "element",
                  value == 1 ? "" : "s",
                  suffix.c_str());
    return true;
}

lldb_private::formatters::NSArrayMSyntheticFrontEnd::NSArrayMSyntheticFrontEnd (lldb::ValueObjectSP valobj_sp) :
SyntheticChildrenFrontEnd(*valobj_sp),
m_exe_ctx_ref(),
m_ptr_size(8),
m_id_type(),
m_children()
{
    if (valobj_sp)
    {
        clang::ASTContext *ast = valobj_sp->GetExecutionContextRef().GetTargetSP()->GetScratchClangASTContext()->getASTContext();
        if (ast)
            m_id_type = CompilerType(ast, ast->ObjCBuiltinIdTy);
        if (valobj_sp->GetProcessSP())
            m_ptr_size = valobj_sp->GetProcessSP()->GetAddressByteSize();
    }
}

lldb_private::formatters::NSArrayMSyntheticFrontEnd_109::NSArrayMSyntheticFrontEnd_109 (lldb::ValueObjectSP valobj_sp) :
NSArrayMSyntheticFrontEnd(valobj_sp),
m_data_32(NULL),
m_data_64(NULL)
{
}

lldb_private::formatters::NSArrayMSyntheticFrontEnd_1010::NSArrayMSyntheticFrontEnd_1010 (lldb::ValueObjectSP valobj_sp) :
NSArrayMSyntheticFrontEnd(valobj_sp),
m_data_32(NULL),
m_data_64(NULL)
{
}

size_t
lldb_private::formatters::NSArrayMSyntheticFrontEnd::CalculateNumChildren ()
{
    return GetUsedCount();
}

lldb::ValueObjectSP
lldb_private::formatters::NSArrayMSyntheticFrontEnd::GetChildAtIndex (size_t idx)
{
    if (idx >= CalculateNumChildren())
        return lldb::ValueObjectSP();
    lldb::addr_t object_at_idx = GetDataAddress();
    size_t pyhs_idx = idx;
    pyhs_idx += GetOffset();
    if (GetSize() <= pyhs_idx)
        pyhs_idx -= GetSize();
    object_at_idx += (pyhs_idx * m_ptr_size);
    StreamString idx_name;
    idx_name.Printf("[%" PRIu64 "]", (uint64_t)idx);
    lldb::ValueObjectSP retval_sp = CreateValueObjectFromAddress(idx_name.GetData(),
                                                                 object_at_idx,
                                                                 m_exe_ctx_ref,
                                                                 m_id_type);
    m_children.push_back(retval_sp);
    return retval_sp;
}

bool
lldb_private::formatters::NSArrayMSyntheticFrontEnd_109::Update()
{
    m_children.clear();
    ValueObjectSP valobj_sp = m_backend.GetSP();
    m_ptr_size = 0;
    delete m_data_32;
    m_data_32 = NULL;
    delete m_data_64;
    m_data_64 = NULL;
    if (!valobj_sp)
        return false;
    m_exe_ctx_ref = valobj_sp->GetExecutionContextRef();
    Error error;
    error.Clear();
    lldb::ProcessSP process_sp(valobj_sp->GetProcessSP());
    if (!process_sp)
        return false;
    m_ptr_size = process_sp->GetAddressByteSize();
    uint64_t data_location = valobj_sp->GetValueAsUnsigned(0) + m_ptr_size;
    if (m_ptr_size == 4)
    {
        m_data_32 = new DataDescriptor_32();
        process_sp->ReadMemory (data_location, m_data_32, sizeof(DataDescriptor_32), error);
    }
    else
    {
        m_data_64 = new DataDescriptor_64();
        process_sp->ReadMemory (data_location, m_data_64, sizeof(DataDescriptor_64), error);
    }
    if (error.Fail())
        return false;
    return false;
}

bool
lldb_private::formatters::NSArrayMSyntheticFrontEnd_1010::Update()
{
    m_children.clear();
    ValueObjectSP valobj_sp = m_backend.GetSP();
    m_ptr_size = 0;
    delete m_data_32;
    m_data_32 = NULL;
    delete m_data_64;
    m_data_64 = NULL;
    if (!valobj_sp)
        return false;
    m_exe_ctx_ref = valobj_sp->GetExecutionContextRef();
    Error error;
    error.Clear();
    lldb::ProcessSP process_sp(valobj_sp->GetProcessSP());
    if (!process_sp)
        return false;
    m_ptr_size = process_sp->GetAddressByteSize();
    uint64_t data_location = valobj_sp->GetValueAsUnsigned(0) + m_ptr_size;
    if (m_ptr_size == 4)
    {
        m_data_32 = new DataDescriptor_32();
        process_sp->ReadMemory (data_location, m_data_32, sizeof(DataDescriptor_32), error);
    }
    else
    {
        m_data_64 = new DataDescriptor_64();
        process_sp->ReadMemory (data_location, m_data_64, sizeof(DataDescriptor_64), error);
    }
    if (error.Fail())
        return false;
    return false;
}

bool
lldb_private::formatters::NSArrayMSyntheticFrontEnd::MightHaveChildren ()
{
    return true;
}

size_t
lldb_private::formatters::NSArrayMSyntheticFrontEnd::GetIndexOfChildWithName (const ConstString &name)
{
    const char* item_name = name.GetCString();
    uint32_t idx = ExtractIndexFromString(item_name);
    if (idx < UINT32_MAX && idx >= CalculateNumChildren())
        return UINT32_MAX;
    return idx;
}

lldb_private::formatters::NSArrayMSyntheticFrontEnd_109::~NSArrayMSyntheticFrontEnd_109()
{
    delete m_data_32;
    m_data_32 = NULL;
    delete m_data_64;
    m_data_64 = NULL;
}

lldb::addr_t
lldb_private::formatters::NSArrayMSyntheticFrontEnd_109::GetDataAddress ()
{
    if (!m_data_32 && !m_data_64)
        return LLDB_INVALID_ADDRESS;
    return m_data_32 ? m_data_32->_data :
    m_data_64->_data;
}

uint64_t
lldb_private::formatters::NSArrayMSyntheticFrontEnd_109::GetUsedCount ()
{
    if (!m_data_32 && !m_data_64)
        return 0;
    return m_data_32 ? m_data_32->_used :
    m_data_64->_used;
}

uint64_t
lldb_private::formatters::NSArrayMSyntheticFrontEnd_109::GetOffset ()
{
    if (!m_data_32 && !m_data_64)
        return 0;
    return m_data_32 ? m_data_32->_offset :
    m_data_64->_offset;
}

uint64_t
lldb_private::formatters::NSArrayMSyntheticFrontEnd_109::GetSize ()
{
    if (!m_data_32 && !m_data_64)
        return 0;
    return m_data_32 ? m_data_32->_size :
    m_data_64->_size;
}

lldb_private::formatters::NSArrayMSyntheticFrontEnd_1010::~NSArrayMSyntheticFrontEnd_1010()
{
    delete m_data_32;
    m_data_32 = NULL;
    delete m_data_64;
    m_data_64 = NULL;
}

lldb::addr_t
lldb_private::formatters::NSArrayMSyntheticFrontEnd_1010::GetDataAddress ()
{
    if (!m_data_32 && !m_data_64)
        return LLDB_INVALID_ADDRESS;
    return m_data_32 ? m_data_32->_data :
    m_data_64->_data;
}

uint64_t
lldb_private::formatters::NSArrayMSyntheticFrontEnd_1010::GetUsedCount ()
{
    if (!m_data_32 && !m_data_64)
        return 0;
    return m_data_32 ? m_data_32->_used :
    m_data_64->_used;
}

uint64_t
lldb_private::formatters::NSArrayMSyntheticFrontEnd_1010::GetOffset ()
{
    if (!m_data_32 && !m_data_64)
        return 0;
    return m_data_32 ? m_data_32->_offset :
    m_data_64->_offset;
}

uint64_t
lldb_private::formatters::NSArrayMSyntheticFrontEnd_1010::GetSize ()
{
    if (!m_data_32 && !m_data_64)
        return 0;
    return m_data_32 ? m_data_32->_size :
    m_data_64->_size;
}

lldb_private::formatters::NSArrayISyntheticFrontEnd::NSArrayISyntheticFrontEnd (lldb::ValueObjectSP valobj_sp) :
SyntheticChildrenFrontEnd (*valobj_sp.get()),
m_exe_ctx_ref (),
m_ptr_size (8),
m_items (0),
m_data_ptr (0)
{
    if (valobj_sp)
    {
        CompilerType type = valobj_sp->GetCompilerType();
        if (type)
        {
            ClangASTContext *ast = valobj_sp->GetExecutionContextRef().GetTargetSP()->GetScratchClangASTContext();
            if (ast)
                m_id_type = CompilerType(ast->getASTContext(), ast->getASTContext()->ObjCBuiltinIdTy);
        }
    }
}

size_t
lldb_private::formatters::NSArrayISyntheticFrontEnd::GetIndexOfChildWithName (const ConstString &name)
{
    const char* item_name = name.GetCString();
    uint32_t idx = ExtractIndexFromString(item_name);
    if (idx < UINT32_MAX && idx >= CalculateNumChildren())
        return UINT32_MAX;
    return idx;
}

size_t
lldb_private::formatters::NSArrayISyntheticFrontEnd::CalculateNumChildren ()
{
    return m_items;
}

bool
lldb_private::formatters::NSArrayISyntheticFrontEnd::Update()
{
    m_ptr_size = 0;
    m_items = 0;
    m_data_ptr = 0;
    m_children.clear();
    ValueObjectSP valobj_sp = m_backend.GetSP();
    if (!valobj_sp)
        return false;
    m_exe_ctx_ref = valobj_sp->GetExecutionContextRef();
    Error error;
    error.Clear();
    lldb::ProcessSP process_sp(valobj_sp->GetProcessSP());
    if (!process_sp)
        return false;
    m_ptr_size = process_sp->GetAddressByteSize();
    uint64_t data_location = valobj_sp->GetValueAsUnsigned(0) + m_ptr_size;
    m_items = process_sp->ReadPointerFromMemory(data_location, error);
    if (error.Fail())
        return false;
    m_data_ptr = data_location+m_ptr_size;
    return false;
}

bool
lldb_private::formatters::NSArrayISyntheticFrontEnd::MightHaveChildren ()
{
    return true;
}

lldb::ValueObjectSP
lldb_private::formatters::NSArrayISyntheticFrontEnd::GetChildAtIndex (size_t idx)
{
    if (idx >= CalculateNumChildren())
        return lldb::ValueObjectSP();
    lldb::addr_t object_at_idx = m_data_ptr;
    object_at_idx += (idx * m_ptr_size);
    ProcessSP process_sp = m_exe_ctx_ref.GetProcessSP();
    if (!process_sp)
        return lldb::ValueObjectSP();
    Error error;
    if (error.Fail())
        return lldb::ValueObjectSP();
    StreamString idx_name;
    idx_name.Printf("[%" PRIu64 "]", (uint64_t)idx);
    lldb::ValueObjectSP retval_sp = CreateValueObjectFromAddress(idx_name.GetData(),
                                                                 object_at_idx,
                                                                 m_exe_ctx_ref,
                                                                 m_id_type);
    m_children.push_back(retval_sp);
    return retval_sp;
}

lldb_private::formatters::NSArray0SyntheticFrontEnd::NSArray0SyntheticFrontEnd (lldb::ValueObjectSP valobj_sp) :
SyntheticChildrenFrontEnd (*valobj_sp.get())
{
}

size_t
lldb_private::formatters::NSArray0SyntheticFrontEnd::GetIndexOfChildWithName (const ConstString &name)
{
    return UINT32_MAX;
}

size_t
lldb_private::formatters::NSArray0SyntheticFrontEnd::CalculateNumChildren ()
{
    return 0;
}

bool
lldb_private::formatters::NSArray0SyntheticFrontEnd::Update()
{
    return false;
}

bool
lldb_private::formatters::NSArray0SyntheticFrontEnd::MightHaveChildren ()
{
    return false;
}

lldb::ValueObjectSP
lldb_private::formatters::NSArray0SyntheticFrontEnd::GetChildAtIndex (size_t idx)
{
    return lldb::ValueObjectSP();
}

SyntheticChildrenFrontEnd* lldb_private::formatters::NSArraySyntheticFrontEndCreator (CXXSyntheticChildren*, lldb::ValueObjectSP valobj_sp)
{
    if (!valobj_sp)
        return nullptr;
    
    lldb::ProcessSP process_sp (valobj_sp->GetProcessSP());
    if (!process_sp)
        return NULL;
    AppleObjCRuntime *runtime = llvm::dyn_cast_or_null<AppleObjCRuntime>(process_sp->GetObjCLanguageRuntime());
    if (!runtime)
        return NULL;
    
    CompilerType valobj_type(valobj_sp->GetCompilerType());
    Flags flags(valobj_type.GetTypeInfo());
    
    if (flags.IsClear(eTypeIsPointer))
    {
        Error error;
        valobj_sp = valobj_sp->AddressOf(error);
        if (error.Fail() || !valobj_sp)
            return NULL;
    }
    
    ObjCLanguageRuntime::ClassDescriptorSP descriptor(runtime->GetClassDescriptor(*valobj_sp.get()));
    
    if (!descriptor.get() || !descriptor->IsValid())
        return NULL;
    
    const char* class_name = descriptor->GetClassName().GetCString();
    
    if (!class_name || !*class_name)
        return NULL;
    
    if (!strcmp(class_name,"__NSArrayI"))
    {
        return (new NSArrayISyntheticFrontEnd(valobj_sp));
    }
    else if (!strcmp(class_name,"__NSArray0"))
    {
        return (new NSArray0SyntheticFrontEnd(valobj_sp));
    }
    else if (!strcmp(class_name,"__NSArrayM"))
    {
        if (runtime->GetFoundationVersion() >= 1100)
            return (new NSArrayMSyntheticFrontEnd_1010(valobj_sp));
        else
            return (new NSArrayMSyntheticFrontEnd_109(valobj_sp));
    }
    else
    {
        return (new NSArrayCodeRunningSyntheticFrontEnd(valobj_sp));
    }
}

lldb_private::formatters::NSArrayCodeRunningSyntheticFrontEnd::NSArrayCodeRunningSyntheticFrontEnd (lldb::ValueObjectSP valobj_sp) :
SyntheticChildrenFrontEnd(*valobj_sp.get())
{}

size_t
lldb_private::formatters::NSArrayCodeRunningSyntheticFrontEnd::CalculateNumChildren ()
{
    uint64_t count = 0;
    if (ExtractValueFromObjCExpression(m_backend, "int", "count", count))
        return count;
    return 0;
}

lldb::ValueObjectSP
lldb_private::formatters::NSArrayCodeRunningSyntheticFrontEnd::GetChildAtIndex (size_t idx)
{
    StreamString idx_name;
    idx_name.Printf("[%" PRIu64 "]", (uint64_t)idx);
    lldb::ValueObjectSP valobj_sp = CallSelectorOnObject(m_backend,"id","objectAtIndex:",idx);
    if (valobj_sp)
    {
        valobj_sp->SetPreferredDisplayLanguage(m_backend.GetPreferredDisplayLanguage());
        valobj_sp->SetName(ConstString(idx_name.GetData()));
    }
    return valobj_sp;
}

bool
lldb_private::formatters::NSArrayCodeRunningSyntheticFrontEnd::Update()
{
    return false;
}

bool
lldb_private::formatters::NSArrayCodeRunningSyntheticFrontEnd::MightHaveChildren ()
{
    return true;
}

size_t
lldb_private::formatters::NSArrayCodeRunningSyntheticFrontEnd::GetIndexOfChildWithName (const ConstString &name)
{
    return 0;
}
