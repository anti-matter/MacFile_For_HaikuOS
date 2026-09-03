#!/usr/bin/env python3
"""Fix comment formatting across afp_sources:
1. Add space after // where missing (//text → // text)
2. Remove orphaned blank // lines while preserving block comment structure."""
import re, sys, glob

def fix_file(filepath):
    with open(filepath, 'r') as f:
        lines = f.readlines()

    def is_blank_comment(line):
        """Check if line is just // with optional whitespace."""
        return bool(re.match(r'^\s*//\s*$', line.rstrip('\n')))

    def has_comment_text(line):
        """Check if line has text after // (not just // alone)."""
        s = line.strip()
        return bool(re.match(r'.*//\s*\S', s))

    def needs_space_after_slash(line):
        """Check if a line has // not followed by space or another /."""
        # Skip URL-like patterns
        if re.search(r'://', line):
            return False
        # Match // not followed by space or /
        for m in re.finditer(r'//', line):
            pos = m.start()
            after = pos + 2
            if after >= len(line) or (line[after] != ' ' and line[after] != '/'):
                return True
        return False

    def fix_slash_space(line):
        """Add space after // where missing."""
        if not needs_space_after_slash(line):
            return line
        # Replace the first // that lacks a space
        result = []
        i = 0
        found = False
        while i < len(line):
            if not found and i + 1 < len(line) and line[i] == '/' and line[i+1] == '/':
                after = i + 2
                if after >= len(line) or (line[after] != ' ' and line[after] != '/'):
                    result.append('// ')
                    i += 2
                    found = True
                    continue
            result.append(line[i])
            i += 1
        return ''.join(result)

    # Pass 1: Add spaces after //
    for i in range(len(lines)):
        lines[i] = fix_slash_space(lines[i])

    # Pass 2: Remove orphaned blank // lines
    result = []
    removed = 0
    i = 0
    while i < len(lines):
        line = lines[i]

        if is_blank_comment(line):
            # Find previous non-blank line
            prev_idx = i - 1
            while prev_idx >= 0 and not lines[prev_idx].strip():
                prev_idx -= 1
            prev_line = lines[prev_idx].strip() if prev_idx >= 0 else ''

            # Find next non-blank line
            next_idx = i + 1
            while next_idx < len(lines) and not lines[next_idx].strip():
                next_idx += 1
            next_line = lines[next_idx].strip() if next_idx < len(lines) else ''

            prev_has_text = bool(re.match(r'.*//\s*\S', prev_line)) if prev_line else False
            next_has_text = bool(re.match(r'.*//\s*\S', next_line)) if next_line else False

            # If prev has '//text' AND next has '//text', this is block comment closing - KEEP
            if prev_has_text and next_has_text:
                result.append(line)
                i += 1
                continue

            # Check if prev line is also a blank // (part of a run)
            prev_is_blank = bool(re.match(r'^\s*//\s*$', prev_line)) if prev_line else False

            # If part of a run of // lines, it's block comment structure - KEEP
            if prev_is_blank:
                result.append(line)
                i += 1
                continue

            # Otherwise remove this orphaned blank comment
            removed += 1
            i += 1
        else:
            result.append(line)
            i += 1

    with open(filepath, 'w') as f:
        f.writelines(result)

    return removed

if __name__ == '__main__':
    files = sys.argv[1:] if len(sys.argv) > 1 else glob.glob('afpserver/afp_sources/*.cpp') + glob.glob('afpserver/afp_sources/*.h')
    total = 0
    for f in sorted(files):
        r = fix_file(f)
        if r > 0:
            print(f'{r} blank // lines removed from {f}')
            total += r
    print(f'Total: {total} blank // lines removed across {len(files)} files')
