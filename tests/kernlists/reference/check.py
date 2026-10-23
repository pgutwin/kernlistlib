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

INVARIANT_ONLY = {
    "arithmetic": "+ and - are arithmetic",
    "ordered_comparison": "< <= > >= are ordered comparisons",
    "unsized_number": "an unsized number (write a sized constant such as 1'b1)",
    "model_ref": "model(...) refers to a motif's model",
    "implication": "-> is implication",
}

def invariant_only_forms(expr, where, top_level_ok):
    """Problems for forms KL08 does not allow outside invariants (KL13), and -> below the top."""
    problems = []
    if not isinstance(expr, Tree):
        if isinstance(expr, Token) and expr.type == "UNSIGNED" and where != "invariant":
            problems.append(f"{INVARIANT_ONLY['unsized_number']}, allowed only in invariant statements, in {where}")
        return problems
    if expr.data == "implication" and where == "invariant" and not top_level_ok:
        problems.append("-> is allowed only as the outermost operator of an invariant")
    if expr.data in INVARIANT_ONLY and where != "invariant":
        problems.append(f"{INVARIANT_ONLY[expr.data]}, allowed only in invariant statements, in {where}"
                        + ("; use a motif" if expr.data in ("arithmetic", "ordered_comparison") else ""))
    for c in expr.children:
        if expr.data in ("select", "replicate", "model_ref") and isinstance(c, Token):
            continue  # indices and counts are not values
        if expr.data in ("name", "unsized_number"):
            continue
        problems += invariant_only_forms(c, where, False)
    return problems

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
                for conn in item.find_data("connection"):
                    if len(conn.children) > 1 and conn.children[1] is not None:
                        problems += invariant_only_forms(conn.children[1], f"connection .{conn.children[0]} of {inst}", False)
                for n in names_used(item):
                    if n not in declared:
                        problems.append(f"{n} used before it is declared")
            elif item.data == "invariant":
                name = str(item.children[0])
                if name in invariants:
                    problems.append(f"invariant {name} defined twice")
                invariants.add(name)
                problems += invariant_only_forms(item.children[1], "invariant", True)
                for n in names_used(item):
                    if n not in declared:
                        problems.append(f"{n} used before it is declared")
            else:  # continuous_assign
                for a in item.find_data("assignment"):
                    problems += invariant_only_forms(a.children[1], "assign", False)
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
