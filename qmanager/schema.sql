BEGIN TRANSACTION;
DELETE FROM sqlite_sequence;
INSERT INTO "sqlite_sequence" VALUES('Jobs',12050);
CREATE TABLE Jobs (
       ID             INTEGER PRIMARY KEY AUTOINCREMENT,
       Name	      TEXT,
       priority	      INTEGER,
       nslots	      INTEGER,
       owner	      INTEGER,
       machines	      TEXT,
       starttime      INTEGER,
       sql	      TEXT  
, running INTEGER);
CREATE TABLE acls (
       Name  	      TEXT PRIMARY KEY,
       uidlist	      TEXT,
       gidlist	      TEXT,
       sense          TEXT
);
INSERT INTO "acls" VALUES('ports-amd64','ports-amd64',NULL, 1);
INSERT INTO "acls" VALUES('ports-i386','ports-i386',NULL, 1);
CREATE TABLE Machines (
       Name  	      TEXT PRIMARY KEY,
       MaxJobs	      INTEGER,
       OSVersion      INTEGER,
       OSVersions     INTEGER,
       Arch	      TEXT,
       Arches	      TEXT,
       Domain	      TEXT,
       PrimaryPool    TEXT,
       Pools	      TEXT,
       NumCPUs        INTEGER,
       HasZFS	      INTEGER,
       ACL            TEXT
);
INSERT INTO "Machines" VALUES('gohan10.freebsd.org',8,800026,800026,'amd64','|amd64|i386|','freebsd.org','package','|package|all|',8,1,'ports-amd64');
INSERT INTO "Machines" VALUES('gohan11.freebsd.org',8,800026,800026,'amd64','|amd64|i386|','freebsd.org','package','|package|all|',8,1,'ports-amd64');
INSERT INTO "Machines" VALUES('gohan12.freebsd.org',8,800026,800026,'amd64','|amd64|i386|','freebsd.org','package','|package|all|',8,1,'ports-amd64');
INSERT INTO "Machines" VALUES('gohan13.freebsd.org',8,800026,800026,'amd64','|amd64|i386|','freebsd.org','package','|package|all|',8,1,'ports-amd64');
INSERT INTO "Machines" VALUES('hammer1.isc.gumbysoft.com',8,800036,800036,'amd64','|amd64|i386|','freebsd.org','package','|package|all|',8,1,'ports-amd64');
INSERT INTO "Machines" VALUES('hammer2.isc.gumbysoft.com',8,800036,800036,'amd64','|amd64|i386|','freebsd.org','package','|package|all|',8,1,'ports-amd64');
INSERT INTO "Machines" VALUES('hammer3.isc.gumbysoft.com',8,800036,800036,'amd64','|amd64|i386|','freebsd.org','package','|package|all|',8,1,'ports-amd64');
INSERT INTO "Machines" VALUES('gohan20.freebsd.org',3,800020,800020,'i386','|i386|','freebsd.org','package','|package|all|',1,1,'ports-i386');
INSERT INTO "Machines" VALUES('gohan21.freebsd.org',3,800020,800020,'i386','|i386|','freebsd.org','package','|package|all|',1,1,'ports-i386');
INSERT INTO "Machines" VALUES('gohan22.freebsd.org',3,800020,800020,'i386','|i386|','freebsd.org','package','|package|all|',1,1,'ports-i386');
INSERT INTO "Machines" VALUES('gohan23.freebsd.org',3,800020,800020,'i386','|i386|','freebsd.org','package','|package|all|',1,1,'ports-i386');
INSERT INTO "Machines" VALUES('gohan24.freebsd.org',3,800020,800020,'i386','|i386|','freebsd.org','package','|package|all|',1,1,'ports-i386');
INSERT INTO "Machines" VALUES('gohan25.freebsd.org',3,800020,800020,'i386','|i386|','freebsd.org','package','|package|all|',1,1,'ports-i386');
INSERT INTO "Machines" VALUES('gohan26.freebsd.org',3,800020,800020,'i386','|i386|','freebsd.org','package','|package|all|',1,1,'ports-i386');
INSERT INTO "Machines" VALUES('gohan27.freebsd.org',3,800020,800020,'i386','|i386|','freebsd.org','package','|package|all|',1,1,'ports-i386');
INSERT INTO "Machines" VALUES('gohan28.freebsd.org',3,800020,800020,'i386','|i386|','freebsd.org','package','|package|all|',1,1,'ports-i386');
INSERT INTO "Machines" VALUES('gohan29.freebsd.org',3,800020,800020,'i386','|i386|','freebsd.org','package','|package|all|',1,1,'ports-i386');
INSERT INTO "Machines" VALUES('gohan30.freebsd.org',3,800020,800020,'i386','|i386|','freebsd.org','package','|package|all|',1,1,'ports-i386');
INSERT INTO "Machines" VALUES('gohan31.freebsd.org',3,800020,800020,'i386','|i386|','freebsd.org','package','|package|all|',1,1,'ports-i386');
INSERT INTO "Machines" VALUES('gohan32.freebsd.org',3,800020,800020,'i386','|i386|','freebsd.org','package','|package|all|',1,1,'ports-i386');
INSERT INTO "Machines" VALUES('gohan33.freebsd.org',3,800020,800020,'i386','|i386|','freebsd.org','package','|package|all|',1,1,'ports-i386');
INSERT INTO "Machines" VALUES('gohan34.freebsd.org',3,800020,800020,'i386','|i386|','freebsd.org','package','|package|all|',1,1,'ports-i386');
INSERT INTO "Machines" VALUES('gohan35.freebsd.org',3,800020,800020,'i386','|i386|','freebsd.org','package','|package|all|',1,1,'ports-i386');
INSERT INTO "Machines" VALUES('gohan36.freebsd.org',3,800020,800020,'i386','|i386|','freebsd.org','package','|package|all|',1,1,'ports-i386');
INSERT INTO "Machines" VALUES('gohan37.freebsd.org',3,800020,800020,'i386','|i386|','freebsd.org','package','|package|all|',1,1,'ports-i386');
INSERT INTO "Machines" VALUES('gohan38.freebsd.org',3,800020,800020,'i386','|i386|','freebsd.org','package','|package|all|',1,1,'ports-i386');
INSERT INTO "Machines" VALUES('gohan39.freebsd.org',3,800020,800020,'i386','|i386|','freebsd.org','package','|package|all|',1,1,'ports-i386');
INSERT INTO "Machines" VALUES('gohan40.freebsd.org',3,800020,800020,'i386','|i386|','freebsd.org','package','|package|all|',1,1,'ports-i386');
INSERT INTO "Machines" VALUES('gohan41.freebsd.org',3,800020,800020,'i386','|i386|','freebsd.org','package','|package|all|',1,1,'ports-i386');
INSERT INTO "Machines" VALUES('gohan42.freebsd.org',3,800020,800020,'i386','|i386|','freebsd.org','package','|package|all|',1,1,'ports-i386');
INSERT INTO "Machines" VALUES('gohan43.freebsd.org',3,800020,800020,'i386','|i386|','freebsd.org','package','|package|all|',1,1,'ports-i386');
INSERT INTO "Machines" VALUES('gohan44.freebsd.org',3,800020,800020,'i386','|i386|','freebsd.org','package','|package|all|',1,1,'ports-i386');
INSERT INTO "Machines" VALUES('gohan45.freebsd.org',3,800020,800020,'i386','|i386|','freebsd.org','package','|package|all|',1,1,'ports-i386');
INSERT INTO "Machines" VALUES('gohan46.freebsd.org',3,800020,800020,'i386','|i386|','freebsd.org','package','|package|all|',1,1,'ports-i386');
INSERT INTO "Machines" VALUES('gohan47.freebsd.org',3,800020,800020,'i386','|i386|','freebsd.org','package','|package|all|',1,1,'ports-i386');
INSERT INTO "Machines" VALUES('gohan48.freebsd.org',3,800020,800020,'i386','|i386|','freebsd.org','package','|package|all|',1,1,'ports-i386');
INSERT INTO "Machines" VALUES('gohan49.freebsd.org',3,800020,800020,'i386','|i386|','freebsd.org','package','|package|all|',1,1,'ports-i386');
INSERT INTO "Machines" VALUES('gohan50.freebsd.org',3,800020,800020,'i386','|i386|','freebsd.org','package','|package|all|',1,1,'ports-i386');
INSERT INTO "Machines" VALUES('gohan51.freebsd.org',3,800020,800020,'i386','|i386|','freebsd.org','package','|package|all|',1,1,'ports-i386');
INSERT INTO "Machines" VALUES('gohan52.freebsd.org',3,800020,800020,'i386','|i386|','freebsd.org','package','|package|all|',1,1,'ports-i386');
INSERT INTO "Machines" VALUES('gohan53.freebsd.org',3,800020,800020,'i386','|i386|','freebsd.org','package','|package|all|',1,1,'ports-i386');
INSERT INTO "Machines" VALUES('gohan54.freebsd.org',3,800020,800020,'i386','|i386|','freebsd.org','package','|package|all|',1,1,'ports-i386');
INSERT INTO "Machines" VALUES('gohan55.freebsd.org',3,800020,800020,'i386','|i386|','freebsd.org','package','|package|all|',1,1,'ports-i386');
INSERT INTO "Machines" VALUES('gohan56.freebsd.org',3,800020,800020,'i386','|i386|','freebsd.org','package','|package|all|',1,1,'ports-i386');
INSERT INTO "Machines" VALUES('gohan57.freebsd.org',3,800020,800020,'i386','|i386|','freebsd.org','package','|package|all|',1,1,'ports-i386');
INSERT INTO "Machines" VALUES('gohan58.freebsd.org',3,800020,800020,'i386','|i386|','freebsd.org','package','|package|all|',1,1,'ports-i386');
INSERT INTO "Machines" VALUES('gohan59.freebsd.org',3,800020,800020,'i386','|i386|','freebsd.org','package','|package|all|',1,1,'ports-i386');
INSERT INTO "Machines" VALUES('ref4.freebsd.org',8,492101,492101,'i386','|i386|','freebsd.org','ref','|ref|',8,1,NULL);
INSERT INTO "Machines" VALUES('ref6-i386.freebsd.org',8,603100,603100,'i386','|amd64|i386|','freebsd.org','ref','|ref|',8,1,NULL);
INSERT INTO "Machines" VALUES('ref6-amd64.freebsd.org',8,603100,603100,'amd64','|amd64|i386|','freebsd.org','ref','|ref|',8,1,NULL);
INSERT INTO "Machines" VALUES('ref7-i386.freebsd.org',8,700102,700102,'i386','|amd64|i386|','freebsd.org','ref','|ref|',8,1,NULL);
INSERT INTO "Machines" VALUES('ref7-amd64.freebsd.org',8,700102,700102,'amd64','|amd64|i386|','freebsd.org','ref','|ref|',8,1,NULL);
INSERT INTO "Machines" VALUES('ref8-i386.freebsd.org',8,800030,800030,'i386','|amd64|i386|','freebsd.org','ref','|ref|',8,1,NULL);
INSERT INTO "Machines" VALUES('ref8-amd64.freebsd.org',8,800030,800030,'amd64','|amd64|i386|','freebsd.org','ref','|ref|',8,1,NULL);
COMMIT;
