#include "parser.hpp"
#include "AST.hpp"
#include "Seguimiento.hpp"
#include <cstdint>
#include <optional>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <iomanip>
#include <cassert>

static std::string trim(const std::string& str)
{
    size_t first = str.find_first_not_of(" \t\r\n");
    if (first == std::string::npos)
    {
        return "";
    }
    size_t last = str.find_last_not_of(" \t\r\n");
    return str.substr(first, (last - first + 1));
}

static std::vector<std::string> split(const std::string& str, char delim)
{
    std::vector<std::string> tokens;
    std::stringstream ss(str);
    std::string item;
    while (std::getline(ss, item, delim))
    {
        std::string t = trim(item);
        if (!t.empty())
        {
            tokens.push_back(t);
        }
    }
    return tokens;
}

int Grammar::getOrAddSymbol(const std::string& name, bool is_nonterminal)
{
    auto it = symbol_to_id.find(name);
    if (it != symbol_to_id.end())
    {
        if (is_nonterminal)
        {
            nonterminal_ids.insert(it->second);
            terminal_ids.erase(it->second);
        }
        return it->second;
    }
    int new_id = (int)symbols.size();
    symbols.push_back(name);
    symbol_to_id[name] = new_id;
    if (is_nonterminal)
    {
        nonterminal_ids.insert(new_id);
    }
    else
    {
        terminal_ids.insert(new_id);
    }
    return new_id;
}

std::string Grammar::getSymbolName(int id) const
{
    if (id >= 0 && id < (int)symbols.size())
    {
        return symbols[id];
    }
    return "";
}

Grammar Grammar::fromString(const std::string& text)
{
    Grammar g;
    g.eps_id = g.getOrAddSymbol("ε", false);
    g.eof_id = g.getOrAddSymbol("eof", false);

    std::stringstream ss(text);
    std::string line;

    struct RawRule
    {
        std::string lhs;
        std::vector<std::string> rhs;
    };

    std::vector<RawRule> raw_rules;
    std::vector<std::string> raw_lhs_order;

    while (std::getline(ss, line))
    {
        line = trim(line);
        if (line.empty() || line[0] == '#')
        {
            continue;
        }
        size_t arrow = line.find("->");
        if (arrow == std::string::npos)
        {
            continue;
        }

        std::string lhs = trim(line.substr(0, arrow));
        std::string rhs_all = trim(line.substr(arrow + 2));

        if (std::find(raw_lhs_order.begin(), raw_lhs_order.end(), lhs) == raw_lhs_order.end())
        {
            raw_lhs_order.push_back(lhs);
        }
        g.getOrAddSymbol(lhs, true);

        // Protect || in rhs
        size_t pos = 0;
        while ((pos = rhs_all.find("||", pos)) != std::string::npos)
        {
            rhs_all.replace(pos, 2, "__OR__");
            pos += 6;
        }

        std::vector<std::string> alts = split(rhs_all, '|');
        for (auto alt : alts)
        {
            pos = 0;
            while ((pos = alt.find("__OR__", pos)) != std::string::npos)
            {
                alt.replace(pos, 6, "||");
                pos += 2;
            }

            std::vector<std::string> syms;
            std::stringstream ass(alt);
            std::string sym;
            while (ass >> sym)
            {
                if (sym == "''" || sym == "ε" || sym == "EPS")
                {
                    // Empty RHS
                }
                else if (!sym.empty())
                {
                    syms.push_back(sym);
                }
            }
            raw_rules.push_back({lhs, syms});
        }
    }

    if (raw_lhs_order.empty())
    {
        throw std::runtime_error("Gramatica vacia o sin reglas validas.");
    }

    std::string user_start = raw_lhs_order[0];
    bool is_already_augmented = (!user_start.empty() && user_start.back() == '\'') || user_start == "Goal";

    if (is_already_augmented && raw_rules[0].rhs.size() == 1)
    {
        std::string goal_name = raw_rules[0].lhs;
        g.goal_id = g.getOrAddSymbol(goal_name, true);
        g.start_id = g.getOrAddSymbol(raw_rules[0].rhs[0], true);
        g.rules.push_back({0, g.goal_id, {g.start_id}});
    }
    else
    {
        std::string goal_name = "Goal";
        g.goal_id = g.getOrAddSymbol(goal_name, true);
        g.start_id = g.getOrAddSymbol(user_start, true);
        g.rules.push_back({0, g.goal_id, {g.start_id}});
    }

    int next_rule_id = 1;
    for (size_t i = 0; i < raw_rules.size(); ++i)
    {
        const auto& rr = raw_rules[i];
        if (i == 0 && rr.lhs == g.getSymbolName(g.goal_id))
        {
            continue; // Already added as rule 0
        }
        int lhs_id = g.getOrAddSymbol(rr.lhs, true);
        std::vector<int> rhs_ids;
        for (const auto& s : rr.rhs)
        {
            rhs_ids.push_back(g.getOrAddSymbol(s, false));
        }
        g.rules.push_back({next_rule_id++, lhs_id, rhs_ids});
    }

    for (const auto& r : g.rules)
    {
        g.prods[r.lhs].push_back(r);
    }

    // Determine symbol appearance order (excluding Goal)
    std::vector<int> appearance;
    auto add_app = [&](int sym_id)
    {
        if (sym_id == g.goal_id || sym_id == g.eps_id)
        {
            return;
        }
        if (std::find(appearance.begin(), appearance.end(), sym_id) == appearance.end())
        {
            appearance.push_back(sym_id);
        }
    };

    // First add nonterminals in order of appearance
    for (const auto& r : g.rules)
    {
        if (r.lhs != g.goal_id)
        {
            add_app(r.lhs);
        }
        for (int s : r.rhs)
        {
            if (g.nonterminal_ids.count(s))
            {
                add_app(s);
            }
        }
    }
    // Then add terminals in order of appearance
    for (const auto& r : g.rules)
    {
        for (int s : r.rhs)
        {
            if (g.terminal_ids.count(s))
            {
                add_app(s);
            }
        }
    }
    add_app(g.eof_id);

    g.symbol_appearance_order = appearance;
    return g;
}

Grammar Grammar::fromFile(const std::string& filepath)
{
    std::ifstream file(filepath);
    if (!file.is_open())
    {
        throw std::runtime_error("No se pudo abrir el archivo de gramatica: " + filepath);
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    return fromString(buffer.str());
}

void Grammar::printGrammar() const
{
    std::cout << "LR(1) grammar ('' is ε):" << std::endl;
    for (size_t i = 0; i < rules.size(); ++i)
    {
        const auto& r = rules[i];
        std::cout << "  (" << r.id << ") " << getSymbolName(r.lhs) << " -> ";
        if (r.rhs.empty())
        {
            std::cout << "ε";
        }
        else
        {
            for (size_t k = 0; k < r.rhs.size(); ++k)
            {
                std::cout << getSymbolName(r.rhs[k]) << (k + 1 < r.rhs.size() ? " " : "");
            }
        }
        std::cout << std::endl;
    }
}

LR1Builder::LR1Builder(const Grammar& grammar) : g(grammar)
{
}

void LR1Builder::first()
{
    for (int t : g.terminal_ids)
    {
        first_sets[t].insert(t);
    }
    for (int nt : g.nonterminal_ids)
    {
        first_sets[nt] = {};
    }

    bool changed = true;
    while (changed)
    {
        changed = false;
        for (const auto& r : g.rules)
        {
            int A = r.lhs;
            std::set<int> res = firstCadena(r.rhs);
            size_t before = first_sets[A].size();
            for (int s : res)
            {
                first_sets[A].insert(s);
            }
            if (first_sets[A].size() > before)
            {
                changed = true;
            }
        }
    }
}

std::set<int> LR1Builder::firstCadena(const std::vector<int>& beta) const
{
    std::set<int> res;
    if (beta.empty())
    {
        res.insert(g.eps_id);
        return res;
    }
    for (int X : beta)
    {
        if (X == g.eof_id)
        {
            res.insert(g.eof_id);
            return res;
        }
        if (g.terminal_ids.count(X))
        {
            res.insert(X);
            return res;
        }
        auto it = first_sets.find(X);
        if (it != first_sets.end())
        {
            const auto& fX = it->second;
            for (int s : fX)
            {
                if (s != g.eps_id)
                {
                    res.insert(s);
                }
            }
            if (fX.find(g.eps_id) == fX.end())
            {
                return res;
            }
        }
    }
    res.insert(g.eps_id);
    return res;
}

std::set<Item> LR1Builder::closure(const std::set<Item>& s)
{
    std::set<Item> Iset = s;
    bool changed = true;
    while (changed)
    {
        changed = false;
        std::vector<Item> list_items(Iset.begin(), Iset.end());
        for (const auto& it : list_items)
        {
            const Rule& r = g.rules[it.rule_id];
            if (it.dot < (int)r.rhs.size())
            {
                int B = r.rhs[it.dot];
                if (g.nonterminal_ids.count(B))
                {
                    std::vector<int> beta;
                    for (size_t k = it.dot + 1; k < r.rhs.size(); ++k)
                    {
                        beta.push_back(r.rhs[k]);
                    }
                    beta.push_back(it.la_id);

                    std::set<int> la_candidates = firstCadena(beta);
                    auto pit = g.prods.find(B);
                    if (pit != g.prods.end())
                    {
                        for (const auto& rule_b : pit->second)
                        {
                            for (int b : la_candidates)
                            {
                                if (b != g.eps_id)
                                {
                                    Item cand{rule_b.id, 0, b};
                                    if (Iset.find(cand) == Iset.end())
                                    {
                                        Iset.insert(cand);
                                        changed = true;
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    return Iset;
}

std::set<Item> LR1Builder::gotoState(const std::set<Item>& s, int x)
{
    std::set<Item> moved;
    for (const auto& it : s)
    {
        const Rule& r = g.rules[it.rule_id];
        if (it.dot < (int)r.rhs.size() && r.rhs[it.dot] == x)
        {
            moved.insert(Item{it.rule_id, it.dot + 1, it.la_id});
        }
    }
    if (moved.empty())
    {
        return {};
    }
    return closure(moved);
}

void LR1Builder::coleccionCanonica()
{
    Item start_item{0, 0, g.eof_id};
    std::set<Item> init_kernel = {start_item};
    CC.push_back(closure(init_kernel));
    cc_kernels[0] = init_kernel;
    cc_gotos[0] = "";

    std::deque<int> work = {0};
    std::map<std::set<Item>, int> seen;
    seen[CC[0]] = 0;

    while (!work.empty())
    {
        int i = work.front();
        work.pop_front();
        std::set<Item> I = CC[i];

        for (int X : g.symbol_appearance_order)
        {
            std::set<Item> moved;
            for (const auto& it : I)
            {
                const Rule& r = g.rules[it.rule_id];
                if (it.dot < (int)r.rhs.size() && r.rhs[it.dot] == X)
                {
                    moved.insert(Item{it.rule_id, it.dot + 1, it.la_id});
                }
            }
            if (moved.empty())
            {
                continue;
            }

            std::set<Item> J = closure(moved);
            if (seen.find(J) == seen.end())
            {
                int new_id = (int)CC.size();
                seen[J] = new_id;
                CC.push_back(J);
                cc_kernels[new_id] = moved;
                cc_gotos[new_id] = "goto(" + std::to_string(i) + ", " + g.getSymbolName(X) + ")";
                work.push_back(new_id);
            }
            transition_registry[{i, X}] = seen[J];
        }
    }
}

bool LR1Builder::llenarTablas()
{
    bool ok = true;
    for (size_t i = 0; i < CC.size(); ++i)
    {
        const auto& cci = CC[i];
        for (const auto& it : cci)
        {
            const Rule& r = g.rules[it.rule_id];
            if (it.dot < (int)r.rhs.size())
            {
                int c = r.rhs[it.dot];
                if (g.terminal_ids.count(c))
                {
                    auto tit = transition_registry.find({(int)i, c});
                    if (tit != transition_registry.end())
                    {
                        int j = tit->second;
                        std::string act = "s" + std::to_string(j);
                        auto key = std::make_pair((int)i, c);
                        if (Action.find(key) != Action.end() && Action[key] != act)
                        {
                            conflicts.push_back({(int)i, c, Action[key], act, {it}});
                            ok = false;
                        }
                        else
                        {
                            Action[key] = act;
                        }
                    }
                }
            }
            else
            {
                if (r.lhs == g.goal_id && it.la_id == g.eof_id)
                {
                    Action[{(int)i, g.eof_id}] = "acc";
                }
                else if (r.lhs != g.goal_id)
                {
                    std::string act = "r" + std::to_string(r.id);
                    auto key = std::make_pair((int)i, it.la_id);
                    if (Action.find(key) != Action.end() && Action[key] != act)
                    {
                        conflicts.push_back({(int)i, it.la_id, Action[key], act, {it}});
                        ok = false;
                    }
                    else
                    {
                        Action[key] = act;
                    }
                }
            }
        }

        for (int N : g.nonterminal_ids)
        {
            if (N == g.goal_id)
            {
                continue;
            }
            auto tit = transition_registry.find({(int)i, N});
            if (tit != transition_registry.end())
            {
                Goto[{(int)i, N}] = tit->second;
            }
        }
    }

    if (!conflicts.empty())
    {
        std::cout << "\n==================================================" << std::endl;
        std::cout << "          DETECCION DE CONFLICTOS                 " << std::endl;
        std::cout << "==================================================" << std::endl;
        for (const auto& conf : conflicts)
        {
            std::cout << "Conflict en Estado " << conf.state << ", simbolo '"
                      << g.getSymbolName(conf.terminal_id) << "': " << conf.action1 << " vs " << conf.action2
                      << std::endl;
        }
    }

    return ok;
}

static std::string formatItemsGrouped(const Grammar& g, const std::set<Item>& items)
{
    typedef std::pair<int, int> CoreKey; // rule_id, dot
    std::map<CoreKey, std::set<int>> grouped;

    for (const auto& it : items)
    {
        grouped[{it.rule_id, it.dot}].insert(it.la_id);
    }

    std::string result = "{";
    bool first = true;
    for (const auto& kv : grouped)
    {
        if (!first)
        {
            result += "; ";
        }
        first = false;

        const Rule& r = g.rules[kv.first.first];
        int dot = kv.first.second;

        std::string rhs_str = "";
        for (int k = 0; k < (int)r.rhs.size(); ++k)
        {
            if (k == dot)
            {
                rhs_str += ".";
            }
            else
            {
                rhs_str += g.getSymbolName(r.rhs[k]);
            }
            if (k + 1 < (int)r.rhs.size() || k == dot)
            {
                rhs_str += " ";
            }
        }
        if (dot == (int)r.rhs.size())
        {
            rhs_str += ".";
        }
        rhs_str = trim(rhs_str);
        if (rhs_str.empty())
        {
            rhs_str = ".";
        }

        std::string las_str = "";
        bool first_la = true;
        for (int la : kv.second)
        {
            if (!first_la)
            {
                las_str += "/";
            }
            first_la = false;
            las_str += g.getSymbolName(la);
        }

        result += "[" + g.getSymbolName(r.lhs) + " -> " + rhs_str + ", " + las_str + "]";
    }
    result += "}";
    return result;
}

void LR1Builder::printFirstTable() const
{
    std::cout << "\nFIRST table" << std::endl;
    std::cout << "-------------------------" << std::endl;
    std::cout << std::left << std::setw(14) << "Symbol" << " | FIRST" << std::endl;
    std::cout << "-------------------------" << std::endl;

    for (int s_id : g.symbol_appearance_order)
    {
        auto it = first_sets.find(s_id);
        if (it != first_sets.end())
        {
            std::cout << std::left << std::setw(14) << g.getSymbolName(s_id) << " | {";
            bool f = true;
            for (int la : it->second)
            {
                if (!f)
                {
                    std::cout << ", ";
                }
                f = false;
                std::cout << g.getSymbolName(la);
            }
            std::cout << "}" << std::endl;
        }
    }
    std::cout << "-------------------------" << std::endl;
}

void LR1Builder::printClosureTable() const
{
    std::cout << "\n==================================================" << std::endl;
    std::cout << "                LR(1) closure table               " << std::endl;
    std::cout << "==================================================" << std::endl;

    std::cout << std::left << std::setw(12) << "Goto"
              << " | " << std::setw(28) << "Kernel"
              << " | " << std::setw(6) << "State"
              << " | " << "Closure" << std::endl;
    std::cout << "--------------------------------------------------------------------------------"
              << std::endl;

    for (size_t s = 0; s < CC.size(); ++s)
    {
        std::string goto_str = cc_gotos.at(s);
        std::string kernel_str = formatItemsGrouped(g, cc_kernels.at(s));
        std::string closure_str = formatItemsGrouped(g, CC[s]);

        std::cout << std::left << std::setw(12) << goto_str << " | " << std::setw(28) << kernel_str << " | "
                  << std::setw(6) << s << " | " << closure_str << std::endl;
    }
}

std::string Grammar::Huella_Gramatica() const
{
    // Stable across invocations. ACTION/GOTO use the order of symbols and rules.
    std::uint64_t hash = 14695981039346656037ULL;
    auto append = [&](std::string_view value)
    {
        for (unsigned char byte : value)
        {
            hash ^= byte;
            hash *= 1099511628211ULL;
        }
        hash ^= 0xff;
        hash *= 1099511628211ULL;
    };
    for (const auto& symbol : symbols)
    {
        append(symbol);
    }
    for (const auto& rule : rules)
    {
        append(std::to_string(rule.lhs));
        for (int symbol : rule.rhs)
        {
            append(std::to_string(symbol));
        }
        append(";");
    }
    std::ostringstream out;
    out << std::hex << hash;
    return out.str();
}

void LR1Builder::printLRTable() const
{
    std::vector<int> term_cols;
    for (int s : g.symbol_appearance_order)
    {
        if (g.terminal_ids.count(s))
        {
            term_cols.push_back(s);
        }
    }
    if (std::find(term_cols.begin(), term_cols.end(), g.eof_id) == term_cols.end())
    {
        term_cols.insert(term_cols.begin(), g.eof_id);
    }

    std::vector<int> nt_cols;
    for (int s : g.symbol_appearance_order)
    {
        if (g.nonterminal_ids.count(s) && s != g.goal_id)
        {
            nt_cols.push_back(s);
        }
    }

    std::cout << "\n=========================================" << std::endl;
    std::cout << "                LR table                 " << std::endl;
    std::cout << "=========================================" << std::endl;

    std::cout << std::left << std::setw(6) << "Estado" << " | ";
    for (int t : term_cols)
    {
        std::cout << std::setw(6) << g.getSymbolName(t);
    }
    std::cout << " | ";
    for (int nt : nt_cols)
    {
        std::cout << std::setw(6) << g.getSymbolName(nt);
    }
    std::cout << std::endl;

    std::cout << "---------------------------------------------------------" << std::endl;

    int lastState = static_cast<int>(CC.size()) - 1;
    if (!Action.empty())
    {
        lastState = std::max(lastState, Action.rbegin()->first.first);
    }
    if (!Goto.empty())
    {
        lastState = std::max(lastState, Goto.rbegin()->first.first);
    }
    for (int s = 0; s <= lastState; ++s)
    {
        std::cout << std::left << std::setw(6) << s << " | ";
        for (int t : term_cols)
        {
            auto it = Action.find({(int)s, t});
            std::string val = (it != Action.end()) ? it->second : "";
            std::cout << std::setw(6) << val;
        }
        std::cout << " | ";
        for (int nt : nt_cols)
        {
            auto it = Goto.find({(int)s, nt});
            std::string val = (it != Goto.end()) ? std::to_string(it->second) : "";
            std::cout << std::setw(6) << val;
        }
        std::cout << std::endl;
    }
}

bool LR1Builder::saveTable(const std::string& filename) const
{
    std::ofstream out(filename);
    if (!out.is_open())
    {
        return false;
    }

    out << Action.size() << "\n";
    for (const auto& kv : Action)
    {
        out << kv.first.first << " " << kv.first.second << " " << kv.second << "\n";
    }

    out << Goto.size() << "\n";
    for (const auto& kv : Goto)
    {
        out << kv.first.first << " " << kv.first.second << " " << kv.second << "\n";
    }
    out.close();
    return static_cast<bool>(out);
}

bool LR1Builder::loadTable(const std::string& filename)
{
    std::ifstream in(filename);
    if (!in.is_open())
    {
        return false;
    }

    Action.clear();
    Goto.clear();

    size_t action_sz, goto_sz;
    if (!(in >> action_sz))
    {
        return false;
    }

    for (size_t i = 0; i < action_sz; ++i)
    {
        int state, sym_id;
        std::string act;
        if (!(in >> state >> sym_id >> act) || state < 0 || !g.terminal_ids.count(sym_id))
        {
            return false;
        }
        Action[{state, sym_id}] = act;
    }

    if (!(in >> goto_sz))
    {
        return false;
    }

    for (size_t i = 0; i < goto_sz; ++i)
    {
        int state, sym_id, next_state;
        if (!(in >> state >> sym_id >> next_state) || state < 0 || next_state < 0 ||
            !g.nonterminal_ids.count(sym_id))
        {
            return false;
        }
        Goto[{state, sym_id}] = next_state;
    }
    return true;
}

// recuperación en Modo Pánico.
bool LR1Builder::parseTokensWithTrace(const std::vector<Token>& tokens) const
{
    Seguimiento Traza(true);
    Traza.Encabezado();
    bool Correcto = Analizar_Con_Seguimiento(tokens, Traza, nullptr);
    for (const auto& Error : Traza.Errores)
    {
        std::cout << "[ERROR SINTACTICO] En linea " << Error.Posicion.Linea << ", columna "
                  << Error.Posicion.Columna << ": " << Error.Mensaje << '\n';
    }
    return Correcto;
}

bool LR1Builder::Analizar_Con_Seguimiento(const std::vector<Token>& tokens, Seguimiento& Traza,
                                          AST* Arbol) const
{
    struct StackElem
    {
        int state;
        std::string symbol;
        Valor_AST value;
    };

    std::optional<Constructor_AST> Constructor;
    if (Arbol)
    {
        Constructor.emplace(g, *Arbol);
    }

    //Modify - follow
    std::map<int, std::set<int>> follow;
    for (int nt : g.nonterminal_ids)
    {
        follow[nt] = {};
    }

    follow[g.goal_id].insert(g.eof_id);
    follow[g.start_id].insert(g.eof_id);

    bool changed = true;
    while (changed)
    {
        changed = false;
        for (const auto& r : g.rules)
        {
            int A = r.lhs;
            for (size_t i = 0; i < r.rhs.size(); ++i)
            {
                int B = r.rhs[i];
                if (!g.nonterminal_ids.count(B))
                {
                    continue;
                }
                std::vector<int> beta(r.rhs.begin() + i + 1, r.rhs.end());
                std::set<int> fb = firstCadena(beta);

                size_t before = follow[B].size();

                for (int t : fb)
                {
                    if (t != g.eps_id)
                    {
                        follow[B].insert(t);
                    }
                }

                if (fb.count(g.eps_id))
                {
                    for (int t : follow[A])
                    {
                        follow[B].insert(t);
                    }
                }
                if (follow[B].size() > before)
                {
                    changed = true;
                }
            }
        }
    }

    // 1) N ordenado.
    std::vector<int> N;
    for (const char* nm : {"simple_stmt", "stmt", "var_def", "decl_item", "stmt_plus"})
    {
        if (g.symbol_to_id.count(nm))
        {
            N.push_back(g.symbol_to_id.at(nm));
        }
    }

    // 2) Estado inicial del parser.

    std::vector<StackElem> stack;
    stack.push_back({0, "", {}});
    size_t cursor = 0;
    std::optional<size_t> Posicion_Ultimo_Error;
    std::map<int, size_t> Errores_Lexicos;
    for (size_t I = 0; I < Traza.Errores.size(); I++)
    {
        if (Traza.Errores[I].Fase == "Léxico")
        {
            Errores_Lexicos.emplace(Traza.Errores[I].Posicion.Linea, I);
        }
    }
    std::vector<int> Niveles(tokens.size() + 1);
    for (size_t I = 0; I < tokens.size(); I++)
    {
        Niveles[I + 1] = Niveles[I];
        if (tokens[I].token_name == "INDENT")
        {
            Niveles[I + 1]++;
        }
        if (tokens[I].token_name == "DEDENT")
        {
            Niveles[I + 1]--;
        }
    }
    bool has_error = false;
    size_t Ancho_Traza = 14;

    auto statesStr = [&]()
    {
        const size_t first = stack.size() > Ancho_Traza ? stack.size() - Ancho_Traza : 0;
        std::string s = first ? "..." : "";
        for (size_t i = first; i < stack.size(); ++i)
        {
            if (!s.empty())
            {
                s += " ";
            }
            s += std::to_string(stack[i].state);
        }
        return s;
    };

    auto symbolsStr = [&]()
    {
        const size_t first = stack.size() > Ancho_Traza ? stack.size() - Ancho_Traza : 0;
        std::string s = first ? "$ ..." : "$";
        for (size_t i = first; i < stack.size(); ++i)
        {
            if (!stack[i].symbol.empty())
            {
                s += " " + stack[i].symbol;
            }
        }
        return s;
    };

    auto Mostrar_Lexema = [](const std::string& Simbolo)
    { return Simbolo == "ID" || Simbolo == "INTEGER" || Simbolo == "STRING" || Simbolo == "IDSTRING"; };

    auto inputStr = [&](size_t from)
    {
        std::string s;
        const size_t end = std::min(tokens.size(), from + Ancho_Traza);
        for (size_t i = from; i < end; ++i)
        {
            const Token& Actual = tokens[i];
            if (!s.empty())
            {
                s += ' ';
            }
            s += Actual.token_name == "EOF" ? "eof" : Actual.token_name;
            if (Mostrar_Lexema(Actual.token_name))
            {
                s += "«" + Texto_Seguimiento(Actual.lexeme) + "»";
            }
        }
        if (end < tokens.size())
        {
            s += " …";
        }
        if (s.empty())
        {
            s = "eof";
        }
        return s;
    };

    auto Atributos_Pila = [&]()
    {
        if (!Constructor)
        {
            return std::string{};
        }
        size_t Primero = stack.size() > Ancho_Traza ? stack.size() - Ancho_Traza : 0;
        std::string Texto = Primero ? "…" : "";
        for (size_t I = Primero; I < stack.size(); I++)
        {
            if (!Texto.empty())
            {
                Texto += ' ';
            }
            Texto += Constructor->Atributo(stack[I].value);
        }
        return Texto;
    };

    auto lexemeAt = [&](size_t pos) -> std::string
    {
        if (pos >= tokens.size())
        {
            return "EOF";
        }
        return tokens[pos].lexeme;
    };

    auto Registrar_Paso = [&](size_t Posicion, std::string_view Accion, std::string_view Accion_AST = {},
                              Id_Nodo Nodo = 0, std::string Atributos = {})
    {
        if (Atributos.empty())
        {
            Atributos = Atributos_Pila();
        }
        int Linea = Posicion < tokens.size() ? tokens[Posicion].line : 0;
        if (Nodo)
        {
            Linea = (*Arbol)[Nodo].Posicion.Linea;
        }
        Traza.Registrar("LR(1)", statesStr(), symbolsStr(), inputStr(Posicion), Accion, Atributos, Accion_AST,
                        Nodo, Linea);
    };

    auto Es_Declaracion = [](std::string_view Simbolo)
    {
        return Simbolo == "var_def" || Simbolo == "typed_var" || Simbolo == "func_def" ||
               Simbolo == "class_def" || Simbolo == "class_item" || Simbolo == "class_item_list" ||
               Simbolo == "def" || Simbolo == "class" || Simbolo == "global" || Simbolo == "nonlocal" ||
               Simbolo.find("decl") != std::string_view::npos;
    };

    auto Inicio_De_Linea = [&](size_t Posicion, int Linea)
    {
        size_t Inicio = std::min(Posicion, tokens.size());
        while (Inicio > 0 && tokens[Inicio - 1].line == Linea)
        {
            Inicio--;
        }
        while (Inicio < Posicion && Inicio < tokens.size() &&
               (tokens[Inicio].token_name == "INDENT" || tokens[Inicio].token_name == "DEDENT"))
        {
            Inicio++;
        }
        return Inicio;
    };

    struct Sincronizacion
    {
        int Simbolo = -1;
        int Estado = -1;
        size_t Posicion = 0;
    };

    // FOLLOW ofrece tokens de todos los contextos. Se conserva solo el que es
    // utilizable en el GOTO actual y no pertenece al interior de otro bloque.
    auto Buscar_Sincronizacion = [&](int Estado, int Nivel, int Linea_Error, bool Solo_Declaraciones)
    {
        std::vector<std::pair<int, int>> Candidatos;
        for (int Simbolo : N)
        {
            if (Solo_Declaraciones && !Es_Declaracion(g.getSymbolName(Simbolo)))
            {
                continue;
            }
            auto Destino = Goto.find({Estado, Simbolo});
            if (Destino != Goto.end())
            {
                Candidatos.emplace_back(Simbolo, Destino->second);
            }
        }
        if (Candidatos.empty())
        {
            return Sincronizacion{};
        }

        for (size_t I = cursor; I < tokens.size() && Niveles[I] >= Nivel; I++)
        {
            const Token& Actual = tokens[I];
            bool Cierre = Actual.token_name == "DEDENT" || Actual.token_name == "EOF";
            bool Inicia_Bloque = Actual.token_name == "NEWLINE" && I + 1 < tokens.size() &&
                                 tokens[I + 1].token_name == "INDENT";
            if (Niveles[I] == Nivel && !Inicia_Bloque && Actual.token_name != "INDENT" &&
                (Cierre || Actual.token_name == "NEWLINE" || Actual.line > Linea_Error))
            {
                auto Id = g.symbol_to_id.find(Actual.token_name == "EOF" ? "eof" : Actual.token_name);
                if (Id != g.symbol_to_id.end())
                {
                    for (auto [Simbolo, Destino] : Candidatos)
                    {
                        if (follow[Simbolo].count(Id->second) && Action.count({Destino, Id->second}))
                        {
                            return Sincronizacion{Simbolo, Destino, I};
                        }
                    }
                }
            }
            if (Actual.token_name == "EOF" || (Cierre && Niveles[I] == Nivel))
            {
                break;
            }
        }
        return Sincronizacion{};
    };

    // 3) Bucle principal.

    while (true)
    {
        int state = stack.back().state;
        std::string curr_token = "EOF";
        if (cursor < tokens.size())
        {
            curr_token = tokens[cursor].token_name;
        }

        if (curr_token == "EOF")
        {
            curr_token = "eof";
        }
        int sym_id = -1;
        if (g.symbol_to_id.count(curr_token))
        {
            sym_id = g.symbol_to_id.at(curr_token);
        }

        auto it = Action.find({state, sym_id});
        std::string act;
        if (it != Action.end())
        {
            act = it->second;
        }

        // Recuperación LR si ACTION no tiene una transición.
        if (act.empty())
        {
            has_error = true;
            Ubicacion Posicion_Error;
            if (!tokens.empty())
            {
                const Token& Actual = tokens[std::min(cursor, tokens.size() - 1)];
                Posicion_Error = {Actual.line, Actual.col};
            }
            auto Lexico = Errores_Lexicos.find(Posicion_Error.Linea);
            bool Error_Lexico = Lexico != Errores_Lexicos.end();

            if (Posicion_Ultimo_Error == cursor)
            {
                if (cursor < tokens.size() && tokens[cursor].token_name != "EOF")
                {
                    Registrar_Paso(cursor, "Pánico: descarta " + Texto_Seguimiento(lexemeAt(cursor)));
                    cursor++;
                    Posicion_Ultimo_Error.reset();
                    continue;
                }
                Registrar_Paso(cursor, "Pánico: sin avance en EOF");
                return false;
            }

            Posicion_Ultimo_Error = cursor;
            size_t Indice_Error = Traza.Errores.size();
            size_t Inicio_Linea = Inicio_De_Linea(cursor, Posicion_Error.Linea);
            if (Error_Lexico)
            {
                Indice_Error = Lexico->second;
                Posicion_Error = Traza.Errores[Indice_Error].Posicion;
            }
            else
            {
                std::string Mensaje =
                    "Token inesperado '" + Texto_Seguimiento(lexemeAt(cursor)) + "' (" + curr_token + ")";
                if (Constructor && curr_token == "NEWLINE" && Inicio_Linea < cursor && cursor < tokens.size())
                {
                    const auto& Inicio = tokens[Inicio_Linea].token_name;
                    bool Cabecera = Inicio == "if" || Inicio == "elif" || Inicio == "else" ||
                                    Inicio == "while" || Inicio == "for" || Inicio == "def" ||
                                    Inicio == "class";
                    auto Dos_Puntos = g.symbol_to_id.find(":");
                    if (Cabecera && Dos_Puntos != g.symbol_to_id.end() &&
                        Action.count({state, Dos_Puntos->second}))
                    {
                        Mensaje = "Falta ':' al final de la cabecera de '" + Inicio + "'";
                    }
                    else if (tokens[cursor - 1].token_name == "=")
                    {
                        Mensaje = "Falta una expresión después de '='";
                    }
                }
                Traza.Errores.push_back({"Sintáctico", Posicion_Error, Mensaje});
            }
            Registrar_Paso(cursor,
                           Error_Lexico ? "Pánico: línea con error léxico" : "Pánico: token inesperado");

            // Paso 1: desapilar hasta un GOTO de una unidad que pueda resincronizar.
            bool Omite_Declaracion = false;
            if (Inicio_Linea < tokens.size())
            {
                const auto& Primero = tokens[Inicio_Linea].token_name;
                Omite_Declaracion =
                    Es_Declaracion(Primero) || (Primero == "ID" && Inicio_Linea + 1 < tokens.size() &&
                                                tokens[Inicio_Linea + 1].line == Posicion_Error.Linea &&
                                                tokens[Inicio_Linea + 1].token_name == ":");
            }
            Sincronizacion Punto;
            size_t Desapilados = 0;
            int Nivel = Niveles[std::min(cursor, tokens.size())];
            while (!stack.empty())
            {
                int Tope = stack.back().state;
                if (Omite_Declaracion)
                {
                    Punto = Buscar_Sincronizacion(Tope, Nivel, Posicion_Error.Linea, true);
                }
                if (Punto.Simbolo == -1)
                {
                    Punto = Buscar_Sincronizacion(Tope, Nivel, Posicion_Error.Linea, false);
                }
                if (Punto.Simbolo != -1 || stack.size() == 1)
                {
                    break;
                }
                const auto& Entrada_Pila = stack.back();
                if (Entrada_Pila.symbol == "INDENT" && Nivel > 0)
                {
                    Nivel--;
                }
                Omite_Declaracion =
                    Omite_Declaracion || Es_Declaracion(Entrada_Pila.symbol) ||
                    (Arbol && Entrada_Pila.value.Nodo && (*Arbol)[Entrada_Pila.value.Nodo].Omite_Declaracion);
                stack.pop_back();
                Desapilados++;
            }
            if (Punto.Simbolo == -1)
            {
                Registrar_Paso(cursor, "Pánico: sin GOTO y FOLLOW compatibles");
                return false;
            }

            std::string Simbolo = g.getSymbolName(Punto.Simbolo);
            Registrar_Paso(cursor, "Pánico: desapila " + std::to_string(Desapilados) + " para " + Simbolo);
            // Paso 2: avanzar hasta FOLLOW(A), dejando intacto el token de sincronización.
            size_t Descartados = Punto.Posicion - cursor;
            cursor = Punto.Posicion;
            Registrar_Paso(cursor, "FOLLOW(" + Simbolo + "): " + tokens[cursor].token_name + ", " +
                                       std::to_string(Descartados) + " descartados");

            // Pasos 3 y 4: apilar A con su GOTO y conservar un marcador en el AST.
            int top = stack.back().state;
            int goto_state = Punto.Estado;
            Id_Nodo Recuperado = 0;
            Valor_AST Valor;
            if (Constructor)
            {
                Tipo_Nodo Clase = Tipo_Nodo::Error_Sintactico;
                if (Error_Lexico)
                {
                    Clase = Tipo_Nodo::Error_Lexico;
                }
                Recuperado = Arbol->Agregar(Clase, Posicion_Error, "Fragmento omitido");
                (*Arbol)[Recuperado].Omite_Declaracion =
                    Omite_Declaracion || Simbolo == "var_def" || Simbolo == "decl_item";
                if (Simbolo == "stmt_plus")
                {
                    Valor = Valor_AST(Lista_Nodos{Recuperado});
                }
                else
                {
                    Valor = Valor_AST(Recuperado);
                }
                if (!Traza.Errores[Indice_Error].Nodo)
                {
                    Traza.Errores[Indice_Error].Nodo = Recuperado;
                }
            }
            stack.push_back({goto_state, Simbolo, std::move(Valor)});

            // Fila: inserción
            Registrar_Paso(cursor,
                           "GOTO desde " + std::to_string(top) + " hacia " + Simbolo + ", estado destino " +
                               std::to_string(goto_state),
                           Recuperado ? "Fragmento omitido = n" + std::to_string(Recuperado) : "",
                           Recuperado);

            // Paso 5: continuar con el token de sincronización.
            continue;
        }

        if (act == "acc")
        {
            if (Arbol)
            {
                Arbol->Raiz = stack.back().value.Nodo;
            }
            Id_Nodo Raiz = Arbol ? Arbol->Raiz : 0;
            if (Raiz && has_error)
            {
                (*Arbol)[Raiz].Incompleto = true;
            }
            Registrar_Paso(cursor, has_error ? "accept tras recuperación" : "accept",
                           Raiz ? "Raíz del AST = n" + std::to_string(Raiz) : "", Raiz);
            return !has_error;
        }

        if (act[0] == 's')
        {
            int next = std::stoi(act.substr(1));
            Valor_AST value;
            if (Constructor && cursor < tokens.size())
            {
                const Token& token = tokens[cursor];
                bool Mostrar = Mostrar_Lexema(curr_token) || curr_token == "True" || curr_token == "False" ||
                               curr_token == "None" || curr_token == "is" || curr_token == "==" ||
                               curr_token == "!=" || curr_token == "<" || curr_token == "<=" ||
                               curr_token == ">" || curr_token == ">=";
                value = Valor_Terminal{{token.line, token.col}, token.lexeme, Mostrar};
            }
            Registrar_Paso(cursor, "shift " + std::to_string(next));
            stack.push_back({next, curr_token, std::move(value)});
            cursor++;
        }
        else if (act[0] == 'r')
        {
            int rid = std::stoi(act.substr(1));
            const Rule& r = g.rules.at(rid);
            if (r.rhs.size() >= stack.size())
            {
                std::cerr << "[CRITICO] Pila insuficiente al reducir.\n";
                return false;
            }
            const size_t first = stack.size() - r.rhs.size();
            Valor_AST value;
            std::string Atributos = Atributos_Pila();
            std::string Accion_AST;
            Id_Nodo Nodo = 0;
            if (Constructor)
            {
                size_t Cantidad_Antes = Arbol->Cantidad_Nodos();
                std::vector<Valor_AST> arguments;
                arguments.reserve(r.rhs.size());
                for (size_t i = first; i < stack.size(); ++i)
                {
                    arguments.push_back(std::move(stack[i].value));
                }
                value = Constructor->Reducir(r, std::move(arguments));
                Accion_AST = Constructor->Describir_Reduccion(r, value);
                Nodo = value.Nodo;
                if (!Nodo && Arbol->Cantidad_Nodos() > Cantidad_Antes)
                {
                    Nodo = Arbol->Cantidad_Nodos();
                }
            }
            std::string Accion_LR = "reduce " + g.getSymbolName(r.lhs) + " →";
            for (int Simbolo : r.rhs)
            {
                Accion_LR += " " + g.getSymbolName(Simbolo);
            }
            if (r.rhs.empty())
            {
                Accion_LR += " ε";
            }
            Accion_LR += " (" + act + ")";
            Registrar_Paso(cursor, Accion_LR, Accion_AST, Nodo, std::move(Atributos));
            stack.resize(first);

            int top = stack.back().state;
            auto git = Goto.find({top, r.lhs});
            if (git == Goto.end())
            {
                std::cerr << "[CRITICO] Goto no encontrado al reducir.\n";
                return false;
            }
            stack.push_back({git->second, g.getSymbolName(r.lhs), std::move(value)});
        }
    }
}
