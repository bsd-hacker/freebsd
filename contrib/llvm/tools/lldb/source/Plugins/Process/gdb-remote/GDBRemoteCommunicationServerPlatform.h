//===-- GDBRemoteCommunicationServerPlatform.h ------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef liblldb_GDBRemoteCommunicationServerPlatform_h_
#define liblldb_GDBRemoteCommunicationServerPlatform_h_

#include "GDBRemoteCommunicationServerCommon.h"

namespace lldb_private {
namespace process_gdb_remote {

class GDBRemoteCommunicationServerPlatform :
    public GDBRemoteCommunicationServerCommon
{
public:
    typedef std::map<uint16_t, lldb::pid_t> PortMap;

    GDBRemoteCommunicationServerPlatform();

    virtual
    ~GDBRemoteCommunicationServerPlatform();

    Error
    LaunchProcess () override;

    // Set both ports to zero to let the platform automatically bind to 
    // a port chosen by the OS.
    void
    SetPortMap (PortMap &&port_map);

    //----------------------------------------------------------------------
    // If we are using a port map where we can only use certain ports,
    // get the next available port.
    //
    // If we are using a port map and we are out of ports, return UINT16_MAX
    //
    // If we aren't using a port map, return 0 to indicate we should bind to
    // port 0 and then figure out which port we used.
    //----------------------------------------------------------------------
    uint16_t
    GetNextAvailablePort ();

    bool
    AssociatePortWithProcess (uint16_t port, lldb::pid_t pid);

    bool
    FreePort (uint16_t port);

    bool
    FreePortForProcess (lldb::pid_t pid);

    void
    SetPortOffset (uint16_t port_offset);

protected:
    lldb::PlatformSP m_platform_sp;

    PortMap m_port_map;
    uint16_t m_port_offset;

    PacketResult
    Handle_qLaunchGDBServer (StringExtractorGDBRemote &packet);

    PacketResult
    Handle_qProcessInfo (StringExtractorGDBRemote &packet);

    PacketResult
    Handle_qGetWorkingDir (StringExtractorGDBRemote &packet);

    PacketResult
    Handle_QSetWorkingDir (StringExtractorGDBRemote &packet);

    PacketResult
    Handle_qC (StringExtractorGDBRemote &packet);

    PacketResult
    Handle_jSignalsInfo(StringExtractorGDBRemote &packet);

private:
    bool
    DebugserverProcessReaped (lldb::pid_t pid);

    static bool
    ReapDebugserverProcess (void *callback_baton,
                            lldb::pid_t pid,
                            bool exited,
                            int signal,
                            int status);

    //------------------------------------------------------------------
    // For GDBRemoteCommunicationServerPlatform only
    //------------------------------------------------------------------
    DISALLOW_COPY_AND_ASSIGN (GDBRemoteCommunicationServerPlatform);
};

} // namespace process_gdb_remote
} // namespace lldb_private

#endif  // liblldb_GDBRemoteCommunicationServerPlatform_h_
