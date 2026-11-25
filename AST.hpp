#ifndef AST_HPP
#define AST_HPP

#include <cstddef>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

class Grammar;
struct Rule;

struct Ubicacion {

    int Linea = 1;
    int Columna = 1;
};

enum class Tipo_Nodo {
    Program, ClassDef, FunctionDef, VarDef, Parameter, TypeName, ListType,
    GlobalDecl, NonlocalDecl, Block, If, Elif, Else, While, For, Pass,
    ExprStmt, Return, Assign, Identifier, Integer, Boolean, String, None,
    List, Unary, Binary, Conditional, Member, Index, Call, Error_Sintactico, Error_Lexico
};

std::string_view Nombre_Nodo(Tipo_Nodo Clase);
std::string Texto_Seguimiento(std::string_view Texto);

using Id_Nodo = std::size_t;
using Lista_Nodos = std::vector<Id_Nodo>;

struct Nodo_AST {

    Tipo_Nodo Clase;
    Ubicacion Posicion;
    std::string Texto;
    Lista_Nodos Hijos;
    std::string Tipo;
    bool Tiene_Error = false;
    bool Incompleto = false;
    bool Omite_Declaracion = false;
};

class AST {

public:
    AST() : Nodos{{Tipo_Nodo::Program, {}, {}, {}, {}}} {}

    Id_Nodo Agregar(Tipo_Nodo Clase, Ubicacion Posicion, std::string Texto = {}, Lista_Nodos Hijos = {});
    const Nodo_AST& operator[](Id_Nodo Id) const { return Nodos.at(Id); }
    Nodo_AST& operator[](Id_Nodo Id) { return Nodos.at(Id); }
    std::size_t Cantidad_Nodos() const { return Nodos.size() - 1; }
    std::string Describir(Id_Nodo Id) const;

    Id_Nodo Raiz = 0;

private:
    std::vector<Nodo_AST> Nodos;
};

struct Valor_Terminal {

    Ubicacion Posicion;
    std::string Texto;
    bool Mostrar = false;
};

struct Valor_AST {

    Valor_Terminal Terminal;
    Id_Nodo Nodo = 0;
    Lista_Nodos Lista;

    Valor_AST() = default;
    Valor_AST(Valor_Terminal Token) : Terminal(std::move(Token)) {}
    Valor_AST(Id_Nodo Id) : Nodo(Id) {}
    Valor_AST(Lista_Nodos Elementos) : Lista(std::move(Elementos)) {}
};
struct Accion_AST;

class Constructor_AST {

public:
    Constructor_AST(const Grammar& Gramatica, AST& Arbol_Actual);
    static bool Admite(const Grammar& Gramatica);
    Valor_AST Reducir(const Rule& Regla, std::vector<Valor_AST> Valores);
    std::string Atributo(const Valor_AST& Valor) const;
    std::string Describir_Reduccion(const Rule& Regla, const Valor_AST& Valor) const;

private:
    AST& Arbol;
    std::vector<const Accion_AST*> Acciones;
};

#endif
