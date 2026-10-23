// Checking one kernlist file (doc/KERNLIST_format.md 2.7, 2.8; KL13, KL14, KL16).
#include "verilog_checker.hpp"
#include "verilog_writer.hpp"
#include <algorithm>
#include <map>
#include <optional>
#include <set>
#include <tuple>

namespace verilog {

  namespace {

    bool before(const Location& a, const Location& b) {
      return std::tie(a.line, a.column) < std::tie(b.line, b.column);
    }

    // What checking knows about a declared name
    struct Signal {
      std::optional<std::pair<int64_t, int64_t>> range;   // msb, lsb as declared
      Location location;
    };

    // The kind of an expression: bits of a width, a number (in invariants), or unknown
    // after a problem already reported.
    struct Kind {
      enum class Is { Bits, Number, Unknown } is = Is::Unknown;
      int64_t width = 0;
      static Kind bits(int64_t w) { return Kind{ Is::Bits, w }; }
      static Kind number() { return Kind{ Is::Number, 0 }; }
      static Kind unknown() { return Kind{}; }
    };

    struct ModuleChecker {
      const Module& m;
      std::vector<Problem>& problems;
      std::map<std::string, Signal> signals;

      // the statement being checked
      Location here;
      std::string where;            // "assign", "invariant", "connection .port of inst"
      bool in_invariant = false;
      bool under_arithmetic = false;   // an unsized number there is part of a problem already reported

      void report(const Location& at, const std::string& message) {
        problems.push_back(Problem{ at.line, at.column, m.module_name, message });
      }
      void report(const std::string& message) { report(here, message); }

      // ---- names declared before use
      void use(const std::string& name) {
        auto it = signals.find(name);
        if (it == signals.end()) { report(name + " is not declared"); return; }
        const Location& d = it->second.location;
        if (d.line != 0 && here.line != 0 && !before(d, here))
          report(name + " is used before its declaration on line " + std::to_string(d.line));
      }

      // ---- forms allowed only in invariants (KL13), and -> only outermost
      void forms(const Expr& e, bool outermost) {
        std::visit([&](const auto& x) { forms_of(x, outermost); }, e);
      }
      void forms_of(const Number& n, bool) {
        if (!in_invariant && !n.length && !under_arithmetic)
          report("an unsized number (" + n.mantissa + ") is allowed only in invariant statements, in " + where
                 + "; write a sized constant such as 1'b1");
      }
      void forms_of(const Identifier&, bool) {}
      void forms_of(const IdentifierIndexed&, bool) {}
      void forms_of(const IdentifierSliced&, bool) {}
      void forms_of(const ModelRef& r, bool) {
        if (!in_invariant)
          report("model(" + r.instance + "." + r.name + ") refers to a motif's model, allowed only in invariant statements, in " + where);
      }
      void forms_of(const std::shared_ptr<Concatenation>& c, bool) { for (const auto& x : c->elements) forms(x, false); }
      void forms_of(const std::shared_ptr<Replication>& r, bool) { for (const auto& x : r->elements) forms(x, false); }
      void forms_of(const std::shared_ptr<UnaryOp>& u, bool) { forms(u->operand, false); }
      void forms_of(const std::shared_ptr<Choice>& c, bool) {
        forms(c->condition, false); forms(c->if_true, false); forms(c->if_false, false);
      }
      void forms_of(const std::shared_ptr<BinaryOp>& b, bool outermost) {
        const std::string& op = b->op;
        if (!in_invariant) {
          if (op == "+" || op == "-")
            report("+ and - are arithmetic, allowed only in invariant statements, in " + where + "; use a motif");
          else if (op == "<" || op == "<=" || op == ">" || op == ">=")
            report(op + " is an ordered comparison, allowed only in invariant statements, in " + where + "; use a motif");
          else if (op == "->")
            report("-> is implication, allowed only in invariant statements, in " + where);
        } else if (op == "->" && !outermost) {
          report("-> is allowed only as the outermost operator of an invariant");
        }
        bool arithmetic = !in_invariant && (op == "+" || op == "-" || op == "<" || op == "<=" || op == ">" || op == ">=");
        bool saved = under_arithmetic;
        under_arithmetic = saved || arithmetic;
        forms(b->lhs, false); forms(b->rhs, false);
        under_arithmetic = saved;
      }

      // ---- names used
      void names(const Expr& e) {
        if (auto x = std::get_if<Identifier>(&e)) use(x->name);
        else if (auto x = std::get_if<IdentifierIndexed>(&e)) use(x->name);
        else if (auto x = std::get_if<IdentifierSliced>(&e)) use(x->name);
        else if (auto x = std::get_if<std::shared_ptr<Concatenation>>(&e)) { for (const auto& y : (*x)->elements) names(y); }
        else if (auto x = std::get_if<std::shared_ptr<Replication>>(&e)) { for (const auto& y : (*x)->elements) names(y); }
        else if (auto x = std::get_if<std::shared_ptr<UnaryOp>>(&e)) names((*x)->operand);
        else if (auto x = std::get_if<std::shared_ptr<BinaryOp>>(&e)) { names((*x)->lhs); names((*x)->rhs); }
        else if (auto x = std::get_if<std::shared_ptr<Choice>>(&e)) { names((*x)->condition); names((*x)->if_true); names((*x)->if_false); }
      }

      // ---- widths (KL16)
      static bool fits(const Number& n) {
        int base = 10;
        switch (std::tolower(static_cast<unsigned char>(n.base.value_or('d')))) {
        case 'b': base = 2; break; case 'o': base = 8; break; case 'h': base = 16; break; default: base = 10;
        }
        int64_t length = n.length.value_or(64);
        if (length >= 63) return true;
        unsigned __int128 v = 0;
        for (char c : n.mantissa) {
          if (c == '_') continue;
          if (std::string("xXzZ").find(c) != std::string::npos) return true;
          int d = std::isdigit(static_cast<unsigned char>(c)) ? c - '0' : 10 + (std::tolower(static_cast<unsigned char>(c)) - 'a');
          v = v * base + d;
          if (v >> length) return false;
        }
        return true;
      }

      int64_t bits_of(const Kind& k, const std::string& what) {
        if (k.is == Kind::Is::Number) {
          report("a model value or unsized number cannot be used in " + what + ", in " + where);
          return -1;
        }
        return k.is == Kind::Is::Bits ? k.width : -1;
      }

      Kind select(const std::string& name, std::vector<int64_t> idx) {
        auto it = signals.find(name);
        if (it == signals.end()) return Kind::unknown();
        const auto& r = it->second.range;
        auto text = name + "[" + std::to_string(idx[0]) + (idx.size() == 2 ? ":" + std::to_string(idx[1]) : "") + "]";
        if (!r) { report(name + " has no range, so " + text + " is not allowed, in " + where); return Kind::unknown(); }
        int64_t lo = std::min(r->first, r->second), hi = std::max(r->first, r->second);
        auto range_text = "[" + std::to_string(r->first) + ":" + std::to_string(r->second) + "]";
        for (auto i : idx) {
          if (i < lo || i > hi) { report(text + " is outside " + name + "'s range " + range_text + ", in " + where); return Kind::unknown(); }
        }
        if (idx.size() == 1) return Kind::bits(1);
        if ((idx[0] - idx[1]) * (r->first - r->second) < 0) {
          report(text + " selects against the direction of " + name + "'s range " + range_text + ", in " + where);
          return Kind::unknown();
        }
        return Kind::bits((idx[0] > idx[1] ? idx[0] - idx[1] : idx[1] - idx[0]) + 1);
      }

      Kind kind(const Expr& e) {
        if (auto n = std::get_if<Number>(&e)) {
          if (!n->length) return Kind::number();
          if (!fits(*n)) report("the constant " + write_number(*n) + " does not fit in " + std::to_string(*n->length) + " bits, in " + where);
          return Kind::bits(*n->length);
        }
        if (auto x = std::get_if<Identifier>(&e)) {
          auto it = signals.find(x->name);
          if (it == signals.end()) return Kind::unknown();
          const auto& r = it->second.range;
          return Kind::bits(r ? (r->first > r->second ? r->first - r->second : r->second - r->first) + 1 : 1);
        }
        if (auto x = std::get_if<IdentifierIndexed>(&e)) return select(x->name, { x->index.as_integer() });
        if (auto x = std::get_if<IdentifierSliced>(&e)) return select(x->name, { x->range.start.as_integer(), x->range.end.as_integer() });
        if (std::get_if<ModelRef>(&e)) return Kind::number();
        if (auto x = std::get_if<std::shared_ptr<Concatenation>>(&e)) return joined((*x)->elements, 1);
        if (auto x = std::get_if<std::shared_ptr<Replication>>(&e)) return joined((*x)->elements, (*x)->count.as_integer());
        if (auto x = std::get_if<std::shared_ptr<UnaryOp>>(&e)) {
          int64_t w = bits_of(kind((*x)->operand), "operator " + (*x)->op);
          if (w < 0) return Kind::unknown();
          return Kind::bits((*x)->op == "~" ? w : 1);
        }
        if (auto x = std::get_if<std::shared_ptr<Choice>>(&e)) {
          int64_t c = bits_of(kind((*x)->condition), "the condition of ?:");
          int64_t a = bits_of(kind((*x)->if_true), "?:");
          int64_t b = bits_of(kind((*x)->if_false), "?:");
          if (c > 1) report("the condition of ?: has " + std::to_string(c) + " bits, not 1, in " + where);
          if (a < 0 || b < 0) return Kind::unknown();
          if (a != b) { report("the two sides of ?: have " + std::to_string(a) + " and " + std::to_string(b) + " bits, in " + where); return Kind::unknown(); }
          return Kind::bits(a);
        }
        const auto& b = *std::get<std::shared_ptr<BinaryOp>>(e);
        const std::string& op = b.op;
        if (op == "&" || op == "|" || op == "^") {
          int64_t l = bits_of(kind(b.lhs), "operator " + op);
          int64_t r = bits_of(kind(b.rhs), "operator " + op);
          if (l < 0 || r < 0) return Kind::unknown();
          if (l != r) { report("operator " + op + " has operands of " + std::to_string(l) + " and " + std::to_string(r) + " bits, in " + where); return Kind::unknown(); }
          return Kind::bits(l);
        }
        if (op == "==" || op == "!=") {
          Kind l = kind(b.lhs), r = kind(b.rhs);
          if (l.is == Kind::Is::Bits && r.is == Kind::Is::Bits && l.width != r.width)
            report("operator " + op + " compares " + std::to_string(l.width) + " bits with " + std::to_string(r.width) + " bits, in " + where);
          return Kind::bits(1);
        }
        if (op == "<" || op == "<=" || op == ">" || op == ">=") { kind(b.lhs); kind(b.rhs); return Kind::bits(1); }
        if (op == "+" || op == "-") { kind(b.lhs); kind(b.rhs); return Kind::number(); }
        // ->
        for (const Expr* side : { &b.lhs, &b.rhs }) {
          int64_t w = bits_of(kind(*side), "->");
          if (w > 1) report("a side of -> has " + std::to_string(w) + " bits, not 1, in " + where);
        }
        return Kind::bits(1);
      }

      Kind joined(const std::vector<Expr>& elements, int64_t times) {
        int64_t total = 0;
        bool known = true;
        for (const auto& x : elements) {
          int64_t w = bits_of(kind(x), "a concatenation");
          if (w < 0) known = false; else total += w;
        }
        return known ? Kind::bits(total * times) : Kind::unknown();
      }

      // Checks one expression: names, forms, then widths only if nothing was found so far
      // (KERNLIST_format.md 2.8: problems are not reported again as width problems).
      std::optional<Kind> expression(const Expr& e, bool outermost) {
        size_t before_count = problems.size();
        names(e);
        forms(e, outermost);
        if (problems.size() != before_count) return std::nullopt;
        Kind k = kind(e);
        if (problems.size() != before_count) return std::nullopt;
        return k;
      }

      void run(const std::vector<std::string>& local_modules) {
        // declarations, and their duplicates
        std::set<std::string> directions;
        auto declare = [&](const NetDeclaration& d, bool port) {
          if (signals.count(d.net_name)) {
            report(d.location, d.net_name + " is declared twice (first on line " + std::to_string(signals[d.net_name].location.line) + ")");
            return;
          }
          Signal s;
          if (d.range) s.range = std::make_pair(d.range->start.as_integer(), d.range->end.as_integer());
          s.location = d.location;
          signals[d.net_name] = s;
          if (port) directions.insert(d.net_name);
        };
        // in the order written, so that "declared twice" names the later one
        std::vector<std::pair<const NetDeclaration*, bool>> all;
        for (const auto& d : m.input_declarations) all.push_back({ &d, true });
        for (const auto& d : m.output_declarations) all.push_back({ &d, true });
        for (const auto& d : m.net_declarations) all.push_back({ &d, false });
        std::stable_sort(all.begin(), all.end(), [](const auto& a, const auto& b) { return before(a.first->location, b.first->location); });
        for (const auto& [d, port] : all) declare(*d, port);

        // ports
        std::set<std::string> listed;
        for (const auto& p : m.port_list) {
          if (!listed.insert(p).second) report(m.location, "port " + p + " is listed twice");
          if (!directions.count(p)) report(m.location, "port " + p + " has no input or output declaration");
        }
        for (const auto& d : directions)
          if (!listed.count(d)) report(signals[d].location, d + " is declared input or output but is not in the port list");

        // assignments
        for (const auto& ca : m.assignments) {
          here = ca.location; where = "assign"; in_invariant = false;
          for (const auto& [lhs, rhs] : ca.assignments) {
            auto l = expression(lhs, true);
            auto r = expression(rhs, true);
            if (l && r && l->is == Kind::Is::Bits && r->is == Kind::Is::Bits && l->width != r->width)
              report("assign of " + std::to_string(r->width) + " bits to " + std::to_string(l->width) + " bits");
          }
        }

        // instances
        std::map<std::string, Location> instances;
        for (const auto& inst : m.module_instances) {
          here = inst.location;
          if (instances.count(inst.instance_name))
            report("instance " + inst.instance_name + " is defined twice (first on line " + std::to_string(instances[inst.instance_name].line) + ")");
          else instances[inst.instance_name] = inst.location;
          if (inst.variant && std::find(local_modules.begin(), local_modules.end(), inst.module_name) != local_modules.end())
            report("instance " + inst.instance_name + " of module " + inst.module_name + " has a variant; only motif instances have one");
          std::set<std::string> ports;
          for (const auto& c : inst.connections) {
            if (!ports.insert(c.port).second) report("port ." + c.port + " of " + inst.instance_name + " is connected twice");
            if (!c.expr) continue;
            where = "connection ." + c.port + " of " + inst.instance_name; in_invariant = false;
            expression(*c.expr, true);
          }
          std::set<std::string> params;
          for (const auto& p : inst.parameters)
            if (!params.insert(p.name).second) report("parameter " + p.name + " of " + inst.instance_name + " is given twice");
        }

        // invariants
        std::map<std::string, Location> invariants;
        for (const auto& inv : m.invariants) {
          here = inv.location; where = "invariant " + inv.name; in_invariant = true;
          if (invariants.count(inv.name))
            report("invariant " + inv.name + " is defined twice (first on line " + std::to_string(invariants[inv.name].line) + ")");
          else invariants[inv.name] = inv.location;
          auto k = expression(inv.expr, true);
          if (k && !(k->is == Kind::Is::Bits && k->width == 1))
            report("invariant " + inv.name + " is not 1 bit wide");
        }
      }
    };

  } // namespace

  std::vector<Problem> check(const Netlist& nl) {
    std::vector<Problem> problems;
    if (nl.format_version != 1)
      problems.push_back(Problem{ 1, 1, "", "format version " + std::to_string(nl.format_version) + " is not known; this reader knows version 1" });
    std::vector<std::string> names;
    for (const auto& m : nl.modules) {
      if (std::find(names.begin(), names.end(), m.module_name) != names.end())
        problems.push_back(Problem{ m.location.line, m.location.column, m.module_name, "module " + m.module_name + " is defined twice" });
      names.push_back(m.module_name);
    }
    for (const auto& m : nl.modules) {
      ModuleChecker c{ m, problems, {}, {}, {}, false };
      c.run(names);
    }
    std::stable_sort(problems.begin(), problems.end(), [](const Problem& a, const Problem& b) {
      return std::tie(a.line, a.column) < std::tie(b.line, b.column);
    });
    return problems;
  }

  std::string format_problem(const Problem& p, const std::string& source) {
    return source + ":" + std::to_string(p.line) + ":" + std::to_string(p.column) + ": " + p.message;
  }

} // namespace verilog
