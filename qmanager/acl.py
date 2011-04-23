# Validate a (uid, (gids)) tuple against an ACL.

import pwd, grp

def getuidbyname(username):
    if str(username).isdigit():
        return int(username)
    return pwd.getpwnam(username)[2]

def getgidbyname(grname):
    if str(grname).isdigit():
        return int(grname)
    return grp.getgrnam(grname)[2]

class ACLElement(object):
    """ Component of an ACL. """

    def __init__(self, name, uidlist, gidlist, sense):
        self.name = name
        self.uidlist = [getuidbyname(uid) for uid in uidlist]
        self.gidlist = [getgidbyname(gid) for gid in gidlist]
        self.sense = bool(sense)

    def validate(self, uid, gids):
        """ Validate an ACL Element.  In order to match, the following must
        hold:

        * uid is a subset of self.uidlist, or self.uidlist is empty
        * one of the gids must be present in self.gidlist, or
          self.gidlist is empty

        If both conditions hold, then the validation returns self.sense

        Returns: True/False if Element matches
        None if Element fails to match
        """

        if (len(self.uidlist) == 0 or uid in self.uidlist) and \
                (len(self.gidlist) == 0 or set(gids).intersection(self.gidlist)):
            return self.sense
        return None

class ACL(object):
    """ List of ACLElements that form an ACL """

    def __init__(self, acllist):
        self.acls = acllist

    def validate(self, uid, gids):
        uid=getuidbyname(uid)
        gids=set(getgidbyname(gid) for gid in gids)

        for acl in self.acls:
            res=acl.validate(uid, gids)
            if res is not None:
                return res
        return False

if __name__ == "__main__":

    from sys import exit

    assert getuidbyname(123) == 123
    assert getuidbyname('123') == 123

    try:
        ACLElement("test", ["foobar"], [""], True)
    except KeyError:
        pass

    try:
        ACLElement("test", [123, "foobar"], [""], True)
    except KeyError:
        pass

    assert ACLElement("test", [123], [], True) != None
    assert ACLElement("test", ["123"], [], True) != None

    acl = ACL([ACLElement("el 1", ["kris"], [], True),
               ACLElement("el 2", [], ["wheel"], True),
               ACLElement("el 3", [], [], False)])

    assert acl.validate(getuidbyname('kris'), []) == True
    assert acl.validate(getuidbyname('simon'), []) == False
    assert acl.validate(getuidbyname('simon'), [getgidbyname('devel'), getgidbyname('wheel')]) == True
    assert acl.validate(getuidbyname('root'), [pwd.getpwnam('root')[3]]) == True

    acl = ACL([ACLElement("el 1", ["kris"], ["distcc"], True),
               ACLElement("el 2", [], ["wheel"], True),
               ACLElement("el 3", [], [], False)])
    assert acl.validate("kris", ["wheel"]) == True
    assert acl.validate("kris", ["staff"]) == False

    acl = ACL([ACLElement("", ('kris',), (), True),
               ACLElement("", (), ('wheel', 'devel'), True),
               ACLElement("", (), (), False)])

    assert acl.validate(getuidbyname('simon'), [getgidbyname('devel'), getgidbyname('wheel')]) == True
    assert acl.validate(getuidbyname('simon'), [getgidbyname('staff'), getgidbyname('wheel')]) == True
    assert acl.validate(getuidbyname('simon'), [getgidbyname('staff')]) == False

    acl = ACL([ACLElement("", ('kris',), (), True),
               ACLElement("", (), ('devel',), True),
               ACLElement("", (), ('wheel',), True),
               ACLElement("", (), (), False)])

    assert acl.validate(getuidbyname('simon'), [getgidbyname('devel'), getgidbyname('wheel')]) == True
    assert acl.validate(getuidbyname('simon'), [getgidbyname('staff'), getgidbyname('wheel')]) == True
    assert acl.validate(getuidbyname('simon'), [getgidbyname('staff')]) == False

    acl = ACL([ACLElement("", ('kris',), (), True),
               ACLElement("", (), ('devel',), False),
               ACLElement("", (), ('wheel',), True),
               ACLElement("", (), (), False)])

    assert acl.validate(getuidbyname('simon'), [getgidbyname('devel'), getgidbyname('wheel')]) == False
    assert acl.validate(getuidbyname('simon'), [getgidbyname('staff'), getgidbyname('wheel')]) == True
    assert acl.validate(getuidbyname('simon'), [getgidbyname('staff')]) == False


    acl = ACL([ACLElement("", ('kris',), (), True),
               ACLElement("", (), ('devel',), True),
               ACLElement("", (), ('wheel',), False),
               ACLElement("", (), (), False)])

    assert acl.validate(getuidbyname('simon'), [getgidbyname('devel'), getgidbyname('wheel')]) == True
    assert acl.validate(getuidbyname('simon'), [getgidbyname('staff'), getgidbyname('wheel')]) == False
    assert acl.validate(getuidbyname('simon'), [getgidbyname('staff')]) == False


    acl = ACL([ACLElement("", ('kris',), (), True),
               ACLElement("", (), ('devel',), True),
               ACLElement("", (), ('wheel',), False),
               ACLElement("", (), (), True)])

    assert acl.validate(getuidbyname('simon'), []) == True
    assert acl.validate(getuidbyname('simon'), [getgidbyname('devel'), getgidbyname('wheel')]) == True
    assert acl.validate(getuidbyname('simon'), [getgidbyname('staff'), getgidbyname('wheel')]) == False
    assert acl.validate(getuidbyname('simon'), [getgidbyname('staff')]) == True

    acl = ACL([ACLElement("", ('kris',), (), False),
               ACLElement("", (), ('devel',), True),
               ACLElement("", (), ('wheel',), False),
               ACLElement("", (), (), True)])

    assert acl.validate(getuidbyname('simon'), []) == True
    assert acl.validate(getuidbyname('kris'), []) == False
    assert acl.validate(getuidbyname('simon'), [getgidbyname('devel'), getgidbyname('wheel')]) == True
    assert acl.validate(getuidbyname('simon'), [getgidbyname('staff'), getgidbyname('wheel')]) == False
    assert acl.validate(getuidbyname('simon'), [getgidbyname('staff')]) == True

    acl = ACL([ACLElement("", (4206,), set([]), True),
               ACLElement("", (), set([]), False)])

    assert acl.validate(4206, (4206, 31337)) == True
    assert acl.validate(4201, (4201, 31337)) == False
