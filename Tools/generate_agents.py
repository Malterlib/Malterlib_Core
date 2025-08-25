#!/usr/bin/env python3
"""
Expand @-mentions in Markdown files recursively.

Default behavior: read CLAUDE.md (repo root) and write AGENTS.md (repo root).
An include is any line that consists solely of an optional code-fence tick wrapper
around an @path token, e.g.:

    @Malterlib/Core/CLAUDE.md
    `@../String/CLAUDE.md`

Inline @path occurrences are left untouched to avoid breaking prose.
Paths are resolved relative to the file that declares the include.
Cycles are detected and skipped with a comment marker.
"""

from __future__ import annotations

import argparse
import io
import os
import re
import sys
from typing import Set, List


INCLUDE_RE = re.compile(r"^\s*`{0,3}\s*@(?P<path>[^\s`]+)\s*`{0,3}\s*$")
INLINE_CODE_INCLUDE_RE = re.compile(r"`@(?P<path>[^\s`]+)`")
INLINE_BARE_INCLUDE_RE = re.compile(r"(?<!`)@(?P<path>[^\s`]+)(?!`)")


def read_text(path: str) -> str:
    with io.open(path, "r", encoding="utf-8", errors="replace") as f:
        return f.read()


def normalize(path: str) -> str:
    # Normalize and collapse .. elements
    return os.path.normpath(path)


def expand_file(path: str, *, root_dir: str, stack: List[str], visited: Set[str]) -> List[str]:
    abspath = path if os.path.isabs(path) else os.path.join(root_dir, path)
    abspath = os.path.abspath(abspath)

    if not os.path.exists(abspath):
        return [f"<!-- Missing include: {path} (resolved to {abspath}) -->\n"]

    # Detect cycles (stack-based for immediate recursion), but also keep a visited set to avoid duplicates if desired
    if abspath in stack:
        cycle = " -> ".join([*stack, abspath])
        return [f"<!-- Cyclic include detected: {cycle} -->\n"]

    text = read_text(abspath)
    out_lines: List[str] = []
    rel_self = os.path.relpath(abspath, root_dir)
    out_lines.append(f"<!-- Begin include: {rel_self} -->\n")

    # Recurse with this file on the stack
    stack.append(abspath)
    parent_dir = os.path.dirname(abspath)

    # Collect encountered includes to expand AFTER the file content
    # We prioritize whole-line includes' order over inline mentions.
    whole_line_order: List[str] = []
    whole_line_seen: Set[str] = set()
    inline_order: List[str] = []
    inline_seen: Set[str] = set()

    for line in text.splitlines(keepends=True):
        m = INCLUDE_RE.match(line)
        if m:
            # Whole-line include: keep reference, defer expansion
            inc_path_raw = m.group("path")
            inc_path = inc_path_raw if os.path.isabs(inc_path_raw) else os.path.join(parent_dir, inc_path_raw)
            inc_abs = os.path.abspath(inc_path)
            if inc_abs not in whole_line_seen:
                whole_line_seen.add(inc_abs)
                whole_line_order.append(inc_abs)
            out_lines.append(line.rstrip("\n") + "  (see below)\n")
            continue

        # Inline code-form includes: annotate and defer
        def _sub_code(mo: re.Match) -> str:
            inc_path_raw = mo.group("path")
            inc_path = inc_path_raw if os.path.isabs(inc_path_raw) else os.path.join(parent_dir, inc_path_raw)
            inc_abs = os.path.abspath(inc_path)
            if inc_abs not in inline_seen:
                inline_seen.add(inc_abs)
                inline_order.append(inc_abs)
            return f"`@{inc_path_raw}` (see below)"

        line = INLINE_CODE_INCLUDE_RE.sub(_sub_code, line)

        # Inline bare includes: annotate and defer
        def _sub_bare(mo: re.Match) -> str:
            inc_path_raw = mo.group("path")
            inc_path = inc_path_raw if os.path.isabs(inc_path_raw) else os.path.join(parent_dir, inc_path_raw)
            inc_abs = os.path.abspath(inc_path)
            if inc_abs not in inline_seen:
                inline_seen.add(inc_abs)
                inline_order.append(inc_abs)
            return f"@{inc_path_raw} (see below)"

        line = INLINE_BARE_INCLUDE_RE.sub(_sub_bare, line)

        out_lines.append(line)

    # After writing file contents, expand each encountered include in order
    # Whole-line includes first, then inline-only includes that weren't present as whole-line.
    combined_order = whole_line_order + [p for p in inline_order if p not in whole_line_seen]
    for inc_abs in combined_order:
        if inc_abs in stack:
            cycle = " -> ".join([*stack, inc_abs])
            out_lines.append(f"<!-- Cyclic include detected: {cycle} -->\n")
            continue
        if inc_abs in visited:
            out_lines.append(f"<!-- Duplicate include skipped: {os.path.relpath(inc_abs, root_dir)} -->\n")
            continue
        rel_from_root = os.path.relpath(inc_abs, root_dir)
        out_lines.extend(expand_file(rel_from_root, root_dir=root_dir, stack=stack, visited=visited))
    stack.pop()

    out_lines.append(f"\n<!-- End include: {rel_self} -->\n")
    visited.add(abspath)
    return out_lines


def convert(input_path: str, output_path: str, repo_root: str) -> None:
    # Start expansion from input_path relative to repo_root
    start_rel = input_path if os.path.isabs(input_path) else os.path.relpath(os.path.abspath(input_path), repo_root)
    lines = expand_file(start_rel, root_dir=repo_root, stack=[], visited=set())

    # Write output
    with io.open(output_path, "w", encoding="utf-8") as f:
        f.write("<!-- GENERATED FILE: Do not edit manually. Run Tools/expand_agents.py to regenerate. -->\n\n")
        f.writelines(lines)


def main(argv: List[str]) -> int:
    parser = argparse.ArgumentParser(description="Expand @ includes from CLAUDE.md into AGENTS.md")
    parser.add_argument("--input", default="CLAUDE.md", help="Input Markdown file (default: CLAUDE.md)")
    parser.add_argument("--output", default="AGENTS.md", help="Output Markdown file (default: AGENTS.md)")
    args = parser.parse_args(argv)

    repo_root = os.path.abspath(os.getcwd())
    input_abs = os.path.abspath(args.input)
    output_abs = os.path.abspath(args.output)

    if not os.path.exists(input_abs):
        print(f"Input not found: {input_abs}", file=sys.stderr)
        return 2

    convert(input_abs, output_abs, repo_root)
    print(f"Wrote {output_abs}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
