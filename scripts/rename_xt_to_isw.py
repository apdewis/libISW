#!/usr/bin/env python3
"""Bulk rename Xt prefixes to Isw across the ISW codebase.

Rewrites #include directives for moved headers (X11/ -> ISW/).
Renames Xt/XT_ prefixed C symbols to Isw/ISW_ equivalents.
Does NOT touch Xrm prefixes or xrm_ struct fields.

Usage:
    python3 scripts/rename_xt_to_isw.py --check    # dry-run
    python3 scripts/rename_xt_to_isw.py --apply     # do it
"""

import os
import re
import sys
from pathlib import Path

# Headers that were moved from include/X11/ to include/ISW/.
OUR_HEADERS = {
    "CallbackI.h", "Composite.h", "CompositeP.h", "ConstrainP.h",
    "Constraint.h", "ContextI.h", "ConvertI.h", "Core.h", "CoreP.h",
    "CreateI.h", "EventI.h", "HookObjI.h", "InitialI.h", "Intrinsic.h",
    "IntrinsicI.h", "IntrinsicP.h", "Object.h", "ObjectP.h",
    "PassivGraI.h", "RectObj.h", "RectObjP.h", "ResConfigP.h",
    "ResourceI.h", "SelectionI.h", "Shell.h", "ShellI.h", "ShellP.h",
    "StringDefs.h", "ThreadsI.h", "TranslateI.h", "VarargsI.h",
    "Vendor.h", "VendorP.h", "XtDatabase.h", "XtFuncproto.h",
    "XtOptions.h", "XtQuark.h", "XtTypes.h", "XtValue.h", "Xtos.h",
    "utlist.h",
}

_header_names = "|".join(re.escape(h) for h in sorted(OUR_HEADERS))
INCLUDE_RE = re.compile(
    r'(#\s*include\s*[<"])X11/(' + _header_names + r')([>"])'
)

# Symbol rename rules — Xt only, no Xrm.
SYMBOL_RULES = [
    # 1. Exact macro
    (re.compile(r'\bXTSTRINGDEFINES\b'), 'ISWSTRINGDEFINES'),
    # 2. Internal _Xt prefix (followed by any word char)
    (re.compile(r'\b_Xt(\w)'), r'_Isw\1'),
    # 3. All-caps XT_ prefix
    (re.compile(r'\bXT_(\w)'), r'ISW_\1'),
    # 4. Xt prefix (followed by uppercase letter)
    (re.compile(r'\bXt([A-Z])'), r'Isw\1'),
    # 5. Lowercase _xt_ internal names
    (re.compile(r'\b_xt_(\w)'), r'_isw_\1'),
    # 6-10. Xlib-compat types
    (re.compile(r'\bXColor\b'), 'IswColor'),
    (re.compile(r'\bXVisualInfo\b'), 'IswVisualInfo'),
    (re.compile(r'\bXFontStruct\b'), 'IswFontStruct'),
    (re.compile(r'\bXFontSet\b'), 'IswFontSet'),
    (re.compile(r'\bXSetWindowAttributes\b'), 'IswSetWindowAttributes'),
]

SKIP_DIRS = {'.git', 'build', 'plans', '__pycache__'}
SKIP_FILES = {'rename_xt_to_isw.py'}
EXTENSIONS = {'.c', '.h', '.l', '.y'}


def find_files(root: Path):
    for dirpath, dirnames, filenames in os.walk(root):
        dirnames[:] = [d for d in dirnames if d not in SKIP_DIRS]
        for fname in filenames:
            if fname in SKIP_FILES:
                continue
            p = Path(dirpath) / fname
            if p.suffix in EXTENSIONS:
                yield p


def transform(content: str) -> str:
    # Rewrite #include paths
    content = INCLUDE_RE.sub(r'\1ISW/\2\3', content)
    # Symbol renames
    for pattern, replacement in SYMBOL_RULES:
        content = pattern.sub(replacement, content)
    return content


def main():
    if len(sys.argv) != 2 or sys.argv[1] not in ('--check', '--apply'):
        print(f"Usage: {sys.argv[0]} --check|--apply")
        sys.exit(1)

    mode = sys.argv[1]
    root = Path(__file__).resolve().parent.parent

    changed_files = []
    total_changes = 0

    for filepath in sorted(find_files(root)):
        try:
            original = filepath.read_text(encoding='utf-8', errors='surrogateescape')
        except Exception as e:
            print(f"SKIP {filepath}: {e}")
            continue

        transformed = transform(original)

        if transformed != original:
            n_changes = sum(
                1 for a, b in zip(original.split('\n'), transformed.split('\n'))
                if a != b
            )
            changed_files.append((filepath.relative_to(root), n_changes))
            total_changes += n_changes

            if mode == '--apply':
                filepath.write_text(transformed, encoding='utf-8',
                                    errors='surrogateescape')

    for fpath, n in changed_files:
        print(f"  {'WOULD CHANGE' if mode == '--check' else 'CHANGED'}: "
              f"{fpath} ({n} lines)")

    print(f"\n{'Would change' if mode == '--check' else 'Changed'} "
          f"{len(changed_files)} files, ~{total_changes} lines total.")


if __name__ == '__main__':
    main()
