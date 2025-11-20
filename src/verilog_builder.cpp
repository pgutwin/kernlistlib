#include "verilog_builder.hpp"
#include <algorithm>
#include <cctype>

namespace verilog {

// ============================================================================
// Number Construction
// ============================================================================

Number make_number(int64_t value) {
    Number n;
    n.length = std::nullopt;
    n.base = std::nullopt;
    n.mantissa = std::to_string(std::abs(value));
    if (value < 0) {
        n.mantissa = "-" + n.mantissa;
    }
    return n;
}

Number make_number(int length, char base, const std::string& mantissa) {
    Number n;
    n.length = length;
    n.base = base;
    n.mantissa = mantissa;
    return n;
}

Number make_hex_number(int length, const std::string& hex_digits) {
    return make_number(length, 'h', hex_digits);
}

Number make_bin_number(int length, const std::string& bin_digits) {
    return make_number(length, 'b', bin_digits);
}

// ============================================================================
// Range Construction
// ============================================================================

Range make_range(int64_t msb, int64_t lsb) {
    Range r;
    r.start = make_number(msb);
    r.end = make_number(lsb);
    return r;
}

// ============================================================================
// Identifier Escaping
// ============================================================================

bool needs_escaping(const std::string& name) {
    if (name.empty()) return true;
    
    // Check if starts with valid character (letter or underscore)
    if (!std::isalpha(static_cast<unsigned char>(name[0])) && name[0] != '_') {
        return true;
    }
    
    // Check if all characters are valid (alnum, underscore, or dollar)
    for (char c : name) {
        if (!std::isalnum(static_cast<unsigned char>(c)) && c != '_' && c != '$') {
            return true;
        }
    }
    
    // Check if it's a keyword
    static const std::vector<std::string> keywords = {
        "module", "endmodule", "input", "output", "inout", "wire", "assign",
        "reg", "always", "initial", "begin", "end", "if", "else", "case",
        "casex", "casez", "default", "for", "while", "repeat", "forever"
    };
    
    if (std::find(keywords.begin(), keywords.end(), name) != keywords.end()) {
        return true;
    }
    
    return false;
}

std::string escape_identifier(const std::string& name) {
    if (!needs_escaping(name)) {
        return name;
    }
    return "\\" + name + " ";
}

// ============================================================================
// Expression Construction
// ============================================================================

Expr make_identifier(const std::string& name) {
    Identifier id;
    id.name = escape_identifier(name);
    return id;
}

Expr make_indexed(const std::string& name, int64_t index) {
    IdentifierIndexed ii;
    ii.name = escape_identifier(name);
    ii.index = make_number(index);
    return ii;
}

Expr make_indexed(const std::string& name, const Number& index) {
    IdentifierIndexed ii;
    ii.name = escape_identifier(name);
    ii.index = index;
    return ii;
}

Expr make_sliced(const std::string& name, int64_t msb, int64_t lsb) {
    IdentifierSliced is;
    is.name = escape_identifier(name);
    is.range = make_range(msb, lsb);
    return is;
}

Expr make_sliced(const std::string& name, const Range& range) {
    IdentifierSliced is;
    is.name = escape_identifier(name);
    is.range = range;
    return is;
}

Expr make_concatenation(const std::vector<Expr>& elements) {
    auto concat = std::make_shared<Concatenation>();
    concat->elements = elements;
    return concat;
}

// ============================================================================
// Module Construction
// ============================================================================

Module make_module(const std::string& name) {
    Module m;
    m.module_name = escape_identifier(name);
    return m;
}

// ============================================================================
// Helper: Check if signal name exists
// ============================================================================

static bool signal_exists_in_module(const Module& m, const std::string& name) {
    // Check inputs
    for (const auto& decl : m.input_declarations) {
        if (decl.net_name == name) return true;
    }
    // Check outputs
    for (const auto& decl : m.output_declarations) {
        if (decl.net_name == name) return true;
    }
    // Check inouts
    for (const auto& decl : m.inout_declarations) {
        if (decl.net_name == name) return true;
    }
    // Check wires
    for (const auto& decl : m.net_declarations) {
        if (decl.net_name == name) return true;
    }
    return false;
}

// ============================================================================
// Adding Declarations
// ============================================================================

void add_input(Module& m, const std::string& name, std::optional<Range> range) {
    std::string escaped_name = escape_identifier(name);
    
    if (signal_exists_in_module(m, escaped_name)) {
        throw std::invalid_argument("Signal '" + name + "' already exists in module");
    }
    
    InputDeclaration decl;
    decl.net_name = escaped_name;
    decl.range = range;
    
    m.input_declarations.push_back(decl);
    m.port_list.push_back(escaped_name);
}

void add_output(Module& m, const std::string& name, std::optional<Range> range) {
    std::string escaped_name = escape_identifier(name);
    
    if (signal_exists_in_module(m, escaped_name)) {
        throw std::invalid_argument("Signal '" + name + "' already exists in module");
    }
    
    OutputDeclaration decl;
    decl.net_name = escaped_name;
    decl.range = range;
    
    m.output_declarations.push_back(decl);
    m.port_list.push_back(escaped_name);
}

void add_inout(Module& m, const std::string& name, std::optional<Range> range) {
    std::string escaped_name = escape_identifier(name);
    
    if (signal_exists_in_module(m, escaped_name)) {
        throw std::invalid_argument("Signal '" + name + "' already exists in module");
    }
    
    InoutDeclaration decl;
    decl.net_name = escaped_name;
    decl.range = range;
    
    m.inout_declarations.push_back(decl);
    m.port_list.push_back(escaped_name);
}

void add_wire(Module& m, const std::string& name, std::optional<Range> range) {
    std::string escaped_name = escape_identifier(name);
    
    if (signal_exists_in_module(m, escaped_name)) {
        throw std::invalid_argument("Signal '" + name + "' already exists in module");
    }
    
    NetDeclaration decl;
    decl.net_name = escaped_name;
    decl.range = range;
    
    m.net_declarations.push_back(decl);
}

// ============================================================================
// Adding Assignments and Instances
// ============================================================================

void add_assignment(Module& m, const Expr& lhs, const Expr& rhs) {
    ContinuousAssign ca;
    ca.assignments.push_back({lhs, rhs});
    m.assignments.push_back(ca);
}

void add_instance(Module& m, const ModuleInstance& inst) {
    // Check if instance name already exists
    for (const auto& existing : m.module_instances) {
        if (existing.instance_name == inst.instance_name) {
            throw std::invalid_argument("Instance '" + inst.instance_name + "' already exists in module");
        }
    }
    
    m.module_instances.push_back(inst);
}

// ============================================================================
// Module Instance Construction
// ============================================================================

ModuleInstance make_instance(
    const std::string& module_name,
    const std::string& instance_name,
    const std::vector<Expr>& positional_ports
) {
    ModuleInstance inst;
    inst.module_name = escape_identifier(module_name);
    inst.instance_name = escape_identifier(instance_name);
    inst.ports_pos = positional_ports;
    return inst;
}

ModuleInstance make_instance_named(
    const std::string& module_name,
    const std::string& instance_name,
    const std::map<std::string, Expr>& named_ports
) {
    ModuleInstance inst;
    inst.module_name = escape_identifier(module_name);
    inst.instance_name = escape_identifier(instance_name);
    inst.ports_named = named_ports;
    return inst;
}

// ============================================================================
// Removing Elements
// ============================================================================

bool remove_input(Module& m, const std::string& name) {
    std::string escaped_name = escape_identifier(name);
    
    auto it = std::remove_if(m.input_declarations.begin(), m.input_declarations.end(),
        [&escaped_name](const InputDeclaration& decl) { return decl.net_name == escaped_name; });
    
    if (it != m.input_declarations.end()) {
        m.input_declarations.erase(it, m.input_declarations.end());
        remove_from_port_list(m, name);
        return true;
    }
    return false;
}

bool remove_output(Module& m, const std::string& name) {
    std::string escaped_name = escape_identifier(name);
    
    auto it = std::remove_if(m.output_declarations.begin(), m.output_declarations.end(),
        [&escaped_name](const OutputDeclaration& decl) { return decl.net_name == escaped_name; });
    
    if (it != m.output_declarations.end()) {
        m.output_declarations.erase(it, m.output_declarations.end());
        remove_from_port_list(m, name);
        return true;
    }
    return false;
}

bool remove_inout(Module& m, const std::string& name) {
    std::string escaped_name = escape_identifier(name);
    
    auto it = std::remove_if(m.inout_declarations.begin(), m.inout_declarations.end(),
        [&escaped_name](const InoutDeclaration& decl) { return decl.net_name == escaped_name; });
    
    if (it != m.inout_declarations.end()) {
        m.inout_declarations.erase(it, m.inout_declarations.end());
        remove_from_port_list(m, name);
        return true;
    }
    return false;
}

bool remove_wire(Module& m, const std::string& name) {
    std::string escaped_name = escape_identifier(name);
    
    auto it = std::remove_if(m.net_declarations.begin(), m.net_declarations.end(),
        [&escaped_name](const NetDeclaration& decl) { return decl.net_name == escaped_name; });
    
    if (it != m.net_declarations.end()) {
        m.net_declarations.erase(it, m.net_declarations.end());
        return true;
    }
    return false;
}

bool remove_instance(Module& m, const std::string& instance_name) {
    std::string escaped_name = escape_identifier(instance_name);
    
    auto it = std::remove_if(m.module_instances.begin(), m.module_instances.end(),
        [&escaped_name](const ModuleInstance& inst) { return inst.instance_name == escaped_name; });
    
    if (it != m.module_instances.end()) {
        m.module_instances.erase(it, m.module_instances.end());
        return true;
    }
    return false;
}

bool remove_from_port_list(Module& m, const std::string& name) {
    std::string escaped_name = escape_identifier(name);
    
    auto it = std::find(m.port_list.begin(), m.port_list.end(), escaped_name);
    if (it != m.port_list.end()) {
        m.port_list.erase(it);
        return true;
    }
    return false;
}

// ============================================================================
// Query Functions
// ============================================================================

bool has_port(const Module& m, const std::string& name) {
    std::string escaped_name = escape_identifier(name);
    
    for (const auto& decl : m.input_declarations) {
        if (decl.net_name == escaped_name) return true;
    }
    for (const auto& decl : m.output_declarations) {
        if (decl.net_name == escaped_name) return true;
    }
    for (const auto& decl : m.inout_declarations) {
        if (decl.net_name == escaped_name) return true;
    }
    return false;
}

bool has_wire(const Module& m, const std::string& name) {
    std::string escaped_name = escape_identifier(name);
    
    for (const auto& decl : m.net_declarations) {
        if (decl.net_name == escaped_name) return true;
    }
    return false;
}

bool has_signal(const Module& m, const std::string& name) {
    return has_port(m, name) || has_wire(m, name);
}

const ModuleInstance* find_instance(const Module& m, const std::string& instance_name) {
    std::string escaped_name = escape_identifier(instance_name);
    
    for (const auto& inst : m.module_instances) {
        if (inst.instance_name == escaped_name) {
            return &inst;
        }
    }
    return nullptr;
}

std::vector<const ModuleInstance*> find_instances_of_type(const Module& m, const std::string& module_type) {
    std::string escaped_type = escape_identifier(module_type);
    std::vector<const ModuleInstance*> result;
    
    for (const auto& inst : m.module_instances) {
        if (inst.module_name == escaped_type) {
            result.push_back(&inst);
        }
    }
    return result;
}

// ============================================================================
// Netlist Operations
// ============================================================================

void add_module(Netlist& nl, const Module& m) {
    // Check if module already exists
    for (const auto& existing : nl.modules) {
        if (existing.module_name == m.module_name) {
            throw std::invalid_argument("Module '" + m.module_name + "' already exists in netlist");
        }
    }
    
    nl.modules.push_back(m);
}

const Module* find_module(const Netlist& nl, const std::string& module_name) {
    std::string escaped_name = escape_identifier(module_name);
    
    for (const auto& mod : nl.modules) {
        if (mod.module_name == escaped_name) {
            return &mod;
        }
    }
    return nullptr;
}

Module* find_module(Netlist& nl, const std::string& module_name) {
    std::string escaped_name = escape_identifier(module_name);
    
    for (auto& mod : nl.modules) {
        if (mod.module_name == escaped_name) {
            return &mod;
        }
    }
    return nullptr;
}

} // namespace verilog
