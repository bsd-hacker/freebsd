import commands, os

class NoSuchFS(Exception):
    pass

class NoSuchSnap(Exception):
    pass

def getallfs():
    """ Get list of all filesystems """

    (err, out) = commands.getstatusoutput("zfs list -Ht filesystem")
    if err:
        raise (OSError, err)

    res=[]
    for i in out.split('\n'):
        (fs, used, avail, refer, mountpt) = i.split()
        res.append((fs, used, avail, refer, mountpt))

    return tuple(res)

def getfs(fs):
    """ Return status of a specific filesystem """

    (err, out) = commands.getstatusoutput("zfs list -Ht filesystem '%s'" % fs)
    if err:
        if "dataset does not exist" in out:
            raise NoSuchFS
        print "err = %s, out = %s" % (err, out)
        raise (OSError, err)

    (fs, used, avail, refer, mountpt) = out.split()
    return (fs, used, avail, refer, mountpt)

def getallsnaps(fs):

    """
    Return list of all snapshots for a specific filesystem

    Entries are: (snap, used, avail, refer, mountpoint)
    where snap is the snapshot name (i.e. after "@")
    """

    if len(getfs(fs)) == 0:
        raise NoSuchFS

    (err, out) = commands.getstatusoutput("zfs list -Ht snapshot")
    if err:
        print "err = %s, out = %s" % (err, out)
        raise (OSError, err)

    res = []
    for i in out.split('\n'):
        (name, used, avail, refer, mountpoint) = i.split()
        if name.startswith("%s@" % fs):
            snap=name.partition("@")[2]
            res.append((snap, used, avail, refer, mountpoint))

    return tuple(res)

def getsnap(fs, snap):
    """ Return a specific snapshot on a specific filesystem """

    snaps = getallsnaps(fs)

    for i in snaps:
        if i[0] == snap:
            return i

    raise NoSuchSnap

def createsnap(fs, name):
    (err, out) = commands.getstatusoutput("zfs snapshot %s@%s" % (fs, name))
    if err:
        print "err = %s, out = %s" % (err, out)
        raise (OSError, err)

def send(fs, old, new = None):

#    if new:
#        return os.popen("echo %s@%s %s@%s" % (fs, old, fs, new))
#    else:
#        return os.popen("echo %s@%s" % (fs, old))

    if new:
        return os.popen("zfs send -i %s %s@%s" % (old, fs, new))
    else:
        return os.popen("zfs send %s@%s" % (fs, old))

if __name__ == "__main__":
    print getallfs()
    
    print getfs("a")


    print getallsnaps("a")
    
    print getsnap("a", "20080531")

    try:
        getsnap("b", "20080531")
    except NoSuchFS:
        pass

    try:
        getfs("b")
    except NoSuchFS:
        pass

    try:
        getallsnaps("b")
    except NoSuchFS:
        pass

    try:
        getsnap("a", "foo")
    except NoSuchSnap:
        pass

