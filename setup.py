#!/usr/bin/env python

from distutils.core import setup

setup(
    name='procServUtils',
    description='Support scripts for procServ',
    packages = ['procServUtils'],
    scripts = [
        'manage-procs',
        'procServ-launcher',
        'prtelnet',
    ],
)
