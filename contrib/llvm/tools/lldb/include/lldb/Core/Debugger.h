//===-- Debugger.h ----------------------------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef liblldb_Debugger_h_
#define liblldb_Debugger_h_
#if defined(__cplusplus)


#include <stdint.h>

#include "lldb/lldb-public.h"
#include "lldb/Core/Broadcaster.h"
#include "lldb/Core/FormatEntity.h"
#include "lldb/Core/IOHandler.h"
#include "lldb/Core/Listener.h"
#include "lldb/Core/SourceManager.h"
#include "lldb/Core/UserID.h"
#include "lldb/Core/UserSettingsController.h"
#include "lldb/Host/HostThread.h"
#include "lldb/Host/Terminal.h"
#include "lldb/Target/Platform.h"
#include "lldb/Target/TargetList.h"

namespace llvm
{
namespace sys
{
class DynamicLibrary;
}
}

namespace lldb_private {

//----------------------------------------------------------------------
/// @class Debugger Debugger.h "lldb/Core/Debugger.h"
/// @brief A class to manage flag bits.
///
/// Provides a global root objects for the debugger core.
//----------------------------------------------------------------------


class Debugger :
    public std::enable_shared_from_this<Debugger>,
    public UserID,
    public Properties,
    public BroadcasterManager
{
friend class SourceManager;  // For GetSourceFileCache.

public:

    static lldb::DebuggerSP
    CreateInstance (lldb::LogOutputCallback log_callback = NULL, void *baton = NULL);

    static lldb::TargetSP
    FindTargetWithProcessID (lldb::pid_t pid);
    
    static lldb::TargetSP
    FindTargetWithProcess (Process *process);

    static void
    Initialize(LoadPluginCallbackType load_plugin_callback);
    
    static void
    Terminate();
    
    static void
    SettingsInitialize ();
    
    static void
    SettingsTerminate ();
    
    static void
    Destroy (lldb::DebuggerSP &debugger_sp);

    virtual
    ~Debugger ();
    
    void Clear();

    bool
    GetAsyncExecution ();

    void
    SetAsyncExecution (bool async);

    lldb::StreamFileSP
    GetInputFile ()
    {
        return m_input_file_sp;
    }

    lldb::StreamFileSP
    GetOutputFile ()
    {
        return m_output_file_sp;
    }

    lldb::StreamFileSP
    GetErrorFile ()
    {
        return m_error_file_sp;
    }

    
    
    void
    SetInputFileHandle (FILE *fh, bool tranfer_ownership);

    void
    SetOutputFileHandle (FILE *fh, bool tranfer_ownership);

    void
    SetErrorFileHandle (FILE *fh, bool tranfer_ownership);
    
    void
    SaveInputTerminalState();
    
    void
    RestoreInputTerminalState();

    lldb::StreamSP
    GetAsyncOutputStream ();
    
    lldb::StreamSP
    GetAsyncErrorStream ();
    
    CommandInterpreter &
    GetCommandInterpreter ()
    {
        assert (m_command_interpreter_ap.get());
        return *m_command_interpreter_ap;
    }

    Listener &
    GetListener ()
    {
        return m_listener;
    }

    // This returns the Debugger's scratch source manager.  It won't be able to look up files in debug
    // information, but it can look up files by absolute path and display them to you.
    // To get the target's source manager, call GetSourceManager on the target instead.
    SourceManager &
    GetSourceManager ();
    
    lldb::TargetSP
    GetSelectedTarget ()
    {
        return m_target_list.GetSelectedTarget ();
    }

    ExecutionContext
    GetSelectedExecutionContext();
    //------------------------------------------------------------------
    /// Get accessor for the target list.
    ///
    /// The target list is part of the global debugger object. This
    /// the single debugger shared instance to control where targets
    /// get created and to allow for tracking and searching for targets
    /// based on certain criteria.
    ///
    /// @return
    ///     A global shared target list.
    //------------------------------------------------------------------
    TargetList &
    GetTargetList ()
    {
        return m_target_list;
    }

    PlatformList &
    GetPlatformList ()
    {
        return m_platform_list;
    }

    void
    DispatchInputInterrupt ();

    void
    DispatchInputEndOfFile ();

    //------------------------------------------------------------------
    // If any of the streams are not set, set them to the in/out/err
    // stream of the top most input reader to ensure they at least have
    // something
    //------------------------------------------------------------------
    void
    AdoptTopIOHandlerFilesIfInvalid (lldb::StreamFileSP &in,
                                     lldb::StreamFileSP &out,
                                     lldb::StreamFileSP &err);

    void
    PushIOHandler (const lldb::IOHandlerSP& reader_sp);

    bool
    PopIOHandler (const lldb::IOHandlerSP& reader_sp);
    
    // Synchronously run an input reader until it is done
    void
    RunIOHandler (const lldb::IOHandlerSP& reader_sp);
    
    bool
    IsTopIOHandler (const lldb::IOHandlerSP& reader_sp);

    void
    PrintAsync (const char *s, size_t len, bool is_stdout);

    ConstString
    GetTopIOHandlerControlSequence(char ch);

    const char *
    GetIOHandlerCommandPrefix();

    const char *
    GetIOHandlerHelpPrologue();

    static lldb::DebuggerSP
    FindDebuggerWithID (lldb::user_id_t id);
    
    static lldb::DebuggerSP
    FindDebuggerWithInstanceName (const ConstString &instance_name);
    
    static size_t
    GetNumDebuggers();
    
    static lldb::DebuggerSP
    GetDebuggerAtIndex (size_t index);

    static bool
    FormatDisassemblerAddress (const FormatEntity::Entry *format,
                               const SymbolContext *sc,
                               const SymbolContext *prev_sc,
                               const ExecutionContext *exe_ctx,
                               const Address *addr,
                               Stream &s);

    void
    ClearIOHandlers ();

    bool
    GetCloseInputOnEOF () const;
    
    void
    SetCloseInputOnEOF (bool b);
    
    bool
    EnableLog (const char *channel, const char **categories, const char *log_file, uint32_t log_options, Stream &error_stream);

    void
    SetLoggingCallback (lldb::LogOutputCallback log_callback, void *baton);
    

    //----------------------------------------------------------------------
    // Properties Functions
    //----------------------------------------------------------------------
    enum StopDisassemblyType
    {
        eStopDisassemblyTypeNever = 0,
        eStopDisassemblyTypeNoSource,
        eStopDisassemblyTypeAlways
    };
    
    virtual Error
    SetPropertyValue (const ExecutionContext *exe_ctx,
                      VarSetOperationType op,
                      const char *property_path,
                      const char *value);

    bool
    GetAutoConfirm () const;

    const FormatEntity::Entry *
    GetDisassemblyFormat() const;

    const FormatEntity::Entry *
    GetFrameFormat() const;

    const FormatEntity::Entry *
    GetThreadFormat() const;
    
    lldb::ScriptLanguage
    GetScriptLanguage() const;
    
    bool
    SetScriptLanguage (lldb::ScriptLanguage script_lang);
    
    uint32_t
    GetTerminalWidth () const;
    
    bool
    SetTerminalWidth (uint32_t term_width);
    
    const char *
    GetPrompt() const;
    
    void
    SetPrompt(const char *p);
    
    bool
    GetUseExternalEditor () const;
    
    bool
    SetUseExternalEditor (bool use_external_editor_p);
    
    bool
    GetUseColor () const;
    
    bool
    SetUseColor (bool use_color);
    
    uint32_t
    GetStopSourceLineCount (bool before) const;
    
    StopDisassemblyType
    GetStopDisassemblyDisplay () const;
    
    uint32_t
    GetDisassemblyLineCount () const;
    
    bool
    GetAutoOneLineSummaries () const;
    
    bool
    GetEscapeNonPrintables () const;
    
    bool
    GetNotifyVoid () const;
    
    const ConstString &
    GetInstanceName()
    {
        return m_instance_name;
    }
        
    bool
    LoadPlugin (const FileSpec& spec, Error& error);

    void
    ExecuteIOHandlers();
    
    bool
    IsForwardingEvents ();

    void
    EnableForwardEvents (const lldb::ListenerSP &listener_sp);

    void
    CancelForwardEvents (const lldb::ListenerSP &listener_sp);
    
    bool
    IsHandlingEvents () const
    {
        return m_event_handler_thread.IsJoinable();
    }

    // This is for use in the command interpreter, when you either want the selected target, or if no target
    // is present you want to prime the dummy target with entities that will be copied over to new targets.
    Target *GetSelectedOrDummyTarget(bool prefer_dummy = false);
    Target *GetDummyTarget();

protected:

    friend class CommandInterpreter;

    bool
    StartEventHandlerThread();

    void
    StopEventHandlerThread();

    static lldb::thread_result_t
    EventHandlerThread (lldb::thread_arg_t arg);

    bool
    StartIOHandlerThread();
    
    void
    StopIOHandlerThread();
    
    static lldb::thread_result_t
    IOHandlerThread (lldb::thread_arg_t arg);

    void
    DefaultEventHandler();

    void
    HandleBreakpointEvent (const lldb::EventSP &event_sp);
    
    void
    HandleProcessEvent (const lldb::EventSP &event_sp);

    void
    HandleThreadEvent (const lldb::EventSP &event_sp);

    size_t
    GetProcessSTDOUT (Process *process, Stream *stream);
    
    size_t
    GetProcessSTDERR (Process *process, Stream *stream);

    SourceManager::SourceFileCache &
    GetSourceFileCache ()
    {
        return m_source_file_cache;
    }

    void
    InstanceInitialize ();

    lldb::StreamFileSP m_input_file_sp;
    lldb::StreamFileSP m_output_file_sp;
    lldb::StreamFileSP m_error_file_sp;
    TerminalState m_terminal_state;
    TargetList m_target_list;

    PlatformList m_platform_list;
    Listener m_listener;
    std::unique_ptr<SourceManager> m_source_manager_ap;    // This is a scratch source manager that we return if we have no targets.
    SourceManager::SourceFileCache m_source_file_cache; // All the source managers for targets created in this debugger used this shared
                                                        // source file cache.
    std::unique_ptr<CommandInterpreter> m_command_interpreter_ap;

    IOHandlerStack m_input_reader_stack;
    typedef std::map<std::string, lldb::StreamWP> LogStreamMap;
    LogStreamMap m_log_streams;
    lldb::StreamSP m_log_callback_stream_sp;
    ConstString m_instance_name;
    static LoadPluginCallbackType g_load_plugin_callback;
    typedef std::vector<llvm::sys::DynamicLibrary> LoadedPluginsList;
    LoadedPluginsList m_loaded_plugins;
    HostThread m_event_handler_thread;
    HostThread m_io_handler_thread;
    Broadcaster m_sync_broadcaster;
    lldb::ListenerSP m_forward_listener_sp;

    //----------------------------------------------------------------------
    // Events for m_sync_broadcaster
    //----------------------------------------------------------------------
    enum
    {
        eBroadcastBitEventThreadIsListening   = (1 << 0),
    };

private:

    // Use Debugger::CreateInstance() to get a shared pointer to a new
    // debugger object
    Debugger (lldb::LogOutputCallback m_log_callback, void *baton);

    DISALLOW_COPY_AND_ASSIGN (Debugger);
    
};

} // namespace lldb_private

#endif  // #if defined(__cplusplus)
#endif  // liblldb_Debugger_h_
