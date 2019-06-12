import os
import unittest

from .. import manage
from ..manage import getargs, main
from . import TestDir

manage.systemctl = '/bin/true'

class TestGen(unittest.TestCase):
    def test_add(self):
        with TestDir() as t:
            main(getargs(['add', '-C', '/somedir', 'instname', '--', '/bin/sh', '-c', 'blah']), test=True)
            #os.system('find '+t.dir)

            confname = t.dir+'/procServ.d/instname.conf'

            self.assertTrue(os.path.isfile(confname))
            with open(confname, 'r') as F:
                content = F.read()

            self.assertEqual(content, """
[instname]
command = /bin/sh -c blah
chdir = /somedir
""")

            main(getargs(['add',
                          '-C', '/somedir',
                          '-U', 'someone',
                          '-G', 'controls',
                          'other', '--', '/bin/sh', '-c', 'blah']), test=True)

            confname = t.dir+'/procServ.d/other.conf'

            self.assertTrue(os.path.isfile(confname))
            with open(confname, 'r') as F:
                content = F.read()

            self.assertEqual(content, """
[other]
command = /bin/sh -c blah
chdir = /somedir
user = someone
group = controls
""")

    def test_remove(self):
        with TestDir() as t:
            # we won't remove this config, so it should not be touched
            with open(t.dir+'/procServ.d/other.conf', 'w') as F:
                F.write("""
[other]
command = /bin/sh -c blah
chdir = /somedir
user = someone
group = controls
""")

            confname = t.dir+'/procServ.d/blah.conf'
            with open(confname, 'w') as F:
                F.write("""
[blah]
command = /bin/sh -c blah
chdir = /somedir
user = someone
group = controls
""")

            main(getargs(['remove', '-f', 'blah']), test=True)

            self.assertFalse(os.path.isfile(confname))
            self.assertTrue(os.path.isfile(t.dir+'/procServ.d/other.conf'))

            confname = t.dir+'/procServ.d/blah.conf'
            with open(confname, 'w') as F:
                F.write("""
[blah]
command = /bin/sh -c blah
chdir = /somedir
user = someone
group = controls

[more]
# not normal, but we shouldn't nuke this file if it contains other instances
""")

            main(getargs(['remove', '-f', 'blah']), test=True)

            self.assertTrue(os.path.isfile(confname))
            self.assertTrue(os.path.isfile(t.dir+'/procServ.d/other.conf'))
