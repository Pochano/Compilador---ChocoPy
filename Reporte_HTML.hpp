#ifndef REPORTE_HTML_HPP
#define REPORTE_HTML_HPP

#include "Analisis_Semantico.hpp"
#include "Seguimiento.hpp"

#include <filesystem>

void Guardar_Reporte(const std::filesystem::path& Ruta, std::string_view Archivo, std::string_view Estado,
                     AST& Arbol, Seguimiento& Traza, std::vector<Tabla_Ambito>& Tablas);

#endif
