//===-- GDBRemoteCommunicationServerPlatform.cpp ----------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "GDBRemoteCommunicationServerPlatform.h"

#include <errno.h>

// C Includes
// C++ Includes
#include <cstring>
#include <chrono>

// Other libraries and framework includes
#include "lldb/Core/Log.h"
#include "lldb/Core/StreamString.h"
#include "lldb/Core/StructuredData.h"
#include "lldb/Host/Config.h"
#include "lldb/Host/ConnectionFileDescriptor.h"
#include "lldb/Host/Host.h"
#include "lldb/Host/StringConvert.h"
#include "lldb/Target/FileAction.h"
#include "lldb/Target/Platform.h"
#include "lldb/Target/Process.h"
#include "lldb/Target/UnixSignals.h"

// Project includes
#include "Utility/StringExtractorGDBRemote.h"
#include "Utility/UriParser.h"

using namespace lldb;
using namespace lldb_private;
using namespace lldb_private::process_gdb_remote;

//----------------------------------------------------------------------
// GDBRemoteCommunicationServerPlatform constructor
//----------------------------------------------------------------------
GDBRemoteCommunicationServerPlatform::GDBRemoteCommunicationServerPlatform() :
    GDBRemoteCommunicationServerCommon ("gdb-remote.server", "gdb-remote.server.rx_packet"),
    m_platform_sp (Platform::GetHostPlatform ()),
    m_port_map (),
    m_port_offset(0)
{
    RegisterMemberFunctionHandler(StringExtractorGDBRemote::eServerPacketType_qC,
                                  &GDBRemoteCommunicationServerPlatform::Handle_qC);
    RegisterMemberFunctionHandler(StringExtractorGDBRemote::eServerPacketType_qGetWorkingDir,
                                  &GDBRemoteCommunicationServerPlatform::Handle_qGetWorkingDir);
    RegisterMemberFunctionHandler(StringExtractorGDBRemote::eServerPacketType_qLaunchGDBServer,
                                  &GDBRemoteCommunicationServerPlatform::Handle_qLaunchGDBServer);
    RegisterMemberFunctionHandler(StringExtractorGDBRemote::eServerPacketType_qProcessInfo,
                                  &GDBRemoteCommunicationServerPlatform::Handle_qProcessInfo);
    RegisterMemberFunctionHandler(StringExtractorGDBRemote::eServerPacketType_QSetWorkingDir,
                                  &GDBRemoteCommunicationServerPlatform::Handle_QSetWorkingDir);
    RegisterMemberFunctionHandler(StringExtractorGDBRemote::eServerPacketType_jSignalsInfo,
                                  &GDBRemoteCommunicationServerPlatform::Handle_jSignalsInfo);

    RegisterPacketHandler(StringExtractorGDBRemote::eServerPacketType_interrupt,
                          [this](StringExtractorGDBRemote packet,
                                 Error &error,
                                 bool &interrupt,
                                 bool &quit)
                          {
                              error.SetErrorString("interrupt received");
                              interrupt = true;
                              return PacketResult::Success;
                          });
}

//----------------------------------------------------------------------
// Destructor
//----------------------------------------------------------------------
GDBRemoteCommunicationServerPlatform::~GDBRemoteCommunicationServerPlatform()
{
}

GDBRemoteCommunication::PacketResult
GDBRemoteCommunicationServerPlatform::Handle_qLaunchGDBServer (StringExtractorGDBRemote &packet)
{
#ifdef _WIN32
    return SendErrorResponse(9);
#else
    // Spawn a local debugserver as a platform so we can then attach or launch
    // a process...

    Log *log(GetLogIfAnyCategoriesSet(LIBLLDB_LOG_PLATFORM));
    if (log)
        log->Printf ("GDBRemoteCommunicationServerPlatform::%s() called", __FUNCTION__);

    // Sleep and wait a bit for debugserver to start to listen...
    ConnectionFileDescriptor file_conn;
    std::string hostname;
    // TODO: /tmp/ should not be hardcoded. User might want to override /tmp
    // with the TMPDIR environment variable
    packet.SetFilePos(::strlen ("qLaunchGDBServer;"));
    std::string name;
    std::string value;
    uint16_t port = UINT16_MAX;
    while (packet.GetNameColonValue(name, value))
    {
        if (name.compare ("host") == 0)
            hostname.swap(value);
        else if (name.compare ("port") == 0)
            port = StringConvert::ToUInt32(value.c_str(), 0, 0);
    }
    if (port == UINT16_MAX)
        port = GetNextAvailablePort();

    // Spawn a new thread to accept the port that gets bound after
    // binding to port 0 (zero).

    // ignore the hostname send from the remote end, just use the ip address
    // that we're currently communicating with as the hostname

    // Spawn a debugserver and try to get the port it listens to.
    ProcessLaunchInfo debugserver_launch_info;
    if (hostname.empty())
        hostname = "127.0.0.1";
    if (log)
        log->Printf("Launching debugserver with: %s:%u...", hostname.c_str(), port);

    // Do not run in a new session so that it can not linger after the
    // platform closes.
    debugserver_launch_info.SetLaunchInSeparateProcessGroup(false);
    debugserver_launch_info.SetMonitorProcessCallback(ReapDebugserverProcess, this, false);

    std::string platform_scheme;
    std::string platform_ip;
    int platform_port;
    std::string platform_path;
    bool ok = UriParser::Parse(GetConnection()->GetURI().c_str(), platform_scheme, platform_ip, platform_port, platform_path);
    assert(ok);
    Error error = StartDebugserverProcess (
                                     platform_ip.c_str(),
                                     port,
                                     debugserver_launch_info,
                                     port);

    lldb::pid_t debugserver_pid = debugserver_launch_info.GetProcessID();


    if (debugserver_pid != LLDB_INVALID_PROCESS_ID)
    {
        Mutex::Locker locker (m_spawned_pids_mutex);
        m_spawned_pids.insert(debugserver_pid);
        if (port > 0)
            AssociatePortWithProcess(port, debugserver_pid);
    }
    else
    {
        if (port > 0)
            FreePort (port);
    }

    if (error.Success())
    {
        if (log)
            log->Printf ("GDBRemoteCommunicationServerPlatform::%s() debugserver launched successfully as pid %" PRIu64, __FUNCTION__, debugserver_pid);

        char response[256];
        const int response_len = ::snprintf (response, sizeof(response), "pid:%" PRIu64 ";port:%u;", debugserver_pid, port + m_port_offset);
        assert (response_len < (int)sizeof(response));
        PacketResult packet_result = SendPacketNoLock (response, response_len);

        if (packet_result != PacketResult::Success)
        {
            if (debugserver_pid != LLDB_INVALID_PROCESS_ID)
                ::kill (debugserver_pid, SIGINT);
        }
        return packet_result;
    }
    else
    {
        if (log)
            log->Printf ("GDBRemoteCommunicationServerPlatform::%s() debugserver launch failed: %s", __FUNCTION__, error.AsCString ());
    }
    return SendErrorResponse (9);
#endif
}

GDBRemoteCommunication::PacketResult
GDBRemoteCommunicationServerPlatform::Handle_qProcessInfo (StringExtractorGDBRemote &packet)
{
    lldb::pid_t pid = m_process_launch_info.GetProcessID ();
    m_process_launch_info.Clear ();

    if (pid == LLDB_INVALID_PROCESS_ID)
        return SendErrorResponse (1);

    ProcessInstanceInfo proc_info;
    if (!Host::GetProcessInfo (pid, proc_info))
        return SendErrorResponse (1);

    StreamString response;
    CreateProcessInfoResponse_DebugServerStyle(proc_info, response);
    return SendPacketNoLock (response.GetData (), response.GetSize ());
}

GDBRemoteCommunication::PacketResult
GDBRemoteCommunicationServerPlatform::Handle_qGetWorkingDir (StringExtractorGDBRemote &packet)
{
    // If this packet is sent to a platform, then change the current working directory

    char cwd[PATH_MAX];
    if (getcwd(cwd, sizeof(cwd)) == NULL)
        return SendErrorResponse(errno);

    StreamString response;
    response.PutBytesAsRawHex8(cwd, strlen(cwd));
    return SendPacketNoLock(response.GetData(), response.GetSize());
}

GDBRemoteCommunication::PacketResult
GDBRemoteCommunicationServerPlatform::Handle_QSetWorkingDir (StringExtractorGDBRemote &packet)
{
    packet.SetFilePos (::strlen ("QSetWorkingDir:"));
    std::string path;
    packet.GetHexByteString (path);

    // If this packet is sent to a platform, then change the current working directory
    if (::chdir(path.c_str()) != 0)
        return SendErrorResponse (errno);
    return SendOKResponse ();
}

GDBRemoteCommunication::PacketResult
GDBRemoteCommunicationServerPlatform::Handle_qC (StringExtractorGDBRemote &packet)
{
    // NOTE: lldb should now be using qProcessInfo for process IDs.  This path here
    // should not be used.  It is reporting process id instead of thread id.  The
    // correct answer doesn't seem to make much sense for lldb-platform.
    // CONSIDER: flip to "unsupported".
    lldb::pid_t pid = m_process_launch_info.GetProcessID();

    StreamString response;
    response.Printf("QC%" PRIx64, pid);

    // If we launch a process and this GDB server is acting as a platform,
    // then we need to clear the process launch state so we can start
    // launching another process. In order to launch a process a bunch or
    // packets need to be sent: environment packets, working directory,
    // disable ASLR, and many more settings. When we launch a process we
    // then need to know when to clear this information. Currently we are
    // selecting the 'qC' packet as that packet which seems to make the most
    // sense.
    if (pid != LLDB_INVALID_PROCESS_ID)
    {
        m_process_launch_info.Clear();
    }

    return SendPacketNoLock (response.GetData(), response.GetSize());
}

GDBRemoteCommunication::PacketResult
GDBRemoteCommunicationServerPlatform::Handle_jSignalsInfo(StringExtractorGDBRemote &packet)
{
    StructuredData::Array signal_array;

    const auto &signals = Host::GetUnixSignals();
    for (auto signo = signals->GetFirstSignalNumber();
         signo != LLDB_INVALID_SIGNAL_NUMBER;
         signo = signals->GetNextSignalNumber(signo))
    {
        auto dictionary = std::make_shared<StructuredData::Dictionary>();

        dictionary->AddIntegerItem("signo", signo);
        dictionary->AddStringItem("name", signals->GetSignalAsCString(signo));

        bool suppress, stop, notify;
        signals->GetSignalInfo(signo, suppress, stop, notify);
        dictionary->AddBooleanItem("suppress", suppress);
        dictionary->AddBooleanItem("stop", stop);
        dictionary->AddBooleanItem("notify", notify);

        signal_array.Push(dictionary);
    }

    StreamString response;
    signal_array.Dump(response);
    return SendPacketNoLock(response.GetData(), response.GetSize());
}

bool
GDBRemoteCommunicationServerPlatform::DebugserverProcessReaped (lldb::pid_t pid)
{
    Mutex::Locker locker (m_spawned_pids_mutex);
    FreePortForProcess(pid);
    return m_spawned_pids.erase(pid) > 0;
}

bool
GDBRemoteCommunicationServerPlatform::ReapDebugserverProcess (void *callback_baton,
                                                   lldb::pid_t pid,
                                                   bool exited,
                                                   int signal,    // Zero for no signal
                                                   int status)    // Exit value of process if signal is zero
{
    GDBRemoteCommunicationServerPlatform *server = (GDBRemoteCommunicationServerPlatform *)callback_baton;
    server->DebugserverProcessReaped (pid);
    return true;
}

Error
GDBRemoteCommunicationServerPlatform::LaunchProcess ()
{
    if (!m_process_launch_info.GetArguments ().GetArgumentCount ())
        return Error ("%s: no process command line specified to launch", __FUNCTION__);

    // specify the process monitor if not already set.  This should
    // generally be what happens since we need to reap started
    // processes.
    if (!m_process_launch_info.GetMonitorProcessCallback ())
        m_process_launch_info.SetMonitorProcessCallback(ReapDebugserverProcess, this, false);

    Error error = m_platform_sp->LaunchProcess (m_process_launch_info);
    if (!error.Success ())
    {
        fprintf (stderr, "%s: failed to launch executable %s", __FUNCTION__, m_process_launch_info.GetArguments ().GetArgumentAtIndex (0));
        return error;
    }

    printf ("Launched '%s' as process %" PRIu64 "...\n", m_process_launch_info.GetArguments ().GetArgumentAtIndex (0), m_process_launch_info.GetProcessID());

    // add to list of spawned processes.  On an lldb-gdbserver, we
    // would expect there to be only one.
    const auto pid = m_process_launch_info.GetProcessID();
    if (pid != LLDB_INVALID_PROCESS_ID)
    {
        // add to spawned pids
        Mutex::Locker locker (m_spawned_pids_mutex);
        m_spawned_pids.insert(pid);
    }

    return error;
}

void
GDBRemoteCommunicationServerPlatform::SetPortMap (PortMap &&port_map)
{
    m_port_map = port_map;
}

uint16_t
GDBRemoteCommunicationServerPlatform::GetNextAvailablePort ()
{
    if (m_port_map.empty())
        return 0; // Bind to port zero and get a port, we didn't have any limitations

    for (auto &pair : m_port_map)
    {
        if (pair.second == LLDB_INVALID_PROCESS_ID)
        {
            pair.second = ~(lldb::pid_t)LLDB_INVALID_PROCESS_ID;
            return pair.first;
        }
    }
    return UINT16_MAX;
}

bool
GDBRemoteCommunicationServerPlatform::AssociatePortWithProcess (uint16_t port, lldb::pid_t pid)
{
    PortMap::iterator pos = m_port_map.find(port);
    if (pos != m_port_map.end())
    {
        pos->second = pid;
        return true;
    }
    return false;
}

bool
GDBRemoteCommunicationServerPlatform::FreePort (uint16_t port)
{
    PortMap::iterator pos = m_port_map.find(port);
    if (pos != m_port_map.end())
    {
        pos->second = LLDB_INVALID_PROCESS_ID;
        return true;
    }
    return false;
}

bool
GDBRemoteCommunicationServerPlatform::FreePortForProcess (lldb::pid_t pid)
{
    if (!m_port_map.empty())
    {
        for (auto &pair : m_port_map)
        {
            if (pair.second == pid)
            {
                pair.second = LLDB_INVALID_PROCESS_ID;
                return true;
            }
        }
    }
    return false;
}

void
GDBRemoteCommunicationServerPlatform::SetPortOffset (uint16_t port_offset)
{
    m_port_offset = port_offset;
}
