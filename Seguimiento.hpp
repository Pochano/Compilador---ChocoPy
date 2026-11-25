#ifndef SEGUIMIENTO_HPP
#define SEGUIMIENTO_HPP

#include "AST.hpp"

#include <iostream>
#include <string>
#include <string_view>
#include <vector>

struct Paso_Seguimiento {

    std::string Fase;
    std::string Estado;
    std::string Simbolos;
    std::string Entrada;
    std::string Accion;
    std::string Atributos;
    std::string Accion_AST;
    Id_Nodo Nodo = 0;
    int Linea = 0;
};

struct Error_Compilacion {

    std::string Fase;
    Ubicacion Posicion;
    std::string Mensaje;
    Id_Nodo Nodo = 0;
};

class Seguimiento {

public:
    explicit Seguimiento(bool Consola = false) : Mostrar_Consola(Consola) {}

    std::vector<Paso_Seguimiento> Pasos;
    std::vector<Error_Compilacion> Errores;

    void Encabezado() {

        if (Mostrar_Consola) {
            std::cout << "#\tLinea\tFase\tEstados / ambito\tSimbolos / nodo\tATTR\tEntrada\tAccion\tAccion AST\n";
        }
    }

    void Registrar(std::string_view Fase, std::string_view Estado, std::string_view Simbolos,
                   std::string_view Entrada, std::string_view Accion, std::string_view Atributos = {},
                   std::string_view Accion_AST = {}, Id_Nodo Nodo = 0, int Linea = 0) {

        Pasos.push_back({std::string(Fase), std::string(Estado), std::string(Simbolos),
                         std::string(Entrada), std::string(Accion), std::string(Atributos), std::string(Accion_AST), Nodo, Linea});
        if (Mostrar_Consola) {
            std::cout << Pasos.size() << '\t' << Linea << '\t' << Fase << '\t' << Estado << '\t' << Simbolos << '\t'
                      << Atributos << '\t' << Entrada << '\t' << Accion << '\t' << Accion_AST << '\n';
        }
    }

private:
    bool Mostrar_Consola;
};

#endif
