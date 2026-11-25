#ifndef ANALISIS_SEMANTICO_HPP
#define ANALISIS_SEMANTICO_HPP

#include "AST.hpp"

#include <deque>
#include <map>
#include <set>
#include <string>
#include <vector>

class Seguimiento;

struct Fila_Symbol {

    std::string Nombre;
    std::string Categoria;
    std::string Tipo;
    std::string Declaracion;
    std::string Origen;
};

struct Tabla_Ambito {

    std::string Nombre;
    std::string Padre;
    std::string Superclase;
    std::vector<Fila_Symbol> Simbolos;
};

class Analisis_Semantico {

public:
    Analisis_Semantico(AST& Arbol_Actual, Seguimiento& Seguimiento_Actual);
    void Analizar();
    std::vector<Tabla_Ambito> Obtener_Tablas();

private:
    enum class Clase_Symbol { Variable, Parametro, Clase, Atributo, Funcion, Metodo, Global, Nonlocal };
    enum class Clase_Ambito { Global, Clase, Funcion };
    enum class Uso { Valor, Destino, Invocable };

    struct Ambito;
    struct Symbol {

        Clase_Symbol Clase;
        std::string Tipo;
        Id_Nodo Nodo = 0;
        std::vector<std::string> Parametros = {};
        const Ambito* Origen = nullptr;
    };

    struct Ambito {

        Clase_Ambito Clase;
        std::string Nombre;
        Ambito* Padre;
        std::map<std::string, Symbol> Simbolos;
        std::string Nombre_Clase;
        bool Declaraciones_Incompletas = false;
    };

    struct Datos_Clase {

        Ambito* Miembros;
        std::string Padre;
        Id_Nodo Nodo;
    };

    AST& Arbol;
    Seguimiento& Traza;
    Ambito* Global = nullptr;
    std::deque<Ambito> Ambitos;
    std::map<std::string, Datos_Clase> Clases;
    std::map<Id_Nodo, Ambito*> Ambito_Por_Nodo;
    std::set<int> Lineas_Danadas;

    Ambito& Crear_Ambito(Clase_Ambito Clase, std::string Nombre, Ambito* Padre);
    void Agregar_Predefinidos();
    void Preparar_Declaraciones(const Lista_Nodos& Nodos, Ambito& Actual);
    void Declarar_Clase(Id_Nodo Nodo);
    void Preparar_Clase(Id_Nodo Nodo);
    void Preparar_Funcion(Id_Nodo Nodo, Ambito& Exterior);
    void Declarar_Alias(Id_Nodo Nodo, Ambito& Actual);
    bool Declarar(Id_Nodo Nodo, Ambito& Actual, Symbol Simbolo);
    Symbol Funcion_Symbol(Id_Nodo Nodo, Ambito& Actual);
    std::string Tipo_Anotado(Id_Nodo Nodo, const Ambito& Actual);
    bool Fragmento_Danado(Id_Nodo Id) const;

    const Symbol* Buscar(const Ambito& Actual, const std::string& Nombre, const Ambito*& Origen) const;
    const Symbol* Buscar_Miembro(const std::string& Tipo, const std::string& Nombre, const Ambito*& Origen) const;
    static const Symbol* Buscar_Local(const Ambito& Actual, const std::string& Nombre);
    static bool Es_Invocable(Clase_Symbol Clase);
    static std::string Tipo_Symbol(const Symbol& Simbolo);
    static const char* Nombre_Clase_Symbol(Clase_Symbol Clase);

    bool Es_Compatible(const std::string& Obtenido, const std::string& Esperado) const;
    std::string Unir_Tipos(const std::string& Izquierdo, const std::string& Derecho) const;
    bool Comprobar_Tipo(Id_Nodo Nodo, const Ambito& Actual, const std::string& Obtenido,
                const std::string& Esperado, const std::string& Contexto);
    bool Comprobar_Condicion(Id_Nodo Nodo, const Ambito& Actual);
    bool Comprobar_Bloque(const Lista_Nodos& Nodos, const Ambito& Actual, const std::string* Tipo_Retorno);
    bool Comprobar_Sentencia(Id_Nodo Nodo, const Ambito& Actual, const std::string* Tipo_Retorno);
    const std::string& Comprobar_Expresion(Id_Nodo Nodo, const Ambito& Actual, Uso Uso_Nodo = Uso::Valor);
    std::string Inferir(Id_Nodo Nodo, const Ambito& Actual, Uso Uso_Nodo);
    std::string Inferir_Nombre(Id_Nodo Nodo, const Ambito& Actual, Uso Uso_Nodo);
    std::string Inferir_Llamada(Id_Nodo Nodo, const Ambito& Actual);
    std::string Inferir_Binaria(Id_Nodo Nodo, const Ambito& Actual);
    const Symbol* Resolver_Nodo(Id_Nodo Nodo, const Ambito& Actual, const Ambito*& Origen) const;

    void Registrar(Id_Nodo Nodo, const Ambito& Actual, std::string_view Accion, std::string_view Detalle = {});
    std::string Registrar_Error(Id_Nodo Nodo, const Ambito& Actual, std::string Mensaje);
};

#endif
