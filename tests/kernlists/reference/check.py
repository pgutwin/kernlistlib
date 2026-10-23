#!/usr/bin/env python3
"""Reads and checks every test file with the reference grammar (kernlist.lark) and a few
one-file checks (doc/KERNLIST_format.md 2.7), and compares with each file's EXPECT line.
Used only to check the test set before the C++ reader exists. Needs the Python package lark."""
import os, re, sys
from lark import Lark, Tree, Token
from lark.exceptions import LarkError

HERE = os.path.dirname(os.path.abspath(__file__))
PARSER = Lark(open(os.path.join(HERE, "kernlist.lark")).read(), start="file", parser="earley", ambiguity="resolve")

def names_used(tree):
    for t in tree.iter_subtrees_topdown():
        if t.data == "name":
            yield str(t.children[0])
        if t.data == "lvalue" and isinstance(t.children[0], Token):
            yield str(t.children[0])

def check(tree):
    """One-file checks. Returns a list of problems."""
    problems = []
    version = int(tree.children[0].children[0])
    if version != 1:
        problems.append(f"format version {version} is not known")
    modules = [m for m in tree.children[1:]]
    module_names = [str(m.children[0]) for m in modules]
    for n in set(module_names):
        if module_names.count(n) > 1:
            problems.append(f"module {n} defined twice")
    for m in modules:
        ports = [str(c) for c in m.children[1:] if isinstance(c, Token)]
        declared, instances, invariants = set(), set(), set()
        directions = set()
        for item in m.children[1:]:
            if not isinstance(item, Tree):
                continue
            if item.data == "declaration":
                direction = str(item.children[0])
                for n in [str(c) for c in item.children[2:]]:
                    if n in declared and not (direction == "wire"):
                        problems.append(f"{n} declared twice")
                    elif n in declared and direction == "wire":
                        problems.append(f"{n} declared twice")
                    declared.add(n)
                    if direction in ("input", "output"):
                        directions.add(n)
            elif item.data == "instantiation":
                kids = item.children
                type_name, inst = str(kids[0]), str(kids[3])
                if kids[2] is not None and type_name in module_names:
                    problems.append(f"instance {inst} of module {type_name} has a variant")
                if inst in instances:
                    problems.append(f"instance {inst} defined twice")
                instances.add(inst)
                for n in names_used(item):
                    if n not in declared:
                        problems.append(f"{n} used before it is declared")
            elif item.data == "invariant":
                name = str(item.children[0])
                if name in invariants:
                    problems.append(f"invariant {name} defined twice")
                invariants.add(name)
                for n in names_used(item):
                    if n not in declared:
                        problems.append(f"{n} used before it is declared")
            else:  # continuous_assign
                for n in names_used(item):
                    if n not in declared:
                        problems.append(f"{n} used before it is declared")
        for p in ports:
            if p not in directions:
                problems.append(f"port {p} has no input or output declaration")
    return problems

def run(path):
    text = open(path).read()
    try:
        tree = PARSER.parse(text)
    except LarkError as e:
        return "reading", str(e).splitlines()[0]
    problems = check(tree)
    return ("checking", "; ".join(problems)) if problems else ("accepted", "")

def main():
    failures = 0
    for kind in ("valid", "invalid"):
        folder = os.path.join(HERE, "..", kind)
        for f in sorted(os.listdir(folder)):
            if not f.endswith(".kl"):
                continue
            got, why = run(os.path.join(folder, f))
            m = re.search(r"EXPECT (\w+)", open(os.path.join(folder, f)).read())
            expected = m.group(1) if m else "accepted"
            ok = got == expected
            failures += not ok
            print(f"{'ok  ' if ok else 'FAIL'} {kind}/{f}: expected {expected}, got {got}" + (f" ({why})" if why else ""))
    print(f"{failures} unexpected")
    return 1 if failures else 0

if __name__ == "__main__":
    sys.exit(main())
