SELECT ino FROM memories WHERE kind = 'note' ORDER BY ino;
SELECT ino FROM memories WHERE kind = 'decision' AND score > 500 ORDER BY ino;
SELECT ino FROM memories WHERE namespace = 'ns3' AND agent = 'a07' ORDER BY ino;
SELECT ino FROM memories WHERE namespace = 'ns1' AND kind = 'fact' AND created >= 1760000300000000000 ORDER BY ino;
SELECT ino FROM memories WHERE pinned = 1 AND score < 500 ORDER BY ino;
SELECT ino FROM memories WHERE kind >= 'de' AND kind < 'df' ORDER BY ino;
SELECT ino FROM memories WHERE score >= 250 AND score <= 750 ORDER BY ino;
SELECT ino FROM memories WHERE weight IS NOT NULL ORDER BY ino;
