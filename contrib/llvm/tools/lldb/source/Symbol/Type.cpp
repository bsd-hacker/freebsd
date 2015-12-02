//===-- Type.cpp ------------------------------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

// Other libraries and framework includes

#include "lldb/Core/DataExtractor.h"
#include "lldb/Core/DataBufferHeap.h"
#include "lldb/Core/Module.h"
#include "lldb/Core/Scalar.h"
#include "lldb/Core/StreamString.h"

#include "lldb/Symbol/ClangASTType.h"
#include "lldb/Symbol/ClangASTContext.h"
#include "lldb/Symbol/ObjectFile.h"
#include "lldb/Symbol/SymbolContextScope.h"
#include "lldb/Symbol/SymbolFile.h"
#include "lldb/Symbol/SymbolVendor.h"
#include "lldb/Symbol/Type.h"
#include "lldb/Symbol/TypeList.h"

#include "lldb/Target/ExecutionContext.h"
#include "lldb/Target/Process.h"
#include "lldb/Target/Target.h"

#include "llvm/ADT/StringRef.h"

#include "clang/AST/Decl.h"
#include "clang/AST/DeclObjC.h"

using namespace lldb;
using namespace lldb_private;

class TypeAppendVisitor
{
public:
    TypeAppendVisitor(TypeListImpl &type_list) :
        m_type_list(type_list)
    {
    }
    
    bool
    operator() (const lldb::TypeSP& type)
    {
        m_type_list.Append(TypeImplSP(new TypeImpl(type)));
        return true;
    }
    
private:
    TypeListImpl &m_type_list;
};

void
TypeListImpl::Append (const lldb_private::TypeList &type_list)
{
    TypeAppendVisitor cb(*this);
    type_list.ForEach(cb);
}


Type *
SymbolFileType::GetType ()
{
    if (!m_type_sp)
    {
        Type *resolved_type = m_symbol_file.ResolveTypeUID (GetID());
        if (resolved_type)
            m_type_sp = resolved_type->shared_from_this();
    }
    return m_type_sp.get();
}


Type::Type
(
    lldb::user_id_t uid,
    SymbolFile* symbol_file,
    const ConstString &name,
    uint64_t byte_size,
    SymbolContextScope *context,
    user_id_t encoding_uid,
    EncodingDataType encoding_uid_type,
    const Declaration& decl,
    const ClangASTType &clang_type,
    ResolveState clang_type_resolve_state
) :
    std::enable_shared_from_this<Type> (),
    UserID (uid),
    m_name (name),
    m_symbol_file (symbol_file),
    m_context (context),
    m_encoding_type (nullptr),
    m_encoding_uid (encoding_uid),
    m_encoding_uid_type (encoding_uid_type),
    m_byte_size (byte_size),
    m_decl (decl),
    m_clang_type (clang_type)
{
    m_flags.clang_type_resolve_state = (clang_type ? clang_type_resolve_state : eResolveStateUnresolved);
    m_flags.is_complete_objc_class = false;
}

Type::Type () :
    std::enable_shared_from_this<Type> (),
    UserID (0),
    m_name ("<INVALID TYPE>"),
    m_symbol_file (nullptr),
    m_context (nullptr),
    m_encoding_type (nullptr),
    m_encoding_uid (LLDB_INVALID_UID),
    m_encoding_uid_type (eEncodingInvalid),
    m_byte_size (0),
    m_decl (),
    m_clang_type ()
{
    m_flags.clang_type_resolve_state = eResolveStateUnresolved;
    m_flags.is_complete_objc_class = false;
}


Type::Type (const Type &rhs) :
    std::enable_shared_from_this<Type> (rhs),
    UserID (rhs),
    m_name (rhs.m_name),
    m_symbol_file (rhs.m_symbol_file),
    m_context (rhs.m_context),
    m_encoding_type (rhs.m_encoding_type),
    m_encoding_uid (rhs.m_encoding_uid),
    m_encoding_uid_type (rhs.m_encoding_uid_type),
    m_byte_size (rhs.m_byte_size),
    m_decl (rhs.m_decl),
    m_clang_type (rhs.m_clang_type),
    m_flags (rhs.m_flags)
{
}

const Type&
Type::operator= (const Type& rhs)
{
    if (this != &rhs)
    {
    }
    return *this;
}


void
Type::GetDescription (Stream *s, lldb::DescriptionLevel level, bool show_name)
{
    *s << "id = " << (const UserID&)*this;

    // Call the name accessor to make sure we resolve the type name
    if (show_name)
    {
        const ConstString &type_name = GetName();
        if (type_name)
        {
            *s << ", name = \"" << type_name << '"';
            ConstString qualified_type_name (GetQualifiedName());
            if (qualified_type_name != type_name)
            {
                *s << ", qualified = \"" << qualified_type_name << '"';
            }
        }
    }

    // Call the get byte size accesor so we resolve our byte size
    if (GetByteSize())
        s->Printf(", byte-size = %" PRIu64, m_byte_size);
    bool show_fullpaths = (level == lldb::eDescriptionLevelVerbose);
    m_decl.Dump(s, show_fullpaths);

    if (m_clang_type.IsValid())
    {
        *s << ", clang_type = \"";
        GetClangForwardType().DumpTypeDescription(s);
        *s << '"';
    }
    else if (m_encoding_uid != LLDB_INVALID_UID)
    {
        s->Printf(", type_uid = 0x%8.8" PRIx64, m_encoding_uid);
        switch (m_encoding_uid_type)
        {
        case eEncodingInvalid: break;
        case eEncodingIsUID: s->PutCString(" (unresolved type)"); break;
        case eEncodingIsConstUID: s->PutCString(" (unresolved const type)"); break;
        case eEncodingIsRestrictUID: s->PutCString(" (unresolved restrict type)"); break;
        case eEncodingIsVolatileUID: s->PutCString(" (unresolved volatile type)"); break;
        case eEncodingIsTypedefUID: s->PutCString(" (unresolved typedef)"); break;
        case eEncodingIsPointerUID: s->PutCString(" (unresolved pointer)"); break;
        case eEncodingIsLValueReferenceUID: s->PutCString(" (unresolved L value reference)"); break;
        case eEncodingIsRValueReferenceUID: s->PutCString(" (unresolved R value reference)"); break;
        case eEncodingIsSyntheticUID: s->PutCString(" (synthetic type)"); break;
        }
    }    
}


void
Type::Dump (Stream *s, bool show_context)
{
    s->Printf("%p: ", static_cast<void*>(this));
    s->Indent();
    *s << "Type" << static_cast<const UserID&>(*this) << ' ';
    if (m_name)
        *s << ", name = \"" << m_name << "\"";

    if (m_byte_size != 0)
        s->Printf(", size = %" PRIu64, m_byte_size);

    if (show_context && m_context != nullptr)
    {
        s->PutCString(", context = ( ");
        m_context->DumpSymbolContext(s);
        s->PutCString(" )");
    }

    bool show_fullpaths = false;
    m_decl.Dump (s,show_fullpaths);

    if (m_clang_type.IsValid())
    {
        *s << ", clang_type = " << m_clang_type.GetOpaqueQualType() << ' ';
        GetClangForwardType().DumpTypeDescription (s);
    }
    else if (m_encoding_uid != LLDB_INVALID_UID)
    {
        *s << ", type_data = " << (uint64_t)m_encoding_uid;
        switch (m_encoding_uid_type)
        {
        case eEncodingInvalid: break;
        case eEncodingIsUID: s->PutCString(" (unresolved type)"); break;
        case eEncodingIsConstUID: s->PutCString(" (unresolved const type)"); break;
        case eEncodingIsRestrictUID: s->PutCString(" (unresolved restrict type)"); break;
        case eEncodingIsVolatileUID: s->PutCString(" (unresolved volatile type)"); break;
        case eEncodingIsTypedefUID: s->PutCString(" (unresolved typedef)"); break;
        case eEncodingIsPointerUID: s->PutCString(" (unresolved pointer)"); break;
        case eEncodingIsLValueReferenceUID: s->PutCString(" (unresolved L value reference)"); break;
        case eEncodingIsRValueReferenceUID: s->PutCString(" (unresolved R value reference)"); break;
        case eEncodingIsSyntheticUID: s->PutCString(" (synthetic type)"); break;
        }
    }

//
//  if (m_access)
//      s->Printf(", access = %u", m_access);
    s->EOL();
}

const ConstString &
Type::GetName()
{
    if (!m_name)
        m_name = GetClangForwardType().GetConstTypeName();
    return m_name;
}

void
Type::DumpTypeName(Stream *s)
{
    GetName().Dump(s, "<invalid-type-name>");
}


void
Type::DumpValue
(
    ExecutionContext *exe_ctx,
    Stream *s,
    const DataExtractor &data,
    uint32_t data_byte_offset,
    bool show_types,
    bool show_summary,
    bool verbose,
    lldb::Format format
)
{
    if (ResolveClangType(eResolveStateForward))
    {
        if (show_types)
        {
            s->PutChar('(');
            if (verbose)
                s->Printf("Type{0x%8.8" PRIx64 "} ", GetID());
            DumpTypeName (s);
            s->PutCString(") ");
        }

        GetClangForwardType().DumpValue (exe_ctx,
                                         s,
                                         format == lldb::eFormatDefault ? GetFormat() : format,
                                         data,
                                         data_byte_offset,
                                         GetByteSize(),
                                         0, // Bitfield bit size
                                         0, // Bitfield bit offset
                                         show_types,
                                         show_summary,
                                         verbose,
                                         0);
    }
}

Type *
Type::GetEncodingType ()
{
    if (m_encoding_type == nullptr && m_encoding_uid != LLDB_INVALID_UID)
        m_encoding_type = m_symbol_file->ResolveTypeUID(m_encoding_uid);
    return m_encoding_type;
}
    


uint64_t
Type::GetByteSize()
{
    if (m_byte_size == 0)
    {
        switch (m_encoding_uid_type)
        {
        case eEncodingInvalid:
        case eEncodingIsSyntheticUID:
            break;
        case eEncodingIsUID:
        case eEncodingIsConstUID:
        case eEncodingIsRestrictUID:
        case eEncodingIsVolatileUID:
        case eEncodingIsTypedefUID:
            {
                Type *encoding_type = GetEncodingType ();
                if (encoding_type)
                    m_byte_size = encoding_type->GetByteSize();
                if (m_byte_size == 0)
                    m_byte_size = GetClangLayoutType().GetByteSize(nullptr);
            }
            break;

        // If we are a pointer or reference, then this is just a pointer size;
        case eEncodingIsPointerUID:
        case eEncodingIsLValueReferenceUID:
        case eEncodingIsRValueReferenceUID:
            m_byte_size = m_symbol_file->GetClangASTContext().GetPointerByteSize();
            break;
        }
    }
    return m_byte_size;
}


uint32_t
Type::GetNumChildren (bool omit_empty_base_classes)
{
    return GetClangForwardType().GetNumChildren(omit_empty_base_classes);
}

bool
Type::IsAggregateType ()
{
    return GetClangForwardType().IsAggregateType();
}

lldb::TypeSP
Type::GetTypedefType()
{
    lldb::TypeSP type_sp;
    if (IsTypedef())
    {
        Type *typedef_type = m_symbol_file->ResolveTypeUID(m_encoding_uid);
        if (typedef_type)
            type_sp = typedef_type->shared_from_this();
    }
    return type_sp;
}



lldb::Format
Type::GetFormat ()
{
    return GetClangForwardType().GetFormat();
}



lldb::Encoding
Type::GetEncoding (uint64_t &count)
{
    // Make sure we resolve our type if it already hasn't been.
    return GetClangForwardType().GetEncoding(count);
}

bool
Type::DumpValueInMemory
(
    ExecutionContext *exe_ctx,
    Stream *s,
    lldb::addr_t address,
    AddressType address_type,
    bool show_types,
    bool show_summary,
    bool verbose
)
{
    if (address != LLDB_INVALID_ADDRESS)
    {
        DataExtractor data;
        Target *target = nullptr;
        if (exe_ctx)
            target = exe_ctx->GetTargetPtr();
        if (target)
            data.SetByteOrder (target->GetArchitecture().GetByteOrder());
        if (ReadFromMemory (exe_ctx, address, address_type, data))
        {
            DumpValue(exe_ctx, s, data, 0, show_types, show_summary, verbose);
            return true;
        }
    }
    return false;
}


bool
Type::ReadFromMemory (ExecutionContext *exe_ctx, lldb::addr_t addr, AddressType address_type, DataExtractor &data)
{
    if (address_type == eAddressTypeFile)
    {
        // Can't convert a file address to anything valid without more
        // context (which Module it came from)
        return false;
    }

    const uint64_t byte_size = GetByteSize();
    if (data.GetByteSize() < byte_size)
    {
        lldb::DataBufferSP data_sp(new DataBufferHeap (byte_size, '\0'));
        data.SetData(data_sp);
    }

    uint8_t* dst = (uint8_t*)data.PeekData(0, byte_size);
    if (dst != nullptr)
    {
        if (address_type == eAddressTypeHost)
        {
            // The address is an address in this process, so just copy it
            if (addr == 0)
                return false;
            memcpy (dst, (uint8_t*)nullptr + addr, byte_size);
            return true;
        }
        else
        {
            if (exe_ctx)
            {
                Process *process = exe_ctx->GetProcessPtr();
                if (process)
                {
                    Error error;
                    return exe_ctx->GetProcessPtr()->ReadMemory(addr, dst, byte_size, error) == byte_size;
                }
            }
        }
    }
    return false;
}


bool
Type::WriteToMemory (ExecutionContext *exe_ctx, lldb::addr_t addr, AddressType address_type, DataExtractor &data)
{
    return false;
}


TypeList*
Type::GetTypeList()
{
    return GetSymbolFile()->GetTypeList();
}

const Declaration &
Type::GetDeclaration () const
{
    return m_decl;
}

bool
Type::ResolveClangType (ResolveState clang_type_resolve_state)
{
    Type *encoding_type = nullptr;
    if (!m_clang_type.IsValid())
    {
        encoding_type = GetEncodingType();
        if (encoding_type)
        {
            switch (m_encoding_uid_type)
            {
            case eEncodingIsUID:
                {
                    ClangASTType encoding_clang_type = encoding_type->GetClangForwardType();
                    if (encoding_clang_type.IsValid())
                    {
                        m_clang_type = encoding_clang_type;
                        m_flags.clang_type_resolve_state = encoding_type->m_flags.clang_type_resolve_state;
                    }
                }
                break;

            case eEncodingIsConstUID:
                m_clang_type = encoding_type->GetClangForwardType().AddConstModifier();
                break;

            case eEncodingIsRestrictUID:
                m_clang_type = encoding_type->GetClangForwardType().AddRestrictModifier();
                break;

            case eEncodingIsVolatileUID:
                m_clang_type = encoding_type->GetClangForwardType().AddVolatileModifier();
                break;

            case eEncodingIsTypedefUID:
                m_clang_type = encoding_type->GetClangForwardType().CreateTypedefType (GetName().AsCString(),
                                                                                       GetSymbolFile()->GetClangDeclContextContainingTypeUID(GetID()));
                m_name.Clear();
                break;

            case eEncodingIsPointerUID:
                m_clang_type = encoding_type->GetClangForwardType().GetPointerType();
                break;

            case eEncodingIsLValueReferenceUID:
                m_clang_type = encoding_type->GetClangForwardType().GetLValueReferenceType();
                break;

            case eEncodingIsRValueReferenceUID:
                m_clang_type = encoding_type->GetClangForwardType().GetRValueReferenceType();
                break;

            default:
                assert(!"Unhandled encoding_data_type.");
                break;
            }
        }
        else
        {
            // We have no encoding type, return void?
            ClangASTType void_clang_type (ClangASTContext::GetBasicType(GetClangASTContext().getASTContext(), eBasicTypeVoid));
            switch (m_encoding_uid_type)
            {
            case eEncodingIsUID:
                m_clang_type = void_clang_type;
                break;

            case eEncodingIsConstUID:
                m_clang_type = void_clang_type.AddConstModifier ();
                break;

            case eEncodingIsRestrictUID:
                m_clang_type = void_clang_type.AddRestrictModifier ();
                break;

            case eEncodingIsVolatileUID:
                m_clang_type = void_clang_type.AddVolatileModifier ();
                break;

            case eEncodingIsTypedefUID:
                m_clang_type = void_clang_type.CreateTypedefType (GetName().AsCString(),
                                                                  GetSymbolFile()->GetClangDeclContextContainingTypeUID(GetID()));
                break;

            case eEncodingIsPointerUID:
                m_clang_type = void_clang_type.GetPointerType ();
                break;

            case eEncodingIsLValueReferenceUID:
                m_clang_type = void_clang_type.GetLValueReferenceType ();
                break;

            case eEncodingIsRValueReferenceUID:
                m_clang_type = void_clang_type.GetRValueReferenceType ();
                break;

            default:
                assert(!"Unhandled encoding_data_type.");
                break;
            }
        }

        // When we have a EncodingUID, our "m_flags.clang_type_resolve_state" is set to eResolveStateUnresolved
        // so we need to update it to say that we now have a forward declaration since that is what we created
        // above.
        if (m_clang_type.IsValid())
            m_flags.clang_type_resolve_state = eResolveStateForward;

    }

    // Check if we have a forward reference to a class/struct/union/enum?
    if (clang_type_resolve_state == eResolveStateLayout || clang_type_resolve_state == eResolveStateFull)
    {
        // Check if we have a forward reference to a class/struct/union/enum?
        if (m_clang_type.IsValid() && m_flags.clang_type_resolve_state < clang_type_resolve_state)
        {
            m_flags.clang_type_resolve_state = eResolveStateFull;
            if (!m_clang_type.IsDefined ())
            {
                // We have a forward declaration, we need to resolve it to a complete definition.
                m_symbol_file->ResolveClangOpaqueTypeDefinition (m_clang_type);
            }
        }
    }
    
    // If we have an encoding type, then we need to make sure it is 
    // resolved appropriately.
    if (m_encoding_uid != LLDB_INVALID_UID)
    {
        if (encoding_type == nullptr)
            encoding_type = GetEncodingType();
        if (encoding_type)
        {
            ResolveState encoding_clang_type_resolve_state = clang_type_resolve_state;
            
            if (clang_type_resolve_state == eResolveStateLayout)
            {
                switch (m_encoding_uid_type)
                {
                case eEncodingIsPointerUID:
                case eEncodingIsLValueReferenceUID:
                case eEncodingIsRValueReferenceUID:
                    encoding_clang_type_resolve_state = eResolveStateForward;
                    break;
                default:
                    break;
                }
            }
            encoding_type->ResolveClangType (encoding_clang_type_resolve_state);
        }
    }
    return m_clang_type.IsValid();
}
uint32_t
Type::GetEncodingMask ()
{
    uint32_t encoding_mask = 1u << m_encoding_uid_type;
    Type *encoding_type = GetEncodingType();
    assert (encoding_type != this);
    if (encoding_type)
        encoding_mask |= encoding_type->GetEncodingMask ();
    return encoding_mask;
}

ClangASTType
Type::GetClangFullType ()
{
    ResolveClangType(eResolveStateFull);
    return m_clang_type;
}

ClangASTType
Type::GetClangLayoutType ()
{
    ResolveClangType(eResolveStateLayout);
    return m_clang_type;
}

ClangASTType 
Type::GetClangForwardType ()
{
    ResolveClangType (eResolveStateForward);
    return m_clang_type;
}

ClangASTContext &
Type::GetClangASTContext ()
{
    return m_symbol_file->GetClangASTContext();
}

int
Type::Compare(const Type &a, const Type &b)
{
    // Just compare the UID values for now...
    lldb::user_id_t a_uid = a.GetID();
    lldb::user_id_t b_uid = b.GetID();
    if (a_uid < b_uid)
        return -1;
    if (a_uid > b_uid)
        return 1;
    return 0;
//  if (a.getQualType() == b.getQualType())
//      return 0;
}


#if 0  // START REMOVE
// Move this into ClangASTType
void *
Type::CreateClangPointerType (Type *type)
{
    assert(type);
    return GetClangASTContext().CreatePointerType(type->GetClangForwardType());
}

void *
Type::CreateClangTypedefType (Type *typedef_type, Type *base_type)
{
    assert(typedef_type && base_type);
    return GetClangASTContext().CreateTypedefType (typedef_type->GetName().AsCString(), 
                                                   base_type->GetClangForwardType(), 
                                                   typedef_type->GetSymbolFile()->GetClangDeclContextContainingTypeUID(typedef_type->GetID()));
}

void *
Type::CreateClangLValueReferenceType (Type *type)
{
    assert(type);
    return GetClangASTContext().CreateLValueReferenceType(type->GetClangForwardType());
}

void *
Type::CreateClangRValueReferenceType (Type *type)
{
    assert(type);
    return GetClangASTContext().CreateRValueReferenceType (type->GetClangForwardType());
}
#endif // END REMOVE

bool
Type::IsRealObjCClass()
{
    // For now we are just skipping ObjC classes that get made by hand from the runtime, because
    // those don't have any information.  We could extend this to only return true for "full 
    // definitions" if we can figure that out.
    
    if (m_clang_type.IsObjCObjectOrInterfaceType() && GetByteSize() != 0)
        return true;
    else
        return false;
}

ConstString
Type::GetQualifiedName ()
{
    return GetClangForwardType().GetConstTypeName();
}


bool
Type::GetTypeScopeAndBasename (const char* &name_cstr,
                               std::string &scope,
                               std::string &basename,
                               TypeClass &type_class)
{
    // Protect against null c string.
    
    type_class = eTypeClassAny;

    if (name_cstr && name_cstr[0])
    {
        llvm::StringRef name_strref(name_cstr);
        if (name_strref.startswith("struct "))
        {
            name_cstr += 7;
            type_class = eTypeClassStruct;
        }
        else if (name_strref.startswith("class "))
        {
            name_cstr += 6;
            type_class = eTypeClassClass;
        }
        else if (name_strref.startswith("union "))
        {
            name_cstr += 6;
            type_class = eTypeClassUnion;
        }
        else if (name_strref.startswith("enum "))
        {
            name_cstr += 5;
            type_class = eTypeClassEnumeration;
        }
        else if (name_strref.startswith("typedef "))
        {
            name_cstr += 8;
            type_class = eTypeClassTypedef;
        }
        const char *basename_cstr = name_cstr;
        const char* namespace_separator = ::strstr (basename_cstr, "::");
        if (namespace_separator)
        {
            const char* template_arg_char = ::strchr (basename_cstr, '<');
            while (namespace_separator != nullptr)
            {
                if (template_arg_char && namespace_separator > template_arg_char) // but namespace'd template arguments are still good to go
                    break;
                basename_cstr = namespace_separator + 2;
                namespace_separator = strstr(basename_cstr, "::");
            }
            if (basename_cstr > name_cstr)
            {
                scope.assign (name_cstr, basename_cstr - name_cstr);
                basename.assign (basename_cstr);
                return true;
            }
        }
    }
    return false;
}


ModuleSP
Type::GetModule()
{
    if (m_symbol_file)
        return m_symbol_file->GetObjectFile()->GetModule();
    return ModuleSP();
}


TypeAndOrName::TypeAndOrName () : m_type_pair(), m_type_name()
{

}

TypeAndOrName::TypeAndOrName (TypeSP &in_type_sp) : m_type_pair(in_type_sp)
{
    if (in_type_sp)
        m_type_name = in_type_sp->GetName();
}

TypeAndOrName::TypeAndOrName (const char *in_type_str) : m_type_name(in_type_str)
{
}

TypeAndOrName::TypeAndOrName (const TypeAndOrName &rhs) : m_type_pair (rhs.m_type_pair), m_type_name (rhs.m_type_name)
{

}

TypeAndOrName::TypeAndOrName (ConstString &in_type_const_string) : m_type_name (in_type_const_string)
{
}

TypeAndOrName &
TypeAndOrName::operator= (const TypeAndOrName &rhs)
{
    if (this != &rhs)
    {
        m_type_name = rhs.m_type_name;
        m_type_pair = rhs.m_type_pair;
    }
    return *this;
}

bool
TypeAndOrName::operator==(const TypeAndOrName &other) const
{
    if (m_type_pair != other.m_type_pair)
        return false;
    if (m_type_name != other.m_type_name)
        return false;
    return true;
}

bool
TypeAndOrName::operator!=(const TypeAndOrName &other) const
{
    if (m_type_pair != other.m_type_pair)
        return true;
    if (m_type_name != other.m_type_name)
        return true;
    return false;
}

ConstString
TypeAndOrName::GetName () const
{
    if (m_type_name)
        return m_type_name;
    if (m_type_pair)
        return m_type_pair.GetName();
    return ConstString("<invalid>");
}

void
TypeAndOrName::SetName (const ConstString &type_name)
{
    m_type_name = type_name;
}

void
TypeAndOrName::SetName (const char *type_name_cstr)
{
    m_type_name.SetCString (type_name_cstr);
}

void
TypeAndOrName::SetTypeSP (lldb::TypeSP type_sp)
{
    m_type_pair.SetType(type_sp);
    if (m_type_pair)
        m_type_name = m_type_pair.GetName();
}

void
TypeAndOrName::SetClangASTType (ClangASTType clang_type)
{
    m_type_pair.SetType(clang_type);
    if (m_type_pair)
        m_type_name = m_type_pair.GetName();
}

bool
TypeAndOrName::IsEmpty()  const
{
    if ((bool)m_type_name || (bool)m_type_pair)
        return false;
    else
        return true;
}

void
TypeAndOrName::Clear ()
{
    m_type_name.Clear();
    m_type_pair.Clear();
}

bool
TypeAndOrName::HasName () const
{
    return (bool)m_type_name;
}

bool
TypeAndOrName::HasTypeSP () const
{
    return m_type_pair.GetTypeSP().get() != nullptr;
}

bool
TypeAndOrName::HasClangASTType () const
{
    return m_type_pair.GetClangASTType().IsValid();
}


TypeImpl::TypeImpl() :
    m_module_wp(),
    m_static_type(),
    m_dynamic_type()
{
}

TypeImpl::TypeImpl(const TypeImpl& rhs) :
    m_module_wp (rhs.m_module_wp),
    m_static_type(rhs.m_static_type),
    m_dynamic_type(rhs.m_dynamic_type)
{
}

TypeImpl::TypeImpl (const lldb::TypeSP &type_sp) :
    m_module_wp (),
    m_static_type(),
    m_dynamic_type()
{
    SetType (type_sp);
}

TypeImpl::TypeImpl (const ClangASTType &clang_type) :
    m_module_wp (),
    m_static_type(),
    m_dynamic_type()
{
    SetType (clang_type);
}

TypeImpl::TypeImpl (const lldb::TypeSP &type_sp, const ClangASTType &dynamic) :
    m_module_wp (),
    m_static_type (type_sp),
    m_dynamic_type(dynamic)
{
    SetType (type_sp, dynamic);
}

TypeImpl::TypeImpl (const ClangASTType &static_type, const ClangASTType &dynamic_type) :
    m_module_wp (),
    m_static_type (),
    m_dynamic_type()
{
    SetType (static_type, dynamic_type);
}

TypeImpl::TypeImpl (const TypePair &pair, const ClangASTType &dynamic) :
    m_module_wp (),
    m_static_type (),
    m_dynamic_type()
{
    SetType (pair, dynamic);
}

void
TypeImpl::SetType (const lldb::TypeSP &type_sp)
{
    m_static_type.SetType(type_sp);
    if (type_sp)
        m_module_wp = type_sp->GetModule();
    else
        m_module_wp = lldb::ModuleWP();
}

void
TypeImpl::SetType (const ClangASTType &clang_type)
{
    m_module_wp = lldb::ModuleWP();
    m_static_type.SetType (clang_type);
}

void
TypeImpl::SetType (const lldb::TypeSP &type_sp, const ClangASTType &dynamic)
{
    SetType (type_sp);
    m_dynamic_type = dynamic;
}

void
TypeImpl::SetType (const ClangASTType &clang_type, const ClangASTType &dynamic)
{
    m_module_wp = lldb::ModuleWP();
    m_static_type.SetType (clang_type);
    m_dynamic_type = dynamic;
}

void
TypeImpl::SetType (const TypePair &pair, const ClangASTType &dynamic)
{
    m_module_wp = pair.GetModule();
    m_static_type = pair;
    m_dynamic_type = dynamic;
}

TypeImpl&
TypeImpl::operator = (const TypeImpl& rhs)
{
    if (rhs != *this)
    {
        m_module_wp = rhs.m_module_wp;
        m_static_type = rhs.m_static_type;
        m_dynamic_type = rhs.m_dynamic_type;
    }
    return *this;
}

bool
TypeImpl::CheckModule (lldb::ModuleSP &module_sp) const
{
    // Check if we have a module for this type. If we do and the shared pointer is
    // can be successfully initialized with m_module_wp, return true. Else return false
    // if we didn't have a module, or if we had a module and it has been deleted. Any
    // functions doing anything with a TypeSP in this TypeImpl class should call this
    // function and only do anything with the ivars if this function returns true. If
    // we have a module, the "module_sp" will be filled in with a strong reference to the
    // module so that the module will at least stay around long enough for the type
    // query to succeed.
    module_sp = m_module_wp.lock();
    if (!module_sp)
    {
        lldb::ModuleWP empty_module_wp;
        // If either call to "std::weak_ptr::owner_before(...) value returns true, this
        // indicates that m_module_wp once contained (possibly still does) a reference
        // to a valid shared pointer. This helps us know if we had a valid reference to
        // a section which is now invalid because the module it was in was deleted
        if (empty_module_wp.owner_before(m_module_wp) || m_module_wp.owner_before(empty_module_wp))
        {
            // m_module_wp had a valid reference to a module, but all strong references
            // have been released and the module has been deleted
            return false;
        }
    }
    // We either successfully locked the module, or didn't have one to begin with
    return true;
}

bool
TypeImpl::operator == (const TypeImpl& rhs) const
{
    return m_static_type == rhs.m_static_type && m_dynamic_type == rhs.m_dynamic_type;
}

bool
TypeImpl::operator != (const TypeImpl& rhs) const
{
    return m_static_type != rhs.m_static_type || m_dynamic_type != rhs.m_dynamic_type;
}

bool
TypeImpl::IsValid() const
{
    // just a name is not valid
    ModuleSP module_sp;
    if (CheckModule (module_sp))
        return m_static_type.IsValid() || m_dynamic_type.IsValid();
    return false;
}

TypeImpl::operator bool () const
{
    return IsValid();
}

void
TypeImpl::Clear()
{
    m_module_wp = lldb::ModuleWP();
    m_static_type.Clear();
    m_dynamic_type.Clear();
}

ConstString
TypeImpl::GetName ()  const
{
    ModuleSP module_sp;
    if (CheckModule (module_sp))
    {
        if (m_dynamic_type)
            return m_dynamic_type.GetTypeName();
        return m_static_type.GetName ();
    }
    return ConstString();
}

ConstString
TypeImpl::GetDisplayTypeName ()  const
{
    ModuleSP module_sp;
    if (CheckModule (module_sp))
    {
        if (m_dynamic_type)
            return m_dynamic_type.GetDisplayTypeName();
        return m_static_type.GetDisplayTypeName();
    }
    return ConstString();
}

TypeImpl
TypeImpl::GetPointerType () const
{
    ModuleSP module_sp;
    if (CheckModule (module_sp))
    {
        if (m_dynamic_type.IsValid())
        {
            return TypeImpl(m_static_type.GetPointerType(), m_dynamic_type.GetPointerType());
        }
        return TypeImpl(m_static_type.GetPointerType());
    }
    return TypeImpl();
}

TypeImpl
TypeImpl::GetPointeeType () const
{
    ModuleSP module_sp;
    if (CheckModule (module_sp))
    {
        if (m_dynamic_type.IsValid())
        {
            return TypeImpl(m_static_type.GetPointeeType(), m_dynamic_type.GetPointeeType());
        }
        return TypeImpl(m_static_type.GetPointeeType());
    }
    return TypeImpl();
}

TypeImpl
TypeImpl::GetReferenceType () const
{
    ModuleSP module_sp;
    if (CheckModule (module_sp))
    {
        if (m_dynamic_type.IsValid())
        {
            return TypeImpl(m_static_type.GetReferenceType(), m_dynamic_type.GetLValueReferenceType());
        }
        return TypeImpl(m_static_type.GetReferenceType());
    }
    return TypeImpl();
}

TypeImpl
TypeImpl::GetTypedefedType () const
{
    ModuleSP module_sp;
    if (CheckModule (module_sp))
    {
        if (m_dynamic_type.IsValid())
        {
            return TypeImpl(m_static_type.GetTypedefedType(), m_dynamic_type.GetTypedefedType());
        }
        return TypeImpl(m_static_type.GetTypedefedType());
    }
    return TypeImpl();
}

TypeImpl
TypeImpl::GetDereferencedType () const
{
    ModuleSP module_sp;
    if (CheckModule (module_sp))
    {
        if (m_dynamic_type.IsValid())
        {
            return TypeImpl(m_static_type.GetDereferencedType(), m_dynamic_type.GetNonReferenceType());
        }
        return TypeImpl(m_static_type.GetDereferencedType());
    }
    return TypeImpl();
}

TypeImpl
TypeImpl::GetUnqualifiedType() const
{
    ModuleSP module_sp;
    if (CheckModule (module_sp))
    {
        if (m_dynamic_type.IsValid())
        {
            return TypeImpl(m_static_type.GetUnqualifiedType(), m_dynamic_type.GetFullyUnqualifiedType());
        }
        return TypeImpl(m_static_type.GetUnqualifiedType());
    }
    return TypeImpl();
}

TypeImpl
TypeImpl::GetCanonicalType() const
{
    ModuleSP module_sp;
    if (CheckModule (module_sp))
    {
        if (m_dynamic_type.IsValid())
        {
            return TypeImpl(m_static_type.GetCanonicalType(), m_dynamic_type.GetCanonicalType());
        }
        return TypeImpl(m_static_type.GetCanonicalType());
    }
    return TypeImpl();
}

ClangASTType
TypeImpl::GetClangASTType (bool prefer_dynamic)
{
    ModuleSP module_sp;
    if (CheckModule (module_sp))
    {
        if (prefer_dynamic)
        {
            if (m_dynamic_type.IsValid())
                return m_dynamic_type;
        }
        return m_static_type.GetClangASTType();
    }
    return ClangASTType();
}

clang::ASTContext *
TypeImpl::GetClangASTContext (bool prefer_dynamic)
{
    ModuleSP module_sp;
    if (CheckModule (module_sp))
    {
        if (prefer_dynamic)
        {
            if (m_dynamic_type.IsValid())
                return m_dynamic_type.GetASTContext();
        }
        return m_static_type.GetClangASTContext();
    }
    return NULL;
}

bool
TypeImpl::GetDescription (lldb_private::Stream &strm,
                          lldb::DescriptionLevel description_level)
{
    ModuleSP module_sp;
    if (CheckModule (module_sp))
    {
        if (m_dynamic_type.IsValid())
        {
            strm.Printf("Dynamic:\n");
            m_dynamic_type.DumpTypeDescription(&strm);
            strm.Printf("\nStatic:\n");
        }
        m_static_type.GetClangASTType().DumpTypeDescription(&strm);
    }
    else
    {
        strm.PutCString("Invalid TypeImpl module for type has been deleted\n");
    }
    return true;
}

TypeMemberFunctionImpl&
TypeMemberFunctionImpl::operator = (const TypeMemberFunctionImpl& rhs)
{
    if (this != &rhs)
    {
        m_type = rhs.m_type;
        m_objc_method_decl = rhs.m_objc_method_decl;
        m_name = rhs.m_name;
        m_kind = rhs.m_kind;
    }
    return *this;
}

bool
TypeMemberFunctionImpl::IsValid ()
{
    return m_type.IsValid() && m_kind != lldb::eMemberFunctionKindUnknown;
}

ConstString
TypeMemberFunctionImpl::GetName () const
{
    return m_name;
}

ClangASTType
TypeMemberFunctionImpl::GetType () const
{
    return m_type;
}

lldb::MemberFunctionKind
TypeMemberFunctionImpl::GetKind () const
{
    return m_kind;
}

std::string
TypeMemberFunctionImpl::GetPrintableTypeName ()
{
    if (m_type)
        return m_type.GetTypeName().AsCString("<unknown>");
    if (m_objc_method_decl)
    {
        if (m_objc_method_decl->getClassInterface())
        {
            return m_objc_method_decl->getClassInterface()->getName();
        }
    }
    return "<unknown>";
}

bool
TypeMemberFunctionImpl::GetDescription (Stream& stream)
{
    switch (m_kind) {
        case lldb::eMemberFunctionKindUnknown:
            return false;
        case lldb::eMemberFunctionKindConstructor:
            stream.Printf("constructor for %s", GetPrintableTypeName().c_str());
            break;
        case lldb::eMemberFunctionKindDestructor:
            stream.Printf("destructor for %s",  GetPrintableTypeName().c_str());
            break;
        case lldb::eMemberFunctionKindInstanceMethod:
            stream.Printf("instance method %s of type %s",
                          m_name.AsCString(),
                          GetPrintableTypeName().c_str());
            break;
        case lldb::eMemberFunctionKindStaticMethod:
            stream.Printf("static method %s of type %s",
                          m_name.AsCString(),
                          GetPrintableTypeName().c_str());
            break;
    }
    return true;
}

ClangASTType
TypeMemberFunctionImpl::GetReturnType () const
{
    if (m_type)
        return m_type.GetFunctionReturnType();
    if (m_objc_method_decl)
        return ClangASTType(&m_objc_method_decl->getASTContext(),m_objc_method_decl->getReturnType().getAsOpaquePtr());
    return ClangASTType();
}

size_t
TypeMemberFunctionImpl::GetNumArguments () const
{
    if (m_type)
        return m_type.GetNumberOfFunctionArguments();
    if (m_objc_method_decl)
        return m_objc_method_decl->param_size();
    return 0;
}

ClangASTType
TypeMemberFunctionImpl::GetArgumentAtIndex (size_t idx) const
{
    if (m_type)
        return m_type.GetFunctionArgumentAtIndex (idx);
    if (m_objc_method_decl)
    {
        if (idx < m_objc_method_decl->param_size())
            return ClangASTType(&m_objc_method_decl->getASTContext(), m_objc_method_decl->parameters()[idx]->getOriginalType().getAsOpaquePtr());
    }
    return ClangASTType();
}

TypeEnumMemberImpl::TypeEnumMemberImpl (const clang::EnumConstantDecl* enum_member_decl,
                                        const lldb_private::ClangASTType& integer_type) :
    m_integer_type_sp(),
    m_name(),
    m_value(),
    m_valid(false)

{
    if (enum_member_decl)
    {
        m_integer_type_sp.reset(new TypeImpl(integer_type));
        m_name = ConstString(enum_member_decl->getNameAsString().c_str());
        m_value = enum_member_decl->getInitVal();
        m_valid = true;
    }
}
