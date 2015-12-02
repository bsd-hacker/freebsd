//===-- ValueObjectChild.cpp ------------------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "lldb/Core/ValueObjectChild.h"

#include "lldb/Core/Module.h"
#include "lldb/Core/ValueObjectList.h"

#include "lldb/Symbol/ClangASTType.h"
#include "lldb/Symbol/ObjectFile.h"
#include "lldb/Symbol/SymbolContext.h"
#include "lldb/Symbol/Type.h"
#include "lldb/Symbol/Variable.h"

#include "lldb/Target/ExecutionContext.h"
#include "lldb/Target/Process.h"
#include "lldb/Target/Target.h"

using namespace lldb_private;

ValueObjectChild::ValueObjectChild
(
    ValueObject &parent,
    const ClangASTType &clang_type,
    const ConstString &name,
    uint64_t byte_size,
    int32_t byte_offset,
    uint32_t bitfield_bit_size,
    uint32_t bitfield_bit_offset,
    bool is_base_class,
    bool is_deref_of_parent,
    AddressType child_ptr_or_ref_addr_type
) :
    ValueObject (parent),
    m_clang_type (clang_type),
    m_byte_size (byte_size),
    m_byte_offset (byte_offset),
    m_bitfield_bit_size (bitfield_bit_size),
    m_bitfield_bit_offset (bitfield_bit_offset),
    m_is_base_class (is_base_class),
    m_is_deref_of_parent (is_deref_of_parent)
{
    m_name = name;
    SetAddressTypeOfChildren(child_ptr_or_ref_addr_type);
}

ValueObjectChild::~ValueObjectChild()
{
}

lldb::ValueType
ValueObjectChild::GetValueType() const
{
    return m_parent->GetValueType();
}

size_t
ValueObjectChild::CalculateNumChildren()
{
    return GetClangType().GetNumChildren (true);
}

static void
AdjustForBitfieldness(ConstString& name,
                      uint8_t bitfield_bit_size)
{
    if (name && bitfield_bit_size)
    {
        const char *clang_type_name = name.AsCString();
        if (clang_type_name)
        {
            std::vector<char> bitfield_type_name (strlen(clang_type_name) + 32, 0);
            ::snprintf (&bitfield_type_name.front(), bitfield_type_name.size(), "%s:%u", clang_type_name, bitfield_bit_size);
            name.SetCString(&bitfield_type_name.front());
        }
    }
}

ConstString
ValueObjectChild::GetTypeName()
{
    if (m_type_name.IsEmpty())
    {
        m_type_name = GetClangType().GetConstTypeName ();
        AdjustForBitfieldness(m_type_name, m_bitfield_bit_size);
    }
    return m_type_name;
}

ConstString
ValueObjectChild::GetQualifiedTypeName()
{
    ConstString qualified_name = GetClangType().GetConstTypeName();
    AdjustForBitfieldness(qualified_name, m_bitfield_bit_size);
    return qualified_name;
}

ConstString
ValueObjectChild::GetDisplayTypeName()
{
    ConstString display_name = GetClangType().GetDisplayTypeName();
    AdjustForBitfieldness(display_name, m_bitfield_bit_size);
    return display_name;
}

bool
ValueObjectChild::CanUpdateWithInvalidExecutionContext ()
{
    if (m_parent)
        return m_parent->CanUpdateWithInvalidExecutionContext();
    return this->ValueObject::CanUpdateWithInvalidExecutionContext();
}

bool
ValueObjectChild::UpdateValue ()
{
    m_error.Clear();
    SetValueIsValid (false);
    ValueObject* parent = m_parent;
    if (parent)
    {
        if (parent->UpdateValueIfNeeded(false))
        {
            m_value.SetClangType(GetClangType());

            // Copy the parent scalar value and the scalar value type
            m_value.GetScalar() = parent->GetValue().GetScalar();
            Value::ValueType value_type = parent->GetValue().GetValueType();
            m_value.SetValueType (value_type);

            if (parent->GetClangType().IsPointerOrReferenceType ())
            {
                lldb::addr_t addr = parent->GetPointerValue ();
                m_value.GetScalar() = addr;

                if (addr == LLDB_INVALID_ADDRESS)
                {
                    m_error.SetErrorString ("parent address is invalid.");
                }
                else if (addr == 0)
                {
                    m_error.SetErrorString ("parent is NULL");
                }
                else
                {
                    m_value.GetScalar() += m_byte_offset;
                    AddressType addr_type = parent->GetAddressTypeOfChildren();
                    
                    switch (addr_type)
                    {
                        case eAddressTypeFile:
                            {
                                lldb::ProcessSP process_sp (GetProcessSP());
                                if (process_sp && process_sp->IsAlive() == true)
                                    m_value.SetValueType (Value::eValueTypeLoadAddress);
                                else
                                    m_value.SetValueType(Value::eValueTypeFileAddress);
                            }
                            break;
                        case eAddressTypeLoad:
                            m_value.SetValueType (Value::eValueTypeLoadAddress);
                            break;
                        case eAddressTypeHost:
                            m_value.SetValueType(Value::eValueTypeHostAddress);
                            break;
                        case eAddressTypeInvalid:
                            // TODO: does this make sense?
                            m_value.SetValueType(Value::eValueTypeScalar);
                            break;
                    }
                }
            }
            else
            {
                switch (value_type)
                {
                case Value::eValueTypeLoadAddress:
                case Value::eValueTypeFileAddress:
                case Value::eValueTypeHostAddress:
                    {
                        lldb::addr_t addr = m_value.GetScalar().ULongLong(LLDB_INVALID_ADDRESS);
                        if (addr == LLDB_INVALID_ADDRESS)
                        {
                            m_error.SetErrorString ("parent address is invalid.");
                        }
                        else if (addr == 0)
                        {
                            m_error.SetErrorString ("parent is NULL");
                        }
                        else
                        {
                            // Set this object's scalar value to the address of its
                            // value by adding its byte offset to the parent address
                            m_value.GetScalar() += GetByteOffset();
                        }
                    }
                    break;

                case Value::eValueTypeScalar:
                    // TODO: What if this is a register value? Do we try and
                    // extract the child value from within the parent data?
                    // Probably...
                default:
                    m_error.SetErrorString ("parent has invalid value.");
                    break;
                }
            }

            if (m_error.Success())
            {
                const bool thread_and_frame_only_if_stopped = true;
                ExecutionContext exe_ctx (GetExecutionContextRef().Lock(thread_and_frame_only_if_stopped));
                if (GetClangType().GetTypeInfo() & lldb::eTypeHasValue)
                    m_error = m_value.GetValueAsData (&exe_ctx, m_data, 0, GetModule().get());
                else
                    m_error.Clear(); // No value so nothing to read...
            }
        }
        else
        {
            m_error.SetErrorStringWithFormat("parent failed to evaluate: %s", parent->GetError().AsCString());
        }
    }
    else
    {
        m_error.SetErrorString("ValueObjectChild has a NULL parent ValueObject.");
    }
    
    return m_error.Success();
}


bool
ValueObjectChild::IsInScope ()
{
    ValueObject* root(GetRoot());
    if (root)
        return root->IsInScope ();
    return false;
}
