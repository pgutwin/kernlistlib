// Logic groups (doc/KERNLIST_format.md 2.4, KL17, KL18).
//
// Operators (~ & | ^, reductions, == != ?:) are the operations; names, selects,
// concatenations, replications and constants are wiring (KL18). In each statement (an assignment,
// or a connection's expression) every operator whose parent is wiring, or which is the whole
// expression, starts a piece: it and every operator below it. Wiring is followed bit by bit to
// the source of each bit: a piece's output bit, a bit that no assignment drives (a motif output
// or a module input), or a constant. Two pieces are in the same group if one reads a bit whose
// source is the other's output. A group is named after the first signal it drives (KL17).
#include "verilog_groups.hpp"
#include "verilog_builder.hpp"
#include <algorithm>
#include <functional>
#include <map>
#include <numeric>
#include <optional>
#include <set>
#include <variant>

namespace verilog {

  namespace {

    using Bit = std::pair<std::string, int64_t>;          // a signal and an index
    struct Constant {};
    struct PieceBit { size_t piece; int64_t index; };      // bit index of a piece's output, 0 = least significant
    using Element = std::variant<Bit, Constant, PieceBit>;

    struct Piece {
      const Expr* expr;
      int operations = 0;
      std::vector<Bit> reads;
      Location location;
      size_t order;          // statement order, for naming
    };

    bool is_operator(const Expr& e) {
      return std::holds_alternative<std::shared_ptr<UnaryOp>>(e) || std::holds_alternative<std::shared_ptr<BinaryOp>>(e)
        || std::holds_alternative<std::shared_ptr<Choice>>(e);
    }

    struct Builder {
      std::map<std::string, std::pair<int64_t, int64_t>> ranges;   // msb, lsb as declared
      std::vector<Piece> pieces;

      std::vector<int64_t> indices(int64_t from, int64_t to) const {
        std::vector<int64_t> out;
        if (from >= to) for (int64_t i = from; i >= to; --i) out.push_back(i);
        else for (int64_t i = from; i <= to; ++i) out.push_back(i);
        return out;
      }
      std::vector<int64_t> all(const std::string& name) const {
        auto it = ranges.find(name);
        return it == ranges.end() ? std::vector<int64_t>{ 0 } : indices(it->second.first, it->second.second);
      }
      int64_t width_of_name(const std::string& name) const { return static_cast<int64_t>(all(name).size()); }

      // Width of an expression in a file that passed checking
      int64_t width(const Expr& e) const {
        if (auto x = std::get_if<Number>(&e)) return x->length.value_or(1);
        if (auto x = std::get_if<Identifier>(&e)) return width_of_name(x->name);
        if (std::get_if<IdentifierIndexed>(&e)) return 1;
        if (auto x = std::get_if<IdentifierSliced>(&e))
          return static_cast<int64_t>(indices(x->range.start.as_integer(), x->range.end.as_integer()).size());
        if (auto x = std::get_if<std::shared_ptr<Concatenation>>(&e)) {
          int64_t w = 0; for (const auto& y : (*x)->elements) w += width(y); return w;
        }
        if (auto x = std::get_if<std::shared_ptr<Replication>>(&e)) {
          int64_t w = 0; for (const auto& y : (*x)->elements) w += width(y); return w * (*x)->count.as_integer();
        }
        if (auto x = std::get_if<std::shared_ptr<UnaryOp>>(&e)) return (*x)->op == "~" ? width((*x)->operand) : 1;
        if (auto x = std::get_if<std::shared_ptr<BinaryOp>>(&e)) {
          const auto& op = (*x)->op;
          return (op == "&" || op == "|" || op == "^") ? width((*x)->lhs) : 1;
        }
        if (auto x = std::get_if<std::shared_ptr<Choice>>(&e)) return width((*x)->if_true);
        return 1;
      }

      void reads(const Expr& e, std::vector<Bit>& out) const {
        if (auto x = std::get_if<Identifier>(&e)) { for (auto i : all(x->name)) out.push_back({ x->name, i }); }
        else if (auto x = std::get_if<IdentifierIndexed>(&e)) out.push_back({ x->name, x->index.as_integer() });
        else if (auto x = std::get_if<IdentifierSliced>(&e)) {
          for (auto i : indices(x->range.start.as_integer(), x->range.end.as_integer())) out.push_back({ x->name, i });
        }
        else if (auto x = std::get_if<std::shared_ptr<Concatenation>>(&e)) { for (const auto& y : (*x)->elements) reads(y, out); }
        else if (auto x = std::get_if<std::shared_ptr<Replication>>(&e)) { for (const auto& y : (*x)->elements) reads(y, out); }
        else if (auto x = std::get_if<std::shared_ptr<UnaryOp>>(&e)) reads((*x)->operand, out);
        else if (auto x = std::get_if<std::shared_ptr<BinaryOp>>(&e)) { reads((*x)->lhs, out); reads((*x)->rhs, out); }
        else if (auto x = std::get_if<std::shared_ptr<Choice>>(&e)) { reads((*x)->condition, out); reads((*x)->if_true, out); reads((*x)->if_false, out); }
      }

      int operators(const Expr& e) const {
        int n = is_operator(e) ? 1 : 0;
        if (auto x = std::get_if<std::shared_ptr<Concatenation>>(&e)) for (const auto& y : (*x)->elements) n += operators(y);
        if (auto x = std::get_if<std::shared_ptr<Replication>>(&e)) for (const auto& y : (*x)->elements) n += operators(y);
        if (auto x = std::get_if<std::shared_ptr<UnaryOp>>(&e)) n += operators((*x)->operand);
        if (auto x = std::get_if<std::shared_ptr<BinaryOp>>(&e)) n += operators((*x)->lhs) + operators((*x)->rhs);
        if (auto x = std::get_if<std::shared_ptr<Choice>>(&e))
          n += operators((*x)->condition) + operators((*x)->if_true) + operators((*x)->if_false);
        return n;
      }

      // The bits of an expression, most significant first; each operator below wiring becomes a piece
      std::vector<Element> elements(const Expr& e, const Location& at, size_t order) {
        std::vector<Element> out;
        if (is_operator(e)) {
          Piece p{ &e, operators(e), {}, at, order };
          reads(e, p.reads);
          pieces.push_back(std::move(p));
          size_t id = pieces.size() - 1;
          for (int64_t i = width(e) - 1; i >= 0; --i) out.push_back(PieceBit{ id, i });
          return out;
        }
        if (auto x = std::get_if<Number>(&e)) { out.assign(static_cast<size_t>(x->length.value_or(1)), Constant{}); return out; }
        if (auto x = std::get_if<Identifier>(&e)) { for (auto i : all(x->name)) out.push_back(Bit{ x->name, i }); return out; }
        if (auto x = std::get_if<IdentifierIndexed>(&e)) { out.push_back(Bit{ x->name, x->index.as_integer() }); return out; }
        if (auto x = std::get_if<IdentifierSliced>(&e)) {
          for (auto i : indices(x->range.start.as_integer(), x->range.end.as_integer())) out.push_back(Bit{ x->name, i });
          return out;
        }
        const std::vector<Expr>* list = nullptr;
        int64_t times = 1;
        if (auto x = std::get_if<std::shared_ptr<Concatenation>>(&e)) list = &(*x)->elements;
        if (auto x = std::get_if<std::shared_ptr<Replication>>(&e)) { list = &(*x)->elements; times = (*x)->count.as_integer(); }
        if (list) {
          std::vector<Element> once;
          for (const auto& y : *list) { auto b = elements(y, at, order); once.insert(once.end(), b.begin(), b.end()); }
          for (int64_t t = 0; t < times; ++t) out.insert(out.end(), once.begin(), once.end());
        }
        return out;   // a model value cannot appear outside invariants
      }
    };

    // "name", "name[i]" or "name[hi:lo]" for a set of bits of one signal driven together
    std::string bits_text(const std::string& name, std::vector<int64_t> idx, const std::vector<int64_t>& all_bits) {
      std::string n = escape_identifier(name);
      std::sort(idx.begin(), idx.end());
      std::vector<int64_t> sorted_all = all_bits;
      std::sort(sorted_all.begin(), sorted_all.end());
      bool has_range = all_bits.size() > 1 || (all_bits.size() == 1 && all_bits[0] != 0);
      if (idx == sorted_all && all_bits.size() == idx.size() && (!has_range || idx.size() == all_bits.size()))
        return n;
      if (idx.size() == 1) return n + "[" + std::to_string(idx[0]) + "]";
      bool contiguous = idx.back() - idx.front() + 1 == static_cast<int64_t>(idx.size());
      if (contiguous) {
        bool descending = all_bits.front() >= all_bits.back();
        return descending ? n + "[" + std::to_string(idx.back()) + ":" + std::to_string(idx.front()) + "]"
                          : n + "[" + std::to_string(idx.front()) + ":" + std::to_string(idx.back()) + "]";
      }
      std::string s;
      for (auto it = idx.rbegin(); it != idx.rend(); ++it) s += (s.empty() ? "" : ", ") + n + "[" + std::to_string(*it) + "]";
      return "{" + s + "}";
    }

  } // namespace

  std::vector<LogicGroup> logic_groups(const Module& m) {
    Builder b;
    auto declare = [&](const NetDeclaration& d) {
      if (d.range) b.ranges[d.net_name] = { d.range->start.as_integer(), d.range->end.as_integer() };
    };
    for (const auto& d : m.input_declarations) declare(d);
    for (const auto& d : m.output_declarations) declare(d);
    for (const auto& d : m.net_declarations) declare(d);

    // What drives each assigned bit, and what each piece drives, in statement order
    std::map<Bit, Element> driver;
    struct Target { size_t piece; std::string signal; int64_t index; bool port; bool whole; };   // port: instance.port
    std::vector<Target> targets;
    size_t order = 0;

    for (const auto& ca : m.assignments) {
      for (const auto& [lhs, rhs] : ca.assignments) {
        auto left = b.elements(lhs, ca.location, order);     // names and selects only
        auto right = b.elements(rhs, ca.location, order);
        for (size_t k = 0; k < left.size() && k < right.size(); ++k) {
          const auto& l = left[left.size() - 1 - k];
          const auto& r = right[right.size() - 1 - k];
          if (auto bit = std::get_if<Bit>(&l)) {
            driver[*bit] = r;
            if (auto pb = std::get_if<PieceBit>(&r)) targets.push_back(Target{ pb->piece, bit->first, bit->second, false, false });
          }
        }
        ++order;
      }
    }
    for (const auto& inst : m.module_instances) {
      for (const auto& c : inst.connections) {
        if (!c.expr) continue;
        auto right = b.elements(*c.expr, inst.location, order);
        // whole: one operator is the whole connection, so the group drives the whole port
        bool whole = is_operator(*c.expr);
        for (size_t k = 0; k < right.size(); ++k) {
          if (auto pb = std::get_if<PieceBit>(&right[right.size() - 1 - k]))
            targets.push_back(Target{ pb->piece, inst.instance_name + "." + c.port, static_cast<int64_t>(k), true, whole });
        }
        ++order;
      }
    }

    // The piece whose output is the source of a bit, if any
    auto source_piece = [&](Bit bit) -> std::optional<size_t> {
      std::set<Bit> seen;
      for (;;) {
        auto it = driver.find(bit);
        if (it == driver.end() || !seen.insert(bit).second) return std::nullopt;
        if (auto pb = std::get_if<PieceBit>(&it->second)) return pb->piece;
        if (auto next = std::get_if<Bit>(&it->second)) { bit = *next; continue; }
        return std::nullopt;   // a constant
      }
    };
    auto source_signal = [&](Bit bit) -> std::optional<std::string> {
      std::set<Bit> seen;
      for (;;) {
        auto it = driver.find(bit);
        if (it == driver.end() || !seen.insert(bit).second) return bit.first;
        if (auto next = std::get_if<Bit>(&it->second)) { bit = *next; continue; }
        return std::nullopt;
      }
    };

    // Join pieces that read one another's outputs
    std::vector<size_t> parent(b.pieces.size());
    std::iota(parent.begin(), parent.end(), 0);
    std::function<size_t(size_t)> find = [&](size_t i) { return parent[i] == i ? i : parent[i] = find(parent[i]); };
    for (size_t i = 0; i < b.pieces.size(); ++i)
      for (const auto& bit : b.pieces[i].reads)
        if (auto p = source_piece(bit)) parent[find(i)] = find(*p);

    // Collect the groups, in the order of their first piece
    std::vector<LogicGroup> groups;
    std::map<size_t, size_t> group_of_root;
    std::vector<size_t> group_of_piece(b.pieces.size());
    std::vector<size_t> piece_index(b.pieces.size());     // a piece's place in its group's pieces
    for (size_t i = 0; i < b.pieces.size(); ++i) {
      auto [it, added] = group_of_root.emplace(find(i), groups.size());
      if (added) groups.emplace_back();
      group_of_piece[i] = it->second;
      LogicGroup& g = groups[it->second];
      g.operations += b.pieces[i].operations;
      g.pieces.push_back(GroupPiece{ *b.pieces[i].expr, {}, b.pieces[i].location });
      piece_index[i] = g.pieces.size() - 1;
      if (std::find_if(g.statements.begin(), g.statements.end(), [&](const Location& l) {
            return l.line == b.pieces[i].location.line && l.column == b.pieces[i].location.column; }) == g.statements.end())
        g.statements.push_back(b.pieces[i].location);
      for (const auto& bit : b.pieces[i].reads) {
        if (source_piece(bit)) continue;
        auto s = source_signal(bit);
        if (s && std::find(g.inputs.begin(), g.inputs.end(), *s) == g.inputs.end()) g.inputs.push_back(*s);
      }
    }

    // Outputs: per group, the bits it drives, per signal in the order first driven
    std::vector<std::vector<std::pair<std::string, std::vector<int64_t>>>> driven(groups.size());
    std::vector<std::vector<const Target*>> first_target(groups.size());
    for (const auto& t : targets) {
      size_t g = group_of_piece[t.piece];
      groups[g].pieces[piece_index[t.piece]].drives.push_back(DrivenBit{ t.signal, t.index, t.port });
      auto& list = driven[g];
      auto it = std::find_if(list.begin(), list.end(), [&](const auto& p) { return p.first == t.signal; });
      if (it == list.end()) { list.push_back({ t.signal, {} }); first_target[g].push_back(&t); it = list.end() - 1; }
      it->second.push_back(t.index);
    }
    for (size_t g = 0; g < groups.size(); ++g) {
      for (size_t k = 0; k < driven[g].size(); ++k) {
        const auto& [signal, idx] = driven[g][k];
        const Target& t = *first_target[g][k];
        std::string text;
        if (t.port) {
          // instance.port, with the bits (from 0 at the least significant) when the group drives part of it
          if (t.whole) text = signal;
          else {
            auto lo = *std::min_element(idx.begin(), idx.end()), hi = *std::max_element(idx.begin(), idx.end());
            text = signal + (lo == hi ? "[" + std::to_string(lo) + "]" : "[" + std::to_string(hi) + ":" + std::to_string(lo) + "]");
          }
        } else {
          text = bits_text(signal, idx, b.all(signal));
        }
        groups[g].outputs.push_back(text);
      }
      groups[g].name = groups[g].outputs.empty() ? "(drives nothing)" : groups[g].outputs.front();
    }
    return groups;
  }

} // namespace verilog
