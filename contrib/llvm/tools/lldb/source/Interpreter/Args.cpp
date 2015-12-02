//===-- Args.cpp ------------------------------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

// C Includes
#include <cstdlib>
// C++ Includes
// Other libraries and framework includes
// Project includes
#include "lldb/Interpreter/Args.h"
#include "lldb/Core/Stream.h"
#include "lldb/Core/StreamFile.h"
#include "lldb/Core/StreamString.h"
#include "lldb/DataFormatters/FormatManager.h"
#include "lldb/Host/StringConvert.h"
#include "lldb/Interpreter/Options.h"
#include "lldb/Interpreter/CommandInterpreter.h"
#include "lldb/Interpreter/CommandReturnObject.h"
#include "lldb/Target/Process.h"
#include "lldb/Target/StackFrame.h"
#include "lldb/Target/Target.h"

using namespace lldb;
using namespace lldb_private;

//----------------------------------------------------------------------
// Args constructor
//----------------------------------------------------------------------
Args::Args (llvm::StringRef command) :
    m_args(),
    m_argv(),
    m_args_quote_char()
{
    SetCommandString (command);
}


//----------------------------------------------------------------------
// We have to be very careful on the copy constructor of this class
// to make sure we copy all of the string values, but we can't copy the
// rhs.m_argv into m_argv since it will point to the "const char *" c 
// strings in rhs.m_args. We need to copy the string list and update our
// own m_argv appropriately. 
//----------------------------------------------------------------------
Args::Args (const Args &rhs) :
    m_args (rhs.m_args),
    m_argv (),
    m_args_quote_char(rhs.m_args_quote_char)
{
    UpdateArgvFromArgs();
}

//----------------------------------------------------------------------
// We have to be very careful on the copy constructor of this class
// to make sure we copy all of the string values, but we can't copy the
// rhs.m_argv into m_argv since it will point to the "const char *" c 
// strings in rhs.m_args. We need to copy the string list and update our
// own m_argv appropriately. 
//----------------------------------------------------------------------
const Args &
Args::operator= (const Args &rhs)
{
    // Make sure we aren't assigning to self
    if (this != &rhs)
    {
        m_args = rhs.m_args;
        m_args_quote_char = rhs.m_args_quote_char;
        UpdateArgvFromArgs();
    }
    return *this;
}

//----------------------------------------------------------------------
// Destructor
//----------------------------------------------------------------------
Args::~Args ()
{
}

void
Args::Dump (Stream *s)
{
    const size_t argc = m_argv.size();
    for (size_t i=0; i<argc; ++i)
    {
        s->Indent();
        const char *arg_cstr = m_argv[i];
        if (arg_cstr)
            s->Printf("argv[%zi]=\"%s\"\n", i, arg_cstr);
        else
            s->Printf("argv[%zi]=NULL\n", i);
    }
    s->EOL();
}

bool
Args::GetCommandString (std::string &command) const
{
    command.clear();
    const size_t argc = GetArgumentCount();
    for (size_t i=0; i<argc; ++i)
    {
        if (i > 0)
            command += ' ';
        command += m_argv[i];
    }
    return argc > 0;
}

bool
Args::GetQuotedCommandString (std::string &command) const
{
    command.clear ();
    const size_t argc = GetArgumentCount();
    for (size_t i = 0; i < argc; ++i)
    {
        if (i > 0)
            command.append (1, ' ');
        char quote_char = GetArgumentQuoteCharAtIndex(i);
        if (quote_char)
        {
            command.append (1, quote_char);
            command.append (m_argv[i]);
            command.append (1, quote_char);
        }
        else
            command.append (m_argv[i]);
    }
    return argc > 0;
}

// A helper function for argument parsing.
// Parses the initial part of the first argument using normal double quote rules:
// backslash escapes the double quote and itself. The parsed string is appended to the second
// argument. The function returns the unparsed portion of the string, starting at the closing
// quote.
static llvm::StringRef
ParseDoubleQuotes(llvm::StringRef quoted, std::string &result)
{
    // Inside double quotes, '\' and '"' are special.
    static const char *k_escapable_characters = "\"\\";
    while (true)
    {
        // Skip over over regular characters and append them.
        size_t regular = quoted.find_first_of(k_escapable_characters);
        result += quoted.substr(0, regular);
        quoted = quoted.substr(regular);

        // If we have reached the end of string or the closing quote, we're done.
        if (quoted.empty() || quoted.front() == '"')
            break;

        // We have found a backslash.
        quoted = quoted.drop_front();

        if (quoted.empty())
        {
            // A lone backslash at the end of string, let's just append it.
            result += '\\';
            break;
        }

        // If the character after the backslash is not a whitelisted escapable character, we
        // leave the character sequence untouched.
        if (strchr(k_escapable_characters, quoted.front()) == nullptr)
            result += '\\';

        result += quoted.front();
        quoted = quoted.drop_front();
    }

    return quoted;
}

// A helper function for SetCommandString.
// Parses a single argument from the command string, processing quotes and backslashes in a
// shell-like manner. The parsed argument is appended to the m_args array. The function returns
// the unparsed portion of the string, starting at the first unqouted, unescaped whitespace
// character.
llvm::StringRef
Args::ParseSingleArgument(llvm::StringRef command)
{
    // Argument can be split into multiple discontiguous pieces,
    // for example:
    //  "Hello ""World"
    // this would result in a single argument "Hello World" (without/
    // the quotes) since the quotes would be removed and there is
    // not space between the strings.

    std::string arg;

    // Since we can have multiple quotes that form a single command
    // in a command like: "Hello "world'!' (which will make a single
    // argument "Hello world!") we remember the first quote character
    // we encounter and use that for the quote character.
    char first_quote_char = '\0';

    bool arg_complete = false;
    do
    {
        // Skip over over regular characters and append them.
        size_t regular = command.find_first_of(" \t\"'`\\");
        arg += command.substr(0, regular);
        command = command.substr(regular);

        if (command.empty())
            break;

        char special = command.front();
        command = command.drop_front();
        switch (special)
        {
        case '\\':
            if (command.empty())
            {
                arg += '\\';
                break;
            }

            // If the character after the backslash is not a whitelisted escapable character, we
            // leave the character sequence untouched.
            if (strchr(" \t\\'\"`", command.front()) == nullptr)
                arg += '\\';

            arg += command.front();
            command = command.drop_front();

            break;

        case ' ':
        case '\t':
            // We are not inside any quotes, we just found a space after an
            // argument. We are done.
            arg_complete = true;
            break;

        case '"':
        case '\'':
        case '`':
            // We found the start of a quote scope.
            if (first_quote_char == '\0')
                first_quote_char = special;

            if (special == '"')
                command = ParseDoubleQuotes(command, arg);
            else
            {
                // For single quotes, we simply skip ahead to the matching quote character
                // (or the end of the string).
                size_t quoted = command.find(special);
                arg += command.substr(0, quoted);
                command = command.substr(quoted);
            }

            // If we found a closing quote, skip it.
            if (! command.empty())
                command = command.drop_front();

            break;
        }
    } while (!arg_complete);

    m_args.push_back(arg);
    m_args_quote_char.push_back (first_quote_char);
    return command;
}

void
Args::SetCommandString (llvm::StringRef command)
{
    m_args.clear();
    m_argv.clear();
    m_args_quote_char.clear();

    static const char *k_space_separators = " \t";
    command = command.ltrim(k_space_separators);
    while (!command.empty())
    {
        command = ParseSingleArgument(command);
        command = command.ltrim(k_space_separators);
    }

    UpdateArgvFromArgs();
}

void
Args::UpdateArgsAfterOptionParsing()
{
    // Now m_argv might be out of date with m_args, so we need to fix that
    arg_cstr_collection::const_iterator argv_pos, argv_end = m_argv.end();
    arg_sstr_collection::iterator args_pos;
    arg_quote_char_collection::iterator quotes_pos;

    for (argv_pos = m_argv.begin(), args_pos = m_args.begin(), quotes_pos = m_args_quote_char.begin();
         argv_pos != argv_end && args_pos != m_args.end();
         ++argv_pos)
    {
        const char *argv_cstr = *argv_pos;
        if (argv_cstr == nullptr)
            break;

        while (args_pos != m_args.end())
        {
            const char *args_cstr = args_pos->c_str();
            if (args_cstr == argv_cstr)
            {
                // We found the argument that matches the C string in the
                // vector, so we can now look for the next one
                ++args_pos;
                ++quotes_pos;
                break;
            }
            else
            {
                quotes_pos = m_args_quote_char.erase (quotes_pos);
                args_pos = m_args.erase (args_pos);
            }
        }
    }

    if (args_pos != m_args.end())
        m_args.erase (args_pos, m_args.end());

    if (quotes_pos != m_args_quote_char.end())
        m_args_quote_char.erase (quotes_pos, m_args_quote_char.end());
}

void
Args::UpdateArgvFromArgs()
{
    m_argv.clear();
    arg_sstr_collection::const_iterator pos, end = m_args.end();
    for (pos = m_args.begin(); pos != end; ++pos)
        m_argv.push_back(pos->c_str());
    m_argv.push_back(nullptr);
    // Make sure we have enough arg quote chars in the array
    if (m_args_quote_char.size() < m_args.size())
        m_args_quote_char.resize (m_argv.size());
}

size_t
Args::GetArgumentCount() const
{
    if (m_argv.empty())
        return 0;
    return m_argv.size() - 1;
}

const char *
Args::GetArgumentAtIndex (size_t idx) const
{
    if (idx < m_argv.size())
        return m_argv[idx];
    return nullptr;
}

char
Args::GetArgumentQuoteCharAtIndex (size_t idx) const
{
    if (idx < m_args_quote_char.size())
        return m_args_quote_char[idx];
    return '\0';
}

char **
Args::GetArgumentVector()
{
    if (!m_argv.empty())
        return (char **)&m_argv[0];
    return nullptr;
}

const char **
Args::GetConstArgumentVector() const
{
    if (!m_argv.empty())
        return (const char **)&m_argv[0];
    return nullptr;
}

void
Args::Shift ()
{
    // Don't pop the last NULL terminator from the argv array
    if (m_argv.size() > 1)
    {
        m_argv.erase(m_argv.begin());
        m_args.pop_front();
        if (!m_args_quote_char.empty())
            m_args_quote_char.erase(m_args_quote_char.begin());
    }
}

const char *
Args::Unshift (const char *arg_cstr, char quote_char)
{
    m_args.push_front(arg_cstr);
    m_argv.insert(m_argv.begin(), m_args.front().c_str());
    m_args_quote_char.insert(m_args_quote_char.begin(), quote_char);
    return GetArgumentAtIndex (0);
}

void
Args::AppendArguments (const Args &rhs)
{
    const size_t rhs_argc = rhs.GetArgumentCount();
    for (size_t i=0; i<rhs_argc; ++i)
        AppendArgument(rhs.GetArgumentAtIndex(i),
                       rhs.GetArgumentQuoteCharAtIndex(i));
}

void
Args::AppendArguments (const char **argv)
{
    if (argv)
    {
        for (uint32_t i=0; argv[i]; ++i)
            AppendArgument(argv[i]);
    }
}

const char *
Args::AppendArgument (const char *arg_cstr, char quote_char)
{
    return InsertArgumentAtIndex (GetArgumentCount(), arg_cstr, quote_char);
}

const char *
Args::InsertArgumentAtIndex (size_t idx, const char *arg_cstr, char quote_char)
{
    // Since we are using a std::list to hold onto the copied C string and
    // we don't have direct access to the elements, we have to iterate to
    // find the value.
    arg_sstr_collection::iterator pos, end = m_args.end();
    size_t i = idx;
    for (pos = m_args.begin(); i > 0 && pos != end; ++pos)
        --i;

    pos = m_args.insert(pos, arg_cstr);
    
    if (idx >= m_args_quote_char.size())
    {
        m_args_quote_char.resize(idx + 1);
        m_args_quote_char[idx] = quote_char;
    }
    else
        m_args_quote_char.insert(m_args_quote_char.begin() + idx, quote_char);
    
    UpdateArgvFromArgs();
    return GetArgumentAtIndex(idx);
}

const char *
Args::ReplaceArgumentAtIndex (size_t idx, const char *arg_cstr, char quote_char)
{
    // Since we are using a std::list to hold onto the copied C string and
    // we don't have direct access to the elements, we have to iterate to
    // find the value.
    arg_sstr_collection::iterator pos, end = m_args.end();
    size_t i = idx;
    for (pos = m_args.begin(); i > 0 && pos != end; ++pos)
        --i;

    if (pos != end)
    {
        pos->assign(arg_cstr);
        assert(idx < m_argv.size() - 1);
        m_argv[idx] = pos->c_str();
        if (idx >= m_args_quote_char.size())
            m_args_quote_char.resize(idx + 1);
        m_args_quote_char[idx] = quote_char;
        return GetArgumentAtIndex(idx);
    }
    return nullptr;
}

void
Args::DeleteArgumentAtIndex (size_t idx)
{
    // Since we are using a std::list to hold onto the copied C string and
    // we don't have direct access to the elements, we have to iterate to
    // find the value.
    arg_sstr_collection::iterator pos, end = m_args.end();
    size_t i = idx;
    for (pos = m_args.begin(); i > 0 && pos != end; ++pos)
        --i;

    if (pos != end)
    {
        m_args.erase (pos);
        assert(idx < m_argv.size() - 1);
        m_argv.erase(m_argv.begin() + idx);
        if (idx < m_args_quote_char.size())
            m_args_quote_char.erase(m_args_quote_char.begin() + idx);
    }
}

void
Args::SetArguments (size_t argc, const char **argv)
{
    // m_argv will be rebuilt in UpdateArgvFromArgs() below, so there is
    // no need to clear it here.
    m_args.clear();
    m_args_quote_char.clear();

    // First copy each string
    for (size_t i=0; i<argc; ++i)
    {
        m_args.push_back (argv[i]);
        if ((argv[i][0] == '\'') || (argv[i][0] == '"') || (argv[i][0] == '`'))
            m_args_quote_char.push_back (argv[i][0]);
        else
            m_args_quote_char.push_back ('\0');
    }

    UpdateArgvFromArgs();
}

void
Args::SetArguments (const char **argv)
{
    // m_argv will be rebuilt in UpdateArgvFromArgs() below, so there is
    // no need to clear it here.
    m_args.clear();
    m_args_quote_char.clear();
    
    if (argv)
    {
        // First copy each string
        for (size_t i=0; argv[i]; ++i)
        {
            m_args.push_back (argv[i]);
            if ((argv[i][0] == '\'') || (argv[i][0] == '"') || (argv[i][0] == '`'))
                m_args_quote_char.push_back (argv[i][0]);
            else
                m_args_quote_char.push_back ('\0');
        }
    }
    
    UpdateArgvFromArgs();
}


Error
Args::ParseOptions (Options &options)
{
    StreamString sstr;
    Error error;
    Option *long_options = options.GetLongOptions();
    if (long_options == nullptr)
    {
        error.SetErrorStringWithFormat("invalid long options");
        return error;
    }

    for (int i=0; long_options[i].definition != nullptr; ++i)
    {
        if (long_options[i].flag == nullptr)
        {
            if (isprint8(long_options[i].val))
            {
                sstr << (char)long_options[i].val;
                switch (long_options[i].definition->option_has_arg)
                {
                default:
                case OptionParser::eNoArgument:                       break;
                case OptionParser::eRequiredArgument: sstr << ':';    break;
                case OptionParser::eOptionalArgument: sstr << "::";   break;
                }
            }
        }
    }
    OptionParser::Prepare();
    int val;
    while (1)
    {
        int long_options_index = -1;
        val = OptionParser::Parse(GetArgumentCount(),
                                 GetArgumentVector(),
                                 sstr.GetData(),
                                 long_options,
                                 &long_options_index);
        if (val == -1)
            break;

        // Did we get an error?
        if (val == '?')
        {
            error.SetErrorStringWithFormat("unknown or ambiguous option");
            break;
        }
        // The option auto-set itself
        if (val == 0)
            continue;

        ((Options *) &options)->OptionSeen (val);

        // Lookup the long option index
        if (long_options_index == -1)
        {
            for (int i=0;
                 long_options[i].definition || long_options[i].flag || long_options[i].val;
                 ++i)
            {
                if (long_options[i].val == val)
                {
                    long_options_index = i;
                    break;
                }
            }
        }
        // Call the callback with the option
        if (long_options_index >= 0 && long_options[long_options_index].definition)
        {
            const OptionDefinition *def = long_options[long_options_index].definition;
            CommandInterpreter &interpreter = options.GetInterpreter();
            OptionValidator *validator = def->validator;
            if (validator && !validator->IsValid(*interpreter.GetPlatform(true), interpreter.GetExecutionContext()))
            {
                error.SetErrorStringWithFormat("Option \"%s\" invalid.  %s", def->long_option, def->validator->LongConditionString());
            }
            else
            {
                error = options.SetOptionValue(long_options_index,
                                               (def->option_has_arg == OptionParser::eNoArgument) ? nullptr : OptionParser::GetOptionArgument());
            }
        }
        else
        {
            error.SetErrorStringWithFormat("invalid option with value '%i'", val);
        }
        if (error.Fail())
            break;
    }

    // Update our ARGV now that get options has consumed all the options
    m_argv.erase(m_argv.begin(), m_argv.begin() + OptionParser::GetOptionIndex());
    UpdateArgsAfterOptionParsing ();
    return error;
}

void
Args::Clear ()
{
    m_args.clear ();
    m_argv.clear ();
    m_args_quote_char.clear();
}

lldb::addr_t
Args::StringToAddress (const ExecutionContext *exe_ctx, const char *s, lldb::addr_t fail_value, Error *error_ptr)
{
    bool error_set = false;
    if (s && s[0])
    {
        char *end = nullptr;
        lldb::addr_t addr = ::strtoull (s, &end, 0);
        if (*end == '\0')
        {
            if (error_ptr)
                error_ptr->Clear();
            return addr; // All characters were used, return the result
        }
        // Try base 16 with no prefix...
        addr = ::strtoull (s, &end, 16);
        if (*end == '\0')
        {
            if (error_ptr)
                error_ptr->Clear();
            return addr; // All characters were used, return the result
        }
        
        if (exe_ctx)
        {
            Target *target = exe_ctx->GetTargetPtr();
            if (target)
            {
                lldb::ValueObjectSP valobj_sp;
                EvaluateExpressionOptions options;
                options.SetCoerceToId(false);
                options.SetUnwindOnError(true);
                options.SetKeepInMemory(false);
                options.SetTryAllThreads(true);
                
                ExpressionResults expr_result = target->EvaluateExpression(s,
                                                                           exe_ctx->GetFramePtr(),
                                                                           valobj_sp,
                                                                           options);

                bool success = false;
                if (expr_result == eExpressionCompleted)
                {
                    if (valobj_sp)
                        valobj_sp = valobj_sp->GetQualifiedRepresentationIfAvailable(valobj_sp->GetDynamicValueType(), true);
                    // Get the address to watch.
                    if (valobj_sp)
                        addr = valobj_sp->GetValueAsUnsigned(fail_value, &success);
                    if (success)
                    {
                        if (error_ptr)
                            error_ptr->Clear();
                        return addr;
                    }
                    else
                    {
                        if (error_ptr)
                        {
                            error_set = true;
                            error_ptr->SetErrorStringWithFormat("address expression \"%s\" resulted in a value whose type can't be converted to an address: %s", s, valobj_sp->GetTypeName().GetCString());
                        }
                    }
                    
                }
                else
                {
                    // Since the compiler can't handle things like "main + 12" we should
                    // try to do this for now. The compliler doesn't like adding offsets
                    // to function pointer types.
                    static RegularExpression g_symbol_plus_offset_regex("^(.*)([-\\+])[[:space:]]*(0x[0-9A-Fa-f]+|[0-9]+)[[:space:]]*$");
                    RegularExpression::Match regex_match(3);
                    if (g_symbol_plus_offset_regex.Execute(s, &regex_match))
                    {
                        uint64_t offset = 0;
                        bool add = true;
                        std::string name;
                        std::string str;
                        if (regex_match.GetMatchAtIndex(s, 1, name))
                        {
                            if (regex_match.GetMatchAtIndex(s, 2, str))
                            {
                                add = str[0] == '+';
                                
                                if (regex_match.GetMatchAtIndex(s, 3, str))
                                {
                                    offset = StringConvert::ToUInt64(str.c_str(), 0, 0, &success);
                                    
                                    if (success)
                                    {
                                        Error error;
                                        addr = StringToAddress (exe_ctx, name.c_str(), LLDB_INVALID_ADDRESS, &error);
                                        if (addr != LLDB_INVALID_ADDRESS)
                                        {
                                            if (add)
                                                return addr + offset;
                                            else
                                                return addr - offset;
                                        }
                                    }
                                }
                            }
                        }
                    }
                    
                    if (error_ptr)
                    {
                        error_set = true;
                        error_ptr->SetErrorStringWithFormat("address expression \"%s\" evaluation failed", s);
                    }
                }
            }
        }
    }
    if (error_ptr)
    {
        if (!error_set)
            error_ptr->SetErrorStringWithFormat("invalid address expression \"%s\"", s);
    }
    return fail_value;
}

const char *
Args::StripSpaces (std::string &s, bool leading, bool trailing, bool return_null_if_empty)
{
    static const char *k_white_space = " \t\v";
    if (!s.empty())
    {
        if (leading)
        {
            size_t pos = s.find_first_not_of (k_white_space);
            if (pos == std::string::npos)
                s.clear();
            else if (pos > 0)
                s.erase(0, pos);
        }
        
        if (trailing)
        {
            size_t rpos = s.find_last_not_of(k_white_space);
            if (rpos != std::string::npos && rpos + 1 < s.size())
                s.erase(rpos + 1);
        }
    }
    if (return_null_if_empty && s.empty())
        return nullptr;
    return s.c_str();
}

bool
Args::StringToBoolean (const char *s, bool fail_value, bool *success_ptr)
{
    llvm::StringRef ref = llvm::StringRef(s).trim();
    if (ref.equals_lower("false") ||
        ref.equals_lower("off") ||
        ref.equals_lower("no") ||
        ref.equals_lower("0"))
    {
        if (success_ptr)
            *success_ptr = true;
        return false;
    }
    else
    if (ref.equals_lower("true") ||
        ref.equals_lower("on") ||
        ref.equals_lower("yes") ||
        ref.equals_lower("1"))
    {
        if (success_ptr) *success_ptr = true;
        return true;
    }
    if (success_ptr) *success_ptr = false;
    return fail_value;
}

char
Args::StringToChar(const char *s, char fail_value, bool *success_ptr)
{
    bool success = false;
    char result = fail_value;

    if (s)
    {
        size_t length = strlen(s);
        if (length == 1)
        {
            success = true;
            result = s[0];
        }
    }
    if (success_ptr)
        *success_ptr = success;
    return result;
}

const char *
Args::StringToVersion (const char *s, uint32_t &major, uint32_t &minor, uint32_t &update)
{
    major = UINT32_MAX;
    minor = UINT32_MAX;
    update = UINT32_MAX;

    if (s && s[0])
    {
        char *pos = nullptr;
        unsigned long uval32 = ::strtoul (s, &pos, 0);
        if (pos == s)
            return s;
        major = uval32;
        if (*pos == '\0')
        {
            return pos;   // Decoded major and got end of string
        }
        else if (*pos == '.')
        {
            const char *minor_cstr = pos + 1;
            uval32 = ::strtoul (minor_cstr, &pos, 0);
            if (pos == minor_cstr)
                return pos; // Didn't get any digits for the minor version...
            minor = uval32;
            if (*pos == '.')
            {
                const char *update_cstr = pos + 1;
                uval32 = ::strtoul (update_cstr, &pos, 0);
                if (pos == update_cstr)
                    return pos;
                update = uval32;
            }
            return pos;
        }
    }
    return nullptr;
}

const char *
Args::GetShellSafeArgument (const char *unsafe_arg, std::string &safe_arg)
{
    safe_arg.assign (unsafe_arg);
    size_t prev_pos = 0;
    while (prev_pos < safe_arg.size())
    {
        // Escape spaces and quotes
        size_t pos = safe_arg.find_first_of(" '\"", prev_pos);
        if (pos != std::string::npos)
        {
            safe_arg.insert (pos, 1, '\\');
            prev_pos = pos + 2;
        }
        else
            break;
    }
    return safe_arg.c_str();
}


int64_t
Args::StringToOptionEnum (const char *s, OptionEnumValueElement *enum_values, int32_t fail_value, Error &error)
{    
    if (enum_values)
    {
        if (s && s[0])
        {
            for (int i = 0; enum_values[i].string_value != nullptr ; i++)
            {
                if (strstr(enum_values[i].string_value, s) == enum_values[i].string_value)
                {
                    error.Clear();
                    return enum_values[i].value;
                }
            }
        }

        StreamString strm;
        strm.PutCString ("invalid enumeration value, valid values are: ");
        for (int i = 0; enum_values[i].string_value != nullptr; i++)
        {
            strm.Printf ("%s\"%s\"", 
                         i > 0 ? ", " : "",
                         enum_values[i].string_value);
        }
        error.SetErrorString(strm.GetData());
    }
    else
    {
        error.SetErrorString ("invalid enumeration argument");
    }
    return fail_value;
}

ScriptLanguage
Args::StringToScriptLanguage (const char *s, ScriptLanguage fail_value, bool *success_ptr)
{
    if (s && s[0])
    {
        if ((::strcasecmp (s, "python") == 0) ||
            (::strcasecmp (s, "default") == 0 && eScriptLanguagePython == eScriptLanguageDefault))
        {
            if (success_ptr) *success_ptr = true;
            return eScriptLanguagePython;
        }
        if (::strcasecmp (s, "none"))
        {
            if (success_ptr) *success_ptr = true;
            return eScriptLanguageNone;
        }
    }
    if (success_ptr) *success_ptr = false;
    return fail_value;
}

Error
Args::StringToFormat
(
    const char *s,
    lldb::Format &format,
    size_t *byte_size_ptr
)
{
    format = eFormatInvalid;
    Error error;

    if (s && s[0])
    {
        if (byte_size_ptr)
        {
            if (isdigit (s[0]))
            {
                char *format_char = nullptr;
                unsigned long byte_size = ::strtoul (s, &format_char, 0);
                if (byte_size != ULONG_MAX)
                    *byte_size_ptr = byte_size;
                s = format_char;
            }
            else
                *byte_size_ptr = 0;
        }

        const bool partial_match_ok = true;
        if (!FormatManager::GetFormatFromCString (s, partial_match_ok, format))
        {
            StreamString error_strm;
            error_strm.Printf ("Invalid format character or name '%s'. Valid values are:\n", s);
            for (Format f = eFormatDefault; f < kNumFormats; f = Format(f+1))
            {
                char format_char = FormatManager::GetFormatAsFormatChar(f);
                if (format_char)
                    error_strm.Printf ("'%c' or ", format_char);
            
                error_strm.Printf ("\"%s\"", FormatManager::GetFormatAsCString(f));
                error_strm.EOL();
            }
            
            if (byte_size_ptr)
                error_strm.PutCString ("An optional byte size can precede the format character.\n");
            error.SetErrorString(error_strm.GetString().c_str());
        }

        if (error.Fail())
            return error;
    }
    else
    {
        error.SetErrorStringWithFormat("%s option string", s ? "empty" : "invalid");
    }
    return error;
}

lldb::Encoding
Args::StringToEncoding (const char *s, lldb::Encoding fail_value)
{
    if (s && s[0])
    {
        if (strcmp(s, "uint") == 0)
            return eEncodingUint;
        else if (strcmp(s, "sint") == 0)
            return eEncodingSint;
        else if (strcmp(s, "ieee754") == 0)
            return eEncodingIEEE754;
        else if (strcmp(s, "vector") == 0)
            return eEncodingVector;
    }
    return fail_value;
}

uint32_t
Args::StringToGenericRegister (const char *s)
{
    if (s && s[0])
    {
        if (strcmp(s, "pc") == 0)
            return LLDB_REGNUM_GENERIC_PC;
        else if (strcmp(s, "sp") == 0)
            return LLDB_REGNUM_GENERIC_SP;
        else if (strcmp(s, "fp") == 0)
            return LLDB_REGNUM_GENERIC_FP;
        else if (strcmp(s, "ra") == 0 || strcmp(s, "lr") == 0)
            return LLDB_REGNUM_GENERIC_RA;
        else if (strcmp(s, "flags") == 0)
            return LLDB_REGNUM_GENERIC_FLAGS;
        else if (strncmp(s, "arg", 3) == 0)
        {
            if (s[3] && s[4] == '\0')
            {
                switch (s[3])
                {
                    case '1': return LLDB_REGNUM_GENERIC_ARG1;
                    case '2': return LLDB_REGNUM_GENERIC_ARG2;
                    case '3': return LLDB_REGNUM_GENERIC_ARG3;
                    case '4': return LLDB_REGNUM_GENERIC_ARG4;
                    case '5': return LLDB_REGNUM_GENERIC_ARG5;
                    case '6': return LLDB_REGNUM_GENERIC_ARG6;
                    case '7': return LLDB_REGNUM_GENERIC_ARG7;
                    case '8': return LLDB_REGNUM_GENERIC_ARG8;
                }
            }
        }
    }
    return LLDB_INVALID_REGNUM;
}


void
Args::LongestCommonPrefix (std::string &common_prefix)
{
    arg_sstr_collection::iterator pos, end = m_args.end();
    pos = m_args.begin();
    if (pos == end)
        common_prefix.clear();
    else
        common_prefix = (*pos);

    for (++pos; pos != end; ++pos)
    {
        size_t new_size = (*pos).size();

        // First trim common_prefix if it is longer than the current element:
        if (common_prefix.size() > new_size)
            common_prefix.erase (new_size);

        // Then trim it at the first disparity:

        for (size_t i = 0; i < common_prefix.size(); i++)
        {
            if ((*pos)[i]  != common_prefix[i])
            {
                common_prefix.erase(i);
                break;
            }
        }

        // If we've emptied the common prefix, we're done.
        if (common_prefix.empty())
            break;
    }
}

size_t
Args::FindArgumentIndexForOption (Option *long_options, int long_options_index)
{
    char short_buffer[3];
    char long_buffer[255];
    ::snprintf (short_buffer, sizeof (short_buffer), "-%c", long_options[long_options_index].val);
    ::snprintf (long_buffer, sizeof (long_buffer),  "--%s", long_options[long_options_index].definition->long_option);
    size_t end = GetArgumentCount ();
    size_t idx = 0;
    while (idx < end)
    {   
        if ((::strncmp (GetArgumentAtIndex (idx), short_buffer, strlen (short_buffer)) == 0)
            || (::strncmp (GetArgumentAtIndex (idx), long_buffer, strlen (long_buffer)) == 0))
            {
                return idx;
            }
        ++idx;
    }

    return end;
}

bool
Args::IsPositionalArgument (const char *arg)
{
    if (arg == nullptr)
        return false;
        
    bool is_positional = true;
    const char *cptr = arg;
    
    if (cptr[0] == '%')
    {
        ++cptr;
        while (isdigit (cptr[0]))
            ++cptr;
        if (cptr[0] != '\0')
            is_positional = false;
    }
    else
        is_positional = false;

    return is_positional;
}

void
Args::ParseAliasOptions (Options &options,
                         CommandReturnObject &result,
                         OptionArgVector *option_arg_vector,
                         std::string &raw_input_string)
{
    StreamString sstr;
    int i;
    Option *long_options = options.GetLongOptions();

    if (long_options == nullptr)
    {
        result.AppendError ("invalid long options");
        result.SetStatus (eReturnStatusFailed);
        return;
    }

    for (i = 0; long_options[i].definition != nullptr; ++i)
    {
        if (long_options[i].flag == nullptr)
        {
            sstr << (char) long_options[i].val;
            switch (long_options[i].definition->option_has_arg)
            {
                default:
                case OptionParser::eNoArgument:
                    break;
                case OptionParser::eRequiredArgument:
                    sstr << ":";
                    break;
                case OptionParser::eOptionalArgument:
                    sstr << "::";
                    break;
            }
        }
    }

    OptionParser::Prepare();
    int val;
    while (1)
    {
        int long_options_index = -1;
        val = OptionParser::Parse (GetArgumentCount(),
                                  GetArgumentVector(),
                                  sstr.GetData(),
                                  long_options,
                                  &long_options_index);

        if (val == -1)
            break;

        if (val == '?')
        {
            result.AppendError ("unknown or ambiguous option");
            result.SetStatus (eReturnStatusFailed);
            break;
        }

        if (val == 0)
            continue;

        options.OptionSeen (val);

        // Look up the long option index
        if (long_options_index == -1)
        {
            for (int j = 0;
                 long_options[j].definition || long_options[j].flag || long_options[j].val;
                 ++j)
            {
                if (long_options[j].val == val)
                {
                    long_options_index = j;
                    break;
                }
            }
        }

        // See if the option takes an argument, and see if one was supplied.
        if (long_options_index >= 0)
        {
            StreamString option_str;
            option_str.Printf ("-%c", val);
            const OptionDefinition *def = long_options[long_options_index].definition;
            int has_arg = (def == nullptr) ? OptionParser::eNoArgument : def->option_has_arg;

            switch (has_arg)
            {
            case OptionParser::eNoArgument:
                option_arg_vector->push_back (OptionArgPair (std::string (option_str.GetData()), 
                                                             OptionArgValue (OptionParser::eNoArgument, "<no-argument>")));
                result.SetStatus (eReturnStatusSuccessFinishNoResult);
                break;
            case OptionParser::eRequiredArgument:
                if (OptionParser::GetOptionArgument() != nullptr)
                {
                    option_arg_vector->push_back (OptionArgPair (std::string (option_str.GetData()),
                                                                 OptionArgValue (OptionParser::eRequiredArgument,
                                                                                 std::string (OptionParser::GetOptionArgument()))));
                    result.SetStatus (eReturnStatusSuccessFinishNoResult);
                }
                else
                {
                    result.AppendErrorWithFormat ("Option '%s' is missing argument specifier.\n",
                                                 option_str.GetData());
                    result.SetStatus (eReturnStatusFailed);
                }
                break;
            case OptionParser::eOptionalArgument:
                if (OptionParser::GetOptionArgument() != nullptr)
                {
                    option_arg_vector->push_back (OptionArgPair (std::string (option_str.GetData()),
                                                                 OptionArgValue (OptionParser::eOptionalArgument,
                                                                                 std::string (OptionParser::GetOptionArgument()))));
                    result.SetStatus (eReturnStatusSuccessFinishNoResult);
                }
                else
                {
                    option_arg_vector->push_back (OptionArgPair (std::string (option_str.GetData()),
                                                                 OptionArgValue (OptionParser::eOptionalArgument, "<no-argument>")));
                    result.SetStatus (eReturnStatusSuccessFinishNoResult);
                }
                break;
            default:
                result.AppendErrorWithFormat ("error with options table; invalid value in has_arg field for option '%c'.\n", val);
                result.SetStatus (eReturnStatusFailed);
                break;
            }
        }
        else
        {
            result.AppendErrorWithFormat ("Invalid option with value '%c'.\n", val);
            result.SetStatus (eReturnStatusFailed);
        }

        if (long_options_index >= 0)
        {
            // Find option in the argument list; also see if it was supposed to take an argument and if one was
            // supplied.  Remove option (and argument, if given) from the argument list.  Also remove them from
            // the raw_input_string, if one was passed in.
            size_t idx = FindArgumentIndexForOption (long_options, long_options_index);
            if (idx < GetArgumentCount())
            {
                if (raw_input_string.size() > 0)
                {
                    const char *tmp_arg = GetArgumentAtIndex (idx);
                    size_t pos = raw_input_string.find (tmp_arg);
                    if (pos != std::string::npos)
                        raw_input_string.erase (pos, strlen (tmp_arg));
                }
                ReplaceArgumentAtIndex (idx, "");
                if ((long_options[long_options_index].definition->option_has_arg != OptionParser::eNoArgument)
                    && (OptionParser::GetOptionArgument() != nullptr)
                    && (idx+1 < GetArgumentCount())
                    && (strcmp (OptionParser::GetOptionArgument(), GetArgumentAtIndex(idx+1)) == 0))
                {
                    if (raw_input_string.size() > 0)
                    {
                        const char *tmp_arg = GetArgumentAtIndex (idx+1);
                        size_t pos = raw_input_string.find (tmp_arg);
                        if (pos != std::string::npos)
                            raw_input_string.erase (pos, strlen (tmp_arg));
                    }
                    ReplaceArgumentAtIndex (idx+1, "");
                }
            }
        }

        if (!result.Succeeded())
            break;
    }
}

void
Args::ParseArgsForCompletion
(
    Options &options,
    OptionElementVector &option_element_vector,
    uint32_t cursor_index
)
{
    StreamString sstr;
    Option *long_options = options.GetLongOptions();
    option_element_vector.clear();

    if (long_options == nullptr)
    {
        return;
    }

    // Leading : tells getopt to return a : for a missing option argument AND
    // to suppress error messages.

    sstr << ":";
    for (int i = 0; long_options[i].definition != nullptr; ++i)
    {
        if (long_options[i].flag == nullptr)
        {
            sstr << (char) long_options[i].val;
            switch (long_options[i].definition->option_has_arg)
            {
                default:
                case OptionParser::eNoArgument:
                    break;
                case OptionParser::eRequiredArgument:
                    sstr << ":";
                    break;
                case OptionParser::eOptionalArgument:
                    sstr << "::";
                    break;
            }
        }
    }

    OptionParser::Prepare();
    OptionParser::EnableError(false);

    int val;
    const OptionDefinition *opt_defs = options.GetDefinitions();

    // Fooey... OptionParser::Parse permutes the GetArgumentVector to move the options to the front.
    // So we have to build another Arg and pass that to OptionParser::Parse so it doesn't
    // change the one we have.

    std::vector<const char *> dummy_vec (GetArgumentVector(), GetArgumentVector() + GetArgumentCount() + 1);

    bool failed_once = false;
    uint32_t dash_dash_pos = -1;
        
    while (1)
    {
        bool missing_argument = false;
        int long_options_index = -1;
        
        val = OptionParser::Parse (dummy_vec.size() - 1,
                                  (char *const *) &dummy_vec.front(),
                                  sstr.GetData(),
                                  long_options,
                                  &long_options_index);

        if (val == -1)
        {
            // When we're completing a "--" which is the last option on line, 
            if (failed_once)
                break;
                
            failed_once = true;
            
            // If this is a bare  "--" we mark it as such so we can complete it successfully later.
            // Handling the "--" is a little tricky, since that may mean end of options or arguments, or the
            // user might want to complete options by long name.  I make this work by checking whether the
            // cursor is in the "--" argument, and if so I assume we're completing the long option, otherwise
            // I let it pass to OptionParser::Parse which will terminate the option parsing.
            // Note, in either case we continue parsing the line so we can figure out what other options
            // were passed.  This will be useful when we come to restricting completions based on what other
            // options we've seen on the line.

            if (static_cast<size_t>(OptionParser::GetOptionIndex()) < dummy_vec.size() - 1
                && (strcmp (dummy_vec[OptionParser::GetOptionIndex()-1], "--") == 0))
            {
                dash_dash_pos = OptionParser::GetOptionIndex() - 1;
                if (static_cast<size_t>(OptionParser::GetOptionIndex() - 1) == cursor_index)
                {
                    option_element_vector.push_back (OptionArgElement (OptionArgElement::eBareDoubleDash, OptionParser::GetOptionIndex() - 1,
                                                                   OptionArgElement::eBareDoubleDash));
                    continue;
                }
                else
                    break;
            }
            else
                break;
        }
        else if (val == '?')
        {
            option_element_vector.push_back (OptionArgElement (OptionArgElement::eUnrecognizedArg, OptionParser::GetOptionIndex() - 1,
                                                               OptionArgElement::eUnrecognizedArg));
            continue;
        }
        else if (val == 0)
        {
            continue;
        }
        else if (val == ':')
        {
            // This is a missing argument.
            val = OptionParser::GetOptionErrorCause();
            missing_argument = true;
        }

        ((Options *) &options)->OptionSeen (val);

        // Look up the long option index
        if (long_options_index == -1)
        {
            for (int j = 0;
                 long_options[j].definition || long_options[j].flag || long_options[j].val;
                 ++j)
            {
                if (long_options[j].val == val)
                {
                    long_options_index = j;
                    break;
                }
            }
        }

        // See if the option takes an argument, and see if one was supplied.
        if (long_options_index >= 0)
        {
            int opt_defs_index = -1;
            for (int i = 0; ; i++)
            {
                if (opt_defs[i].short_option == 0)
                    break;
                else if (opt_defs[i].short_option == val)
                {
                    opt_defs_index = i;
                    break;
                }
            }

            const OptionDefinition *def = long_options[long_options_index].definition;
            int has_arg = (def == nullptr) ? OptionParser::eNoArgument : def->option_has_arg;
            switch (has_arg)
            {
            case OptionParser::eNoArgument:
                option_element_vector.push_back (OptionArgElement (opt_defs_index, OptionParser::GetOptionIndex() - 1, 0));
                break;
            case OptionParser::eRequiredArgument:
                if (OptionParser::GetOptionArgument() != nullptr)
                {
                    int arg_index;
                    if (missing_argument)
                        arg_index = -1;
                    else
                        arg_index = OptionParser::GetOptionIndex() - 1;

                    option_element_vector.push_back (OptionArgElement (opt_defs_index, OptionParser::GetOptionIndex() - 2, arg_index));
                }
                else
                {
                    option_element_vector.push_back (OptionArgElement (opt_defs_index, OptionParser::GetOptionIndex() - 1, -1));
                }
                break;
            case OptionParser::eOptionalArgument:
                if (OptionParser::GetOptionArgument() != nullptr)
                {
                    option_element_vector.push_back (OptionArgElement (opt_defs_index, OptionParser::GetOptionIndex() - 2, OptionParser::GetOptionIndex() - 1));
                }
                else
                {
                    option_element_vector.push_back (OptionArgElement (opt_defs_index, OptionParser::GetOptionIndex() - 2, OptionParser::GetOptionIndex() - 1));
                }
                break;
            default:
                // The options table is messed up.  Here we'll just continue
                option_element_vector.push_back (OptionArgElement (OptionArgElement::eUnrecognizedArg, OptionParser::GetOptionIndex() - 1,
                                                                   OptionArgElement::eUnrecognizedArg));
                break;
            }
        }
        else
        {
            option_element_vector.push_back (OptionArgElement (OptionArgElement::eUnrecognizedArg, OptionParser::GetOptionIndex() - 1,
                                                               OptionArgElement::eUnrecognizedArg));
        }
    }
    
    // Finally we have to handle the case where the cursor index points at a single "-".  We want to mark that in
    // the option_element_vector, but only if it is not after the "--".  But it turns out that OptionParser::Parse just ignores
    // an isolated "-".  So we have to look it up by hand here.  We only care if it is AT the cursor position.
    // Note, a single quoted dash is not the same as a single dash...
    
    if ((static_cast<int32_t>(dash_dash_pos) == -1 || cursor_index < dash_dash_pos)
         && m_args_quote_char[cursor_index] == '\0'
         && strcmp (GetArgumentAtIndex(cursor_index), "-") == 0)
    {
        option_element_vector.push_back (OptionArgElement (OptionArgElement::eBareDash, cursor_index, 
                                                           OptionArgElement::eBareDash));
        
    }
}

void
Args::EncodeEscapeSequences (const char *src, std::string &dst)
{
    dst.clear();
    if (src)
    {
        for (const char *p = src; *p != '\0'; ++p)
        {
            size_t non_special_chars = ::strcspn (p, "\\");
            if (non_special_chars > 0)
            {
                dst.append(p, non_special_chars);
                p += non_special_chars;
                if (*p == '\0')
                    break;
            }
            
            if (*p == '\\')
            {
                ++p; // skip the slash
                switch (*p)
                {
                    case 'a' : dst.append(1, '\a'); break;
                    case 'b' : dst.append(1, '\b'); break;
                    case 'f' : dst.append(1, '\f'); break;
                    case 'n' : dst.append(1, '\n'); break;
                    case 'r' : dst.append(1, '\r'); break;
                    case 't' : dst.append(1, '\t'); break;
                    case 'v' : dst.append(1, '\v'); break;
                    case '\\': dst.append(1, '\\'); break;
                    case '\'': dst.append(1, '\''); break;
                    case '"' : dst.append(1, '"'); break;
                    case '0' :
                        // 1 to 3 octal chars
                    {
                        // Make a string that can hold onto the initial zero char,
                        // up to 3 octal digits, and a terminating NULL.
                        char oct_str[5] = { '\0', '\0', '\0', '\0', '\0' };
                        
                        int i;
                        for (i=0; (p[i] >= '0' && p[i] <= '7') && i<4; ++i)
                            oct_str[i] = p[i];
                        
                        // We don't want to consume the last octal character since
                        // the main for loop will do this for us, so we advance p by
                        // one less than i (even if i is zero)
                        p += i - 1;
                        unsigned long octal_value = ::strtoul (oct_str, nullptr, 8);
                        if (octal_value <= UINT8_MAX)
                        {
                            dst.append(1, (char)octal_value);
                        }
                    }
                        break;
                        
                    case 'x':
                        // hex number in the format
                        if (isxdigit(p[1]))
                        {
                            ++p;    // Skip the 'x'
                            
                            // Make a string that can hold onto two hex chars plus a
                            // NULL terminator
                            char hex_str[3] = { *p, '\0', '\0' };
                            if (isxdigit(p[1]))
                            {
                                ++p; // Skip the first of the two hex chars
                                hex_str[1] = *p;
                            }
                            
                            unsigned long hex_value = strtoul (hex_str, nullptr, 16);
                            if (hex_value <= UINT8_MAX)
                                dst.append (1, (char)hex_value);
                        }
                        else
                        {
                            dst.append(1, 'x');
                        }
                        break;
                        
                    default:
                        // Just desensitize any other character by just printing what
                        // came after the '\'
                        dst.append(1, *p);
                        break;
                        
                }
            }
        }
    }
}


void
Args::ExpandEscapedCharacters (const char *src, std::string &dst)
{
    dst.clear();
    if (src)
    {
        for (const char *p = src; *p != '\0'; ++p)
        {
            if (isprint8(*p))
                dst.append(1, *p);
            else
            {
                switch (*p)
                {
                    case '\a': dst.append("\\a"); break;
                    case '\b': dst.append("\\b"); break;
                    case '\f': dst.append("\\f"); break;
                    case '\n': dst.append("\\n"); break;
                    case '\r': dst.append("\\r"); break;
                    case '\t': dst.append("\\t"); break;
                    case '\v': dst.append("\\v"); break;
                    case '\'': dst.append("\\'"); break;
                    case '"': dst.append("\\\""); break;
                    case '\\': dst.append("\\\\"); break;
                    default:
                        {
                            // Just encode as octal
                            dst.append("\\0");
                            char octal_str[32];
                            snprintf(octal_str, sizeof(octal_str), "%o", *p);
                            dst.append(octal_str);
                        }
                        break;
                }
            }
        }
    }
}

