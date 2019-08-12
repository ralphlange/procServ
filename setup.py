#!/usr/bin/env python

import os
import distutils.command.install_data
from distutils.core import setup
from distutils import log

class custom_install_data(distutils.command.install_data.install_data):
    """need to set the systemd generators to mode 0755"""
    def run(self):
        distutils.command.install_data.install_data.run(self)
        install_cmd = self.get_finalized_command('install')
        dst = getattr(install_cmd, 'install_data')
        for type in ['user', 'system']:
            file = os.path.join(dst, 'lib/systemd/%s-generators/systemd-procserv-generator-%s'%(type,type))
            self.announce('changing mode of %s to 755' % file, level=log.INFO)
            os.chmod(file, 0o755)

setup(
    name='procServUtils',
    description='Support scripts for procServ',
    packages = ['procServUtils'],
    scripts = [
        'manage-procs',
        'procServ-launcher',
    ],
    data_files = [
        ('lib/systemd/system-generators', ['systemd-procserv-generator-system']),
        ('lib/systemd/user-generators', ['systemd-procserv-generator-user']),
    ],
    cmdclass = { 'install_data': custom_install_data },
)
