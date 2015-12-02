//===-- Debugger.cpp --------------------------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "lldb/Core/Debugger.h"

#include <map>

#include "clang/AST/DeclCXX.h"
#include "clang/AST/Type.h"
#include "llvm/ADT/StringRef.h"

#include "lldb/lldb-private.h"
#include "lldb/Core/FormatEntity.h"
#include "lldb/Core/Module.h"
#include "lldb/Core/PluginManager.h"
#include "lldb/Core/RegisterValue.h"
#include "lldb/Core/State.h"
#include "lldb/Core/StreamAsynchronousIO.h"
#include "lldb/Core/StreamCallback.h"
#include "lldb/Core/StreamFile.h"
#include "lldb/Core/StreamString.h"
#include "lldb/Core/StructuredData.h"
#include "lldb/Core/Timer.h"
#include "lldb/Core/ValueObject.h"
#include "lldb/Core/ValueObjectVariable.h"
#include "lldb/DataFormatters/DataVisualization.h"
#include "lldb/DataFormatters/FormatManager.h"
#include "lldb/DataFormatters/TypeSummary.h"
#include "lldb/Host/ConnectionFileDescriptor.h"
#include "lldb/Host/HostInfo.h"
#include "lldb/Host/Terminal.h"
#include "lldb/Host/ThreadLauncher.h"
#include "lldb/Interpreter/CommandInterpreter.h"
#include "lldb/Interpreter/OptionValueProperties.h"
#include "lldb/Interpreter/OptionValueSInt64.h"
#include "lldb/Interpreter/OptionValueString.h"
#include "lldb/Symbol/ClangASTContext.h"
#include "lldb/Symbol/CompileUnit.h"
#include "lldb/Symbol/Function.h"
#include "lldb/Symbol/Symbol.h"
#include "lldb/Symbol/VariableList.h"
#include "lldb/Target/CPPLanguageRuntime.h"
#include "lldb/Target/ObjCLanguageRuntime.h"
#include "lldb/Target/TargetList.h"
#include "lldb/Target/Process.h"
#include "lldb/Target/RegisterContext.h"
#include "lldb/Target/SectionLoadList.h"
#include "lldb/Target/StopInfo.h"
#include "lldb/Target/Target.h"
#include "lldb/Target/Thread.h"
#include "lldb/Utility/AnsiTerminal.h"

#include "llvm/Support/DynamicLibrary.h"

using namespace lldb;
using namespace lldb_private;


static lldb::user_id_t g_unique_id = 1;
static size_t g_debugger_event_thread_stack_bytes = 8 * 1024 * 1024;

#pragma mark Static Functions

static Mutex &
GetDebuggerListMutex ()
{
    static Mutex g_mutex(Mutex::eMutexTypeRecursive);
    return g_mutex;
}

typedef std::vector<DebuggerSP> DebuggerList;

static DebuggerList &
GetDebuggerList()
{
    // hide the static debugger list inside a singleton accessor to avoid
    // global init constructors
    static DebuggerList g_list;
    return g_list;
}

OptionEnumValueElement
g_show_disassembly_enum_values[] =
{
    { Debugger::eStopDisassemblyTypeNever,    "never",     "Never show disassembly when displaying a stop context."},
    { Debugger::eStopDisassemblyTypeNoSource, "no-source", "Show disassembly when there is no source information, or the source file is missing when displaying a stop context."},
    { Debugger::eStopDisassemblyTypeAlways,   "always",    "Always show disassembly when displaying a stop context."},
    { 0, NULL, NULL }
};

OptionEnumValueElement
g_language_enumerators[] =
{
    { eScriptLanguageNone,      "none",     "Disable scripting languages."},
    { eScriptLanguagePython,    "python",   "Select python as the default scripting language."},
    { eScriptLanguageDefault,   "default",  "Select the lldb default as the default scripting language."},
    { 0, NULL, NULL }
};

#define MODULE_WITH_FUNC "{ ${module.file.basename}{`${function.name-with-args}${function.pc-offset}}}"
#define FILE_AND_LINE "{ at ${line.file.basename}:${line.number}}"

#define DEFAULT_THREAD_FORMAT "thread #${thread.index}: tid = ${thread.id%tid}"\
    "{, ${frame.pc}}"\
    MODULE_WITH_FUNC\
    FILE_AND_LINE\
    "{, name = '${thread.name}'}"\
    "{, queue = '${thread.queue}'}"\
    "{, activity = '${thread.info.activity.name}'}" \
    "{, ${thread.info.trace_messages} messages}" \
    "{, stop reason = ${thread.stop-reason}}"\
    "{\\nReturn value: ${thread.return-value}}"\
    "{\\nCompleted expression: ${thread.completed-expression}}"\
    "\\n"

#define DEFAULT_FRAME_FORMAT "frame #${frame.index}: ${frame.pc}"\
    MODULE_WITH_FUNC\
    FILE_AND_LINE\
    "\\n"

// Three parts to this disassembly format specification:
//   1. If this is a new function/symbol (no previous symbol/function), print
//      dylib`funcname:\n
//   2. If this is a symbol context change (different from previous symbol/function), print
//      dylib`funcname:\n
//   3. print 
//      address <+offset>: 
#define DEFAULT_DISASSEMBLY_FORMAT "{${function.initial-function}{${module.file.basename}`}{${function.name-without-args}}:\n}{${function.changed}\n{${module.file.basename}`}{${function.name-without-args}}:\n}{${current-pc-arrow} }${addr-file-or-load}{ <${function.concrete-only-addr-offset-no-padding}>}: "

// gdb's disassembly format can be emulated with
// ${current-pc-arrow}${addr-file-or-load}{ <${function.name-without-args}${function.concrete-only-addr-offset-no-padding}>}: 

// lldb's original format for disassembly would look like this format string -
// {${function.initial-function}{${module.file.basename}`}{${function.name-without-args}}:\n}{${function.changed}\n{${module.file.basename}`}{${function.name-without-args}}:\n}{${current-pc-arrow} }{${addr-file-or-load}}: 


static PropertyDefinition
g_properties[] =
{
{   "auto-confirm",             OptionValue::eTypeBoolean     , true, false, NULL, NULL, "If true all confirmation prompts will receive their default reply." },
{   "disassembly-format",       OptionValue::eTypeFormatEntity, true, 0    , DEFAULT_DISASSEMBLY_FORMAT, NULL, "The default disassembly format string to use when disassembling instruction sequences." },
{   "frame-format",             OptionValue::eTypeFormatEntity, true, 0    , DEFAULT_FRAME_FORMAT, NULL, "The default frame format string to use when displaying stack frame information for threads." },
{   "notify-void",              OptionValue::eTypeBoolean     , true, false, NULL, NULL, "Notify the user explicitly if an expression returns void (default: false)." },
{   "prompt",                   OptionValue::eTypeString      , true, OptionValueString::eOptionEncodeCharacterEscapeSequences, "(lldb) ", NULL, "The debugger command line prompt displayed for the user." },
{   "script-lang",              OptionValue::eTypeEnum        , true, eScriptLanguagePython, NULL, g_language_enumerators, "The script language to be used for evaluating user-written scripts." },
{   "stop-disassembly-count",   OptionValue::eTypeSInt64      , true, 4    , NULL, NULL, "The number of disassembly lines to show when displaying a stopped context." },
{   "stop-disassembly-display", OptionValue::eTypeEnum        , true, Debugger::eStopDisassemblyTypeNoSource, NULL, g_show_disassembly_enum_values, "Control when to display disassembly when displaying a stopped context." },
{   "stop-line-count-after",    OptionValue::eTypeSInt64      , true, 3    , NULL, NULL, "The number of sources lines to display that come after the current source line when displaying a stopped context." },
{   "stop-line-count-before",   OptionValue::eTypeSInt64      , true, 3    , NULL, NULL, "The number of sources lines to display that come before the current source line when displaying a stopped context." },
{   "term-width",               OptionValue::eTypeSInt64      , true, 80   , NULL, NULL, "The maximum number of columns to use for displaying text." },
{   "thread-format",            OptionValue::eTypeFormatEntity, true, 0    , DEFAULT_THREAD_FORMAT, NULL, "The default thread format string to use when displaying thread information." },
{   "use-external-editor",      OptionValue::eTypeBoolean     , true, false, NULL, NULL, "Whether to use an external editor or not." },
{   "use-color",                OptionValue::eTypeBoolean     , true, true , NULL, NULL, "Whether to use Ansi color codes or not." },
{   "auto-one-line-summaries",  OptionValue::eTypeBoolean     , true, true, NULL, NULL, "If true, LLDB will automatically display small structs in one-liner format (default: true)." },
{   "escape-non-printables",    OptionValue::eTypeBoolean     , true, true, NULL, NULL, "If true, LLDB will automatically escape non-printable and escape characters when formatting strings." },
{   NULL,                       OptionValue::eTypeInvalid     , true, 0    , NULL, NULL, NULL }
};

enum
{
    ePropertyAutoConfirm = 0,
    ePropertyDisassemblyFormat,
    ePropertyFrameFormat,
    ePropertyNotiftVoid,
    ePropertyPrompt,
    ePropertyScriptLanguage,
    ePropertyStopDisassemblyCount,
    ePropertyStopDisassemblyDisplay,
    ePropertyStopLineCountAfter,
    ePropertyStopLineCountBefore,
    ePropertyTerminalWidth,
    ePropertyThreadFormat,
    ePropertyUseExternalEditor,
    ePropertyUseColor,
    ePropertyAutoOneLineSummaries,
    ePropertyEscapeNonPrintables
};

LoadPluginCallbackType Debugger::g_load_plugin_callback = NULL;

Error
Debugger::SetPropertyValue (const ExecutionContext *exe_ctx,
                            VarSetOperationType op,
                            const char *property_path,
                            const char *value)
{
    bool is_load_script = strcmp(property_path,"target.load-script-from-symbol-file") == 0;
    bool is_escape_non_printables = strcmp(property_path, "escape-non-printables") == 0;
    TargetSP target_sp;
    LoadScriptFromSymFile load_script_old_value;
    if (is_load_script && exe_ctx->GetTargetSP())
    {
        target_sp = exe_ctx->GetTargetSP();
        load_script_old_value = target_sp->TargetProperties::GetLoadScriptFromSymbolFile();
    }
    Error error (Properties::SetPropertyValue (exe_ctx, op, property_path, value));
    if (error.Success())
    {
        // FIXME it would be nice to have "on-change" callbacks for properties
        if (strcmp(property_path, g_properties[ePropertyPrompt].name) == 0)
        {
            const char *new_prompt = GetPrompt();
            std::string str = lldb_utility::ansi::FormatAnsiTerminalCodes (new_prompt, GetUseColor());
            if (str.length())
                new_prompt = str.c_str();
            GetCommandInterpreter().UpdatePrompt(new_prompt);
            EventSP prompt_change_event_sp (new Event(CommandInterpreter::eBroadcastBitResetPrompt, new EventDataBytes (new_prompt)));
            GetCommandInterpreter().BroadcastEvent (prompt_change_event_sp);
        }
        else if (strcmp(property_path, g_properties[ePropertyUseColor].name) == 0)
        {
			// use-color changed. Ping the prompt so it can reset the ansi terminal codes.
            SetPrompt (GetPrompt());
        }
        else if (is_load_script && target_sp && load_script_old_value == eLoadScriptFromSymFileWarn)
        {
            if (target_sp->TargetProperties::GetLoadScriptFromSymbolFile() == eLoadScriptFromSymFileTrue)
            {
                std::list<Error> errors;
                StreamString feedback_stream;
                if (!target_sp->LoadScriptingResources(errors,&feedback_stream))
                {
                    StreamFileSP stream_sp (GetErrorFile());
                    if (stream_sp)
                    {
                        for (auto error : errors)
                        {
                            stream_sp->Printf("%s\n",error.AsCString());
                        }
                        if (feedback_stream.GetSize())
                            stream_sp->Printf("%s",feedback_stream.GetData());
                    }
                }
            }
        }
        else if (is_escape_non_printables)
        {
            DataVisualization::ForceUpdate();
        }
    }
    return error;
}

bool
Debugger::GetAutoConfirm () const
{
    const uint32_t idx = ePropertyAutoConfirm;
    return m_collection_sp->GetPropertyAtIndexAsBoolean (NULL, idx, g_properties[idx].default_uint_value != 0);
}

const FormatEntity::Entry *
Debugger::GetDisassemblyFormat() const
{
    const uint32_t idx = ePropertyDisassemblyFormat;
    return m_collection_sp->GetPropertyAtIndexAsFormatEntity(NULL, idx);
}

const FormatEntity::Entry *
Debugger::GetFrameFormat() const
{
    const uint32_t idx = ePropertyFrameFormat;
    return m_collection_sp->GetPropertyAtIndexAsFormatEntity(NULL, idx);
}

bool
Debugger::GetNotifyVoid () const
{
    const uint32_t idx = ePropertyNotiftVoid;
    return m_collection_sp->GetPropertyAtIndexAsBoolean (NULL, idx, g_properties[idx].default_uint_value != 0);
}

const char *
Debugger::GetPrompt() const
{
    const uint32_t idx = ePropertyPrompt;
    return m_collection_sp->GetPropertyAtIndexAsString (NULL, idx, g_properties[idx].default_cstr_value);
}

void
Debugger::SetPrompt(const char *p)
{
    const uint32_t idx = ePropertyPrompt;
    m_collection_sp->SetPropertyAtIndexAsString (NULL, idx, p);
    const char *new_prompt = GetPrompt();
    std::string str = lldb_utility::ansi::FormatAnsiTerminalCodes (new_prompt, GetUseColor());
    if (str.length())
        new_prompt = str.c_str();
    GetCommandInterpreter().UpdatePrompt(new_prompt);
}

const FormatEntity::Entry *
Debugger::GetThreadFormat() const
{
    const uint32_t idx = ePropertyThreadFormat;
    return m_collection_sp->GetPropertyAtIndexAsFormatEntity(NULL, idx);
}

lldb::ScriptLanguage
Debugger::GetScriptLanguage() const
{
    const uint32_t idx = ePropertyScriptLanguage;
    return (lldb::ScriptLanguage)m_collection_sp->GetPropertyAtIndexAsEnumeration (NULL, idx, g_properties[idx].default_uint_value);
}

bool
Debugger::SetScriptLanguage (lldb::ScriptLanguage script_lang)
{
    const uint32_t idx = ePropertyScriptLanguage;
    return m_collection_sp->SetPropertyAtIndexAsEnumeration (NULL, idx, script_lang);
}

uint32_t
Debugger::GetTerminalWidth () const
{
    const uint32_t idx = ePropertyTerminalWidth;
    return m_collection_sp->GetPropertyAtIndexAsSInt64 (NULL, idx, g_properties[idx].default_uint_value);
}

bool
Debugger::SetTerminalWidth (uint32_t term_width)
{
    const uint32_t idx = ePropertyTerminalWidth;
    return m_collection_sp->SetPropertyAtIndexAsSInt64 (NULL, idx, term_width);
}

bool
Debugger::GetUseExternalEditor () const
{
    const uint32_t idx = ePropertyUseExternalEditor;
    return m_collection_sp->GetPropertyAtIndexAsBoolean (NULL, idx, g_properties[idx].default_uint_value != 0);
}

bool
Debugger::SetUseExternalEditor (bool b)
{
    const uint32_t idx = ePropertyUseExternalEditor;
    return m_collection_sp->SetPropertyAtIndexAsBoolean (NULL, idx, b);
}

bool
Debugger::GetUseColor () const
{
    const uint32_t idx = ePropertyUseColor;
    return m_collection_sp->GetPropertyAtIndexAsBoolean (NULL, idx, g_properties[idx].default_uint_value != 0);
}

bool
Debugger::SetUseColor (bool b)
{
    const uint32_t idx = ePropertyUseColor;
    bool ret = m_collection_sp->SetPropertyAtIndexAsBoolean (NULL, idx, b);
    SetPrompt (GetPrompt());
    return ret;
}

uint32_t
Debugger::GetStopSourceLineCount (bool before) const
{
    const uint32_t idx = before ? ePropertyStopLineCountBefore : ePropertyStopLineCountAfter;
    return m_collection_sp->GetPropertyAtIndexAsSInt64 (NULL, idx, g_properties[idx].default_uint_value);
}

Debugger::StopDisassemblyType
Debugger::GetStopDisassemblyDisplay () const
{
    const uint32_t idx = ePropertyStopDisassemblyDisplay;
    return (Debugger::StopDisassemblyType)m_collection_sp->GetPropertyAtIndexAsEnumeration (NULL, idx, g_properties[idx].default_uint_value);
}

uint32_t
Debugger::GetDisassemblyLineCount () const
{
    const uint32_t idx = ePropertyStopDisassemblyCount;
    return m_collection_sp->GetPropertyAtIndexAsSInt64 (NULL, idx, g_properties[idx].default_uint_value);
}

bool
Debugger::GetAutoOneLineSummaries () const
{
    const uint32_t idx = ePropertyAutoOneLineSummaries;
    return m_collection_sp->GetPropertyAtIndexAsBoolean (NULL, idx, true);
}

bool
Debugger::GetEscapeNonPrintables () const
{
    const uint32_t idx = ePropertyEscapeNonPrintables;
    return m_collection_sp->GetPropertyAtIndexAsBoolean (NULL, idx, true);
}

#pragma mark Debugger

//const DebuggerPropertiesSP &
//Debugger::GetSettings() const
//{
//    return m_properties_sp;
//}
//

static bool lldb_initialized = false;
void
Debugger::Initialize(LoadPluginCallbackType load_plugin_callback)
{
    assert(!lldb_initialized && "Debugger::Initialize called more than once!");

    lldb_initialized = true;
    g_load_plugin_callback = load_plugin_callback;
}

void
Debugger::Terminate ()
{
    assert(lldb_initialized && "Debugger::Terminate called without a matching Debugger::Initialize!");

    // Clear our master list of debugger objects
    Mutex::Locker locker (GetDebuggerListMutex ());
    GetDebuggerList().clear();
}

void
Debugger::SettingsInitialize ()
{
    Target::SettingsInitialize ();
}

void
Debugger::SettingsTerminate ()
{
    Target::SettingsTerminate ();
}

bool
Debugger::LoadPlugin (const FileSpec& spec, Error& error)
{
    if (g_load_plugin_callback)
    {
        llvm::sys::DynamicLibrary dynlib = g_load_plugin_callback (shared_from_this(), spec, error);
        if (dynlib.isValid())
        {
            m_loaded_plugins.push_back(dynlib);
            return true;
        }
    }
    else
    {
        // The g_load_plugin_callback is registered in SBDebugger::Initialize()
        // and if the public API layer isn't available (code is linking against
        // all of the internal LLDB static libraries), then we can't load plugins
        error.SetErrorString("Public API layer is not available");
    }
    return false;
}

static FileSpec::EnumerateDirectoryResult
LoadPluginCallback
(
 void *baton,
 FileSpec::FileType file_type,
 const FileSpec &file_spec
 )
{
    Error error;
    
    static ConstString g_dylibext("dylib");
    static ConstString g_solibext("so");
    
    if (!baton)
        return FileSpec::eEnumerateDirectoryResultQuit;
    
    Debugger *debugger = (Debugger*)baton;
    
    // If we have a regular file, a symbolic link or unknown file type, try
    // and process the file. We must handle unknown as sometimes the directory
    // enumeration might be enumerating a file system that doesn't have correct
    // file type information.
    if (file_type == FileSpec::eFileTypeRegular         ||
        file_type == FileSpec::eFileTypeSymbolicLink    ||
        file_type == FileSpec::eFileTypeUnknown          )
    {
        FileSpec plugin_file_spec (file_spec);
        plugin_file_spec.ResolvePath ();
        
        if (plugin_file_spec.GetFileNameExtension() != g_dylibext &&
            plugin_file_spec.GetFileNameExtension() != g_solibext)
        {
            return FileSpec::eEnumerateDirectoryResultNext;
        }

        Error plugin_load_error;
        debugger->LoadPlugin (plugin_file_spec, plugin_load_error);
        
        return FileSpec::eEnumerateDirectoryResultNext;
    }
    
    else if (file_type == FileSpec::eFileTypeUnknown     ||
        file_type == FileSpec::eFileTypeDirectory   ||
        file_type == FileSpec::eFileTypeSymbolicLink )
    {
        // Try and recurse into anything that a directory or symbolic link.
        // We must also do this for unknown as sometimes the directory enumeration
        // might be enumerating a file system that doesn't have correct file type
        // information.
        return FileSpec::eEnumerateDirectoryResultEnter;
    }
    
    return FileSpec::eEnumerateDirectoryResultNext;
}

void
Debugger::InstanceInitialize ()
{
    FileSpec dir_spec;
    const bool find_directories = true;
    const bool find_files = true;
    const bool find_other = true;
    char dir_path[PATH_MAX];
    if (HostInfo::GetLLDBPath(ePathTypeLLDBSystemPlugins, dir_spec))
    {
        if (dir_spec.Exists() && dir_spec.GetPath(dir_path, sizeof(dir_path)))
        {
            FileSpec::EnumerateDirectory (dir_path,
                                          find_directories,
                                          find_files,
                                          find_other,
                                          LoadPluginCallback,
                                          this);
        }
    }

    if (HostInfo::GetLLDBPath(ePathTypeLLDBUserPlugins, dir_spec))
    {
        if (dir_spec.Exists() && dir_spec.GetPath(dir_path, sizeof(dir_path)))
        {
            FileSpec::EnumerateDirectory (dir_path,
                                          find_directories,
                                          find_files,
                                          find_other,
                                          LoadPluginCallback,
                                          this);
        }
    }
    
    PluginManager::DebuggerInitialize (*this);
}

DebuggerSP
Debugger::CreateInstance (lldb::LogOutputCallback log_callback, void *baton)
{
    DebuggerSP debugger_sp (new Debugger(log_callback, baton));
    if (lldb_initialized)
    {
        Mutex::Locker locker (GetDebuggerListMutex ());
        GetDebuggerList().push_back(debugger_sp);
    }
    debugger_sp->InstanceInitialize ();
    return debugger_sp;
}

void
Debugger::Destroy (DebuggerSP &debugger_sp)
{
    if (debugger_sp.get() == NULL)
        return;
        
    debugger_sp->Clear();

    if (lldb_initialized)
    {
        Mutex::Locker locker (GetDebuggerListMutex ());
        DebuggerList &debugger_list = GetDebuggerList ();
        DebuggerList::iterator pos, end = debugger_list.end();
        for (pos = debugger_list.begin (); pos != end; ++pos)
        {
            if ((*pos).get() == debugger_sp.get())
            {
                debugger_list.erase (pos);
                return;
            }
        }
    }
}

DebuggerSP
Debugger::FindDebuggerWithInstanceName (const ConstString &instance_name)
{
    DebuggerSP debugger_sp;
    if (lldb_initialized)
    {
        Mutex::Locker locker (GetDebuggerListMutex ());
        DebuggerList &debugger_list = GetDebuggerList();
        DebuggerList::iterator pos, end = debugger_list.end();

        for (pos = debugger_list.begin(); pos != end; ++pos)
        {
            if ((*pos).get()->m_instance_name == instance_name)
            {
                debugger_sp = *pos;
                break;
            }
        }
    }
    return debugger_sp;
}

TargetSP
Debugger::FindTargetWithProcessID (lldb::pid_t pid)
{
    TargetSP target_sp;
    if (lldb_initialized)
    {
        Mutex::Locker locker (GetDebuggerListMutex ());
        DebuggerList &debugger_list = GetDebuggerList();
        DebuggerList::iterator pos, end = debugger_list.end();
        for (pos = debugger_list.begin(); pos != end; ++pos)
        {
            target_sp = (*pos)->GetTargetList().FindTargetWithProcessID (pid);
            if (target_sp)
                break;
        }
    }
    return target_sp;
}

TargetSP
Debugger::FindTargetWithProcess (Process *process)
{
    TargetSP target_sp;
    if (lldb_initialized)
    {
        Mutex::Locker locker (GetDebuggerListMutex ());
        DebuggerList &debugger_list = GetDebuggerList();
        DebuggerList::iterator pos, end = debugger_list.end();
        for (pos = debugger_list.begin(); pos != end; ++pos)
        {
            target_sp = (*pos)->GetTargetList().FindTargetWithProcess (process);
            if (target_sp)
                break;
        }
    }
    return target_sp;
}

Debugger::Debugger(lldb::LogOutputCallback log_callback, void *baton) :
    UserID(g_unique_id++),
    Properties(OptionValuePropertiesSP(new OptionValueProperties())),
    m_input_file_sp(new StreamFile(stdin, false)),
    m_output_file_sp(new StreamFile(stdout, false)),
    m_error_file_sp(new StreamFile(stderr, false)),
    m_terminal_state(),
    m_target_list(*this),
    m_platform_list(),
    m_listener("lldb.Debugger"),
    m_source_manager_ap(),
    m_source_file_cache(),
    m_command_interpreter_ap(new CommandInterpreter(*this, eScriptLanguageDefault, false)),
    m_input_reader_stack(),
    m_instance_name(),
    m_loaded_plugins(),
    m_event_handler_thread (),
    m_io_handler_thread (),
    m_sync_broadcaster (NULL, "lldb.debugger.sync")
{
    char instance_cstr[256];
    snprintf(instance_cstr, sizeof(instance_cstr), "debugger_%d", (int)GetID());
    m_instance_name.SetCString(instance_cstr);
    if (log_callback)
        m_log_callback_stream_sp.reset (new StreamCallback (log_callback, baton));
    m_command_interpreter_ap->Initialize ();
    // Always add our default platform to the platform list
    PlatformSP default_platform_sp (Platform::GetHostPlatform());
    assert (default_platform_sp.get());
    m_platform_list.Append (default_platform_sp, true);
    
    m_collection_sp->Initialize (g_properties);
    m_collection_sp->AppendProperty (ConstString("target"),
                                     ConstString("Settings specify to debugging targets."),
                                     true,
                                     Target::GetGlobalProperties()->GetValueProperties());
    m_collection_sp->AppendProperty (ConstString("platform"),
                                     ConstString("Platform settings."),
                                     true,
                                     Platform::GetGlobalPlatformProperties()->GetValueProperties());
    if (m_command_interpreter_ap.get())
    {
        m_collection_sp->AppendProperty (ConstString("interpreter"),
                                         ConstString("Settings specify to the debugger's command interpreter."),
                                         true,
                                         m_command_interpreter_ap->GetValueProperties());
    }
    OptionValueSInt64 *term_width = m_collection_sp->GetPropertyAtIndexAsOptionValueSInt64 (NULL, ePropertyTerminalWidth);
    term_width->SetMinimumValue(10);
    term_width->SetMaximumValue(1024);

    // Turn off use-color if this is a dumb terminal.
    const char *term = getenv ("TERM");
    if (term && !strcmp (term, "dumb"))
        SetUseColor (false);
}

Debugger::~Debugger ()
{
    Clear();
}

void
Debugger::Clear()
{
    ClearIOHandlers();
    StopIOHandlerThread();
    StopEventHandlerThread();
    m_listener.Clear();
    int num_targets = m_target_list.GetNumTargets();
    for (int i = 0; i < num_targets; i++)
    {
        TargetSP target_sp (m_target_list.GetTargetAtIndex (i));
        if (target_sp)
        {
            ProcessSP process_sp (target_sp->GetProcessSP());
            if (process_sp)
                process_sp->Finalize();
            target_sp->Destroy();
        }
    }
    BroadcasterManager::Clear ();
    
    // Close the input file _before_ we close the input read communications class
    // as it does NOT own the input file, our m_input_file does.
    m_terminal_state.Clear();
    if (m_input_file_sp)
        m_input_file_sp->GetFile().Close ();
    
    m_command_interpreter_ap->Clear();
}

bool
Debugger::GetCloseInputOnEOF () const
{
//    return m_input_comm.GetCloseOnEOF();
    return false;
}

void
Debugger::SetCloseInputOnEOF (bool b)
{
//    m_input_comm.SetCloseOnEOF(b);
}

bool
Debugger::GetAsyncExecution ()
{
    return !m_command_interpreter_ap->GetSynchronous();
}

void
Debugger::SetAsyncExecution (bool async_execution)
{
    m_command_interpreter_ap->SetSynchronous (!async_execution);
}

    
void
Debugger::SetInputFileHandle (FILE *fh, bool tranfer_ownership)
{
    if (m_input_file_sp)
        m_input_file_sp->GetFile().SetStream (fh, tranfer_ownership);
    else
        m_input_file_sp.reset (new StreamFile (fh, tranfer_ownership));

    File &in_file = m_input_file_sp->GetFile();
    if (in_file.IsValid() == false)
        in_file.SetStream (stdin, true);

    // Save away the terminal state if that is relevant, so that we can restore it in RestoreInputState.
    SaveInputTerminalState ();
}

void
Debugger::SetOutputFileHandle (FILE *fh, bool tranfer_ownership)
{
    if (m_output_file_sp)
        m_output_file_sp->GetFile().SetStream (fh, tranfer_ownership);
    else
        m_output_file_sp.reset (new StreamFile (fh, tranfer_ownership));
    
    File &out_file = m_output_file_sp->GetFile();
    if (out_file.IsValid() == false)
        out_file.SetStream (stdout, false);
    
    // do not create the ScriptInterpreter just for setting the output file handle
    // as the constructor will know how to do the right thing on its own
    const bool can_create = false;
    ScriptInterpreter* script_interpreter = GetCommandInterpreter().GetScriptInterpreter(can_create);
    if (script_interpreter)
        script_interpreter->ResetOutputFileHandle (fh);
}

void
Debugger::SetErrorFileHandle (FILE *fh, bool tranfer_ownership)
{
    if (m_error_file_sp)
        m_error_file_sp->GetFile().SetStream (fh, tranfer_ownership);
    else
        m_error_file_sp.reset (new StreamFile (fh, tranfer_ownership));
    
    File &err_file = m_error_file_sp->GetFile();
    if (err_file.IsValid() == false)
        err_file.SetStream (stderr, false);
}

void
Debugger::SaveInputTerminalState ()
{
    if (m_input_file_sp)
    {
        File &in_file = m_input_file_sp->GetFile();
        if (in_file.GetDescriptor() != File::kInvalidDescriptor)
            m_terminal_state.Save(in_file.GetDescriptor(), true);
    }
}

void
Debugger::RestoreInputTerminalState ()
{
    m_terminal_state.Restore();
}

ExecutionContext
Debugger::GetSelectedExecutionContext ()
{
    ExecutionContext exe_ctx;
    TargetSP target_sp(GetSelectedTarget());
    exe_ctx.SetTargetSP (target_sp);
    
    if (target_sp)
    {
        ProcessSP process_sp (target_sp->GetProcessSP());
        exe_ctx.SetProcessSP (process_sp);
        if (process_sp && process_sp->IsRunning() == false)
        {
            ThreadSP thread_sp (process_sp->GetThreadList().GetSelectedThread());
            if (thread_sp)
            {
                exe_ctx.SetThreadSP (thread_sp);
                exe_ctx.SetFrameSP (thread_sp->GetSelectedFrame());
                if (exe_ctx.GetFramePtr() == NULL)
                    exe_ctx.SetFrameSP (thread_sp->GetStackFrameAtIndex (0));
            }
        }
    }
    return exe_ctx;
}

void
Debugger::DispatchInputInterrupt ()
{
    Mutex::Locker locker (m_input_reader_stack.GetMutex());
    IOHandlerSP reader_sp (m_input_reader_stack.Top());
    if (reader_sp)
        reader_sp->Interrupt();
}

void
Debugger::DispatchInputEndOfFile ()
{
    Mutex::Locker locker (m_input_reader_stack.GetMutex());
    IOHandlerSP reader_sp (m_input_reader_stack.Top());
    if (reader_sp)
        reader_sp->GotEOF();
}

void
Debugger::ClearIOHandlers ()
{
    // The bottom input reader should be the main debugger input reader.  We do not want to close that one here.
    Mutex::Locker locker (m_input_reader_stack.GetMutex());
    while (m_input_reader_stack.GetSize() > 1)
    {
        IOHandlerSP reader_sp (m_input_reader_stack.Top());
        if (reader_sp)
            PopIOHandler (reader_sp);
    }
}

void
Debugger::ExecuteIOHandlers()
{
    while (1)
    {
        IOHandlerSP reader_sp(m_input_reader_stack.Top());
        if (!reader_sp)
            break;

        reader_sp->Run();

        // Remove all input readers that are done from the top of the stack
        while (1)
        {
            IOHandlerSP top_reader_sp = m_input_reader_stack.Top();
            if (top_reader_sp && top_reader_sp->GetIsDone())
                PopIOHandler (top_reader_sp);
            else
                break;
        }
    }
    ClearIOHandlers();
}

bool
Debugger::IsTopIOHandler (const lldb::IOHandlerSP& reader_sp)
{
    return m_input_reader_stack.IsTop (reader_sp);
}

void
Debugger::PrintAsync (const char *s, size_t len, bool is_stdout)
{
    lldb::StreamFileSP stream = is_stdout ? GetOutputFile() : GetErrorFile();
    m_input_reader_stack.PrintAsync(stream.get(), s, len);
}

ConstString
Debugger::GetTopIOHandlerControlSequence(char ch)
{
    return m_input_reader_stack.GetTopIOHandlerControlSequence (ch);
}

const char *
Debugger::GetIOHandlerCommandPrefix()
{
    return m_input_reader_stack.GetTopIOHandlerCommandPrefix();
}

const char *
Debugger::GetIOHandlerHelpPrologue()
{
    return m_input_reader_stack.GetTopIOHandlerHelpPrologue();
}

void
Debugger::RunIOHandler (const IOHandlerSP& reader_sp)
{
    PushIOHandler (reader_sp);

    IOHandlerSP top_reader_sp = reader_sp;
    while (top_reader_sp)
    {
        top_reader_sp->Run();

        if (top_reader_sp.get() == reader_sp.get())
        {
            if (PopIOHandler (reader_sp))
                break;
        }

        while (1)
        {
            top_reader_sp = m_input_reader_stack.Top();
            if (top_reader_sp && top_reader_sp->GetIsDone())
                PopIOHandler (top_reader_sp);
            else
                break;
        }
    }
}

void
Debugger::AdoptTopIOHandlerFilesIfInvalid (StreamFileSP &in, StreamFileSP &out, StreamFileSP &err)
{
    // Before an IOHandler runs, it must have in/out/err streams.
    // This function is called when one ore more of the streams
    // are NULL. We use the top input reader's in/out/err streams,
    // or fall back to the debugger file handles, or we fall back
    // onto stdin/stdout/stderr as a last resort.
    
    Mutex::Locker locker (m_input_reader_stack.GetMutex());
    IOHandlerSP top_reader_sp (m_input_reader_stack.Top());
    // If no STDIN has been set, then set it appropriately
    if (!in)
    {
        if (top_reader_sp)
            in = top_reader_sp->GetInputStreamFile();
        else
            in = GetInputFile();
        
        // If there is nothing, use stdin
        if (!in)
            in = StreamFileSP(new StreamFile(stdin, false));
    }
    // If no STDOUT has been set, then set it appropriately
    if (!out)
    {
        if (top_reader_sp)
            out = top_reader_sp->GetOutputStreamFile();
        else
            out = GetOutputFile();
        
        // If there is nothing, use stdout
        if (!out)
            out = StreamFileSP(new StreamFile(stdout, false));
    }
    // If no STDERR has been set, then set it appropriately
    if (!err)
    {
        if (top_reader_sp)
            err = top_reader_sp->GetErrorStreamFile();
        else
            err = GetErrorFile();
        
        // If there is nothing, use stderr
        if (!err)
            err = StreamFileSP(new StreamFile(stdout, false));
        
    }
}

void
Debugger::PushIOHandler (const IOHandlerSP& reader_sp)
{
    if (!reader_sp)
        return;
 
    Mutex::Locker locker (m_input_reader_stack.GetMutex());

    // Get the current top input reader...
    IOHandlerSP top_reader_sp (m_input_reader_stack.Top());
    
    // Don't push the same IO handler twice...
    if (reader_sp == top_reader_sp)
        return;

    // Push our new input reader
    m_input_reader_stack.Push (reader_sp);
    reader_sp->Activate();

    // Interrupt the top input reader to it will exit its Run() function
    // and let this new input reader take over
    if (top_reader_sp)
    {
        top_reader_sp->Deactivate();
        top_reader_sp->Cancel();
    }
}

bool
Debugger::PopIOHandler (const IOHandlerSP& pop_reader_sp)
{
    if (! pop_reader_sp)
        return false;

    Mutex::Locker locker (m_input_reader_stack.GetMutex());

    // The reader on the stop of the stack is done, so let the next
    // read on the stack refresh its prompt and if there is one...
    if (m_input_reader_stack.IsEmpty())
        return false;

    IOHandlerSP reader_sp(m_input_reader_stack.Top());

    if (pop_reader_sp != reader_sp)
        return false;

    reader_sp->Deactivate();
    reader_sp->Cancel();
    m_input_reader_stack.Pop ();

    reader_sp = m_input_reader_stack.Top();
    if (reader_sp)
        reader_sp->Activate();

    return true;
}

StreamSP
Debugger::GetAsyncOutputStream ()
{
    return StreamSP (new StreamAsynchronousIO (*this, true));
}

StreamSP
Debugger::GetAsyncErrorStream ()
{
    return StreamSP (new StreamAsynchronousIO (*this, false));
}    

size_t
Debugger::GetNumDebuggers()
{
    if (lldb_initialized)
    {
        Mutex::Locker locker (GetDebuggerListMutex ());
        return GetDebuggerList().size();
    }
    return 0;
}

lldb::DebuggerSP
Debugger::GetDebuggerAtIndex (size_t index)
{
    DebuggerSP debugger_sp;
    
    if (lldb_initialized)
    {
        Mutex::Locker locker (GetDebuggerListMutex ());
        DebuggerList &debugger_list = GetDebuggerList();
        
        if (index < debugger_list.size())
            debugger_sp = debugger_list[index];
    }

    return debugger_sp;
}

DebuggerSP
Debugger::FindDebuggerWithID (lldb::user_id_t id)
{
    DebuggerSP debugger_sp;

    if (lldb_initialized)
    {
        Mutex::Locker locker (GetDebuggerListMutex ());
        DebuggerList &debugger_list = GetDebuggerList();
        DebuggerList::iterator pos, end = debugger_list.end();
        for (pos = debugger_list.begin(); pos != end; ++pos)
        {
            if ((*pos).get()->GetID() == id)
            {
                debugger_sp = *pos;
                break;
            }
        }
    }
    return debugger_sp;
}

#if 0
static void
TestPromptFormats (StackFrame *frame)
{
    if (frame == NULL)
        return;

    StreamString s;
    const char *prompt_format =         
    "{addr = '${addr}'\n}"
    "{addr-file-or-load = '${addr-file-or-load}'\n}"
    "{current-pc-arrow = '${current-pc-arrow}'\n}"
    "{process.id = '${process.id}'\n}"
    "{process.name = '${process.name}'\n}"
    "{process.file.basename = '${process.file.basename}'\n}"
    "{process.file.fullpath = '${process.file.fullpath}'\n}"
    "{thread.id = '${thread.id}'\n}"
    "{thread.index = '${thread.index}'\n}"
    "{thread.name = '${thread.name}'\n}"
    "{thread.queue = '${thread.queue}'\n}"
    "{thread.stop-reason = '${thread.stop-reason}'\n}"
    "{target.arch = '${target.arch}'\n}"
    "{module.file.basename = '${module.file.basename}'\n}"
    "{module.file.fullpath = '${module.file.fullpath}'\n}"
    "{file.basename = '${file.basename}'\n}"
    "{file.fullpath = '${file.fullpath}'\n}"
    "{frame.index = '${frame.index}'\n}"
    "{frame.pc = '${frame.pc}'\n}"
    "{frame.sp = '${frame.sp}'\n}"
    "{frame.fp = '${frame.fp}'\n}"
    "{frame.flags = '${frame.flags}'\n}"
    "{frame.reg.rdi = '${frame.reg.rdi}'\n}"
    "{frame.reg.rip = '${frame.reg.rip}'\n}"
    "{frame.reg.rsp = '${frame.reg.rsp}'\n}"
    "{frame.reg.rbp = '${frame.reg.rbp}'\n}"
    "{frame.reg.rflags = '${frame.reg.rflags}'\n}"
    "{frame.reg.xmm0 = '${frame.reg.xmm0}'\n}"
    "{frame.reg.carp = '${frame.reg.carp}'\n}"
    "{function.id = '${function.id}'\n}"
    "{function.changed = '${function.changed}'\n}"
    "{function.initial-function = '${function.initial-function}'\n}"
    "{function.name = '${function.name}'\n}"
    "{function.name-without-args = '${function.name-without-args}'\n}"
    "{function.name-with-args = '${function.name-with-args}'\n}"
    "{function.addr-offset = '${function.addr-offset}'\n}"
    "{function.concrete-only-addr-offset-no-padding = '${function.concrete-only-addr-offset-no-padding}'\n}"
    "{function.line-offset = '${function.line-offset}'\n}"
    "{function.pc-offset = '${function.pc-offset}'\n}"
    "{line.file.basename = '${line.file.basename}'\n}"
    "{line.file.fullpath = '${line.file.fullpath}'\n}"
    "{line.number = '${line.number}'\n}"
    "{line.start-addr = '${line.start-addr}'\n}"
    "{line.end-addr = '${line.end-addr}'\n}"
;

    SymbolContext sc (frame->GetSymbolContext(eSymbolContextEverything));
    ExecutionContext exe_ctx;
    frame->CalculateExecutionContext(exe_ctx);
    if (Debugger::FormatPrompt (prompt_format, &sc, &exe_ctx, &sc.line_entry.range.GetBaseAddress(), s))
    {
        printf("%s\n", s.GetData());
    }
    else
    {
        printf ("what we got: %s\n", s.GetData());
    }
}
#endif

bool
Debugger::FormatDisassemblerAddress (const FormatEntity::Entry *format,
                                     const SymbolContext *sc,
                                     const SymbolContext *prev_sc,
                                     const ExecutionContext *exe_ctx,
                                     const Address *addr,
                                     Stream &s)
{
    FormatEntity::Entry format_entry;

    if (format == NULL)
    {
        if (exe_ctx != NULL && exe_ctx->HasTargetScope())
            format = exe_ctx->GetTargetRef().GetDebugger().GetDisassemblyFormat();
        if (format == NULL)
        {
            FormatEntity::Parse("${addr}: ", format_entry);
            format = &format_entry;
        }
    }
    bool function_changed = false;
    bool initial_function = false;
    if (prev_sc && (prev_sc->function || prev_sc->symbol))
    {
        if (sc && (sc->function || sc->symbol))
        {
            if (prev_sc->symbol && sc->symbol)
            {
                if (!sc->symbol->Compare (prev_sc->symbol->GetName(), prev_sc->symbol->GetType()))
                {
                    function_changed = true;
                }
            }
            else if (prev_sc->function && sc->function)
            {
                if (prev_sc->function->GetMangled() != sc->function->GetMangled())
                {
                    function_changed = true;
                }
            }
        }
    }
    // The first context on a list of instructions will have a prev_sc that
    // has no Function or Symbol -- if SymbolContext had an IsValid() method, it
    // would return false.  But we do get a prev_sc pointer.
    if ((sc && (sc->function || sc->symbol))
        && prev_sc && (prev_sc->function == NULL && prev_sc->symbol == NULL))
    {
        initial_function = true;
    }
    return FormatEntity::Format(*format, s, sc, exe_ctx, addr, NULL, function_changed, initial_function);
}


void
Debugger::SetLoggingCallback (lldb::LogOutputCallback log_callback, void *baton)
{
    // For simplicity's sake, I am not going to deal with how to close down any
    // open logging streams, I just redirect everything from here on out to the
    // callback.
    m_log_callback_stream_sp.reset (new StreamCallback (log_callback, baton));
}

bool
Debugger::EnableLog (const char *channel, const char **categories, const char *log_file, uint32_t log_options, Stream &error_stream)
{
    StreamSP log_stream_sp;
    if (m_log_callback_stream_sp)
    {
        log_stream_sp = m_log_callback_stream_sp;
        // For now when using the callback mode you always get thread & timestamp.
        log_options |= LLDB_LOG_OPTION_PREPEND_TIMESTAMP | LLDB_LOG_OPTION_PREPEND_THREAD_NAME;
    }
    else if (log_file == NULL || *log_file == '\0')
    {
        log_stream_sp = GetOutputFile();
    }
    else
    {
        LogStreamMap::iterator pos = m_log_streams.find(log_file);
        if (pos != m_log_streams.end())
            log_stream_sp = pos->second.lock();
        if (!log_stream_sp)
        {
            uint32_t options = File::eOpenOptionWrite | File::eOpenOptionCanCreate
                                | File::eOpenOptionCloseOnExec | File::eOpenOptionAppend;
            if (! (log_options & LLDB_LOG_OPTION_APPEND))
                options |= File::eOpenOptionTruncate;

            log_stream_sp.reset (new StreamFile (log_file, options));
            m_log_streams[log_file] = log_stream_sp;
        }
    }
    assert (log_stream_sp.get());
    
    if (log_options == 0)
        log_options = LLDB_LOG_OPTION_PREPEND_THREAD_NAME | LLDB_LOG_OPTION_THREADSAFE;
        
    return Log::EnableLogChannel(log_stream_sp, log_options, channel, categories, error_stream);
}

SourceManager &
Debugger::GetSourceManager ()
{
    if (m_source_manager_ap.get() == NULL)
        m_source_manager_ap.reset (new SourceManager (shared_from_this()));
    return *m_source_manager_ap;
}



// This function handles events that were broadcast by the process.
void
Debugger::HandleBreakpointEvent (const EventSP &event_sp)
{
    using namespace lldb;
    const uint32_t event_type = Breakpoint::BreakpointEventData::GetBreakpointEventTypeFromEvent (event_sp);
    
//    if (event_type & eBreakpointEventTypeAdded
//        || event_type & eBreakpointEventTypeRemoved
//        || event_type & eBreakpointEventTypeEnabled
//        || event_type & eBreakpointEventTypeDisabled
//        || event_type & eBreakpointEventTypeCommandChanged
//        || event_type & eBreakpointEventTypeConditionChanged
//        || event_type & eBreakpointEventTypeIgnoreChanged
//        || event_type & eBreakpointEventTypeLocationsResolved)
//    {
//        // Don't do anything about these events, since the breakpoint commands already echo these actions.
//    }
//    
    if (event_type & eBreakpointEventTypeLocationsAdded)
    {
        uint32_t num_new_locations = Breakpoint::BreakpointEventData::GetNumBreakpointLocationsFromEvent(event_sp);
        if (num_new_locations > 0)
        {
            BreakpointSP breakpoint = Breakpoint::BreakpointEventData::GetBreakpointFromEvent(event_sp);
            StreamSP output_sp (GetAsyncOutputStream());
            if (output_sp)
            {
                output_sp->Printf("%d location%s added to breakpoint %d\n",
                                  num_new_locations,
                                  num_new_locations == 1 ? "" : "s",
                                  breakpoint->GetID());
                output_sp->Flush();
            }
        }
    }
//    else if (event_type & eBreakpointEventTypeLocationsRemoved)
//    {
//        // These locations just get disabled, not sure it is worth spamming folks about this on the command line.
//    }
//    else if (event_type & eBreakpointEventTypeLocationsResolved)
//    {
//        // This might be an interesting thing to note, but I'm going to leave it quiet for now, it just looked noisy.
//    }
}

size_t
Debugger::GetProcessSTDOUT (Process *process, Stream *stream)
{
    size_t total_bytes = 0;
    if (stream == NULL)
        stream = GetOutputFile().get();

    if (stream)
    {
        //  The process has stuff waiting for stdout; get it and write it out to the appropriate place.
        if (process == NULL)
        {
            TargetSP target_sp = GetTargetList().GetSelectedTarget();
            if (target_sp)
                process = target_sp->GetProcessSP().get();
        }
        if (process)
        {
            Error error;
            size_t len;
            char stdio_buffer[1024];
            while ((len = process->GetSTDOUT (stdio_buffer, sizeof (stdio_buffer), error)) > 0)
            {
                stream->Write(stdio_buffer, len);
                total_bytes += len;
            }
        }
        stream->Flush();
    }
    return total_bytes;
}

size_t
Debugger::GetProcessSTDERR (Process *process, Stream *stream)
{
    size_t total_bytes = 0;
    if (stream == NULL)
        stream = GetOutputFile().get();
    
    if (stream)
    {
        //  The process has stuff waiting for stderr; get it and write it out to the appropriate place.
        if (process == NULL)
        {
            TargetSP target_sp = GetTargetList().GetSelectedTarget();
            if (target_sp)
                process = target_sp->GetProcessSP().get();
        }
        if (process)
        {
            Error error;
            size_t len;
            char stdio_buffer[1024];
            while ((len = process->GetSTDERR (stdio_buffer, sizeof (stdio_buffer), error)) > 0)
            {
                stream->Write(stdio_buffer, len);
                total_bytes += len;
            }
        }
        stream->Flush();
    }
    return total_bytes;
}


// This function handles events that were broadcast by the process.
void
Debugger::HandleProcessEvent (const EventSP &event_sp)
{
    using namespace lldb;
    const uint32_t event_type = event_sp->GetType();
    ProcessSP process_sp = Process::ProcessEventData::GetProcessFromEvent(event_sp.get());

    StreamSP output_stream_sp = GetAsyncOutputStream();
    StreamSP error_stream_sp = GetAsyncErrorStream();
    const bool gui_enabled = IsForwardingEvents();

    if (!gui_enabled)
    {
        bool pop_process_io_handler = false;
        assert (process_sp);

        bool state_is_stopped = false;
        const bool got_state_changed = (event_type & Process::eBroadcastBitStateChanged) != 0;
        const bool got_stdout = (event_type & Process::eBroadcastBitSTDOUT) != 0;
        const bool got_stderr = (event_type & Process::eBroadcastBitSTDERR) != 0;
        if (got_state_changed)
        {
            StateType event_state = Process::ProcessEventData::GetStateFromEvent (event_sp.get());
            state_is_stopped = StateIsStoppedState(event_state, false);
        }

        // Display running state changes first before any STDIO
        if (got_state_changed && !state_is_stopped)
        {
            Process::HandleProcessStateChangedEvent (event_sp, output_stream_sp.get(), pop_process_io_handler);
        }

        // Now display and STDOUT
        if (got_stdout || got_state_changed)
        {
            GetProcessSTDOUT (process_sp.get(), output_stream_sp.get());
        }

        // Now display and STDERR
        if (got_stderr || got_state_changed)
        {
            GetProcessSTDERR (process_sp.get(), error_stream_sp.get());
        }

        // Now display any stopped state changes after any STDIO
        if (got_state_changed && state_is_stopped)
        {
            Process::HandleProcessStateChangedEvent (event_sp, output_stream_sp.get(), pop_process_io_handler);
        }

        output_stream_sp->Flush();
        error_stream_sp->Flush();

        if (pop_process_io_handler)
            process_sp->PopProcessIOHandler();
    }
}

void
Debugger::HandleThreadEvent (const EventSP &event_sp)
{
    // At present the only thread event we handle is the Frame Changed event,
    // and all we do for that is just reprint the thread status for that thread.
    using namespace lldb;
    const uint32_t event_type = event_sp->GetType();
    if (event_type == Thread::eBroadcastBitStackChanged   ||
        event_type == Thread::eBroadcastBitThreadSelected )
    {
        ThreadSP thread_sp (Thread::ThreadEventData::GetThreadFromEvent (event_sp.get()));
        if (thread_sp)
        {
            thread_sp->GetStatus(*GetAsyncOutputStream(), 0, 1, 1);
        }
    }
}

bool
Debugger::IsForwardingEvents ()
{
    return (bool)m_forward_listener_sp;
}

void
Debugger::EnableForwardEvents (const ListenerSP &listener_sp)
{
    m_forward_listener_sp = listener_sp;
}

void
Debugger::CancelForwardEvents (const ListenerSP &listener_sp)
{
    m_forward_listener_sp.reset();
}


void
Debugger::DefaultEventHandler()
{
    Listener& listener(GetListener());
    ConstString broadcaster_class_target(Target::GetStaticBroadcasterClass());
    ConstString broadcaster_class_process(Process::GetStaticBroadcasterClass());
    ConstString broadcaster_class_thread(Thread::GetStaticBroadcasterClass());
    BroadcastEventSpec target_event_spec (broadcaster_class_target,
                                          Target::eBroadcastBitBreakpointChanged);

    BroadcastEventSpec process_event_spec (broadcaster_class_process,
                                           Process::eBroadcastBitStateChanged   |
                                           Process::eBroadcastBitSTDOUT         |
                                           Process::eBroadcastBitSTDERR);

    BroadcastEventSpec thread_event_spec (broadcaster_class_thread,
                                          Thread::eBroadcastBitStackChanged     |
                                          Thread::eBroadcastBitThreadSelected   );
    
    listener.StartListeningForEventSpec (*this, target_event_spec);
    listener.StartListeningForEventSpec (*this, process_event_spec);
    listener.StartListeningForEventSpec (*this, thread_event_spec);
    listener.StartListeningForEvents (m_command_interpreter_ap.get(),
                                      CommandInterpreter::eBroadcastBitQuitCommandReceived      |
                                      CommandInterpreter::eBroadcastBitAsynchronousOutputData   |
                                      CommandInterpreter::eBroadcastBitAsynchronousErrorData    );

    // Let the thread that spawned us know that we have started up and
    // that we are now listening to all required events so no events get missed
    m_sync_broadcaster.BroadcastEvent(eBroadcastBitEventThreadIsListening);

    bool done = false;
    while (!done)
    {
        EventSP event_sp;
        if (listener.WaitForEvent(NULL, event_sp))
        {
            if (event_sp)
            {
                Broadcaster *broadcaster = event_sp->GetBroadcaster();
                if (broadcaster)
                {
                    uint32_t event_type = event_sp->GetType();
                    ConstString broadcaster_class (broadcaster->GetBroadcasterClass());
                    if (broadcaster_class == broadcaster_class_process)
                    {
                        HandleProcessEvent (event_sp);
                    }
                    else if (broadcaster_class == broadcaster_class_target)
                    {
                        if (Breakpoint::BreakpointEventData::GetEventDataFromEvent(event_sp.get()))
                        {
                            HandleBreakpointEvent (event_sp);
                        }
                    }
                    else if (broadcaster_class == broadcaster_class_thread)
                    {
                        HandleThreadEvent (event_sp);
                    }
                    else if (broadcaster == m_command_interpreter_ap.get())
                    {
                        if (event_type & CommandInterpreter::eBroadcastBitQuitCommandReceived)
                        {
                            done = true;
                        }
                        else if (event_type & CommandInterpreter::eBroadcastBitAsynchronousErrorData)
                        {
                            const char *data = reinterpret_cast<const char *>(EventDataBytes::GetBytesFromEvent (event_sp.get()));
                            if (data && data[0])
                            {
                                StreamSP error_sp (GetAsyncErrorStream());
                                if (error_sp)
                                {
                                    error_sp->PutCString(data);
                                    error_sp->Flush();
                                }
                            }
                        }
                        else if (event_type & CommandInterpreter::eBroadcastBitAsynchronousOutputData)
                        {
                            const char *data = reinterpret_cast<const char *>(EventDataBytes::GetBytesFromEvent (event_sp.get()));
                            if (data && data[0])
                            {
                                StreamSP output_sp (GetAsyncOutputStream());
                                if (output_sp)
                                {
                                    output_sp->PutCString(data);
                                    output_sp->Flush();
                                }
                            }
                        }
                    }
                }
                
                if (m_forward_listener_sp)
                    m_forward_listener_sp->AddEvent(event_sp);
            }
        }
    }
}

lldb::thread_result_t
Debugger::EventHandlerThread (lldb::thread_arg_t arg)
{
    ((Debugger *)arg)->DefaultEventHandler();
    return NULL;
}

bool
Debugger::StartEventHandlerThread()
{
    if (!m_event_handler_thread.IsJoinable())
    {
        // We must synchronize with the DefaultEventHandler() thread to ensure
        // it is up and running and listening to events before we return from
        // this function. We do this by listening to events for the
        // eBroadcastBitEventThreadIsListening from the m_sync_broadcaster
        Listener listener("lldb.debugger.event-handler");
        listener.StartListeningForEvents(&m_sync_broadcaster, eBroadcastBitEventThreadIsListening);

        // Use larger 8MB stack for this thread
        m_event_handler_thread = ThreadLauncher::LaunchThread("lldb.debugger.event-handler", EventHandlerThread,
                                                              this,
                                                              NULL,
                                                              g_debugger_event_thread_stack_bytes);

        // Make sure DefaultEventHandler() is running and listening to events before we return
        // from this function. We are only listening for events of type
        // eBroadcastBitEventThreadIsListening so we don't need to check the event, we just need
        // to wait an infinite amount of time for it (NULL timeout as the first parameter)
        lldb::EventSP event_sp;
        listener.WaitForEvent(NULL, event_sp);
    }
    return m_event_handler_thread.IsJoinable();
}

void
Debugger::StopEventHandlerThread()
{
    if (m_event_handler_thread.IsJoinable())
    {
        GetCommandInterpreter().BroadcastEvent(CommandInterpreter::eBroadcastBitQuitCommandReceived);
        m_event_handler_thread.Join(nullptr);
    }
}


lldb::thread_result_t
Debugger::IOHandlerThread (lldb::thread_arg_t arg)
{
    Debugger *debugger = (Debugger *)arg;
    debugger->ExecuteIOHandlers();
    debugger->StopEventHandlerThread();
    return NULL;
}

bool
Debugger::StartIOHandlerThread()
{
    if (!m_io_handler_thread.IsJoinable())
        m_io_handler_thread = ThreadLauncher::LaunchThread ("lldb.debugger.io-handler",
                                                            IOHandlerThread,
                                                            this,
                                                            NULL,
                                                            8*1024*1024); // Use larger 8MB stack for this thread
    return m_io_handler_thread.IsJoinable();
}

void
Debugger::StopIOHandlerThread()
{
    if (m_io_handler_thread.IsJoinable())
    {
        if (m_input_file_sp)
            m_input_file_sp->GetFile().Close();
        m_io_handler_thread.Join(nullptr);
    }
}

Target *
Debugger::GetDummyTarget()
{
    return m_target_list.GetDummyTarget (*this).get();
}

Target *
Debugger::GetSelectedOrDummyTarget(bool prefer_dummy)
{
    Target *target = nullptr;
    if (!prefer_dummy)
    {
        target = m_target_list.GetSelectedTarget().get();
        if (target)
            return target;
    }
    
    return GetDummyTarget();
}

