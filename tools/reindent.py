"""reindent.py [--check] [FILE...] -- rewrite leading indentation by brace structure.

With no files, every .cpp/.h/.inc/.c under forsaken_core, forsaken_3ds,
forsaken_pc and tests (git ls-files) is processed. --check rewrites nothing
and exits 1 listing the files that would change.

House style: tabs (width 4); `case`/`default` labels at the switch's level,
statements one tab in; an unbraced if/else/for/while body one tab in.
Namespace and extern "C" bodies are not indented. Continuation lines keep
their offset from the statement's first line (rendered as tabs then at most
three spaces). Left as written: preprocessor lines and their backslash
continuations (braces in them are not counted, and each #else/#elif branch
starts from the depth at its #if); the lines of a multi-line initialiser,
which keep their hand-laid offsets and move only with the line that opens
it; the interior of a block comment, which moves with its first line. Only
leading whitespace changes.
"""
import re, sys

def vcol(ws):
    c = 0
    for ch in ws:
        c = (c // 4 + 1) * 4 if ch == '\t' else c + 1
    return c

def render(col):
    return '\t' * (col // 4) + ' ' * (col % 4)

CTRL = re.compile(r'^(if|else\s+if|for|while|switch)\b|^else\b|^do\b')

def code_of(line, state):
    """Strip comments/strings; state['bc'] tracks an open block comment."""
    out = []
    i = 0
    n = len(line)
    while i < n:
        if state['bc']:
            j = line.find('*/', i)
            if j < 0:
                return ''.join(out)
            state['bc'] = False
            i = j + 2
            continue
        c = line[i]
        if line.startswith('//', i):
            break
        if line.startswith('/*', i):
            state['bc'] = True
            i += 2
            continue
        if c in '"\'':
            q = c
            i += 1
            while i < n and line[i] != q:
                i += 2 if line[i] == '\\' else 1
            i += 1
            out.append('0')
            continue
        out.append(c)
        i += 1
    return ''.join(out)

def reindent(text):
    lines = text.split('\n')
    out = []
    depth = 0            # brace depth
    paren = 0            # open ( [ across lines
    pending = []         # stack of extra levels for unbraced bodies
    state = {'bc': False}
    in_stmt = False      # inside a statement that continues on the next line
    stmt_old = stmt_new = 0
    bc_delta = None      # column shift for the interior of a block comment
    pp_cont = False      # previous line was a preprocessor line ending in backslash
    init_depth = None    # brace depth outside a multi-line initialiser we are in
    init_delta = 0       # column shift applied to every line of that initialiser
    pp_stack = []        # [state at #if, state at end of first branch]
    ns_depths = []       # brace depth outside each open namespace / extern "C"
    for raw in lines:
        if pp_cont:
            out.append(raw)
            pp_cont = raw.rstrip().endswith('\\')
            continue
        stripped = raw.lstrip(' \t')
        ws = raw[:len(raw) - len(stripped)]
        old = vcol(ws)
        if state['bc']:
            # inside a block comment: move with its first line
            new = max(0, old + (bc_delta or 0))
            out.append((render(new) + stripped) if stripped else '')
            code_of(raw, state)
            continue
        if not stripped:
            out.append('')
            continue
        if stripped.startswith('#') and not state['bc']:
            out.append(raw.rstrip('\n'))
            pp_cont = raw.rstrip().endswith('\\')
            d = re.match(r'#\s*(\w+)', stripped)
            d = d.group(1) if d else ''
            # Only one branch of a conditional is compiled; count braces as if
            # every branch starts where the #if did, and continue after
            # #endif from where the first branch ended.
            if d in ('if', 'ifdef', 'ifndef'):
                pp_stack.append([(depth, paren, list(pending)), None])
            elif d in ('else', 'elif') and pp_stack:
                if pp_stack[-1][1] is None:
                    pp_stack[-1][1] = (depth, paren, list(pending))
                depth, paren, pending = pp_stack[-1][0][0], pp_stack[-1][0][1], list(pp_stack[-1][0][2])
            elif d == 'endif' and pp_stack:
                top = pp_stack.pop()
                if top[1] is not None:
                    depth, paren, pending = top[1][0], top[1][1], list(top[1][2])
            continue
        code = code_of(stripped, state).strip()
        if in_stmt and code.startswith('{') and stmt_code.rstrip().endswith('='):
            # "X x =" then the initialiser's brace on its own line
            new = stmt_new
        elif in_stmt:
            new = stmt_new + max(old - stmt_old, 4) if old > stmt_old else stmt_new + 4
        else:
            # namespace and extern "C" bodies are not indented
            level = depth + len(pending) - len(ns_depths)
            if code.startswith('}'):
                if ns_depths and ns_depths[-1] == depth - 1:
                    level += 1      # this brace closes the namespace itself
                level -= 1
            if re.match(r'^(case\b|default\s*:)', code):
                level -= 1
            if code.startswith('{') and pending:
                # the brace of an unbraced-looking header opens its block
                level -= 1
            new = max(level, 0) * 4
            stmt_old, stmt_new = old, new
        in_init = init_depth is not None and depth > init_depth
        if in_init:
            # hand-laid data: keep each line's offset from the opening line
            new = max(0, old + init_delta)
        if stripped.startswith('/*') and '*/' not in stripped:
            bc_delta = new - old
        out.append(render(new) + stripped)
        # update structure
        if not in_stmt:
            stmt_code = code
            if stmt_code.startswith('{') and len(stmt_code) > 1 and '}' not in stmt_code:
                # "{<tab>if (x)": classify the statement after the brace
                stmt_code = stmt_code[1:].strip()
        else:
            stmt_code += ' ' + code
        opens = code.count('{')
        closes = code.count('}')
        if code.startswith('{') and pending:
            pending.pop()
        depth_before = depth
        depth += opens - closes
        while ns_depths and depth <= ns_depths[-1]:
            ns_depths.pop()
        if opens > closes and re.match(r'^(namespace\b[^{;]*|extern\s+0\s*)\{\s*$', code):
            ns_depths.append(depth_before)
        if init_depth is not None and depth <= init_depth:
            init_depth = None
        if init_depth is None and depth > depth_before and not in_init:
            flat = stmt_code.replace(' ', '')
            if re.search(r'=\{', flat) or (code.startswith('{') and flat.startswith('{') is False and flat.split('{')[0].endswith('=')):
                init_depth = depth_before
                init_delta = new - old
        paren += code.count('(') + code.count('[') - code.count(')') - code.count(']')
        if not code:
            continue
        ends = code.rstrip()[-1]
        label = bool(re.match(r'^(case\b|default\s*:|public\s*:|private\s*:|\w+\s*:$)', stmt_code))
        if paren > 0 or (ends in '=+-*/|&?<>!' ) or (ends == ':' and not label and '?' in stmt_code):
            in_stmt = True
            continue
        in_stmt = False
        if CTRL.match(stmt_code) and ends not in ';{}' and not label \
                and not in_stmt_header_body(stmt_code):
            pending.append(1)
            continue
        if ends in ';}' and pending and not code.startswith('{'):
            pending.clear()
    return '\n'.join(out)

def in_stmt_header_body(code):
    """True when a control header carries its body on the same line."""
    m = re.match(r'^(if|else\s+if|for|while|switch)\s*\(', code)
    if not m:
        return False
    d = 0
    for i, ch in enumerate(code):
        if ch == '(':
            d += 1
        elif ch == ')':
            d -= 1
            if d == 0:
                return bool(code[i + 1:].strip())
    return False

if __name__ == '__main__':
    import subprocess
    args = sys.argv[1:]
    check = '--check' in args
    files = [a for a in args if a != '--check']
    if not files:
        out = subprocess.run(['git', 'ls-files', 'forsaken_core/src', 'forsaken_3ds/src',
                              'forsaken_pc/src', 'tests'], capture_output=True, text=True).stdout
        files = [f for f in out.split() if re.search(r'\.(cpp|h|inc|c)$', f)
                 and not f.endswith(('_shbin.h', 'vulkan_shaders.h'))]
    changed = []
    for p in files:
        t = open(p, encoding='utf-8', errors='surrogateescape').read()
        n = reindent(t)
        if n != t:
            changed.append(p)
            if not check:
                open(p, 'w', encoding='utf-8', errors='surrogateescape').write(n)
    if check and changed:
        print('needs re-indent:', *changed, sep='\n  ')
        sys.exit(1)
