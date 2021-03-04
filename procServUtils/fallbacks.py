"""
Fallback functions when termcolor or tabulate packes are not available
"""


def colored(s, *args, **kwargs):
    return s


def tabulate(table, headers, **kwargs):
    s = ""
    for header in headers:
        s += "%s  " % header.strip()
    s += "\n"
    for line in table:
        for elem in line:
            s += "%s  " % elem.strip()
        s += "\n"
    return s[:-1]