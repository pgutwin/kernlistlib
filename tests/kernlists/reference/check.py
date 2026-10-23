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

# ---- widths (KL16). A kind is ("bits", n), ("int",) for model values and unsized numbers in
# invariants, or None when unknown (an earlier problem, reported once).
def width_of_declaration(rng):
    return None if rng is None else (int(rng[0]), int(rng[1]))

def sized_value_fits(text):
    length, rest = text.split("'")
    base, digits = rest[0].lower(), rest[1:].replace("_", "")
    if any(c in "xXzZ" for c in digits):
        return True
    value = int(digits, {"b": 2, "o": 8, "d": 10, "h": 16}[base])
    return value < (1 << int(length))

def kind_of(e, env, where, problems):
    def bits(n): return ("bits", n)
    def need_bits(k, what):
        if k is None: return None
        if k[0] == "int":
            if where == "invariant":
                problems.append(f"{what}: a model value or unsized number cannot be used in bitwise logic, in {where}")
            return None
        return k[1]
    if isinstance(e, Token):
        if e.type == "SIZED":
            if not sized_value_fits(str(e)):
                problems.append(f"constant {e} does not fit in its width, in {where}")
            return bits(int(str(e).split("'")[0]))
        return ("int",)
    t = e.data
    OPS = ("AND", "OR", "EQOP", "RELOP", "SUMOP")
    if t in ("or_", "xor", "and_", "equality", "ordered_comparison", "arithmetic"):
        operands = [c for c in e.children if not (isinstance(c, Token) and c.type in OPS)]
        opname = {"or_": "|", "xor": "^", "and_": "&"}.get(t) or next(str(c) for c in e.children if isinstance(c, Token) and c.type in OPS)
        e = Tree(t, [operands[0], opname, operands[1]])
    if t == "name":
        name, sel = str(e.children[0]), e.children[1]
        if name not in env:
            return None
        rng = env[name]
        if sel is None:
            return bits(1 if rng is None else abs(rng[0] - rng[1]) + 1)
        if rng is None:
            problems.append(f"{name} has no range and cannot be selected from, in {where}")
            return None
        idx = [int(x) for x in sel.children if x is not None]
        lo, hi = min(rng), max(rng)
        if any(i < lo or i > hi for i in idx):
            problems.append(f"{name}[{':'.join(map(str, idx))}] is outside {name}'s range [{rng[0]}:{rng[1]}], in {where}")
            return None
        if len(idx) == 2 and ((idx[0] - idx[1]) * (rng[0] - rng[1]) < 0):
            problems.append(f"{name}[{idx[0]}:{idx[1]}] selects in the opposite direction to {name}'s range [{rng[0]}:{rng[1]}], in {where}")
            return None
        return bits(1 if len(idx) == 1 else abs(idx[0] - idx[1]) + 1)
    if t == "unsized_number" or t == "model_ref":
        return ("int",)
    if t in ("concat", "replicate"):
        parts = e.children[1:] if t == "replicate" else e.children
        total = 0
        for p in parts:
            n = need_bits(kind_of(p, env, where, problems), "concatenation")
            if n is None: return None
            total += n
        return bits(total * (int(e.children[0]) if t == "replicate" else 1))
    if t == "unary_op":
        n = need_bits(kind_of(e.children[1], env, where, problems), f"operator {e.children[0]}")
        if n is None: return None
        return bits(n if str(e.children[0]) == "~" else 1)
    if t in ("or_", "xor", "and_"):
        a = need_bits(kind_of(e.children[0], env, where, problems), "bitwise operator")
        b = need_bits(kind_of(e.children[2], env, where, problems), "bitwise operator")
        if a is None or b is None: return None
        if a != b:
            problems.append(f"operator {e.children[1]} has operands of {a} and {b} bits, in {where}")
            return None
        return bits(a)
    if t == "equality":
        a = kind_of(e.children[0], env, where, problems); b = kind_of(e.children[2], env, where, problems)
        if a is None or b is None: return bits(1) if a is not None or b is not None else None
        if a[0] == "bits" and b[0] == "bits" and a[1] != b[1]:
            problems.append(f"operator {e.children[1]} compares {a[1]} bits with {b[1]} bits, in {where}")
        return bits(1)
    if t == "ordered_comparison":
        kind_of(e.children[0], env, where, problems); kind_of(e.children[2], env, where, problems)
        return bits(1)
    if t == "arithmetic":
        kind_of(e.children[0], env, where, problems); kind_of(e.children[2], env, where, problems)
        return ("int",)
    if t == "choice":
        c = need_bits(kind_of(e.children[0], env, where, problems), "condition")
        a = need_bits(kind_of(e.children[1], env, where, problems), "choice")
        b = need_bits(kind_of(e.children[2], env, where, problems), "choice")
        if c is not None and c != 1:
            problems.append(f"the condition of ?: has {c} bits, not 1, in {where}")
        if a is None or b is None: return None
        if a != b:
            problems.append(f"the two sides of ?: have {a} and {b} bits, in {where}")
            return None
        return bits(a)
    if t == "implication":
        for side in e.children:
            n = need_bits(kind_of(side, env, where, problems), "->")
            if n is not None and n != 1:
                problems.append(f"a side of -> has {n} bits, not 1, in {where}")
        return bits(1)
    raise ValueError(t)

def lvalue_kind(lv, env, where, problems):
    if isinstance(lv.children[0], Token):
        return kind_of(Tree("name", [lv.children[0], lv.children[1] if len(lv.children) > 1 else None]), env, where, problems)
    total = 0
    for c in lv.children:
        k = lvalue_kind(c, env, where, problems)
        if k is None: return None
        total += k[1]
    return ("bits", total)

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
        env = {}
        for item in m.children[1:]:
            if isinstance(item, Tree) and item.data == "declaration":
                rng = item.children[1]
                for n in item.children[2:]:
                    env[str(n)] = None if rng is None else (int(rng.children[0]), int(rng.children[1]))
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
                        where = f"connection .{conn.children[0]} of {inst}"
                        before = len(problems)
                        problems += invariant_only_forms(conn.children[1], where, False)
                        if len(problems) == before:
                            kind_of(conn.children[1], env, where, problems)
                for n in names_used(item):
                    if n not in declared:
                        problems.append(f"{n} used before it is declared")
            elif item.data == "invariant":
                name = str(item.children[0])
                if name in invariants:
                    problems.append(f"invariant {name} defined twice")
                invariants.add(name)
                problems += invariant_only_forms(item.children[1], "invariant", True)
                k = kind_of(item.children[1], env, "invariant", problems)
                if k is not None and k != ("bits", 1):
                    problems.append(f"invariant {name} is not one bit wide")
                for n in names_used(item):
                    if n not in declared:
                        problems.append(f"{n} used before it is declared")
            else:  # continuous_assign
                for a in item.children:
                    before = len(problems)
                    problems += invariant_only_forms(a.children[1], "assign", False)
                    if len(problems) > before:
                        continue
                    left = lvalue_kind(a.children[0], env, "assign", problems)
                    right = kind_of(a.children[1], env, "assign", problems)
                    if left and right and left != right:
                        problems.append(f"assign of {right[1]} bits to {left[1]} bits")
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
