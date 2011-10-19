--echo #
--echo # Bug#36981 - "innodb crash when selecting for update"
--echo #

#
# Test 1: Test based on the reproduction test case for this bug.
#         This query resulted in a crash in InnoDB due to
#         InnoDB changing from using the index which the push condition
#         where for to use the clustered index due to "SELECT ... FOR UPDATE".
#

CREATE TABLE t1 (
  c1 CHAR(1),
  c2 CHAR(10),
  KEY (c1)
);

INSERT INTO t1 VALUES ('3', null);

SELECT * FROM t1 WHERE c1='3' FOR UPDATE;

DROP TABLE t1;

#
# Test 2: Extended test case to test that the correct rows are returned.
#         This test is for ensuring that if InnoDB refuses to accept
#         the pushed index condition it is still evaluated.
#         

CREATE TABLE t1 (a INT);
INSERT INTO t1 VALUES (0),(1),(2),(3),(4),(5),(6),(7),(8),(9);

CREATE TABLE t2 (a INT);
INSERT INTO t2 SELECT A.a + 10*(B.a + 10*C.a) FROM t1 A, t1 B, t1 C;

CREATE TABLE t3 (
  c1 CHAR(10) NOT NULL,
  c2 CHAR(10) NOT NULL,
  c3 CHAR(200) NOT NULL,
  KEY (c1)
);

INSERT INTO t3 
  SELECT CONCAT('c-',1000+t2.a,'=w'), CONCAT('c-',1000+ t2.a,'=w'), 'filler'
  FROM t2;

INSERT INTO t3 
  SELECT CONCAT('c-',1000+t2.a,'=w'), CONCAT('c-',2000+t2.a,'=w'), 'filler-1'
  FROM t2;
 
INSERT INTO t3
  SELECT CONCAT('c-',1000+t2.a,'=w'), CONCAT('c-',3000+t2.a,'=w'), 'filler-2'
  FROM t2;

--sorted_result
SELECT c1,c3 FROM t3 WHERE c1 >= 'c-1994=w' and c1 != 'c-1996=w' FOR UPDATE;

DROP TABLE t1,t2,t3;

--echo #
--echo # Bug#42580 - Innodb's ORDER BY ..LIMIT returns no rows for
--echo #             null-safe operator <=> NULL
--echo #

CREATE TABLE t1(
  c1 DATE NOT NULL,
  c2 DATE NULL,
  c3 DATETIME,
  c4 TIMESTAMP,
  PRIMARY KEY(c1),
  UNIQUE(c2)
);

--echo
INSERT INTO t1 VALUES('0000-00-00', '0000-00-00', '2008-01-04', '2008-01-05');
INSERT INTO t1 VALUES('2007-05-25', '2007-05-25', '2007-05-26', '2007-05-26');
INSERT INTO t1 VALUES('2008-01-01', NULL        , '2008-01-02', '2008-01-03');
INSERT INTO t1 VALUES('2008-01-17', NULL        , NULL        , '2009-01-29');
INSERT INTO t1 VALUES('2009-01-29', '2009-01-29', '2009-01-29', '2009-01-29');

--echo
SELECT * FROM t1 WHERE c2 <=> NULL ORDER BY c1,c2;
--echo
SELECT * FROM t1 WHERE c2 <=> NULL ORDER BY c1,c2 LIMIT 2;

--echo
DROP TABLE t1;

--echo #
--echo # Bug#40992 - InnoDB: Crash when engine_condition_pushdown is on
--echo #

CREATE TABLE t (
  dummy INT PRIMARY KEY, 
  a INT UNIQUE, 
  b INT
);

INSERT INTO t VALUES (1,1,1),(3,3,3),(5,5,5);

SELECT * FROM t WHERE a > 2 FOR UPDATE;

DROP TABLE t;

--echo #
--echo # Bug#35080 - Innodb crash at mem_block_get_len line 72
--echo #

CREATE TABLE t1 (
  t1_autoinc INT(11) NOT NULL AUTO_INCREMENT,
  uuid VARCHAR(36) DEFAULT NULL,
  PRIMARY KEY (t1_autoinc),
  KEY k (uuid)
);

CREATE TABLE t2 (
  t2_autoinc INT(11) NOT NULL AUTO_INCREMENT,
  uuid VARCHAR(36) DEFAULT NULL,
  date DATETIME DEFAULT NULL,
  PRIMARY KEY (t2_autoinc),
  KEY k (uuid)
);

CREATE VIEW v1 AS 
  SELECT t1_autoinc, uuid
  FROM t1
  WHERE (ISNULL(uuid) OR (uuid like '%-%'));

CREATE VIEW v2 AS 
  SELECT t2_autoinc, uuid, date 
  FROM t2
  WHERE (ISNULL(uuid) OR (LENGTH(uuid) = 36));

CREATE PROCEDURE delete_multi (IN uuid CHAR(36))
  DELETE v1, v2 FROM v1 INNER JOIN v2
  ON v1.uuid = v2.uuid
  WHERE v1.uuid = @uuid;

SET @uuid = UUID();

INSERT INTO v1 (uuid) VALUES (@uuid);
INSERT INTO v2 (uuid, date) VALUES (@uuid, '2009-09-09');

CALL delete_multi(@uuid);

DROP procedure delete_multi;
DROP table t1,t2;
DROP view v1,v2;

--echo #
--echo # Bug#41996 - multi-table delete crashes server (InnoDB table)
--echo #

CREATE TABLE t1 (
  b BIGINT,
  i INT, 
  KEY (b)
);

INSERT INTO t1 VALUES (2, 2);

DELETE t1 FROM t1 a, t1 WHERE a.i=t1.b;

DROP TABLE t1;

--echo #
--echo # Bug#43448 - Server crashes on multi table delete with Innodb
--echo #

CREATE TABLE t1 (
  id1 INT NOT NULL AUTO_INCREMENT PRIMARY KEY, 
  t CHAR(12)
);

CREATE TABLE t2 (
  id2 INT NOT NULL, 
  t CHAR(12)
);

CREATE TABLE t3(
  id3 INT NOT NULL, 
  t CHAR(12), 
  INDEX(id3)
);

delimiter |;

CREATE PROCEDURE insert_data ()
BEGIN
  DECLARE i1 INT DEFAULT 20;
  DECLARE i2 INT;
  DECLARE i3 INT;

  WHILE (i1 > 0) DO
    INSERT INTO t1(t) VALUES (i1);
    SET i2 = 2;
    WHILE (i2 > 0) DO
      INSERT INTO t2(id2, t) VALUES (i1, i2);
      SET i3 = 2;
      WHILE (i3 > 0) DO
        INSERT INTO t3(id3, t) VALUES (i1, i2);
        SET i3 = i3 -1;
      END WHILE;
      SET i2 = i2 -1;
    END WHILE;
    SET i1 = i1 - 1;
  END WHILE;
END |

delimiter ;|

CALL insert_data();

SELECT COUNT(*) FROM t1 WHERE id1 > 10;
SELECT COUNT(*) FROM t2 WHERE id2 > 10;
SELECT COUNT(*) FROM t3 WHERE id3 > 10;

DELETE t1, t2, t3 
FROM t1, t2, t3 
WHERE t1.id1 = t2.id2 AND t2.id2 = t3.id3 AND t1.id1 > 3;

SELECT COUNT(*) FROM t1;
SELECT COUNT(*) FROM t2;
SELECT COUNT(*) FROM t3;

DROP PROCEDURE insert_data;
DROP TABLE t1, t2, t3;

--echo #
--echo # BUG#778434 Wrong result with in_to_exists=on in maria-5.3-mwl89
--echo #
CREATE TABLE t1 ( f11 int) ;
INSERT IGNORE INTO t1 VALUES (0);

CREATE TABLE t2 ( f10 int) ;
INSERT IGNORE INTO t2 VALUES (0);

CREATE TABLE t3 ( f1 int NOT NULL , f10 int, PRIMARY KEY (f1)) ;
INSERT IGNORE INTO t3 VALUES (6,0),(10,0);

CREATE TABLE t4 ( f11 int) ;
INSERT IGNORE INTO t4 VALUES
(0),(0),(0),(0),(0),(0),(0),(0),(0),(0),(NULL),
(0),(0),(0),(0),(0),(0),(0),(0),(0),(0);

set @tmp_778434=@@optimizer_switch;
SET optimizer_switch='materialization=off,in_to_exists=on,subquery_cache=off,semijoin=off';

SELECT * FROM t1 INNER JOIN t2 ON t2.f10 = t1.f11
WHERE (6, 234) IN (
        SELECT t3.f1, t3.f1
        FROM t3 JOIN t4 ON t4.f11 = t3.f10
);

DROP TABLE t1,t2,t3,t4;
set optimizer_switch= @tmp_778434;