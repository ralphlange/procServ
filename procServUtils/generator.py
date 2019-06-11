
import logging
_log = logging.getLogger(__name__)

import sys, os, errno
from .conf import getconf

def write_service(F, conf, sect, user=False):
    opts = {
        'name':sect,
        'user':conf.get(sect, 'user'),
        'group':conf.get(sect, 'group'),
        'chdir':conf.get(sect, 'chdir'),
        'userarg':'--user' if user else '--system',
    }

    F.write("""
[Unit]
Description=procServ for %(name)s
After=network.target remote-fs.target
ConditionPathIsDirectory=%(chdir)s
"""%opts)

    if conf.has_option(sect, 'host'):
        F.write('ConditionHost=%s\n'%conf.get(sect, 'host'))

    F.write("""
[Service]
Type=simple
ExecStart=/usr/local/bin/procServ-launcher %(userarg)s %(name)s
RuntimeDirectory=procserv-%(name)s
StandardOutput=syslog
StandardError=inherit
SyslogIdentifier=procserv-%(name)s
"""%opts)

    if not user:
        F.write("""
User=%(user)s
Group=%(group)s
"""%opts)

    F.write("""
[Install]
WantedBy=multi-user.target
"""%opts)

def run(outdir, user=False):
    conf = getconf(user=user)

    for sect in conf.sections():
        _log.debug('Consider %s', sect)
        if not conf.getboolean(sect, 'instance'):
            continue
        service = 'procserv-%s.service'%sect
        ofile = os.path.join(outdir, service)
        _log.debug('Write %s', service)
        with open(ofile+'.tmp', 'w') as F:
            write_service(F, conf, sect, user=user)

        os.rename(ofile+'.tmp', ofile)
