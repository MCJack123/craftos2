# SPDX-FileCopyrightText: 2019-2024 JackMacWindows
#
# SPDX-License-Identifier: MIT

from apport.hookutils import *
import apport.packaging

def add_info(report, ui):
    if not apport.packaging.is_distro_package(report['Package'].split()[0]):
        report['CrashDB'] = 'craftos_pc'
