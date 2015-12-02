//===-- ValueObjectSyntheticFilter.cpp -----------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "lldb/Core/ValueObjectSyntheticFilter.h"

// C Includes
// C++ Includes
// Other libraries and framework includes
// Project includes
#include "lldb/Core/ValueObject.h"
#include "lldb/DataFormatters/TypeSynthetic.h"

using namespace lldb_private;

class DummySyntheticFrontEnd : public SyntheticChildrenFrontEnd
{
public:
    DummySyntheticFrontEnd(ValueObject &backend) :
    SyntheticChildrenFrontEnd(backend)
    {}

    size_t
    CalculateNumChildren()
    {
        return m_backend.GetNumChildren();
    }
    
    lldb::ValueObjectSP
    GetChildAtIndex (size_t idx)
    {
        return m_backend.GetChildAtIndex(idx, true);
    }
    
    size_t
    GetIndexOfChildWithName (const ConstString &name)
    {
        return m_backend.GetIndexOfChildWithName(name);
    }
    
    bool
    MightHaveChildren ()
    {
        return true;
    }
    
    bool
    Update()
    {
        return false;
    }

};

ValueObjectSynthetic::ValueObjectSynthetic (ValueObject &parent, lldb::SyntheticChildrenSP filter) :
    ValueObject(parent),
    m_synth_sp(filter),
    m_children_byindex(),
    m_name_toindex(),
    m_synthetic_children_count(UINT32_MAX),
    m_parent_type_name(parent.GetTypeName()),
    m_might_have_children(eLazyBoolCalculate),
    m_provides_value(eLazyBoolCalculate)
{
#ifdef FOOBAR
    std::string new_name(parent.GetName().AsCString());
    new_name += "$$__synth__";
    SetName (ConstString(new_name.c_str()));
#else
    SetName(parent.GetName());
#endif
    CopyValueData(m_parent);
    CreateSynthFilter();
}

ValueObjectSynthetic::~ValueObjectSynthetic()
{
}

ClangASTType
ValueObjectSynthetic::GetClangTypeImpl ()
{
    return m_parent->GetClangType();
}

ConstString
ValueObjectSynthetic::GetTypeName()
{
    return m_parent->GetTypeName();
}

ConstString
ValueObjectSynthetic::GetQualifiedTypeName()
{
    return m_parent->GetQualifiedTypeName();
}

ConstString
ValueObjectSynthetic::GetDisplayTypeName()
{
    return m_parent->GetDisplayTypeName();
}

size_t
ValueObjectSynthetic::CalculateNumChildren()
{
    UpdateValueIfNeeded();
    if (m_synthetic_children_count < UINT32_MAX)
        return m_synthetic_children_count;
    return (m_synthetic_children_count = m_synth_filter_ap->CalculateNumChildren());
}

lldb::ValueObjectSP
ValueObjectSynthetic::GetDynamicValue (lldb::DynamicValueType valueType)
{
    if (!m_parent)
        return lldb::ValueObjectSP();
    if (IsDynamic() && GetDynamicValueType() == valueType)
        return GetSP();
    return m_parent->GetDynamicValue(valueType);
}

bool
ValueObjectSynthetic::MightHaveChildren()
{
    if (m_might_have_children == eLazyBoolCalculate)
        m_might_have_children = (m_synth_filter_ap->MightHaveChildren() ? eLazyBoolYes : eLazyBoolNo);
    return (m_might_have_children == eLazyBoolNo ? false : true);
}

uint64_t
ValueObjectSynthetic::GetByteSize()
{
    return m_parent->GetByteSize();
}

lldb::ValueType
ValueObjectSynthetic::GetValueType() const
{
    return m_parent->GetValueType();
}

void
ValueObjectSynthetic::CreateSynthFilter ()
{
    m_synth_filter_ap = (m_synth_sp->GetFrontEnd(*m_parent));
    if (!m_synth_filter_ap.get())
        m_synth_filter_ap.reset(new DummySyntheticFrontEnd(*m_parent));
}

bool
ValueObjectSynthetic::UpdateValue ()
{
    SetValueIsValid (false);
    m_error.Clear();

    if (!m_parent->UpdateValueIfNeeded(false))
    {
        // our parent could not update.. as we are meaningless without a parent, just stop
        if (m_parent->GetError().Fail())
            m_error = m_parent->GetError();
        return false;
    }
    
    // regenerate the synthetic filter if our typename changes
    // <rdar://problem/12424824>
    ConstString new_parent_type_name = m_parent->GetTypeName();
    if (new_parent_type_name != m_parent_type_name)
    {
        m_parent_type_name = new_parent_type_name;
        CreateSynthFilter();
    }

    // let our backend do its update
    if (m_synth_filter_ap->Update() == false)
    {
        // filter said that cached values are stale
        m_children_byindex.Clear();
        m_name_toindex.Clear();
        // usually, an object's value can change but this does not alter its children count
        // for a synthetic VO that might indeed happen, so we need to tell the upper echelons
        // that they need to come back to us asking for children
        m_children_count_valid = false;
        m_synthetic_children_count = UINT32_MAX;
        m_might_have_children = eLazyBoolCalculate;
    }
    
    m_provides_value = eLazyBoolCalculate;
    
    lldb::ValueObjectSP synth_val(m_synth_filter_ap->GetSyntheticValue());
    
    if (synth_val && synth_val->CanProvideValue())
    {
        m_provides_value = eLazyBoolYes;
        CopyValueData(synth_val.get());
    }
    else
    {
        m_provides_value = eLazyBoolNo;
        CopyValueData(m_parent);
    }
    
    SetValueIsValid(true);
    return true;
}

lldb::ValueObjectSP
ValueObjectSynthetic::GetChildAtIndex (size_t idx, bool can_create)
{
    UpdateValueIfNeeded();
    
    ValueObject *valobj;
    if (m_children_byindex.GetValueForKey(idx, valobj) == false)
    {
        if (can_create && m_synth_filter_ap.get() != NULL)
        {
            lldb::ValueObjectSP synth_guy = m_synth_filter_ap->GetChildAtIndex (idx);
            if (!synth_guy)
                return synth_guy;
            m_children_byindex.SetValueForKey(idx, synth_guy.get());
            return synth_guy;
        }
        else
            return lldb::ValueObjectSP();
    }
    else
        return valobj->GetSP();
}

lldb::ValueObjectSP
ValueObjectSynthetic::GetChildMemberWithName (const ConstString &name, bool can_create)
{
    UpdateValueIfNeeded();

    uint32_t index = GetIndexOfChildWithName(name);
    
    if (index == UINT32_MAX)
        return lldb::ValueObjectSP();
    
    return GetChildAtIndex(index, can_create);
}

size_t
ValueObjectSynthetic::GetIndexOfChildWithName (const ConstString &name)
{
    UpdateValueIfNeeded();
    
    uint32_t found_index = UINT32_MAX;
    bool did_find = m_name_toindex.GetValueForKey(name.GetCString(), found_index);
    
    if (!did_find && m_synth_filter_ap.get() != NULL)
    {
        uint32_t index = m_synth_filter_ap->GetIndexOfChildWithName (name);
        if (index == UINT32_MAX)
            return index;
        m_name_toindex.SetValueForKey(name.GetCString(), index);
        return index;
    }
    else if (!did_find && m_synth_filter_ap.get() == NULL)
        return UINT32_MAX;
    else /*if (iter != m_name_toindex.end())*/
        return found_index;
}

bool
ValueObjectSynthetic::IsInScope ()
{
    return m_parent->IsInScope();
}

lldb::ValueObjectSP
ValueObjectSynthetic::GetNonSyntheticValue ()
{
    return m_parent->GetSP();
}

void
ValueObjectSynthetic::CopyValueData (ValueObject *source)
{
    m_value = (source->UpdateValueIfNeeded(), source->GetValue());
    ExecutionContext exe_ctx (GetExecutionContextRef());
    m_error = m_value.GetValueAsData (&exe_ctx, m_data, 0, GetModule().get());
}

bool
ValueObjectSynthetic::CanProvideValue ()
{
    if (!UpdateValueIfNeeded())
        return false;
    if (m_provides_value == eLazyBoolYes)
        return true;
    return m_parent->CanProvideValue();
}

bool
ValueObjectSynthetic::SetValueFromCString (const char *value_str, Error& error)
{
    return m_parent->SetValueFromCString(value_str, error);
}

void
ValueObjectSynthetic::SetFormat (lldb::Format format)
{
    if (m_parent)
    {
        m_parent->ClearUserVisibleData(eClearUserVisibleDataItemsAll);
        m_parent->SetFormat(format);
    }
    this->ValueObject::SetFormat(format);
    this->ClearUserVisibleData(eClearUserVisibleDataItemsAll);
}
