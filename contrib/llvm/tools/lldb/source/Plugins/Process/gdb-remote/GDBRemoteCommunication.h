//===-- GDBRemoteCommunication.h --------------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef liblldb_GDBRemoteCommunication_h_
#define liblldb_GDBRemoteCommunication_h_

// C Includes
// C++ Includes
#include <list>
#include <string>
#include <queue>

// Other libraries and framework includes
// Project includes
#include "lldb/lldb-public.h"
#include "lldb/Core/Communication.h"
#include "lldb/Core/Listener.h"
#include "lldb/Host/HostThread.h"
#include "lldb/Host/Mutex.h"
#include "lldb/Host/Predicate.h"
#include "lldb/Host/TimeValue.h"

#include "Utility/StringExtractorGDBRemote.h"

namespace lldb_private {
namespace process_gdb_remote {

typedef enum
{
    eStoppointInvalid = -1,
    eBreakpointSoftware = 0,
    eBreakpointHardware,
    eWatchpointWrite,
    eWatchpointRead,
    eWatchpointReadWrite
} GDBStoppointType;

enum class CompressionType
{
    None = 0,       // no compression
    ZlibDeflate,    // zlib's deflate compression scheme, requires zlib or Apple's libcompression
    LZFSE,          // an Apple compression scheme, requires Apple's libcompression
    LZ4,            // lz compression - called "lz4 raw" in libcompression terms, compat with https://code.google.com/p/lz4/
    LZMA,           // Lempel–Ziv–Markov chain algorithm
};

class ProcessGDBRemote;

class GDBRemoteCommunication : public Communication
{
public:
    enum
    {
        eBroadcastBitRunPacketSent = kLoUserBroadcastBit,
        eBroadcastBitGdbReadThreadGotNotify = kLoUserBroadcastBit << 1 // Sent when we received a notify packet.
    };

    enum class PacketType
    {
        Invalid = 0,
        Standard,
        Notify
    };

    enum class PacketResult
    {
        Success = 0,        // Success
        ErrorSendFailed,    // Error sending the packet
        ErrorSendAck,       // Didn't get an ack back after sending a packet
        ErrorReplyFailed,   // Error getting the reply
        ErrorReplyTimeout,  // Timed out waiting for reply
        ErrorReplyInvalid,  // Got a reply but it wasn't valid for the packet that was sent
        ErrorReplyAck,      // Sending reply ack failed
        ErrorDisconnected,  // We were disconnected
        ErrorNoSequenceLock // We couldn't get the sequence lock for a multi-packet request
    };

    // Class to change the timeout for a given scope and restore it to the original value when the
    // created ScopedTimeout object got out of scope
    class ScopedTimeout
    {
    public:
        ScopedTimeout (GDBRemoteCommunication& gdb_comm, uint32_t timeout);
        ~ScopedTimeout ();

    private:
        GDBRemoteCommunication& m_gdb_comm;
        uint32_t m_saved_timeout;
    };

    //------------------------------------------------------------------
    // Constructors and Destructors
    //------------------------------------------------------------------
    GDBRemoteCommunication(const char *comm_name, 
                           const char *listener_name);

    virtual
    ~GDBRemoteCommunication();

    PacketResult
    GetAck ();

    size_t
    SendAck ();

    size_t
    SendNack ();

    char
    CalculcateChecksum (const char *payload,
                        size_t payload_length);

    bool
    GetSequenceMutex (Mutex::Locker& locker, const char *failure_message = NULL);

    PacketType
    CheckForPacket (const uint8_t *src, 
                    size_t src_len, 
                    StringExtractorGDBRemote &packet);
    bool
    IsRunning() const
    {
        return m_public_is_running.GetValue();
    }

    bool
    GetSendAcks ()
    {
        return m_send_acks;
    }

    //------------------------------------------------------------------
    // Client and server must implement these pure virtual functions
    //------------------------------------------------------------------
    virtual bool
    GetThreadSuffixSupported () = 0;

    //------------------------------------------------------------------
    // Set the global packet timeout.
    //
    // For clients, this is the timeout that gets used when sending
    // packets and waiting for responses. For servers, this might not
    // get used, and if it doesn't this should be moved to the
    // GDBRemoteCommunicationClient.
    //------------------------------------------------------------------
    uint32_t 
    SetPacketTimeout (uint32_t packet_timeout)
    {
        const uint32_t old_packet_timeout = m_packet_timeout;
        m_packet_timeout = packet_timeout;
        return old_packet_timeout;
    }

    uint32_t
    GetPacketTimeoutInMicroSeconds () const
    {
        return m_packet_timeout * TimeValue::MicroSecPerSec;
    }
    //------------------------------------------------------------------
    // Start a debugserver instance on the current host using the
    // supplied connection URL.
    //------------------------------------------------------------------
    Error
    StartDebugserverProcess (const char *hostname,
                             uint16_t in_port, // If set to zero, then out_port will contain the bound port on exit
                             ProcessLaunchInfo &launch_info,
                             uint16_t &out_port);

    void
    DumpHistory(Stream &strm);
    
protected:

    class History
    {
    public:
        enum PacketType
        {
            ePacketTypeInvalid = 0,
            ePacketTypeSend,
            ePacketTypeRecv
        };

        struct Entry
        {
            Entry() :
                packet(),
                type (ePacketTypeInvalid),
                bytes_transmitted (0),
                packet_idx (0),
                tid (LLDB_INVALID_THREAD_ID)
            {
            }
            
            void
            Clear ()
            {
                packet.clear();
                type = ePacketTypeInvalid;
                bytes_transmitted = 0;
                packet_idx = 0;
                tid = LLDB_INVALID_THREAD_ID;
            }
            std::string packet;
            PacketType type;
            uint32_t bytes_transmitted;
            uint32_t packet_idx;
            lldb::tid_t tid;
        };

        History (uint32_t size);
        
        ~History ();

        // For single char packets for ack, nack and /x03
        void
        AddPacket (char packet_char,
                   PacketType type,
                   uint32_t bytes_transmitted);
        void
        AddPacket (const std::string &src,
                   uint32_t src_len,
                   PacketType type,
                   uint32_t bytes_transmitted);
        
        void
        Dump (Stream &strm) const;

        void
        Dump (Log *log) const;

        bool
        DidDumpToLog () const
        {
            return m_dumped_to_log;
        }
    
protected:
        uint32_t
        GetFirstSavedPacketIndex () const
        {
            if (m_total_packet_count < m_packets.size())
                return 0;
            else
                return m_curr_idx + 1;
        }

        uint32_t
        GetNumPacketsInHistory () const
        {
            if (m_total_packet_count < m_packets.size())
                return m_total_packet_count;
            else
                return (uint32_t)m_packets.size();
        }

        uint32_t
        GetNextIndex()
        {
            ++m_total_packet_count;
            const uint32_t idx = m_curr_idx;
            m_curr_idx = NormalizeIndex(idx + 1);
            return idx;
        }

        uint32_t
        NormalizeIndex (uint32_t i) const
        {
            return i % m_packets.size();
        }

        
        std::vector<Entry> m_packets;
        uint32_t m_curr_idx;
        uint32_t m_total_packet_count;
        mutable bool m_dumped_to_log;
    };

    PacketResult
    SendPacket (const char *payload,
                size_t payload_length);

    PacketResult
    SendPacketNoLock (const char *payload, 
                      size_t payload_length);

    PacketResult
    ReadPacket (StringExtractorGDBRemote &response, uint32_t timeout_usec, bool sync_on_timeout);

    // Pop a packet from the queue in a thread safe manner
    PacketResult
    PopPacketFromQueue (StringExtractorGDBRemote &response, uint32_t timeout_usec);

    PacketResult
    WaitForPacketWithTimeoutMicroSecondsNoLock (StringExtractorGDBRemote &response, 
                                                uint32_t timeout_usec,
                                                bool sync_on_timeout);

    bool
    WaitForNotRunningPrivate (const TimeValue *timeout_ptr);

    bool
    CompressionIsEnabled ()
    {
        return m_compression_type != CompressionType::None;
    }

    // If compression is enabled, decompress the packet in m_bytes and update
    // m_bytes with the uncompressed version.
    // Returns 'true' packet was decompressed and m_bytes is the now-decompressed text.
    // Returns 'false' if unable to decompress or if the checksum was invalid.
    //
    // NB: Once the packet has been decompressed, checksum cannot be computed based
    // on m_bytes.  The checksum was for the compressed packet.
    bool
    DecompressPacket ();

    //------------------------------------------------------------------
    // Classes that inherit from GDBRemoteCommunication can see and modify these
    //------------------------------------------------------------------
    uint32_t m_packet_timeout;
    uint32_t m_echo_number;
    LazyBool m_supports_qEcho;
#ifdef ENABLE_MUTEX_ERROR_CHECKING
    TrackingMutex m_sequence_mutex;
#else
    Mutex m_sequence_mutex;    // Restrict access to sending/receiving packets to a single thread at a time
#endif
    Predicate<bool> m_public_is_running;
    Predicate<bool> m_private_is_running;
    History m_history;
    bool m_send_acks;
    bool m_is_platform; // Set to true if this class represents a platform,
                        // false if this class represents a debug session for
                        // a single process
    
    CompressionType m_compression_type;

    Error
    StartListenThread (const char *hostname = "127.0.0.1", uint16_t port = 0);

    bool
    JoinListenThread ();

    static lldb::thread_result_t
    ListenThread (lldb::thread_arg_t arg);

    // GDB-Remote read thread
    //  . this thread constantly tries to read from the communication
    //    class and stores all packets received in a queue.  The usual
    //    threads read requests simply pop packets off the queue in the
    //    usual order.
    //    This setup allows us to intercept and handle async packets, such
    //    as the notify packet.

    // This method is defined as part of communication.h
    // when the read thread gets any bytes it will pass them on to this function
    virtual void AppendBytesToCache (const uint8_t * bytes, size_t len, bool broadcast, lldb::ConnectionStatus status);

private:

    std::queue<StringExtractorGDBRemote> m_packet_queue; // The packet queue
    lldb_private::Mutex m_packet_queue_mutex;            // Mutex for accessing queue
    Condition m_condition_queue_not_empty;               // Condition variable to wait for packets

    HostThread m_listen_thread;
    std::string m_listen_url;

    //------------------------------------------------------------------
    // For GDBRemoteCommunication only
    //------------------------------------------------------------------
    DISALLOW_COPY_AND_ASSIGN (GDBRemoteCommunication);
};

} // namespace process_gdb_remote
} // namespace lldb_private

#endif  // liblldb_GDBRemoteCommunication_h_
