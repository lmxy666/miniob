"""Regression for mixed string/numeric JOIN predicates; uses an isolated database."""
import operator
import socket
import subprocess
import sys
import tempfile
import time
from collections import Counter
from pathlib import Path

root = Path(sys.argv[1]).resolve() if len(sys.argv) > 1 else Path(__file__).resolve().parents[2]
with tempfile.TemporaryDirectory(prefix='miniob-join-coercion-') as directory:
    work = Path(directory)
    address = str(work / 'observer.sock')
    log = (work / 'console.log').open('w+')
    process = subprocess.Popen(
        [str(root / 'build/bin/observer'), '-f', str(root / 'etc/observer.ini'),
         '-s', address, '-d'], cwd=work, stdout=log, stderr=log)
    sock = socket.socket(socket.AF_UNIX)
    sock.settimeout(20)
    count = 0
    try:
        for attempt in range(150):
            try:
                sock.connect(address)
                break
            except OSError:
                if process.poll() is not None:
                    raise RuntimeError('observer exited')
                time.sleep(0.1)
        else:
            raise RuntimeError('observer did not start')

        def query(sql):
            sock.sendall(sql.encode() + b'\0')
            data = bytearray()
            while b'\0' not in data:
                chunk = sock.recv(4096)
                if not chunk:
                    raise RuntimeError('observer disconnected: ' + sql)
                data.extend(chunk)
            return bytes(data).split(b'\0')[0].decode().strip()

        def check(sql, expected):
            global count
            actual = query(sql)
            if isinstance(expected, list):
                lines = actual.splitlines()
                assert lines and lines[0] not in ('FAILURE', 'SUCCESS'), (sql, actual)
                assert Counter(lines[1:]) == Counter(expected), (sql, actual, expected)
            else:
                assert actual == expected, (sql, actual, expected)
            count += 1

        check('CREATE TABLE join_table_1 (id INT, name CHAR(32));', 'SUCCESS')
        check('CREATE TABLE join_table_2 (id INT, age INT);', 'SUCCESS')
        check("INSERT INTO join_table_1 VALUES (4, '16a');", 'SUCCESS')
        check('INSERT INTO join_table_2 VALUES (4, 17);', 'SUCCESS')
        sql = ('SELECT * FROM join_table_1 INNER JOIN join_table_2 '
               'ON join_table_1.name<join_table_2.age AND join_table_1.id=join_table_2.id;')
        check(sql, ['4 | 16a | 4 | 17'])
        print('PASS reported query: 4 | 16a | 4 | 17', flush=True)

        samples = [(4, '16a', 16.0, 17), (5, '16.5a', 16.5, 16),
                   (6, 'abc', 0.0, 1), (7, '-2.5x', -2.5, 2),
                   (8, '  +2e1tail', 20.0, 20), (9, '', 0.0, 0)]
        for key, text, number, age in samples[1:]:
            check(f"INSERT INTO join_table_1 VALUES ({key},'{text}');", 'SUCCESS')
            check(f'INSERT INTO join_table_2 VALUES ({key},{age});', 'SUCCESS')
        check('INSERT INTO join_table_2 VALUES (4,17);', 'SUCCESS')
        pairs = [(a[0], a[2], b[3]) for a in samples for b in samples if a[0] == b[0]]
        pairs.append((4, 16.0, 17))
        comparisons = {'=': operator.eq, '<>': operator.ne, '<': operator.lt,
                       '>': operator.gt, '<=': operator.le, '>=': operator.ge}
        for symbol, compare in comparisons.items():
            expected = [str(key) for key, number, age in pairs if compare(number, age)]
            check(f'SELECT join_table_1.id FROM join_table_1 INNER JOIN join_table_2 '
                  f'ON join_table_1.name {symbol} join_table_2.age '
                  'AND join_table_1.id=join_table_2.id;', expected)
            check(f'SELECT join_table_1.id FROM join_table_1,join_table_2 '
                  f'WHERE join_table_1.name {symbol} join_table_2.age '
                  'AND join_table_1.id=join_table_2.id;', expected)
            expected = [str(key) for key, number, age in pairs if compare(age, number)]
            check(f'SELECT join_table_1.id FROM join_table_2 INNER JOIN join_table_1 '
                  f'ON join_table_2.age {symbol} join_table_1.name '
                  'AND join_table_1.id=join_table_2.id;', expected)

        check('CREATE TABLE float_side (age FLOAT);', 'SUCCESS')
        check('INSERT INTO float_side VALUES (16.25);', 'SUCCESS')
        check('SELECT join_table_1.id FROM join_table_1 JOIN float_side '
              'ON join_table_1.name>float_side.age;', ['5', '8'])
        check('SELECT join_table_1.id FROM float_side JOIN join_table_1 '
              'ON float_side.age<join_table_1.name;', ['5', '8'])
        check("SELECT id FROM join_table_1 WHERE name<'2';", ['4', '5', '7', '8', '9'])
        for indexed in (False, True):
            if indexed:
                check('CREATE INDEX i_name ON join_table_1(name);', 'SUCCESS')
                check('CREATE INDEX i_age ON join_table_2(age);', 'SUCCESS')
            check("SELECT id FROM join_table_2 WHERE age<'16.5a';", ['5', '6', '7', '9'])
            check("SELECT id FROM join_table_2 WHERE '16.5a'>age;", ['5', '6', '7', '9'])
            check('SELECT id FROM join_table_1 WHERE name>16;', ['5', '8'])
            check('SELECT id FROM join_table_1 WHERE 16<name;', ['5', '8'])
        print(f'PASS {count} checks: mixed predicates, all operators, reversed operands, '
              'decimal/prefix/zero coercion, duplicates, floats, lexical strings, indexes', flush=True)
    finally:
        sock.close()
        process.terminate()
        process.wait(timeout=15)
        log.close()
