#include "Analisis_Semantico.hpp"
#include "Seguimiento.hpp"

#include <algorithm>
#include <utility>

namespace {

using K = Tipo_Nodo;
const std::string Tipo_Error = "<Error>";
const std::string Tipo_None = "<None>";
const std::string Tipo_Vacio = "<Empty>";

bool Es_Primitivo(const std::string& Tipo) {

    return Tipo == "int" || Tipo == "bool" || Tipo == "str";
}

bool Es_Lista(const std::string& Tipo) { return !Tipo.empty() && Tipo.front() == '['; }
std::string Tipo_Elemento(const std::string& Tipo) { return Tipo.substr(1, Tipo.size() - 2); }
std::string Formar_Lista(const std::string& Elemento) { return "[" + Elemento + "]"; }

} // namespace

Analisis_Semantico::Analisis_Semantico(AST& Arbol_Actual, Seguimiento& Seguimiento_Actual)
    : Arbol(Arbol_Actual), Traza(Seguimiento_Actual) {}

Analisis_Semantico::Ambito& Analisis_Semantico::Crear_Ambito(Clase_Ambito Clase, std::string Nombre, Ambito* Padre) {

    Ambitos.push_back({Clase, std::move(Nombre), Padre, {}, {}});
    return Ambitos.back();
}

void Analisis_Semantico::Agregar_Predefinidos() {

    for (const std::string Nombre : {"object", "int", "bool", "str"}) {
        auto& Miembros = Crear_Ambito(Clase_Ambito::Clase, "global." + Nombre, Global);
        Miembros.Nombre_Clase = Nombre;
        Miembros.Simbolos.emplace("__init__", Symbol{Clase_Symbol::Metodo, Tipo_None, 0, {Nombre}});
        Clases.emplace(Nombre, Datos_Clase{&Miembros, Nombre == "object" ? "" : "object", 0});
        Global->Simbolos.emplace(Nombre, Symbol{Clase_Symbol::Clase, Nombre});
    }
    Global->Simbolos.emplace("print", Symbol{Clase_Symbol::Funcion, Tipo_None, 0, {"object"}});
    Global->Simbolos.emplace("len", Symbol{Clase_Symbol::Funcion, "int", 0, {"object"}});
    Global->Simbolos.emplace("input", Symbol{Clase_Symbol::Funcion, "str"});
}

void Analisis_Semantico::Analizar() {

    for (const auto& Error : Traza.Errores) {
        if ((Error.Fase == "Sintáctico" || Error.Fase == "Léxico") && Error.Posicion.Linea > 0) {
            Lineas_Danadas.insert(Error.Posicion.Linea);
        }
    }
    if (!Lineas_Danadas.empty()) {
        Lista_Nodos Orden{Arbol.Raiz};
        for (size_t I = 0; I < Orden.size(); I++) {
            const auto& Hijos = Arbol[Orden[I]].Hijos;
            Orden.insert(Orden.end(), Hijos.begin(), Hijos.end());
        }
        for (auto I = Orden.rbegin(); I != Orden.rend(); ++I) {
            auto& Nodo = Arbol[*I];
            if (Fragmento_Danado(*I)) Nodo.Incompleto = true;
            for (Id_Nodo Hijo : Nodo.Hijos) {
                if (Arbol[Hijo].Incompleto) Nodo.Incompleto = true;
            }
        }
    }
    const auto& Programa = Arbol[Arbol.Raiz];
    Global = &Crear_Ambito(Clase_Ambito::Global, "global", nullptr);
    Agregar_Predefinidos();
    for (Id_Nodo Id : Programa.Hijos) {
        if (Arbol[Id].Clase == K::ClassDef && !Fragmento_Danado(Id)) Declarar_Clase(Id);
    }
    Preparar_Declaraciones(Programa.Hijos, *Global);
    for (Id_Nodo Id : Programa.Hijos) {
        if (Arbol[Id].Clase == K::ClassDef) Preparar_Clase(Id);
        if (Arbol[Id].Clase == K::FunctionDef && !Fragmento_Danado(Id)) Preparar_Funcion(Id, *Global);
    }
    Comprobar_Bloque(Programa.Hijos, *Global, nullptr);
}

void Analisis_Semantico::Registrar(Id_Nodo Id, const Ambito& Actual, std::string_view Accion, std::string_view Detalle) {

    Traza.Registrar("Semántico", Actual.Nombre, Arbol.Describir(Id), "", Accion, Detalle, "", Id,
                    Arbol[Id].Posicion.Linea);
}

std::string Analisis_Semantico::Registrar_Error(Id_Nodo Id, const Ambito& Actual, std::string Mensaje) {

    auto& Nodo = Arbol[Id];
    Nodo.Tiene_Error = true;
    Registrar(Id, Actual, "Error", Mensaje);
    Traza.Errores.push_back({"Semántico", Nodo.Posicion, std::move(Mensaje), Id});
    return Tipo_Error;
}

bool Analisis_Semantico::Declarar(Id_Nodo Id, Ambito& Actual, Symbol Simbolo) {

    const auto& Nombre = Arbol[Id].Texto;
    if (Buscar_Local(Actual, Nombre)) {
        Registrar_Error(Id, Actual, "Redeclaracion de '" + Nombre + "' en el mismo ambito");
        return false;
    }
    if (Simbolo.Clase != Clase_Symbol::Clase && Clases.count(Nombre)) {
        Registrar_Error(Id, Actual, "El nombre '" + Nombre + "' esta reservado para una clase");
        return false;
    }
    Registrar(Id, Actual, "Declarar", std::string(Nombre_Clase_Symbol(Simbolo.Clase)) + " " + Nombre + " : " + Tipo_Symbol(Simbolo));
    Actual.Simbolos.emplace(Nombre, std::move(Simbolo));
    return true;
}

void Analisis_Semantico::Declarar_Clase(Id_Nodo Id) {

    auto& Nodo = Arbol[Id];
    if (!Declarar(Id, *Global, {Clase_Symbol::Clase, Nodo.Texto, Id})) return;
    auto& Actual = Crear_Ambito(Clase_Ambito::Clase, "global." + Nodo.Texto, Global);
    Actual.Nombre_Clase = Nodo.Texto;
    Ambito_Por_Nodo[Id] = &Actual;
    Clases.emplace(Nodo.Texto, Datos_Clase{&Actual, "object", Id});
    Nodo.Tipo = Nodo.Texto;
}

void Analisis_Semantico::Preparar_Clase(Id_Nodo Id) {

    auto Ambito_Encontrado = Ambito_Por_Nodo.find(Id);
    if (Ambito_Encontrado == Ambito_Por_Nodo.end()) return;
    Ambito& Actual = *Ambito_Encontrado->second;
    const auto& Nodo = Arbol[Id];
    auto& Datos = Clases.at(Nodo.Texto);
    auto& Base = Arbol[Nodo.Hijos.front()];
    auto Padre = Clases.find(Base.Texto);
    if (Padre == Clases.end()) {
        if (Global->Declaraciones_Incompletas) {
            Base.Tipo = Tipo_Error;
            Actual.Declaraciones_Incompletas = true;
        } else {
            Base.Tipo = Registrar_Error(Nodo.Hijos.front(), Actual, "Superclase desconocida '" + Base.Texto + "'");
        }
    }

    else if (Es_Primitivo(Base.Texto)) {
        Base.Tipo = Registrar_Error(Nodo.Hijos.front(), Actual, "No se puede heredar de '" + Base.Texto + "'");
    }

    else if (Padre->second.Nodo && Arbol[Padre->second.Nodo].Posicion.Linea >= Nodo.Posicion.Linea) {
        Base.Tipo = Registrar_Error(Nodo.Hijos.front(), Actual, "La superclase '" + Base.Texto + "' debe declararse antes de '" + Nodo.Texto + "'");
    }

    else {
        Base.Tipo = Datos.Padre = Base.Texto;
        Registrar(Nodo.Hijos.front(), Actual, "Resolver superclase", Base.Tipo);
    }
    Preparar_Declaraciones(Nodo.Hijos, Actual);
    for (Id_Nodo Hijo : Nodo.Hijos) {
        if (Arbol[Hijo].Clase == K::FunctionDef && !Fragmento_Danado(Hijo)) Preparar_Funcion(Hijo, Actual);
    }
    for (const auto& [Nombre, Simbolo] : Actual.Simbolos) {
        if (Simbolo.Clase == Clase_Symbol::Metodo &&
            (Simbolo.Parametros.empty() || (Simbolo.Parametros.front() != Tipo_Error &&
             Simbolo.Parametros.front() != Nodo.Texto))) {

            Registrar_Error(Simbolo.Nodo, Actual, "El primer parametro del metodo debe ser de tipo '" + Nodo.Texto + "'");
        }
        const Ambito* Ambito_Heredado = nullptr;
        const Symbol* Heredado = Buscar_Miembro(Datos.Padre, Nombre, Ambito_Heredado);
        if (!Heredado) continue;
        if (Heredado->Clase != Clase_Symbol::Metodo || Simbolo.Clase != Clase_Symbol::Metodo) {
            Registrar_Error(Simbolo.Nodo, Actual, "El miembro '" + Nombre + "' entra en conflicto con un miembro heredado");
            continue;
        }
        const auto& Parametros_Base = Heredado->Parametros;
        const auto& Parametros_Hijo = Simbolo.Parametros;
        if (Parametros_Base.empty() || Parametros_Hijo.empty()) continue;
        bool Iguales = Simbolo.Tipo == Heredado->Tipo || Simbolo.Tipo == Tipo_Error || Heredado->Tipo == Tipo_Error;
        Iguales = Iguales && Parametros_Hijo.size() == Parametros_Base.size();
        for (size_t I = 1; Iguales && I < Parametros_Hijo.size(); ++I) {
            Iguales = Parametros_Hijo[I] == Parametros_Base[I] || Parametros_Hijo[I] == Tipo_Error || Parametros_Base[I] == Tipo_Error;
        }
        if (!Iguales) Registrar_Error(Simbolo.Nodo, Actual, "Firma incompatible al redefinir el metodo heredado '" + Nombre + "'");
    }
}

std::string Analisis_Semantico::Tipo_Anotado(Id_Nodo Id, const Ambito& Actual) {

    auto& Nodo = Arbol[Id];
    if (Nodo.Clase == K::ListType) {
        auto Elemento = Tipo_Anotado(Nodo.Hijos.front(), Actual);
        Nodo.Tipo = Elemento == Tipo_Error ? Tipo_Error : Formar_Lista(Elemento);
    }

    else if (Clases.count(Nodo.Texto)) {
        Nodo.Tipo = Nodo.Texto;
    }

    else {
        Nodo.Tipo = Global->Declaraciones_Incompletas ? Tipo_Error
                  : Registrar_Error(Id, Actual, "Tipo desconocido '" + Nodo.Texto + "'");
    }
    return Nodo.Tipo;
}

Analisis_Semantico::Symbol Analisis_Semantico::Funcion_Symbol(Id_Nodo Id, Ambito& Actual) {

    auto& Nodo = Arbol[Id];
    bool Metodo = Actual.Clase == Clase_Ambito::Clase;
    Symbol Simbolo{Metodo ? Clase_Symbol::Metodo : Clase_Symbol::Funcion, Tipo_None, Id};
    for (Id_Nodo Parte : Nodo.Hijos) {
        auto& Hijo = Arbol[Parte];
        if (Hijo.Clase == K::Parameter) {
            Hijo.Tipo = Tipo_Anotado(Hijo.Hijos.front(), Actual);
            Simbolo.Parametros.push_back(Hijo.Tipo);
        }

        else if (Hijo.Clase == K::TypeName || Hijo.Clase == K::ListType) {
            Simbolo.Tipo = Tipo_Anotado(Parte, Actual);
            if (Metodo && Nodo.Texto == "__init__") {
                Registrar_Error(Parte, Actual, "El metodo __init__ debe omitir la anotacion de retorno");
                Simbolo.Tipo = Tipo_Error;
            }
        }

        else {
            break;
        }
    }
    Nodo.Tipo = Simbolo.Tipo;
    return Simbolo;
}

void Analisis_Semantico::Preparar_Declaraciones(const Lista_Nodos& Nodos, Ambito& Actual) {

    for (Id_Nodo Id : Nodos) {
        const auto& Nodo = Arbol[Id];
        bool Es_Declaracion = Nodo.Clase == K::VarDef || Nodo.Clase == K::ClassDef || Nodo.Clase == K::FunctionDef ||
                             Nodo.Clase == K::GlobalDecl || Nodo.Clase == K::NonlocalDecl;
        if (Nodo.Omite_Declaracion || (Es_Declaracion && Fragmento_Danado(Id))) {
            Actual.Declaraciones_Incompletas = true;
            break;
        }
    }
    bool Miembros = Actual.Clase == Clase_Ambito::Clase;
    for (Id_Nodo Id : Nodos) {
        if (Fragmento_Danado(Id)) continue;
        auto& Nodo = Arbol[Id];
        switch (Nodo.Clase) {
            case K::Parameter:
                Declarar(Id, Actual, {Clase_Symbol::Parametro, Nodo.Tipo, Id});
                break;
            case K::VarDef:
            case K::FunctionDef: {
                Symbol Simbolo{Miembros ? Clase_Symbol::Atributo : Clase_Symbol::Variable, {}, Id};
                if (Nodo.Clase == K::FunctionDef) {
                    Simbolo = Funcion_Symbol(Id, Actual);
                }

                else {
                    Nodo.Tipo = Tipo_Anotado(Nodo.Hijos.front(), Actual);
                    Simbolo.Tipo = Nodo.Tipo;
                }
                Declarar(Id, Actual, std::move(Simbolo));
                break;
            }
            case K::GlobalDecl:
            case K::NonlocalDecl:
                Declarar_Alias(Id, Actual);
                break;
            default:
                break;
        }
    }
}

void Analisis_Semantico::Preparar_Funcion(Id_Nodo Id, Ambito& Exterior) {

    const auto& Nodo = Arbol[Id];
    Ambito* Padre_Lexico = Exterior.Clase == Clase_Ambito::Clase ? Global : &Exterior;
    auto& Actual = Crear_Ambito(Clase_Ambito::Funcion, Exterior.Nombre + "." + Nodo.Texto, Padre_Lexico);
    Ambito_Por_Nodo[Id] = &Actual;
    Preparar_Declaraciones(Nodo.Hijos, Actual);
    for (Id_Nodo Hijo : Nodo.Hijos) {
        if (Arbol[Hijo].Clase == K::FunctionDef && !Fragmento_Danado(Hijo)) Preparar_Funcion(Hijo, Actual);
    }
}

bool Analisis_Semantico::Fragmento_Danado(Id_Nodo Id) const {

    const auto& Nodo = Arbol[Id];
    return Nodo.Clase == K::Error_Sintactico || Nodo.Clase == K::Error_Lexico ||
           Lineas_Danadas.count(Nodo.Posicion.Linea);
}

void Analisis_Semantico::Declarar_Alias(Id_Nodo Id, Ambito& Actual) {

    auto& Nodo = Arbol[Id];
    bool Es_Global = Nodo.Clase == K::GlobalDecl;
    const Ambito* Origen = Global;
    const Symbol* Destino = Es_Global ? Buscar_Local(*Global, Nodo.Texto) : Buscar(*Actual.Padre, Nodo.Texto, Origen);
    bool Valido = Destino && (Es_Global ? Destino->Clase == Clase_Symbol::Variable : Origen != Global &&
                       (Destino->Clase == Clase_Symbol::Variable || Destino->Clase == Clase_Symbol::Parametro ||
                        Destino->Clase == Clase_Symbol::Nonlocal));
    Nodo.Tipo = Valido ? Destino->Tipo : Tipo_Error;
    Symbol Alias{Es_Global ? Clase_Symbol::Global : Clase_Symbol::Nonlocal, Nodo.Tipo, Id};
    if (Valido) Alias.Origen = Destino->Origen ? Destino->Origen : Origen;
    if (Declarar(Id, Actual, std::move(Alias))) {
        if (!Valido) {
            if (Destino || !Origen || !Origen->Declaraciones_Incompletas) {
                Registrar_Error(Id, Actual, "'" + Nodo.Texto + (Es_Global ? "' no designa una variable global" :
                    "' no designa una variable de una funcion envolvente"));
            }
        }

        else {
            Registrar(Id, Actual, "Vincular", Nodo.Texto + " -> " + Origen->Nombre);
        }
    }
}

const Analisis_Semantico::Symbol* Analisis_Semantico::Buscar_Local(const Ambito& Actual, const std::string& Nombre) {

    auto Encontrado = Actual.Simbolos.find(Nombre);
    return Encontrado == Actual.Simbolos.end() ? nullptr : &Encontrado->second;
}

const Analisis_Semantico::Symbol* Analisis_Semantico::Buscar(const Ambito& Actual, const std::string& Nombre,
                                                          const Ambito*& Origen) const {
    for (Origen = &Actual; Origen; Origen = Origen->Padre) {
        if (const Symbol* Simbolo = Buscar_Local(*Origen, Nombre)) return Simbolo;
        if (Origen->Declaraciones_Incompletas) return nullptr;
    }
    return nullptr;
}

const Analisis_Semantico::Symbol* Analisis_Semantico::Buscar_Miembro(const std::string& Tipo, const std::string& Nombre,
                                                          const Ambito*& Origen) const {
    for (auto Encontrado = Clases.find(Tipo); Encontrado != Clases.end(); Encontrado = Clases.find(Encontrado->second.Padre)) {
        Origen = Encontrado->second.Miembros;
        if (const Symbol* Simbolo = Buscar_Local(*Origen, Nombre)) return Simbolo;
        if (Origen->Declaraciones_Incompletas) return nullptr;
    }
    return nullptr;
}

bool Analisis_Semantico::Es_Invocable(Clase_Symbol Clase) {

    return Clase == Clase_Symbol::Funcion || Clase == Clase_Symbol::Metodo || Clase == Clase_Symbol::Clase;
}

const char* Analisis_Semantico::Nombre_Clase_Symbol(Clase_Symbol Clase) {

    switch (Clase) {
        case Clase_Symbol::Variable: return "variable";
        case Clase_Symbol::Parametro: return "parametro";
        case Clase_Symbol::Clase: return "clase";
        case Clase_Symbol::Atributo: return "atributo";
        case Clase_Symbol::Funcion: return "funcion";
        case Clase_Symbol::Metodo: return "metodo";
        case Clase_Symbol::Global: return "global";
        case Clase_Symbol::Nonlocal: return "nonlocal";
    }
    return "";
}

std::string Analisis_Semantico::Tipo_Symbol(const Symbol& Simbolo) {

    if (!Es_Invocable(Simbolo.Clase)) return Simbolo.Tipo;
    std::string Resultado = "(";
    for (size_t I = 0; I < Simbolo.Parametros.size(); ++I) {
        if (I) Resultado += ", ";
        Resultado += Simbolo.Parametros[I];
    }
    return Resultado + ") -> " + Simbolo.Tipo;
}

bool Analisis_Semantico::Es_Compatible(const std::string& Obtenido, const std::string& Esperado) const {

    if (Obtenido == Esperado || Esperado == "object") return true;
    if (Obtenido == Tipo_None) return !Es_Primitivo(Esperado) && Esperado != Tipo_Vacio;
    if (Es_Lista(Esperado)) {
        return Obtenido == Tipo_Vacio || (Obtenido == "[<None>]" && Es_Compatible(Tipo_None, Tipo_Elemento(Esperado)));
    }
    for (auto Encontrado = Clases.find(Obtenido); Encontrado != Clases.end(); Encontrado = Clases.find(Encontrado->second.Padre)) {
        if (Encontrado->second.Padre == Esperado) return true;
    }
    return false;
}

std::string Analisis_Semantico::Unir_Tipos(const std::string& Izquierdo, const std::string& Derecho) const {

    if (Izquierdo == Tipo_Error || Derecho == Tipo_Error) return Tipo_Error;
    if (Es_Compatible(Izquierdo, Derecho)) return Derecho;
    if (Es_Compatible(Derecho, Izquierdo)) return Izquierdo;
    for (auto Encontrado = Clases.find(Izquierdo); Encontrado != Clases.end(); Encontrado = Clases.find(Encontrado->second.Padre)) {
        if (Es_Compatible(Derecho, Encontrado->first)) return Encontrado->first;
    }
    return "object";
}

bool Analisis_Semantico::Comprobar_Tipo(Id_Nodo Id, const Ambito& Actual, const std::string& Obtenido,
                                const std::string& Esperado, const std::string& Contexto) {
    if (Obtenido == Tipo_Error || Esperado == Tipo_Error) return false;
    if (Es_Compatible(Obtenido, Esperado)) {
        Registrar(Id, Actual, "Comprobar " + Contexto, Obtenido + " compatible con " + Esperado);
        return true;
    }
    Registrar_Error(Id, Actual, Contexto + ": se esperaba " + Esperado + " y se obtuvo " + Obtenido);
    return false;
}

bool Analisis_Semantico::Comprobar_Condicion(Id_Nodo Id, const Ambito& Actual) {

    const auto& Tipo = Comprobar_Expresion(Id, Actual);
    return Comprobar_Tipo(Id, Actual, Tipo, "bool", "Condicion");
}

bool Analisis_Semantico::Comprobar_Bloque(const Lista_Nodos& Nodos, const Ambito& Actual, const std::string* Tipo_Retorno) {

    bool Siempre_Retorna = false;
    for (Id_Nodo Id : Nodos) {
        if (Comprobar_Sentencia(Id, Actual, Tipo_Retorno)) Siempre_Retorna = true;
    }
    return Siempre_Retorna;
}

bool Analisis_Semantico::Comprobar_Sentencia(Id_Nodo Id, const Ambito& Actual, const std::string* Tipo_Retorno) {

    // Un token de la línea rechazada puede reconocerse de nuevo al sincronizar la pila.
    // Los cuerpos de bloque conservan las sentencias vecinas que sí se pudieron recuperar.
    if (Arbol[Id].Clase != K::Block && Fragmento_Danado(Id)) return false;
    auto& Nodo = Arbol[Id];
    const auto& Hijos = Nodo.Hijos;
    switch (Nodo.Clase) {
        case K::ClassDef: {
            auto Encontrado = Ambito_Por_Nodo.find(Id);
            if (Encontrado != Ambito_Por_Nodo.end()) Comprobar_Bloque(Hijos, *Encontrado->second, nullptr);
            break;
        }
        case K::FunctionDef: {
            const auto& Cuerpo = *Ambito_Por_Nodo.at(Id);
            if (!Comprobar_Bloque(Hijos, Cuerpo, &Nodo.Tipo) && Es_Primitivo(Nodo.Tipo) && !Nodo.Incompleto) {
                Registrar_Error(Id, Cuerpo, "Falta un return de tipo " + Nodo.Tipo + " en algun camino de '" + Nodo.Texto + "'");
            }
            break;
        }
        case K::VarDef:
            Comprobar_Tipo(Id, Actual, Comprobar_Expresion(Hijos[1], Actual), Nodo.Tipo, "Inicializacion de '" + Nodo.Texto + "'");
            break;
        case K::Block:
            return Comprobar_Bloque(Hijos, Actual, Tipo_Retorno);
        case K::If: {
            Comprobar_Condicion(Hijos[0], Actual);
            bool Todos = Comprobar_Sentencia(Hijos[1], Actual, Tipo_Retorno);
            for (size_t I = 2; I < Hijos.size(); ++I) {
                if (!Comprobar_Sentencia(Hijos[I], Actual, Tipo_Retorno)) Todos = false;
            }
            return Todos && Arbol[Hijos.back()].Clase == K::Else;
        }
        case K::Elif:
            Comprobar_Condicion(Hijos[0], Actual);
            return Comprobar_Sentencia(Hijos[1], Actual, Tipo_Retorno);
        case K::Else:
            return Comprobar_Sentencia(Hijos[0], Actual, Tipo_Retorno);
        case K::While: {
            Comprobar_Condicion(Hijos[0], Actual);
            bool Retorna = Comprobar_Sentencia(Hijos[1], Actual, Tipo_Retorno);
            const auto& Condicion = Arbol[Hijos[0]];
            return Retorna && Condicion.Clase == K::Boolean && Condicion.Texto == "True";
        }
        case K::For: {
            const auto& Destino = Comprobar_Expresion(Hijos[0], Actual, Uso::Destino);
            const auto& Iterable = Comprobar_Expresion(Hijos[1], Actual);
            if (Iterable == "str" || Es_Lista(Iterable)) {
                Comprobar_Tipo(Hijos[0], Actual, Iterable == "str" ? Iterable : Tipo_Elemento(Iterable), Destino, "Variable del for");
            }

            else if (Iterable != Tipo_Error) {
                Registrar_Error(Hijos[1], Actual, "El for requiere una lista tipada o str, se obtuvo " + Iterable);
            }
            Comprobar_Sentencia(Hijos[2], Actual, Tipo_Retorno);
            break;
        }
        case K::ExprStmt:
            Comprobar_Expresion(Hijos.front(), Actual);
            break;
        case K::Return:
            Nodo.Tipo = Hijos.empty() ? Tipo_None : Comprobar_Expresion(Hijos.front(), Actual);
            if (!Tipo_Retorno) Registrar_Error(Id, Actual, "Return fuera de una funcion o metodo");
            else Comprobar_Tipo(Id, Actual, Nodo.Tipo, *Tipo_Retorno, "Tipo de retorno");
            return true;
        case K::Assign: {
            Nodo.Tipo = Comprobar_Expresion(Hijos.back(), Actual);
            if (Hijos.size() > 2 && Nodo.Tipo == "[<None>]") {
                Registrar_Error(Id, Actual, "Una asignacion multiple no admite un valor de tipo [<None>]");
            }
            for (size_t I = 0; I + 1 < Hijos.size(); ++I) {
                const auto& Destino = Comprobar_Expresion(Hijos[I], Actual, Uso::Destino);
                Comprobar_Tipo(Hijos[I], Actual, Nodo.Tipo, Destino, "Asignacion");
            }
            break;
        }
        default:
            break;
    }
    return false;
}

const std::string& Analisis_Semantico::Comprobar_Expresion(Id_Nodo Id, const Ambito& Actual, Uso Uso_Nodo) {

    struct Marco { Id_Nodo Nodo; Uso Uso_Nodo; bool Listo; };
    std::vector<Marco> Pila{{Id, Uso_Nodo, false}};
    while (!Pila.empty()) {
        const Marco Actual_Marco = Pila.back();
        auto& Nodo = Arbol[Actual_Marco.Nodo];
        if (!Actual_Marco.Listo && !Nodo.Hijos.empty()) {
            Pila.back().Listo = true;
            for (size_t I = Nodo.Hijos.size(); I > 0; --I) {
                bool Invocable = Nodo.Clase == K::Call && I == 1;
                Pila.push_back({Nodo.Hijos[I - 1], Invocable ? Uso::Invocable : Uso::Valor, false});
            }
        }

        else {
            Nodo.Tipo = Inferir(Actual_Marco.Nodo, Actual, Actual_Marco.Uso_Nodo);
            Registrar(Actual_Marco.Nodo, Actual, "Tipo inferido", Nodo.Tipo);
            Pila.pop_back();
        }
    }
    return Arbol[Id].Tipo;
}

const Analisis_Semantico::Symbol* Analisis_Semantico::Resolver_Nodo(Id_Nodo Id, const Ambito& Actual,
                                                               const Ambito*& Origen) const {
    const auto& Nodo = Arbol[Id];
    if (Nodo.Clase == K::Member) return Buscar_Miembro(Arbol[Nodo.Hijos.front()].Tipo, Nodo.Texto, Origen);
    return Buscar(Actual, Nodo.Texto, Origen);
}

std::string Analisis_Semantico::Inferir_Nombre(Id_Nodo Id, const Ambito& Actual, Uso Uso_Nodo) {

    const auto& Nodo = Arbol[Id];
    bool Es_Miembro = Nodo.Clase == K::Member;
    const std::string Receptor = Es_Miembro ? Arbol[Nodo.Hijos.front()].Tipo : "";
    if (Receptor == Tipo_Error) return Tipo_Error;
    const Ambito* Origen = nullptr;
    const Symbol* Simbolo = Resolver_Nodo(Id, Actual, Origen);
    if (!Simbolo) {
        if (Origen && Origen->Declaraciones_Incompletas) return Tipo_Error;
        if (Es_Miembro) return Registrar_Error(Id, Actual, "El tipo " + Receptor + " no tiene un miembro '" + Nodo.Texto + "'");
        return Registrar_Error(Id, Actual, "Identificador '" + Nodo.Texto + "' no declarado");
    }
    if (Simbolo->Tipo == Tipo_Error) return Tipo_Error;
    if (Uso_Nodo == Uso::Invocable && !Es_Invocable(Simbolo->Clase)) {
        return Registrar_Error(Id, Actual, "'" + Nodo.Texto + "' no se puede invocar");
    }
    if (Uso_Nodo != Uso::Invocable && Es_Invocable(Simbolo->Clase)) {
        return Registrar_Error(Id, Actual, "'" + Nodo.Texto + "' " + (Uso_Nodo == Uso::Destino ? "no es un destino asignable" :
            "solo se puede usar en una llamada"));
    }
    if (Uso_Nodo == Uso::Destino && !Es_Miembro && Origen != &Actual) {
        return Registrar_Error(Id, Actual, "Para modificar '" + Nodo.Texto + "' de un ambito externo se requiere global o nonlocal");
    }
    if (Simbolo->Origen) {
        Origen = Simbolo->Origen;
    }
    Registrar(Id, Actual, "Resolver '" + Nodo.Texto + "'", Origen->Nombre + " : " + Tipo_Symbol(*Simbolo));
    return Tipo_Symbol(*Simbolo);
}

std::string Analisis_Semantico::Inferir_Llamada(Id_Nodo Id, const Ambito& Actual) {

    const auto& Hijos = Arbol[Id].Hijos;
    const auto& Invocable = Arbol[Hijos.front()];
    if (Invocable.Tipo == Tipo_Error) return Tipo_Error;
    const Ambito* Origen = nullptr;
    const Symbol& Simbolo = *Resolver_Nodo(Hijos.front(), Actual, Origen);
    size_t Inicio = Simbolo.Clase == Clase_Symbol::Metodo && !Simbolo.Parametros.empty() ? 1 : 0;
    size_t Esperado = Simbolo.Parametros.size() - Inicio;
    size_t Obtenido = Hijos.size() - 1;
    bool Correcto = Esperado == Obtenido;
    if (!Correcto) {
        Registrar_Error(Id, Actual, "'" + Invocable.Texto + "' espera " + std::to_string(Esperado) +
                         " argumento(s) y recibio " + std::to_string(Obtenido));
    }
    for (size_t I = 0; I < std::min(Obtenido, Esperado); ++I) {
        Id_Nodo Argumento = Hijos[I + 1];
        if (!Comprobar_Tipo(Argumento, Actual, Arbol[Argumento].Tipo, Simbolo.Parametros[I + Inicio],
                    "Argumento " + std::to_string(I + 1) + " de '" + Invocable.Texto + "'")) Correcto = false;
    }
    return Correcto ? Simbolo.Tipo : Tipo_Error;
}

std::string Analisis_Semantico::Inferir_Binaria(Id_Nodo Id, const Ambito& Actual) {

    const auto& Nodo = Arbol[Id];
    const auto& Izquierdo = Arbol[Nodo.Hijos[0]].Tipo;
    const auto& Derecho = Arbol[Nodo.Hijos[1]].Tipo;
    const auto& Operador = Nodo.Texto;
    if (Izquierdo == Tipo_Error || Derecho == Tipo_Error) return Tipo_Error;
    bool Son_Enteros = Izquierdo == "int" && Derecho == "int";
    if (Operador == "+") {
        if (Son_Enteros || (Izquierdo == "str" && Derecho == "str")) return Izquierdo;
        if (Es_Lista(Izquierdo) && Es_Lista(Derecho)) return Formar_Lista(Unir_Tipos(Tipo_Elemento(Izquierdo), Tipo_Elemento(Derecho)));
    }

    else if (Operador == "-" || Operador == "*" || Operador == "//" || Operador == "%") {
        if (Son_Enteros) return "int";
    }

    else if (Operador == "<" || Operador == "<=" || Operador == ">" || Operador == ">=") {
        if (Son_Enteros) return "bool";
    }

    else if (Operador == "==" || Operador == "!=") {
        if (Es_Primitivo(Izquierdo) && Izquierdo == Derecho) return "bool";
    }

    else if (Operador == "is") {
        if (!Es_Primitivo(Izquierdo) && !Es_Primitivo(Derecho)) return "bool";
    }

    else if (Izquierdo == "bool" && Derecho == "bool") {
        return "bool"; // and, or
    }
    return Registrar_Error(Id, Actual, "El operador '" + Operador + "' no admite " + Izquierdo + " y " + Derecho);
}

std::string Analisis_Semantico::Inferir(Id_Nodo Id, const Ambito& Actual, Uso Uso_Nodo) {

    const auto& Nodo = Arbol[Id];
    const auto& Hijos = Nodo.Hijos;
    switch (Nodo.Clase) {
        case K::Integer: return "int";
        case K::Boolean: return "bool";
        case K::String: return "str";
        case K::None: return Tipo_None;
        case K::Identifier:
        case K::Member:
            return Inferir_Nombre(Id, Actual, Uso_Nodo);
        case K::Call:
            return Inferir_Llamada(Id, Actual);
        case K::Binary:
            return Inferir_Binaria(Id, Actual);
        case K::Unary: {
            const std::string Esperado = Nodo.Texto == "not" ? "bool" : "int";
            const auto& Operando = Arbol[Hijos.front()].Tipo;
            return Comprobar_Tipo(Id, Actual, Operando, Esperado, "Operando de '" + Nodo.Texto + "'") ? Esperado : Tipo_Error;
        }
        case K::List: {
            if (Hijos.empty()) return Tipo_Vacio;
            std::string Elemento = Arbol[Hijos.front()].Tipo;
            for (size_t I = 1; I < Hijos.size(); ++I) Elemento = Unir_Tipos(Elemento, Arbol[Hijos[I]].Tipo);
            return Elemento == Tipo_Error ? Tipo_Error : Formar_Lista(Elemento);
        }
        case K::Conditional: {
            const auto& Condicion = Arbol[Hijos[0]].Tipo;
            if (!Comprobar_Tipo(Hijos[0], Actual, Condicion, "bool", "Condicion")) return Tipo_Error;
            return Unir_Tipos(Arbol[Hijos[1]].Tipo, Arbol[Hijos[2]].Tipo);
        }
        case K::Index: {
            const auto& Receptor = Arbol[Hijos[0]].Tipo;
            const auto& Indice = Arbol[Hijos[1]].Tipo;
            bool Indice_Valido = Comprobar_Tipo(Hijos[1], Actual, Indice, "int", "Tipo del indice");
            if (Receptor == Tipo_Error) return Tipo_Error;
            if (Receptor == "str" && Uso_Nodo == Uso::Destino) return Registrar_Error(Id, Actual, "No se puede asignar a un indice de str");
            if (Receptor != "str" && !Es_Lista(Receptor)) return Registrar_Error(Id, Actual, "No se puede indexar un valor de tipo " + Receptor);
            if (!Indice_Valido) return Tipo_Error;
            return Receptor == "str" ? Receptor : Tipo_Elemento(Receptor);
        }
        default:
            return Registrar_Error(Id, Actual, "Expresion sin regla de tipos");
    }
}

std::vector<Tabla_Ambito> Analisis_Semantico::Obtener_Tablas() {

    std::vector<Tabla_Ambito> Tablas;

    for (Ambito& Actual : Ambitos) {
        Tabla_Ambito Tabla;
        Tabla.Nombre = Actual.Nombre;
        if (Actual.Padre) {
            Tabla.Padre = Actual.Padre->Nombre;
        }
        if (Actual.Clase == Clase_Ambito::Clase) {
            Tabla.Superclase = Clases.at(Actual.Nombre_Clase).Padre;
        }

        for (auto& [Nombre, Simbolo] : Actual.Simbolos) {
            std::string Declaracion = "Predefinido";
            if (Simbolo.Nodo) {
                Ubicacion Posicion = Arbol[Simbolo.Nodo].Posicion;
                Declaracion = std::to_string(Posicion.Linea) + ":" + std::to_string(Posicion.Columna);
            }
            std::string Origen = Simbolo.Origen ? Simbolo.Origen->Nombre : "";
            Tabla.Simbolos.push_back({Nombre, Nombre_Clase_Symbol(Simbolo.Clase),
                                    Tipo_Symbol(Simbolo), Declaracion, Origen});
        }

        Tablas.push_back(std::move(Tabla));
    }

    return Tablas;
}
