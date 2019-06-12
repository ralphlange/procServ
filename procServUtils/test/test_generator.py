
import os
import unittest

from .. import generator
from . import TestDir

class TestGen(unittest.TestCase):
    def test_system(self):
        with TestDir() as t:
            confname = t.dir+'/procServ.d/blah.conf'
            with open(confname, 'w') as F:
                F.write("""
[blah]
command = /bin/sh -c blah
chdir = /somedir
user = someone
group = controls
""")

            generator.run(t.dir+'/run')

            service = t.dir+'/run/procserv-blah.service'

            self.assertTrue(os.path.isfile(service))
            with open(service, 'r') as F:
                content = F.read()

            self.assertEqual(content, """
[Unit]
Description=procServ for blah
After=network.target remote-fs.target
ConditionPathIsDirectory=/somedir

[Service]
Type=simple
ExecStart=%s --system blah
RuntimeDirectory=procserv-blah
StandardOutput=syslog
StandardError=inherit
SyslogIdentifier=procserv-blah

User=someone
Group=controls

[Install]
WantedBy=multi-user.target
""" % generator.which('procServ-launcher'))
