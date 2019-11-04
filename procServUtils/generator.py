
import logging
_log = logging.getLogger(__name__)

import sys, os, errno
from .conf import getconf

def which(file):
    for path in os.environ["PATH"].split(os.pathsep):
        if os.path.exists(os.path.join(path, file)):
            return os.path.join(path, file)
    return None

def write_service(F, conf, sect, user=False):
    opts = {
        'name':sect,
        'user':conf.get(sect, 'user'),
        'group':conf.get(sect, 'group'),
        'chdir':conf.get(sect, 'chdir'),
        'userarg':'--user' if user else '--system',
        'launcher':which('procServ-launcher'),
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
"""%opts)

    if conf.has_option(sect, 'env_file'):
        F.write('EnvironmentFile=%s\n'%conf.get(sect, 'env_file'))

    if conf.has_option(sect, 'environment'):
        F.write('Environment=%s\n'%conf.get(sect, 'environment'))

    F.write("""\
ExecStart=%(launcher)s %(userarg)s %(name)s
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
