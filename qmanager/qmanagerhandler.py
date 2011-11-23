# socket server

import SocketServer, threading, freebsd, sys
from qmanagerclient import *
from acl import *

DEBUG_HANDLER = False

class ServerReplyException(Exception):
    pass

class InvalidCommandError(ServerReplyException):
    error = 401

class NoMachinesError(ServerReplyException):
    error = 402

class WouldBlockError(ServerReplyException):
    error = 403

class NoSuchJobError(ServerReplyException):
    error = 404

class JobNotRunningError(ServerReplyException):
    error = 405

class BodyError(ServerReplyException):
    error = 406

class ArgumentError(ServerReplyException):
    error = 407

class PermissionDeniedError(ServerReplyException):
    error = 408

class JobRunningError(ServerReplyException):
    error = 409

class JobConnectedError(ServerReplyException):
    error = 410

class MachineExistsError(ServerReplyException):
    error = 411

class QManagerServerConn(QManagerConnection):
    """ Server -> Client connection object.  Encapsulates the incoming
    request we are processing and the socket to communicate results.
 """

    def __init__(self, rfile, wfile, event):
        super(QManagerServerConn, self).__init__()

        # Files to communicate with socket
        self.rfile = rfile
        self.wfile = wfile

        # event to signal wakeup of handler for connection termination
        self.event = event

        # Command from received message
        self.cmd = None
        # Arguments of received message
        self.args = None
        # Job that owns the connection
        self.job = None

        # UID of peer
        self.uid = None
        # GIDs of peer
        self.gids = None

    def receive(self):
        return self.CS.receive(self.rfile)

    def send(self, code, args = None, raiseexception = False, finish = True):
        """ Send a status message with optional arguments
        args = {arguments to send}
        more = boolean indicating whether caller will provide more output
        raiseexception = boolean (raise exception on socket error)
        finish = boolean (close connection after sending message)

        Raises: ValueError if required argument missing
        KeyError if invalid code specified
        MissingArgumentError, UnknownArgumentError: if args invalid
        IOError: socket write error (if raiseexception)
        """

        try:
            self.SC.send(self.wfile, str(code), args)
        except (KeyError, MissingArgumentError, UnknownArgumentError):
            raise
        except:
            try:
                self.finish()
                
                if self.job:
                    # Unregister connection from job
                    del self.job.conn
                    self.job.conn = None
                    if not self.job.running:
                        self.job.unblock(None)
                        print "Send error for %s" % self.job.id
                        self.job.finish()
            except:
                # if raiseexception:
                raise # XXX for debugging
        
        if finish:
            self.finish()
        else:
            self.wfile.flush()

    def finish(self):
        """ Connection is finished, flush and terminate it """

        if self.wfile:
            try:
                self.wfile.flush()
            except:
                pass
            self.wfile.close()
        if self.event:
            self.event.set()

class MyUnixStreamServer(SocketServer.ThreadingUnixStreamServer, object):
    """ Extends ThreadingUnixStreamServer with a pointer to the queue associated with the handler """

    wqueue = None

    def __init__(self, sock, handler, wqueue):
        super(MyUnixStreamServer, self).__init__(sock, handler)
        self.request_queue_size = 100
        self.wqueue = wqueue

class UNIXhandler(SocketServer.StreamRequestHandler):

    event = None

    def handle(self):
        """ Input loop: queues commands for processing by the main work loop """

        self.event = threading.Event()
        conn = QManagerServerConn(self.rfile, self.wfile, self.event)

        try:
            if DEBUG_HANDLER:
                print "at UNIXhandler.handle()"
            (conn.cmd, conn.args) = conn.receive()
            if DEBUG_HANDLER:
                print "past conn.receive() for " + conn.cmd + " and " + str( conn.args )
            (conn.uid, conn.gids) = freebsd.getpeerid(self.request)
            if DEBUG_HANDLER:
                print "past freebsd.getpeerid"
            if conn.uid == 0:
                # Allow root to override uid/gids, when proxying for a user
                try:
                    conn.uid = getuidbyname(conn.args['uid'])
                except KeyError, TypeError:
                    pass
                try:
                    conn.gids = tuple(getgidbyname(gid) for gid in conn.args['gids'].split(","))
                except KeyError, TypeError:
                    pass
        except Exception, e:
            if DEBUG_HANDLER:
                print "UNIXhandler.handle(): exception: " + str( e )
            conn.send(401) # XXX other errors too
            return
            
        self.server.wqueue.put(conn)
        if DEBUG_HANDLER:
            print "past wqueue.put() for " + conn.cmd + " and " + str( conn.args )
        self.event.wait() # Don't close socket until the command finishes
        if DEBUG_HANDLER:
            print "past event.wait() for " + conn.cmd + " and " + str( conn.args )
