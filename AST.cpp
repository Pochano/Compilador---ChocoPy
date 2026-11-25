#include "AST.hpp"
#include "parser.hpp"

#include <map>
#include <utility>

std::string_view Nombre_Nodo(Tipo_Nodo Clase) {

    switch (Clase) {
        case Tipo_Nodo::Program: return "Programa";
        case Tipo_Nodo::ClassDef: return "Clase";
        case Tipo_Nodo::FunctionDef: return "Función";
        case Tipo_Nodo::VarDef: return "Variable";
        case Tipo_Nodo::Parameter: return "Parámetro";
        case Tipo_Nodo::TypeName: return "Tipo";
        case Tipo_Nodo::ListType: return "Tipo de lista";
        case Tipo_Nodo::GlobalDecl: return "Global";
        case Tipo_Nodo::NonlocalDecl: return "Nonlocal";
        case Tipo_Nodo::Block: return "Bloque";
        case Tipo_Nodo::If: return "If";
        case Tipo_Nodo::Elif: return "Elif";
        case Tipo_Nodo::Else: return "Else";
        case Tipo_Nodo::While: return "While";
        case Tipo_Nodo::For: return "For";
        case Tipo_Nodo::Pass: return "Pass";
        case Tipo_Nodo::ExprStmt: return "Sentencia de expresión";
        case Tipo_Nodo::Return: return "Return";
        case Tipo_Nodo::Assign: return "Asignación";
        case Tipo_Nodo::Identifier: return "Identificador";
        case Tipo_Nodo::Integer: return "Entero";
        case Tipo_Nodo::Boolean: return "Booleano";
        case Tipo_Nodo::String: return "Cadena";
        case Tipo_Nodo::None: return "None";
        case Tipo_Nodo::List: return "Lista";
        case Tipo_Nodo::Unary: return "Operación unaria";
        case Tipo_Nodo::Binary: return "Operación binaria";
        case Tipo_Nodo::Conditional: return "Expresión condicional";
        case Tipo_Nodo::Member: return "Miembro";
        case Tipo_Nodo::Index: return "Índice";
        case Tipo_Nodo::Call: return "Llamada";
        case Tipo_Nodo::Error_Sintactico: return "Error de sintaxis";
        case Tipo_Nodo::Error_Lexico: return "Error léxico";
    }
    return {};
}

std::string Texto_Seguimiento(std::string_view Texto) {

    std::string Resultado;
    for (char Caracter : Texto) {
        if (Resultado.size() >= 56) {
            Resultado += "...";
            break;
        }
        switch (Caracter) {
            case '\n': Resultado += "\\n"; break;
            case '\r': Resultado += "\\r"; break;
            case '\t': Resultado += "\\t"; break;
            default: Resultado += Caracter;
        }
    }
    return Resultado;
}

Id_Nodo AST::Agregar(Tipo_Nodo Clase, Ubicacion Posicion, std::string Texto, Lista_Nodos Hijos) {

    bool Es_Error = Clase == Tipo_Nodo::Error_Sintactico || Clase == Tipo_Nodo::Error_Lexico;
    bool Incompleto = Es_Error;
    for (Id_Nodo Hijo : Hijos) {
        if (Nodos[Hijo].Incompleto) {
            Incompleto = true;
            break;
        }
    }
    Nodos.push_back({Clase, Posicion, std::move(Texto), std::move(Hijos), {}, Es_Error, Incompleto});
    return Nodos.size() - 1;
}

std::string AST::Describir(Id_Nodo Id) const {

    const auto& Nodo = (*this)[Id];
    std::string Descripcion(Nombre_Nodo(Nodo.Clase));
    if (!Nodo.Texto.empty()) Descripcion += " " + Texto_Seguimiento(Nodo.Texto);
    return Descripcion + " @" + std::to_string(Nodo.Posicion.Linea) + ":" + std::to_string(Nodo.Posicion.Columna);
}

enum class Construccion { Pasar, Secuencia, Crear, Variable, Clase, For, Llamada_ID };

struct Accion_AST {

    Construccion Construir;
    Tipo_Nodo Clase = Tipo_Nodo::Program;
    int Ancla = -1;
    int Texto = -1;
    std::vector<int> Hijos = {};
};

namespace {

using K = Tipo_Nodo;

Accion_AST Pasar(int Indice = 0) { return {Construccion::Pasar, K::Program, Indice}; }
Accion_AST Secuencia(std::vector<int> Indices = {}) {

    return {Construccion::Secuencia, K::Program, -1, -1, std::move(Indices)};
}
Accion_AST Nodo(K Clase, int Ancla, std::vector<int> Hijos = {}, int Texto = -1) {

    return {Construccion::Crear, Clase, Ancla, Texto, std::move(Hijos)};
}

const std::map<std::string, Accion_AST>& Acciones_Por_Regla() {

    static const std::map<std::string, Accion_AST> Reglas = {
        {"program -> decl_list stmt_plus", Nodo(K::Program, -1, {0, 1})},
        {"program -> decl_list", Nodo(K::Program, -1, {0})},
        {"decl_list -> decl_list decl_item", Secuencia({0, 1})},
        {"decl_list ->", Secuencia()},
        {"decl_item -> var_def", Pasar()}, {"decl_item -> func_def", Pasar()},
        {"decl_item -> class_def", Pasar()},
        {"class_def -> class ID ( ID ) : NEWLINE INDENT class_body DEDENT", {Construccion::Clase}},
        {"class_body -> pass NEWLINE", Nodo(K::Pass, 0)},
        {"class_body -> class_item_list", Pasar()},
        {"class_item_list -> class_item_list class_item", Secuencia({0, 1})},
        {"class_item_list -> class_item", Secuencia({0})},
        {"class_item -> var_def", Pasar()}, {"class_item -> func_def", Pasar()},
        {"func_def -> def ID ( params_opt ) ret_opt : NEWLINE INDENT func_body DEDENT",
         Nodo(K::FunctionDef, 0, {3, 5, 9}, 1)},
        {"params_opt -> params", Pasar()}, {"params_opt ->", Secuencia()},
        {"params -> typed_var", Secuencia({0})}, {"params -> params , typed_var", Secuencia({0, 2})},
        {"ret_opt -> -> type", Pasar(1)}, {"ret_opt ->", Secuencia()},
        {"func_body -> func_decl_list stmt_plus", Secuencia({0, 1})},
        {"func_decl_list -> func_decl_list func_decl", Secuencia({0, 1})},
        {"func_decl_list ->", Secuencia()},
        {"func_decl -> global_decl", Pasar()}, {"func_decl -> nonlocal_decl", Pasar()},
        {"func_decl -> var_def", Pasar()}, {"func_decl -> func_def", Pasar()},
        {"typed_var -> ID : type", Nodo(K::Parameter, 0, {2}, 0)},
        {"type -> ID", Nodo(K::TypeName, 0, {}, 0)},
        {"type -> IDSTRING", Nodo(K::TypeName, 0, {}, 0)},
        {"type -> [ type ]", Nodo(K::ListType, 0, {1})},
        {"global_decl -> global ID NEWLINE", Nodo(K::GlobalDecl, 1, {}, 1)},
        {"nonlocal_decl -> nonlocal ID NEWLINE", Nodo(K::NonlocalDecl, 1, {}, 1)},
        {"var_def -> typed_var = literal NEWLINE", {Construccion::Variable}},
        {"stmt_plus -> stmt_plus stmt", Secuencia({0, 1})}, {"stmt_plus -> stmt", Secuencia({0})},
        {"stmt -> simple_stmt NEWLINE", Pasar()},
        {"stmt -> if expr : block elif_list else_opt", Nodo(K::If, 0, {1, 3, 4, 5})},
        {"stmt -> while expr : block", Nodo(K::While, 0, {1, 3})},
        {"stmt -> for ID in expr : block", {Construccion::For}},
        {"elif_list -> elif_list elif expr : block", Nodo(K::Elif, 1, {2, 4})},
        {"elif_list ->", Secuencia()},
        {"else_opt -> else : block", Nodo(K::Else, 0, {2})}, {"else_opt ->", Secuencia()},
        {"block -> NEWLINE INDENT stmt_plus DEDENT", Nodo(K::Block, -1, {2})},
        {"simple_stmt -> pass", Nodo(K::Pass, 0)},
        {"simple_stmt -> expr", Nodo(K::ExprStmt, 0, {0})},
        {"simple_stmt -> return expr_opt", Nodo(K::Return, 0, {1})},
        {"simple_stmt -> assign_list expr", Nodo(K::Assign, -1, {0, 1})},
        {"assign_list -> assign_list target =", Secuencia({0, 1})},
        {"assign_list -> target =", Secuencia({0})},
        {"expr_opt -> expr", Pasar()}, {"expr_opt ->", Secuencia()},
        {"literal -> None", Nodo(K::None, 0, {}, 0)},
        {"literal -> True", Nodo(K::Boolean, 0, {}, 0)},
        {"literal -> False", Nodo(K::Boolean, 0, {}, 0)},
        {"literal -> INTEGER", Nodo(K::Integer, 0, {}, 0)},
        {"literal -> IDSTRING", Nodo(K::String, 0, {}, 0)},
        {"literal -> STRING", Nodo(K::String, 0, {}, 0)},
        {"expr -> or_expr", Pasar()},
        {"expr -> or_expr if or_expr else expr", Nodo(K::Conditional, 0, {2, 0, 4})},
        {"or_expr -> or_expr or and_expr", Nodo(K::Binary, 0, {0, 2}, 1)},
        {"or_expr -> and_expr", Pasar()},
        {"and_expr -> and_expr and not_expr", Nodo(K::Binary, 0, {0, 2}, 1)},
        {"and_expr -> not_expr", Pasar()},
        {"not_expr -> not not_expr", Nodo(K::Unary, 0, {1}, 0)},
        {"not_expr -> cmp_expr", Pasar()},
        {"cmp_expr -> arith cmp_op arith", Nodo(K::Binary, 0, {0, 2}, 1)},
        {"cmp_expr -> arith", Pasar()},
        {"cmp_op -> ==", Pasar()}, {"cmp_op -> !=", Pasar()},
        {"cmp_op -> <=", Pasar()}, {"cmp_op -> >=", Pasar()},
        {"cmp_op -> <", Pasar()}, {"cmp_op -> >", Pasar()}, {"cmp_op -> is", Pasar()},
        {"arith -> arith + term", Nodo(K::Binary, 0, {0, 2}, 1)},
        {"arith -> arith - term", Nodo(K::Binary, 0, {0, 2}, 1)},
        {"arith -> term", Pasar()},
        {"term -> term * unary", Nodo(K::Binary, 0, {0, 2}, 1)},
        {"term -> term // unary", Nodo(K::Binary, 0, {0, 2}, 1)},
        {"term -> term % unary", Nodo(K::Binary, 0, {0, 2}, 1)},
        {"term -> unary", Pasar()},
        {"unary -> - unary", Nodo(K::Unary, 0, {1}, 0)}, {"unary -> primary", Pasar()},
        {"primary -> ID", Nodo(K::Identifier, 0, {}, 0)},
        {"primary -> literal", Pasar()}, {"primary -> [ expr_list_opt ]", Nodo(K::List, 0, {1})},
        {"primary -> ( expr )", Pasar(1)},
        {"primary -> member_expr", Pasar()}, {"primary -> index_expr", Pasar()},
        {"primary -> member_expr ( expr_list_opt )", Nodo(K::Call, 0, {0, 2})},
        {"primary -> ID ( expr_list_opt )", {Construccion::Llamada_ID}},
        {"member_expr -> primary . ID", Nodo(K::Member, 2, {0}, 2)},
        {"index_expr -> primary [ expr ]", Nodo(K::Index, 0, {0, 2})},
        {"expr_list_opt -> expr_list", Pasar()}, {"expr_list_opt ->", Secuencia()},
        {"expr_list -> expr", Secuencia({0})}, {"expr_list -> expr_list , expr", Secuencia({0, 2})},
        {"target -> ID", Nodo(K::Identifier, 0, {}, 0)},
        {"target -> member_expr", Pasar()}, {"target -> index_expr", Pasar()},
    };
    return Reglas;
}

std::string Produccion(const Grammar& Gramatica, const Rule& Regla) {

    std::string Clave = Gramatica.getSymbolName(Regla.lhs) + " ->";
    for (int Simbolo : Regla.rhs) Clave += ' ' + Gramatica.getSymbolName(Simbolo);
    return Clave;
}

void Agregar_Valor(Lista_Nodos& Salida, Valor_AST& Valor) {

    if (Valor.Nodo) {
        Salida.push_back(Valor.Nodo);
    }

    else if (Salida.empty()) {
        Salida = std::move(Valor.Lista);
    }

    else {
        Salida.insert(Salida.end(), Valor.Lista.begin(), Valor.Lista.end());
    }
}

} // namespace

bool Constructor_AST::Admite(const Grammar& Gramatica) {

    if (Gramatica.getSymbolName(Gramatica.start_id) != "program") return false;
    for (const auto& Regla : Gramatica.rules) {
        if (Regla.lhs != Gramatica.goal_id && !Acciones_Por_Regla().count(Produccion(Gramatica, Regla))) return false;
    }
    return true;
}

Constructor_AST::Constructor_AST(const Grammar& Gramatica, AST& Arbol_Actual) : Arbol(Arbol_Actual), Acciones(Gramatica.rules.size()) {

    for (const auto& Regla : Gramatica.rules) {
        if (Regla.lhs != Gramatica.goal_id) Acciones[Regla.id] = &Acciones_Por_Regla().at(Produccion(Gramatica, Regla));
    }
}

Valor_AST Constructor_AST::Reducir(const Rule& Regla, std::vector<Valor_AST> Valores) {

    const auto& Accion = *Acciones.at(Regla.id);
    auto Terminal = [&](int Indice) -> Valor_Terminal& { return Valores.at(Indice).Terminal; };

    switch (Accion.Construir) {
        case Construccion::Pasar:
            return std::move(Valores.at(Accion.Ancla));
        case Construccion::Variable: {
            Nodo_AST& Variable = Arbol[Valores[0].Nodo];
            Variable.Clase = K::VarDef;
            Variable.Hijos.push_back(Valores[2].Nodo);
            return Valores[0].Nodo;
        }
        case Construccion::Clase: {
            Lista_Nodos Hijos{Arbol.Agregar(K::TypeName, Terminal(3).Posicion, std::move(Terminal(3).Texto))};
            Agregar_Valor(Hijos, Valores[8]);
            return Arbol.Agregar(K::ClassDef, Terminal(0).Posicion, std::move(Terminal(1).Texto), std::move(Hijos));
        }
        case Construccion::For: {
            Id_Nodo Destino = Arbol.Agregar(K::Identifier, Terminal(1).Posicion, std::move(Terminal(1).Texto));
            return Arbol.Agregar(K::For, Terminal(0).Posicion, {}, {Destino, Valores[3].Nodo, Valores[5].Nodo});
        }
        case Construccion::Llamada_ID: {
            Lista_Nodos Hijos{Arbol.Agregar(K::Identifier, Terminal(0).Posicion, std::move(Terminal(0).Texto))};
            Agregar_Valor(Hijos, Valores[2]);
            return Arbol.Agregar(K::Call, Terminal(0).Posicion, {}, std::move(Hijos));
        }
        case Construccion::Secuencia:
        case Construccion::Crear:
            break;
    }

    Ubicacion Posicion;
    if (Accion.Ancla >= 0) {
        Valor_AST& Valor = Valores.at(Accion.Ancla);
        Posicion = Valor.Nodo ? Arbol[Valor.Nodo].Posicion : Valor.Terminal.Posicion;
    }
    Lista_Nodos Hijos;
    for (int Indice : Accion.Hijos) Agregar_Valor(Hijos, Valores.at(Indice));
    if (Accion.Construir == Construccion::Secuencia) return Hijos;

    if (Accion.Ancla < 0 && !Hijos.empty()) Posicion = Arbol[Hijos.front()].Posicion;
    std::string Texto = Accion.Texto >= 0 ? std::move(Terminal(Accion.Texto).Texto) : std::string{};
    Id_Nodo Resultado = Arbol.Agregar(Accion.Clase, Posicion, std::move(Texto), std::move(Hijos));
    if (Accion.Clase == K::Elif) {
        Lista_Nodos Lista = std::move(Valores.front().Lista);
        Lista.push_back(Resultado);
        return Lista;
    }
    return Resultado;
}

std::string Constructor_AST::Atributo(const Valor_AST& Valor) const {

    if (Valor.Nodo) {
        return "n" + std::to_string(Valor.Nodo);
    }
    if (!Valor.Lista.empty()) {
        return "lista[" + std::to_string(Valor.Lista.size()) + "]";
    }
    if (Valor.Terminal.Mostrar) {
        return Valor.Terminal.Texto.empty() ? "\"\"" : Texto_Seguimiento(Valor.Terminal.Texto);
    }
    return "–";
}

std::string Constructor_AST::Describir_Reduccion(const Rule& Regla, const Valor_AST& Valor) const {

    const Accion_AST& Accion = *Acciones.at(Regla.id);
    if (Accion.Construir == Construccion::Pasar) {
        return "$$ = $" + std::to_string(Accion.Ancla + 1) + " = " + Atributo(Valor);
    }
    if (!Valor.Nodo) {
        if (Accion.Clase == K::Elif) {
            return "$$ = agregar n" + std::to_string(Valor.Lista.back()) + " → " + Atributo(Valor);
        }
        return "$$ = lista[" + std::to_string(Valor.Lista.size()) + "]";
    }

    const Nodo_AST& Nodo = Arbol[Valor.Nodo];
    std::string Resultado = "$$ = ";
    if (Accion.Construir == Construccion::Variable) Resultado += "completar(";
    else Resultado += Nodo.Hijos.empty() ? "hoja(" : "nodo(";
    Resultado += Nombre_Nodo(Nodo.Clase);
    if (!Nodo.Texto.empty()) Resultado += " «" + Texto_Seguimiento(Nodo.Texto) + "»";
    for (size_t I = 0; I < Nodo.Hijos.size(); I++) {
        if (I == 4) {
            Resultado += ", …";
            break;
        }
        Resultado += ", n" + std::to_string(Nodo.Hijos[I]);
    }
    return Resultado + ") = " + Atributo(Valor);
}
