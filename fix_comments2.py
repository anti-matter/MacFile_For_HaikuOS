#!/usr/bin/env python3
"""Fix // line comments across the afp_createshare and afp_config sources:
1. Remove lines whose comment is blank (a line that is just //).
2. Ensure there is a space after the // comment indicator (//text -> // text),
   without inserting a space inside URL literals such as https://...

Unlike the earlier fix_comments.py, this version removes ALL blank // lines,
including the fence lines that used to bracket block comments."""
import re
import sys

def fix_slash_space(line):
    """Add a space after each // comment indicator that lacks one."""
    had_nl = line.endswith('\n')
    s = line[:-1] if had_nl else line
    out = []
    i = 0
    changed = False
    while True:
        j = s.find('//', i)
        if j == -1:
            out.append(s[i:])
            break
        # Extend to the full run of slashes so /// is left alone.
        k = j
        while k < len(s) and s[k] == '/':
            k += 1
        # A run that is part of a longer slash run, or is a URL (preceded
        # by ':'), is not a comment indicator we should touch.
        if j > 0 and (s[j - 1] == '/' or s[j - 1] == ':'):
            out.append(s[i:k])
            i = k
            continue
        # Comment indicator: ensure it is followed by a space.
        if k < len(s) and s[k] != ' ':
            out.append(s[i:k])
            out.append(' ')
            changed = True
        else:
            out.append(s[i:k])
        i = k
    new = ''.join(out)
    return (new + '\n') if changed and had_nl else (new + '\n' if had_nl else new)

def is_blank_comment(line):
    """True if the line is just // with nothing after it."""
    return bool(re.match(r'^\s*//\s*$', line.rstrip('\n')))

def fix_file(filepath):
    with open(filepath, 'r') as f:
        lines = f.readlines()

    # Pass 1: add the missing space after every // comment indicator.
    for i in range(len(lines)):
        lines[i] = fix_slash_space(lines[i])

    # Pass 2: drop every blank // line (including old block fences).
    result = [line for line in lines if not is_blank_comment(line)]
    removed = len(lines) - len(result)

    with open(filepath, 'w') as f:
        f.writelines(result)

    return removed

if __name__ == '__main__':
    total = 0
    for f in sys.argv[1:]:
        r = fix_file(f)
        print(f'{r} blank // lines removed from {f}')
        total += r
    print(f'Total: {total} blank // lines removed across {len(sys.argv) - 1} files')
