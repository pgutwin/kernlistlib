#pragma once
#include "veriloglib.hpp"
#include "verilog_grammar.hpp"
#include <tao/pegtl.hpp>
#include <cctype>
#include <utility>
#include <memory>

namespace verilog {
  namespace actions {

    // ---------- small helpers for expression text → AST ----------
    namespace detail {

      inline std::string strip_backslash(std::string s) {
	if (!s.empty() && s[0] == '\\') return s.substr(1);
	return s;
      }

      inline Expr make_expr_from_text( const std::string& s_in )
      {
	auto ltrim = [](std::string& x){
	  size_t i = 0; while (i < x.size() && std::isspace((unsigned char)x[i])) ++i;
	  x.erase(0, i);
	};
	auto rtrim = [](std::string& x){
	  size_t i = x.size(); while (i > 0 && std::isspace((unsigned char)x[i-1])) --i;
	  x.erase(i);
	};
	auto trim = [&](std::string& x){ ltrim(x); rtrim(x); };

	auto peel_match = [&](std::string& x, char open='(', char close=')'){
	  for (;;) {
	    if (x.size() >= 2 && x.front()==open && x.back()==close) {
	      int d = 0; bool ok = false;
	      for (size_t i=0;i<x.size();++i){
		if (x[i]==open) ++d;
		else if (x[i]==close) { --d; if (d==0 && i==x.size()-1) ok = true; if (d<0){ ok=false; break; } }
	      }
	      if (ok) { x.erase(x.begin()); x.pop_back(); trim(x); continue; }
	    }
	    break;
	  }
	};

	auto strip_comments = [&](std::string& x){
	  // // ...
	  {
	    std::string out; out.reserve(x.size());
	    for (size_t i=0;i<x.size();){
	      if (i+1<x.size() && x[i]=='/' && x[i+1]=='/') {
		i+=2; while (i<x.size() && x[i]!='\n') ++i;
	      } else {
		out.push_back(x[i++]);
	      }
	    }
	    x.swap(out);
	  }
	  // /*...*/ and (*...*)
	  auto erase_block = [&](const std::string& open, const std::string& close){
	    std::string out; out.reserve(x.size());
	    for (size_t i=0;i<x.size();){
	      if (i+open.size()<=x.size() && x.compare(i, open.size(), open)==0) {
		i += open.size();
		while (i+close.size()<=x.size() && x.compare(i, close.size(), close)!=0) ++i;
		if (i+close.size()<=x.size()) i += close.size();
	      } else {
		out.push_back(x[i++]);
	      }
	    }
	    x.swap(out);
	  };
	  erase_block("/*","*/");
	  erase_block("(*","*)");
	};

	std::string s = s_in;
	strip_comments(s);
	trim(s);
	peel_match(s); // peel outer (...) repeatedly

	// ---- concatenation { e1 , e2 , ... } with top-level scanning ----
	if (!s.empty() && s.front()=='{' && s.back()=='}') {
	  std::string inner = s.substr(1, s.size()-2);
	  std::vector<Expr> elems;
	  int b=0,p=0,br=0;  // {} () []
	  std::string tok;
	  auto flush = [&](){
	    std::string t = tok; trim(t);
	    if (!t.empty()) elems.emplace_back(make_expr_from_text(t));
	    tok.clear();
	  };
	  for (char c : inner) {
	    if      (c=='{') ++b;
	    else if (c=='}') --b;
	    else if (c=='(') ++p;
	    else if (c==')') --p;
	    else if (c=='[') ++br;
	    else if (c==']') --br;

	    if (c==',' && b==0 && p==0 && br==0) flush();
	    else tok.push_back(c);
	  }
	  flush();

	  auto cc = std::make_shared<Concatenation>();
	  cc->elements = std::move(elems);
	  return cc;
	}

	// ---- identifier / select: find the FIRST '[' at top level ----
	size_t lb = std::string::npos;
	{
	  int b=0,p=0,br=0;
	  for (size_t i=0;i<s.size();++i){
	    char c = s[i];
	    if      (c=='{') ++b;
	    else if (c=='}') --b;
	    else if (c=='(') ++p;
	    else if (c==')') --p;
	    else if (c=='[') { if (b==0 && p==0 && br==0) { lb = i; break; } ++br; }
	    else if (c==']') --br;
	  }
	}

	if (lb != std::string::npos) {
	  // find matching ']' for that '['
	  size_t rb = std::string::npos;
	  int depth = 0;
	  for (size_t i = lb+1; i < s.size(); ++i) {
	    if      (s[i]=='[') ++depth;
	    else if (s[i]==']') { if (depth==0) { rb=i; break; } --depth; }
	  }
	  if (rb != std::string::npos && rb > lb+1) {
	    std::string name   = s.substr(0, lb);
	    std::string inside = s.substr(lb+1, rb-lb-1);
	    trim(name); trim(inside);
	    peel_match(name);    // e.g. "(bus)" -> "bus"
	    peel_match(inside);  // e.g. "(7:0)" -> "7:0"

	    // find ':' at top level (in case of nested parens inside [])
	    size_t colon = std::string::npos;
	    {
	      int p=0;
	      for (size_t i=0;i<inside.size();++i){
		char c = inside[i];
		if      (c=='(') ++p;
		else if (c==')') --p;
		else if (c==':' && p==0) { colon = i; break; }
	      }
	    }

	    auto toNumber = [&](std::string t)->Number{
	      trim(t);
	      Number n; n.mantissa = t;   // simple subset: mantissa only
	      return n;
	    };

	    name = strip_backslash(name);

	    if (colon != std::string::npos) {
	      std::string msb = inside.substr(0, colon);
	      std::string lsb = inside.substr(colon+1);
	      Range r{ toNumber(msb), toNumber(lsb) };
	      return IdentifierSliced{ std::move(name), std::move(r) };
	    } else {
	      return IdentifierIndexed{ std::move(name), toNumber(inside) };
	    }
	  }
	}

	// Try to detect if this is a number literal
	// Numbers can be: plain digits, sized ('2'hx), unsized ('hx), or signed (+/-123)
	auto is_number = [&](const std::string& str) -> bool {
	  if (str.empty()) return false;
	  // Check for sized: digits'base...
	  size_t tick = str.find('\'');
	  if (tick != std::string::npos && tick > 0) {
	    // Check if everything before tick is digits
	    for (size_t i = 0; i < tick; ++i) {
	      if (!std::isdigit((unsigned char)str[i])) return false;
	    }
	    return true;  // Sized number like "2'hx"
	  }
	  // Check for unsized: 'base...
	  if (str[0] == '\'') return true;
	  // Check for signed: +/- followed by digits
	  if (str[0] == '+' || str[0] == '-') {
	    if (str.size() > 1 && std::isdigit((unsigned char)str[1])) return true;
	  }
	  // Check for plain number: must contain at least one decimal digit (0-9)
	  // This prevents treating identifiers like "a", "abc" as numbers
	  bool has_digit = false;
	  bool all_valid = true;
	  for (char c : str) {
	    if (std::isdigit((unsigned char)c)) {
	      has_digit = true;
	    } else if (!std::isxdigit((unsigned char)c) && c != 'x' && c != 'X' && c != 'z' && c != 'Z') {
	      all_valid = false;
	      break;
	    }
	  }
	  return has_digit && all_valid && !str.empty();
	};

	if (is_number(s)) {
	  Number num;
	  size_t tick = s.find('\'');
	  if (tick != std::string::npos && tick > 0) {
	    // Sized number: "2'hx"
	    num.length = std::stoi(s.substr(0, tick));
	    if (tick + 1 < s.size()) {
	      num.base = s[tick + 1];
	      if (tick + 2 < s.size()) {
		num.mantissa = s.substr(tick + 2);
	      }
	    }
	  } else if (s[0] == '\'') {
	    // Unsized number: "'hx"
	    if (s.size() > 1) {
	      num.base = s[1];
	      if (s.size() > 2) {
		num.mantissa = s.substr(2);
	      }
	    }
	  } else if (s[0] == '+' || s[0] == '-') {
	    // Signed number: "+123" or "-456"
	    num.mantissa = s;
	  } else {
	    // Plain number: "123" or "abc" (hex)
	    num.mantissa = s;
	  }
	  return num;
	}

	// bare identifier
	return Identifier{ strip_backslash(s) };
      }

    } // namespace detail

    // If you prefer shorter names below:
    using detail::make_expr_from_text;
    using detail::strip_backslash;

    using namespace tao::pegtl;

    struct State {
      std::optional<int> number_len;
      std::optional<char> number_base;
      std::string number_mantissa;

      std::string last_identifier;
      std::vector<std::string> id_history;

      size_t eq_ident_mark = 0;
      std::string last_expr_text; // raw test of the most recently matched <expression>
  
      std::vector<Expr> expr_stack;
      std::vector<std::vector<Expr>> concat_items_stack;

      enum class DeclMode { None, Net, In, Out, Inout };
      DeclMode decl_mode = DeclMode::None;
      std::optional<Range> current_range;
      std::vector<std::string> decl_names;

      std::vector<NetDeclaration>  net_decl_accum;
      std::vector<InputDeclaration>  in_decl_accum;
      std::vector<OutputDeclaration> out_decl_accum;
      std::vector<InoutDeclaration>  inout_decl_accum;

      std::vector<std::pair<Expr,Expr>> current_assign_list;
      std::vector<ContinuousAssign> assign_accum;

      std::string current_inst_module_name;
      // temp for named connection key while parsing ".name(expr)"
      std::string temp_named_port;

      struct PendingInstance {
	std::string instance_name;
	std::vector<Expr> ports_pos;
	std::map<std::string, Expr> ports_named;
      };
      std::vector<PendingInstance> pending_instances;

      Module current_module;
      std::vector<Module> modules_accum;
      bool in_module = false;
    };

    template<typename Rule>
    struct action : tao::pegtl::nothing<Rule> {};

    // ---------- Number parsing actions ----------
    // Capture sized number: "2'hx" -> length=2, base='h', mantissa="x"
    template<> struct action<verilog::grammar::sized_number> {
      template<typename Input>
      static void apply(const Input& in, State& st) {
	std::string s = in.string();
	size_t tick = s.find('\'');
	st.number_len.reset();
	st.number_base.reset();
	st.number_mantissa.clear();
	
	if (tick != std::string::npos && tick > 0) {
	  st.number_len = std::stoi(s.substr(0, tick));
	  if (tick + 1 < s.size()) {
	    st.number_base = s[tick + 1];
	    if (tick + 2 < s.size()) {
	      st.number_mantissa = s.substr(tick + 2);
	    }
	  }
	}
      }
    };

    // Capture unsized number: "'hx" -> base='h', mantissa="x"
    template<> struct action<verilog::grammar::unsized_number> {
      template<typename Input>
      static void apply(const Input& in, State& st) {
	std::string s = in.string();
	st.number_len.reset();
	st.number_base.reset();
	st.number_mantissa.clear();
	
	if (s.size() > 1 && s[0] == '\'') {
	  st.number_base = s[1];
	  if (s.size() > 2) {
	    st.number_mantissa = s.substr(2);
	  }
	}
      }
    };

    // Capture signed/plain number: just mantissa
    template<> struct action<verilog::grammar::signed_number> {
      template<typename Input>
      static void apply(const Input& in, State& st) {
	st.number_len.reset();
	st.number_base.reset();
	st.number_mantissa = in.string();
      }
    };

    template<> struct action<verilog::grammar::plain_number> {
      template<typename Input>
      static void apply(const Input& in, State& st) {
	st.number_len.reset();
	st.number_base.reset();
	st.number_mantissa = in.string();
      }
    };

    // Number components saved by sized_number/unsized_number/etc but not used
    // Expression action will parse numbers via make_expr_from_text instead
    template<> struct action<verilog::grammar::number_1> {
      template<typename Input>
      static void apply(const Input&, State& st) {
	// Clear state for next number (just in case)
	st.number_len.reset();
	st.number_base.reset();
	st.number_mantissa.clear();
      }
    };

    // -- Make sure the decl-mode is active when a decl starts (you already set kw_*; keep it)

    // Width before names: [msb:lsb]
    template<>
    struct action<verilog::grammar::opt_range_decl> {
      template<typename Input>
      static void apply( const Input& in, State& st ) {
	// Empty: no width
	if (in.string().find('[') == std::string::npos) {
	  st.current_range.reset();
	  return;
	}
	// The inner part of opt_range_decl includes spaces & separators; extract the [..] span.
	const std::string s = in.string();
	auto lb = s.find('[');
	auto rb = s.rfind(']');
	if (lb == std::string::npos || rb == std::string::npos || rb <= lb) {
	  st.current_range.reset();
	  return;
	}
	std::string body = s.substr(lb + 1, rb - lb - 1); // "msb : lsb"
	auto colon = body.find(':');
	if (colon == std::string::npos) { st.current_range.reset(); return; }
	auto trim = [](std::string t){
	  auto a = t.find_first_not_of(" \t\r\n");
	  auto b = t.find_last_not_of(" \t\r\n");
	  return (a==std::string::npos) ? std::string() : t.substr(a, b - a + 1);
	};
	Number msb, lsb;
	msb.mantissa = trim(body.substr(0, colon));
	lsb.mantissa = trim(body.substr(colon + 1));
	st.current_range = Range{ msb, lsb };
      }
    };

    // Names after width: parse the whole list and push *every* variable name.
    // IMPORTANT: ignore tokens that are pure numbers (e.g. "7", "0" from [7:0]).
    template<>
    struct action<verilog::grammar::list_of_variables> {
      template<typename Input>
      static void apply(const Input& in, State& st) {
	if (st.decl_mode == State::DeclMode::None)
	  return; // only inside a decl

	const std::string s = in.string();
	std::string tok;

	auto flush = [&]() {
	  if (tok.empty())
	    return;

	  // strip leading backslash for escaped IDs
	  if (tok[0] == '\\')
	    tok.erase(0, 1);

	  if (tok.empty()) {
	    tok.clear();
	    return;
	  }

	  // Verilog identifiers cannot start with a digit; if this token does,
	  // treat it as a number (e.g. "7" or "0" from [7:0]) and ignore it.
	  unsigned char c0 = static_cast<unsigned char>(tok[0]);
	  if (!(std::isalpha(c0) || c0 == '_' || c0 == '$')) {
	    tok.clear();
	    return;
	  }

	  st.decl_names.push_back(tok);
	  tok.clear();
	};

	bool in_esc = false;
	for (char c : s) {
	  if (!in_esc && c == '\\') {
	    flush();
	    tok.push_back(c);
	    in_esc = true;
	    continue;
	  }
	  if (in_esc) {
	    // Escaped id continues until whitespace/comma/semicolon or bracket
	    if (c == ' ' || c == '\t' || c == '\r' || c == '\n' ||
		c == ',' || c == ';' ||
		c == '[' || c == ']' ||
		c == '{' || c == '}' ||
		c == '(' || c == ')') {
	      flush();
	      in_esc = false;
	      // fall through to treat this char as delimiter
	    } else {
	      tok.push_back(c);
	      continue;
	    }
	  }

	  // Regular identifier chars
	  if (std::isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '$') {
	    tok.push_back(c);
	  } else {
	    flush();
	  }
	}
	flush();
      }
    };


    // Capture bare names in declarations (variable_name uses identifier_raw)
    template<> struct action<verilog::grammar::identifier_raw> {
      template<typename Input>
      static void apply( const Input& in, State& st ) {
	std::string id = in.string();
	if (!id.empty() && id[0] == '\\') id.erase(0, 1);   // strip Verilog backslash escape
	st.last_identifier = id;

	// Only collect when inside a declaration; your DeclMode guard prevents noise from expressions
	if (st.decl_mode != State::DeclMode::None) {
	  st.decl_names.push_back(id);
	}
      }
    };

    // Capture [msb:lsb] for declarations (range BEFORE the names)
    template<> struct action<verilog::grammar::range_decl> {
      template<typename Input>
      static void apply( const Input& in, State& st ) {
	std::string s = in.string();  // "[msb:lsb]"
	if (!s.empty() && s.front()=='[') s.erase(0,1);
	if (!s.empty() && s.back()==']')  s.pop_back();

	auto pos = s.find(':');
	if (pos == std::string::npos) { st.current_range.reset(); return; }

	auto trim = [](std::string t){
	  auto a = t.find_first_not_of(" \t\r\n");
	  auto b = t.find_last_not_of(" \t\r\n");
	  return (a==std::string::npos) ? std::string() : t.substr(a, b - a + 1);
	};

	Number msb, lsb;
	msb.mantissa = trim(s.substr(0, pos));
	lsb.mantissa = trim(s.substr(pos + 1));
	st.current_range = Range{ msb, lsb };
      }
    };
    

    // ---------- expression ----------
    template<> struct action<verilog::grammar::expression> {
      template<typename Input>
      static void apply(const Input& in, State& st) {
	st.last_expr_text = in.string();
	std::string text = in.string();
	
	// Skip if this is a concatenation (already handled by concat action)
	if (!text.empty() && text[0] == '{') {
	  return;
	}
	
	// Build expression from text - make_expr_from_text handles numbers and identifiers correctly
	st.expr_stack.emplace_back(make_expr_from_text(text));
      }
    };
    
    // Handle concatenations: collect expressions from stack into Concatenation
    template<> struct action<verilog::grammar::concat> {
      template<typename Input>
      static void apply(const Input& in, State& st) {
	// Count how many elements are in the concatenation by counting commas at depth 1
	std::string text = in.string();
	size_t comma_count = 0;
	int brace_depth = 0;
	for (char c : text) {
	  if (c == '{') brace_depth++;
	  else if (c == '}') brace_depth--;
	  else if (c == ',' && brace_depth == 1) comma_count++;
	}
	size_t element_count = comma_count + 1;
	
	// Pop that many expressions from stack and build Concatenation
	if (st.expr_stack.size() >= element_count) {
	  std::vector<Expr> elements;
	  elements.reserve(element_count);
	  
	  // Pop in reverse order (stack is LIFO)
	  auto start_it = st.expr_stack.end() - element_count;
	  for (auto it = start_it; it != st.expr_stack.end(); ++it) {
	    elements.push_back(std::move(*it));
	  }
	  st.expr_stack.erase(start_it, st.expr_stack.end());
	  
	  // Push the Concatenation
	  auto concat_ptr = std::make_shared<Concatenation>();
	  concat_ptr->elements = std::move(elements);
	  st.expr_stack.emplace_back(std::move(concat_ptr));
	}
      }
    };
    
    // ---------- equality ----------
    template<> struct action<verilog::grammar::equal> {
      template<typename Input>
      static void apply(const Input&, State& st) {
	// Remember how many identifiers we had *before* parsing RHS.
	st.eq_ident_mark = st.id_history.size();
      }
    };

    // ---------- identifiers ----------
    template<> struct action<verilog::grammar::identifier> {
      template<typename Input>
      static void apply(const Input& in, State& st) {
	std::string id = in.string();
	if (!id.empty() && id[0]=='\\') id = id.substr(1);
	st.last_identifier = id;
	st.id_history.push_back(id);
	if (st.id_history.size() > 16)
	  st.id_history.erase(st.id_history.begin(), st.id_history.begin() + (st.id_history.size()-16));
	if (st.decl_mode != State::DeclMode::None)
	  st.decl_names.push_back(id);
      }
    };

    // ---------- numbers & ranges (only state wiring here) ----------
    template<> struct action<verilog::grammar::base> {
      template<typename Input>
      static void apply(const Input& in, State& st) {
	char b = in.string().back();
	if (b >= 'A' && b <= 'Z') b = char(b - 'A' + 'a');
	st.number_base = b;
      }
    };
    template<> struct action<verilog::grammar::unsigned_hex_str> {
      template<typename Input>
      static void apply(const Input& in, State& st) { st.number_mantissa = in.string(); }
    };
    template<> struct action<verilog::grammar::signed_hex_str> {
      template<typename Input>
      static void apply(const Input& in, State& st) { st.number_mantissa = in.string(); st.number_base.reset(); }
    };

    // ---------- declarations — one entry per statement (matches your tests) ----------
    template<> struct action<verilog::grammar::kw_wire>   { template<typename I> static void apply(const I&, State& st){ st.decl_mode=State::DeclMode::Net;   st.decl_names.clear(); } };
    template<> struct action<verilog::grammar::kw_input>  { template<typename I> static void apply(const I&, State& st){ st.decl_mode=State::DeclMode::In;    st.decl_names.clear(); } };
    template<> struct action<verilog::grammar::kw_output> { template<typename I> static void apply(const I&, State& st){ st.decl_mode=State::DeclMode::Out;   st.decl_names.clear(); } };
    template<> struct action<verilog::grammar::kw_inout>  { template<typename I> static void apply(const I&, State& st){ st.decl_mode=State::DeclMode::Inout; st.decl_names.clear(); } };

    template<> struct action<verilog::grammar::net_declaration> {
      template<typename Input>
      static void apply(const Input&, State& st) {
	if (!st.decl_names.empty()) {
	  std::vector<std::string> unique;
	  unique.reserve(st.decl_names.size());
	  for (const auto& n : st.decl_names) {
	    bool seen = false;
	    for (const auto& u : unique) {
	      if (u == n) { seen = true; break; }
	    }
	    if (!seen) {
	      unique.push_back(n);
	      st.net_decl_accum.push_back(NetDeclaration{ n, st.current_range });
	    }
	  }
	}
	st.decl_names.clear();
	st.decl_mode = State::DeclMode::None;
	st.current_range.reset();
      }
    };

    template<> struct action<verilog::grammar::input_declaration> {
      template<typename Input>
      static void apply(const Input&, State& st) {
	if (!st.decl_names.empty()) {
	  std::vector<std::string> unique;
	  unique.reserve(st.decl_names.size());
	  for (const auto& n : st.decl_names) {
	    bool seen = false;
	    for (const auto& u : unique) {
	      if (u == n) { seen = true; break; }
	    }
	    if (!seen) {
	      unique.push_back(n);
	      st.in_decl_accum.push_back(InputDeclaration{ n, st.current_range });
	    }
	  }
	}
	st.decl_names.clear();
	st.decl_mode = State::DeclMode::None;
	st.current_range.reset();
      }
    };

    template<> struct action<verilog::grammar::output_declaration> {
      template<typename Input>
      static void apply(const Input&, State& st) {
	if (!st.decl_names.empty()) {
	  std::vector<std::string> unique;
	  unique.reserve(st.decl_names.size());
	  for (const auto& n : st.decl_names) {
	    bool seen = false;
	    for (const auto& u : unique) {
	      if (u == n) { seen = true; break; }
	    }
	    if (!seen) {
	      unique.push_back(n);
	      st.out_decl_accum.push_back(OutputDeclaration{ n, st.current_range });
	    }
	  }
	}
	st.decl_names.clear();
	st.decl_mode = State::DeclMode::None;
	st.current_range.reset();
      }
    };

    template<> struct action<verilog::grammar::inout_declaration> {
      template<typename Input>
      static void apply(const Input&, State& st) {
	if (!st.decl_names.empty()) {
	  std::vector<std::string> unique;
	  unique.reserve(st.decl_names.size());
	  for (const auto& n : st.decl_names) {
	    bool seen = false;
	    for (const auto& u : unique) {
	      if (u == n) { seen = true; break; }
	    }
	    if (!seen) {
	      unique.push_back(n);
	      st.inout_decl_accum.push_back(InoutDeclaration{ n, st.current_range });
	    }
	  }
	}
	st.decl_names.clear();
	st.decl_mode = State::DeclMode::None;
	st.current_range.reset();
      }
    };

    // ---------- assignments ----------
    template<> struct action<verilog::grammar::assignment> {
      template<typename Input>
      static void apply(const Input&, State& st) {
	// Prefer explicit expression stack if you ever start pushing Exprs:
	if (st.expr_stack.size() >= 2) {
	  auto rhs = std::move(st.expr_stack.back()); st.expr_stack.pop_back();
	  auto lhs = std::move(st.expr_stack.back()); st.expr_stack.pop_back();
	  st.current_assign_list.emplace_back(std::move(lhs), std::move(rhs));
	  return;
	}

	// Fallback: use identifiers around '=' mark.
	// LHS is the identifier *just before* '=', RHS is the last identifier parsed in the RHS.
	if (st.eq_ident_mark >= 1 && st.id_history.size() >= st.eq_ident_mark + 1) {
	  const auto& lhs_id = st.id_history[st.eq_ident_mark - 1];
	  const auto& rhs_id = st.id_history.back();
	  st.current_assign_list.emplace_back( Identifier{ lhs_id }, Identifier{ rhs_id } );
	}
      }
    };
    template<> struct action<verilog::grammar::continuous_assign> {
      template<typename Input>
      static void apply(const Input&, State& st) {
	if (!st.current_assign_list.empty()) {
	  st.assign_accum.push_back( ContinuousAssign{ st.current_assign_list } );
	  st.current_assign_list.clear();
	}
      }
    };

    // ---------------- instantiation ----------------
    template<> struct action<verilog::grammar::module_inst_head> {
      template<typename Input>
      static void apply(const Input& in, State& st) {
	std::string id = in.string();
	if (!id.empty() && id[0]=='\\') id = id.substr(1);
	st.current_inst_module_name = std::move(id);
      }
    };
    // capture instance name before port list parsing
    template<> struct action<verilog::grammar::instance_name_tok> {
      template<typename Input>
      static void apply(const Input& in, State& st) {
	State::PendingInstance pi;
	std::string id = in.string();
	if (!id.empty() && id[0] == '\\') id = id.substr(1);
	pi.instance_name = std::move(id);
	st.pending_instances.emplace_back(std::move(pi));
      }
    };
    // positional connection: build Expr directly from the subrule span
    template<> struct action<verilog::grammar::module_port_connection> {
      template<typename Input>
      static void apply(const Input& in, State& st) {
	if (st.pending_instances.empty()) return;
	st.pending_instances.back().ports_pos.emplace_back(
							   make_expr_from_text(in.string())
							   );
      }
    };

    // named connection: remember ".name" then store expr afterwards
    template<> struct action<verilog::grammar::named_port_name> {
      template<typename Input>
      static void apply(const Input& in, State& st) {
	st.temp_named_port = in.string();
	if (!st.temp_named_port.empty() && st.temp_named_port[0]=='\\')
	  st.temp_named_port = st.temp_named_port.substr(1);
      }
    };
    // named connection: remember ".name" then store expr afterwards
    template<> struct action<verilog::grammar::named_port_connection> {
      template<typename Input>
      static void apply(const Input&, State& st) {
	if (st.pending_instances.empty()) return;
	if (!st.temp_named_port.empty()) {
	  st.pending_instances.back().ports_named.emplace(
							  st.temp_named_port, make_expr_from_text(st.last_expr_text)
							  );
	}
	st.temp_named_port.clear();
      }
    };

    // ---------- module assembly ----------
    template<> struct action<verilog::grammar::kw_module> {
      template<typename Input>
      static void apply(const Input&, State& st) { st.in_module = true; st.current_module = Module{}; }
    };
    template<> struct action<verilog::grammar::module_name_tok> {
      template<typename Input>
      static void apply(const Input& in, State& st) {
	std::string id = in.string();
	if (!id.empty() && id[0]=='\\') id = id.substr(1);
	st.current_module.module_name = id;
      }
    };
    // push each header port ident directly; no manual scanning
    template<> struct action<verilog::grammar::header_port_ident> {
      template<typename Input>
      static void apply(const Input& in, State& st) {
	std::string id = in.string();
	if (!id.empty() && id[0]=='\\') id = id.substr(1);
	st.current_module.port_list.push_back(std::move(id));
      }
    };
    template<> struct action<verilog::grammar::module_item> {
      template<typename Input>
      static void apply(const Input&, State& st) {
	if (!st.net_decl_accum.empty()) { st.current_module.net_declarations.insert(st.current_module.net_declarations.end(), st.net_decl_accum.begin(), st.net_decl_accum.end()); st.net_decl_accum.clear(); }
	if (!st.in_decl_accum.empty())  { st.current_module.input_declarations.insert(st.current_module.input_declarations.end(), st.in_decl_accum.begin(), st.in_decl_accum.end()); st.in_decl_accum.clear(); }
	if (!st.out_decl_accum.empty()) { st.current_module.output_declarations.insert(st.current_module.output_declarations.end(), st.out_decl_accum.begin(), st.out_decl_accum.end()); st.out_decl_accum.clear(); }
	if (!st.inout_decl_accum.empty()){ st.current_module.inout_declarations.insert(st.current_module.inout_declarations.end(), st.inout_decl_accum.begin(), st.inout_decl_accum.end()); st.inout_decl_accum.clear(); }
	if (!st.assign_accum.empty())   { st.current_module.assignments.insert(st.current_module.assignments.end(), st.assign_accum.begin(), st.assign_accum.end()); st.assign_accum.clear(); }
	if (!st.pending_instances.empty()) {
	  for (auto& pi : st.pending_instances) {
	    ModuleInstance mi;
	    mi.module_name  = st.current_inst_module_name;
	    mi.instance_name= pi.instance_name;
	    mi.ports_pos    = std::move(pi.ports_pos);
	    mi.ports_named  = std::move(pi.ports_named);
	    st.current_module.module_instances.emplace_back(std::move(mi));
	  }
	  st.pending_instances.clear();
	}
      }
    };
    template<> struct action<verilog::grammar::module> {
      template<typename Input>
      static void apply(const Input&, State& st) {
	st.modules_accum.emplace_back(std::move(st.current_module));
	st.current_module = Module{}; st.in_module = false;
      }
    };

  }
} // namespace verilog::actions
