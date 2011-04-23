import os, socket, cPickle
from time import sleep

ARGS='args'
OPTARGS='optargs'
HELP='help'

class UnknownArgumentError(Exception):
    """ Unknown argument(s) were supplied """

    def __init__(self, args):
        self.args = args

    def __str__(self):
        return " ".join(self.args)

class MissingArgumentError(Exception):
    """ One of more required arguments was missing """

    def __init__(self, args):
        self.args = args

    def __str__(self):
        return " ".join(self.args)

class ServerError(Exception):
    """ Unexpected response from server """

    def __init__(self, args):
        self.args = args

    def __str__(self):
        return self.args

class RequestError(Exception):
    """ Server replied to request with an error message """

    def __init__(self, value):
        self.value = value

    def __str__(self):
        return self.value

class Message(object):

    MSG_VERSION=1

    def __init__(self, msgs):
        self.msgs = msgs

    def send(self, wfile, cmd, args):
        """
        Send a command to the server.

        Args:
        c == command name (str)
        args == {arguments to send} 
        body == list of lines for message body

        Raises:
        KeyError if cmd is invalid
        (MissingArgumentError, tuple(args)) if required argument(s) missing
        (UnknownArgumentError, tuple(args)) if extra arguments supplied
        XXX more
        """
        if args is None:
            args = {}

        self.validate(cmd, args)

        #print "Writing version"
        wfile.write("%s\n" % self.MSG_VERSION)
        #print "Writing cmd %s" % cmd
        wfile.write("%s\n" % cmd)

        #print "Writing args %s" % args
        cPickle.dump(args, wfile, cPickle.HIGHEST_PROTOCOL)
        wfile.write("EOM\n")
        wfile.flush()

    def receive(self, rfile):
        line = rfile.readline().strip()
        try:
            ver = int(line)
        except:
            #print "Failed to convert version"
            raise ServerError, "Bad version"
        if ver != self.MSG_VERSION:
            #print "Wrong message version"
            raise ServerError, "Wrong version %s" % ver
        
        try:
            cmd = rfile.readline().strip()
            msg = self.msgs[cmd]
            #print "Read command %s" % cmd
        except:
            print "Failed to read cmd:"
            print cmd
            raise ServerError, "Bad cmd"

        u = cPickle.Unpickler(rfile)
        # Prevent unpickling of classes
        u.find_global = None

        try:
            args = u.load()
            if not isinstance(args, dict):
                #print "args of wrong type: %s" % args
                raise ServerError, "Bad pickle"
            #print "Read args %s" % args
        except:
            #print "Error unpickling"
            raise ServerError, "Couldn't unpickle"
        dummy = rfile.readline().strip()
        assert dummy == "EOM"

        return (cmd, args)

    def validate(self, cmd = None, args = None):
        """ Validate an argument dictionary for a command

        Args:
        cmd = str (command name)
        args = {} (arguments to pass)

        Raises:
        KeyError if cmd is invalid
        (MissingArgumentError, tuple(args)) if required argument(s) missing
        (UnknownArgumentError, tuple(args)) if extra arguments supplied
        """

        msg = self.msgs[cmd]

        keys = args.keys()
        req = msg[ARGS]
        opt = msg.get(OPTARGS, [])

        missing = tuple(i for i in req if i not in keys)
        if missing:
            raise MissingArgumentError, (missing,)

        extra = tuple(i for i in keys if i not in req and i not in opt)
        if extra:
            raise UnknownArgumentError, (extra,)

    def getargs(self, cmd):
        """ Return the mandatory arguments for a cmd
        Raises:
        KeyError if cmd not found
        """

        return self.msgs[cmd][ARGS]

    def getoptargs(self, cmd):
        """ Return the optional arguments for a cmd
        Raises:
        KeyError if cmd not found
        """

        return self.msgs[cmd][OPTARGS]

    def getstatus(self, cmd):
        """ Return the status message for a cmd
        Raises:
        KeyError if cmd not found
        """

        return self.msgs[cmd][HELP]

class QManagerConnection(object):
    """ Base class for interacting between client and server """

    # Possible status codes, with arguments returned
    SC = Message({'201':{HELP:'OK',
                       ARGS:(),
                       OPTARGS:('body',)},
                  '202':{HELP:'Job allocated',
                       ARGS:('machine','id'),
                       OPTARGS:('body',)},
                  '203':{HELP:'OK (blocking)',
                       ARGS:('id',),
                       OPTARGS:('body',)},
                  '401':{HELP:'Invalid command',
                       ARGS:(),
                       OPTARGS:('body',)},
                  '402':{HELP:'No machines match constraints',
                       ARGS:(),
                       OPTARGS:('body',)},
                  '403':{HELP:'All machines in use (would block)',
                       ARGS:(),
                       OPTARGS:('body',)},
                  '404':{HELP:'No such job',
                       ARGS:(),
                       OPTARGS:('body',)},
                  '405':{HELP:'Job not running (is blocked)',
                       ARGS:(),
                       OPTARGS:('body',)},
                  '406':{HELP:'Error in body',
                       ARGS:(),
                       OPTARGS:('body',)},
                  '407':{HELP:'Error in argument',
                       ARGS:(),
                       OPTARGS:('body',)},
                  '408':{HELP:'Permission denied',
                       ARGS:(),
                       OPTARGS:('body',)},
                  '409':{HELP:'Job already running',
                       ARGS:(),
                       OPTARGS:('body',)},
                  '410':{HELP:'Job reconnected',
                       ARGS:(),
                       OPTARGS:('body',)},
                  '411':{HELP:'Object already exists',
                       ARGS:('name',),
                       OPTARGS:('body',)},
                  '412':{HELP:'Job cancelled',
                       ARGS:(),
                       OPTARGS:('body',)}
                  })

    # Commands the server knows about, with required arguments
    CS = Message({'status':
                      {ARGS:('mdl',),
                       OPTARGS:(),
                       HELP:'Show status of cluster machines'},
                  'try':
                      {ARGS:('name', 'type', 'priority', 'mdl'),
                       OPTARGS:('uid', 'gids'),
                       HELP:'Attempt to register a job (non-blocking)'},
                  'acquire':
                      {ARGS:('name', 'type', 'priority', 'mdl'),
                       OPTARGS:('uid', 'gids'),
                       HELP:'Register a job (blocking)'},
                  'release':
                      {ARGS:('id',),
                       OPTARGS:('uid', 'gids'),
                       HELP:'Release a previously registered job'},
                  'jobs':
                      {ARGS:(),
                       OPTARGS:(),
                       HELP:'Display running jobs'},
                  'reconnect':
                      {ARGS:('id',),
                       OPTARGS:('uid', 'gids'),
                       HELP:'Reconnect to a blocked job'},
                  'add':
                      {ARGS:('name', 'domain', 'primarypool', 'pools', 'arch', 'osversion', 'numcpus', 'maxjobs', 'haszfs', 'acl', 'online'),
                       OPTARGS:(),
                       HELP:'Add a machine'},
                  'update':
                      {ARGS:('name',),
                       OPTARGS:('domain', 'primarypool', 'pools', 'arch', 'osversion', 'numcpus', 'maxjobs', 'haszfs', 'acl', 'online'),
                       HELP:'Update properties for a machine'},
                  'delete':
                      {ARGS:('name',),
                       OPTARGS:(),
                       HELP:'Delete a machine'},
                  'add_acl':
                      {ARGS:('name', 'uidlist', 'gidlist', 'sense'),
                       OPTARGS:(),
                       HELP:'Add an ACL'},
                  'update_acl':
                      {ARGS:('name'),
                       OPTARGS:('uidlist', 'gidlist', 'sense'),
                       HELP:'Update an ACL'},
                  'del_acl':
                      {ARGS:('name'),
                       OPTARGS:(),
                       HELP:'Delete an ACL'}
                  })

class QManagerClientConn(QManagerConnection):
    """ Client -> Server connection object """

    # socket
    sock = None
    sockfile = None

    # Where to write intermediate notifications for blocking
    stderr = None

    def connect(self, path = None):
        """ Connect to the server

        Returns:
        socket on success
        None on failure
        """

        if path is None:
            path = "/tmp/.qmgr"

        if os.path.exists(path):
            s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
            try:
                s.connect(path)
                self.sock = s
                self.sockfile = self.sock.makefile()
                return s
            # XXX MCL 20110421 debugging
            except Exception, e:
                if self.stderr:
                    self.stderr.write("QManagerClientConn: exception: " + str( e ) + "\n")
                    self.stderr.flush()
                s.close()
        # XXX MCL 20110421 debugging
        else:
            if self.stderr:
                self.stderr.write("QManagerClientConn: qmanager socket file does not exist!\n")
                self.stderr.flush()

        return None

    def close(self):
        """ Close a connection"""

        self.sock.close()
        self.sockfile = None

    def send(self, cmd, vars):
        if not self.sock:
            self.connect()

        timeout=1
        while True:
            try:
                return self.CS.send(self.sockfile, cmd, vars)
            except:
                if self.stderr:
                    self.stderr.write("Error sending command...\n")
                    self.stderr.flush()
                sleep(timeout)
                if timeout < 64:
                    timeout += 2

    def receive(self):
        if not self.sock:
            self.connect()

        timeout=1
        while True:
            try:
                return self.SC.receive(self.sockfile)
            except:
                if self.stderr:
                    self.stderr.write("Error receiving command...\n")
                    self.stderr.flush()
                sleep(timeout)
                if timeout < 64:
                    timeout += 2

    def __init__(self, stderr = None):
        self.stderr = stderr
        super(QManagerClientConn, self).__init__()

    def process(self, cmd):
        """
        Receive the response from the server via dispatch into handler
        functions

        Returns: return value of rcv_* function call

        """
        (code, args) = self.receive()
        return getattr(self, "rcv_%s" % cmd)(code, args)

    def command(self, cmd, args):
        """ Send command and receive response

        Args:
        cmd = str (command name)
        args = {} (arguments to pass)

        Raises:
        KeyError if cmd is invalid
        (MissingArgumentError, tuple(args)) if required argument(s) missing
        (UnknownArgumentError, tuple(args)) if extra arguments supplied

        Returns:
        (code, vars) on success
        """

        if not self.sock:
            timeout = 1
            while True:
                if self.connect():
                    break
                else:
                    self.stderr.write("Error connecting to qmanager...\n")
                    self.stderr.flush()
                    sleep(timeout)
                    if timeout < 64:
                        timeout += 2

        self.send(cmd, args)
        (code, vars) = self.process(cmd)
        self.close()
        if code[0] != "2":
            error(code)

        return (code, vars)

    def error(self, code):
        raise RequestError, code

    # Default implementations of dispatch functions
    #
    # Returns:
    # (code, vardict) if successful
    #
    # Raises:
    # (RequestError, errorstr) if failed
    # ServerError if unexpected server response

    def rcv_generic_cmd(self, code, args):
        if code[0] == '2':
            return (code, args)
        else:
            self.error(code)

    def rcv_acquire(self, code, args):
        """ Blocking acquire, reporting to self.stderr on block """

        if code[0] == '2':
            if code == '203':
                if self.stderr:
                    self.stderr.write("Blocking with job ID %s" % args['id'])
                    self.stderr.flush()
                # Do another receive to wait for job unblock
                return self.process('acquire')
            else:
                return (code, args)
        else:
            self.error(code)

    def rcv_status(self, code, args):
        return self.rcv_generic_cmd(code, args)

    def rcv_try(self, code, args):
        return self.rcv_generic_cmd(code, args)

    def rcv_release(self, code, args):
        return self.rcv_generic_cmd(code, args)

    def rcv_jobs(self, code, args):
        return self.rcv_generic_cmd(code, args)

    def rcv_reconnect(self, code, args):
        return rcv_acquire(code, args)

    def rcv_add(self, code, args):
        return self.rcv_generic_cmd(code, args)

    def rcv_update(self, code, args):
        return self.rcv_generic_cmd(code, args)

    def rcv_delete(self, code, args):
        return self.rcv_generic_cmd(code, args)

    def rcv_add_acl(self, code, args):
        return self.rcv_generic_cmd(code, args)

    def rcv_update_acl(self, code, args):
        return self.rcv_generic_cmd(code, args)

    def rcv_del_acl(self, code, args):
        return self.rcv_generic_cmd(code, args)
