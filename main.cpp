#include "Reporte_HTML.hpp"
#include "parser.hpp"
#include "scanner.hpp"
#include <algorithm>
#include <filesystem>
#include <iostream>
#include <string>

int main(int argc, char* argv[]) {
    namespace fs = std::filesystem;
    fs::path grammarFile;
    fs::path sourceFile;
    bool Mostrar_Traza = false;
    bool Mostrar_Tabla_LR = false;
    try {
        for (int i = 1; i < argc; ++i) {
            const std::string arg = argv[i];
            if (arg == "--sin-traza") Mostrar_Traza = false;
            else if (arg == "--traza-consola") Mostrar_Traza = true;
            else if (arg == "--tabla-lr") Mostrar_Tabla_LR = true;
            else if (arg == "--ayuda" || arg == "-h") {
                sourceFile.clear();
                break;
            } else if (arg.compare(0, 2, "--") == 0) {
                throw std::runtime_error("Opcion desconocida '" + arg + "'");
            } else if (sourceFile.empty()) sourceFile = arg;
            else if (grammarFile.empty()) grammarFile = arg;
            else throw std::runtime_error("Se esperaba un archivo fuente y, opcionalmente, una gramatica");
        }
        if (sourceFile.empty()) {
            std::cout << "Uso: Compilador_Chocopy.exe <archivo.py> [gramatica.txt] [--traza-consola] [--tabla-lr]\n";
            return 0;
        }
        fs::path Directorio_Proyecto = fs::absolute(fs::path(argv[0])).parent_path();
        if (!fs::exists(Directorio_Proyecto / "ChocoPy_Gramatica.txt")) {
            Directorio_Proyecto = fs::current_path();
        }
        if (grammarFile.empty()) {
            grammarFile = Directorio_Proyecto / "ChocoPy_Gramatica.txt";
        }

        Grammar grammar = Grammar::fromFile(grammarFile.string());
        LR1Builder builder(grammar);
        const auto Huella = grammar.Huella_Gramatica();
        // La tabla distribuida corresponde a la gramática incluida.
        const bool Gramatica_Incluida = Huella == "91f81b75b6fa7591";
        const fs::path Cache = grammarFile.parent_path() / (Gramatica_Incluida ? "tabla_chocopy.dat" :
                                                           "tabla_chocopy." + Huella + ".dat");
        builder.first();
        if (!builder.loadTable(Cache.string())) {
            std::cout << "[INFO] Generando coleccion canonica y tabla LR(1)...\n";
            builder.coleccionCanonica();
            if (!builder.llenarTablas()) {
                std::cerr << "[ADVERTENCIA] Se detectaron conflictos en la tabla LR(1).\n";
            }
            if (builder.saveTable(Cache.string())) {
                std::cout << "[INFO] Tabla guardada en '" << Cache.string() << "'.\n";
            } else {
                std::cerr << "[ADVERTENCIA] No se pudo guardar la tabla en '" << Cache.string() << "'.\n";
            }
        } else {
            std::cout << "[INFO] Tabla LR(1) cargada desde archivo de almacenamiento.\n";
        }

        Scanner scanner(sourceFile.string());
        std::vector<Token> tokens = scanner.scan();

        Seguimiento Traza(Mostrar_Traza);
        Traza.Encabezado();
        for (const auto& Error : scanner.Errores()) {
            Traza.Errores.push_back({"Léxico", {Error.Linea, Error.Columna}, Error.Mensaje});
            Traza.Registrar("Léxico", "", "", "", Error.Mensaje, "", "", 0, Error.Linea);
        }

        AST Arbol;
        std::vector<Tabla_Ambito> Tablas;
        bool Sintaxis_Correcta = false;
        if (!tokens.empty()) {
            bool Construir_AST = Constructor_AST::Admite(grammar);
            Sintaxis_Correcta = builder.Analizar_Con_Seguimiento(tokens, Traza, Construir_AST ? &Arbol : nullptr);
            if (Mostrar_Tabla_LR) builder.printLRTable();

            if (Arbol.Raiz) {
                if (scanner.hasError()) Arbol[Arbol.Raiz].Incompleto = true;
                Analisis_Semantico Analisis(Arbol, Traza);
                Analisis.Analizar();
                Tablas = Analisis.Obtener_Tablas();
            }
        }

        bool Lexicos = scanner.hasError();
        bool Sintacticos = !Sintaxis_Correcta && !Lexicos;
        bool Semanticos = false;
        for (const auto& Error : Traza.Errores) {
            if (Error.Fase == "Sintáctico") Sintacticos = true;
            if (Error.Fase == "Semántico") Semanticos = true;
        }

        std::string Estado = Arbol.Raiz ? "Análisis correcto" : "Análisis sintáctico correcto";
        int Codigo_Salida = Lexicos ? 1 : Sintacticos ? 2 : Semanticos ? 3 : 0;
        if (Codigo_Salida) {
            Estado = "Errores";
            if (Lexicos) Estado += " léxicos";
            if (Sintacticos) Estado += Lexicos ? (Semanticos ? ", sintácticos" : " y sintácticos") : " sintácticos";
            if (Semanticos) Estado += (Lexicos || Sintacticos) ? " y semánticos" : " semánticos";
        }

        std::stable_sort(Traza.Errores.begin(), Traza.Errores.end(),
            [](const Error_Compilacion& Primero, const Error_Compilacion& Segundo) {
                if (Primero.Posicion.Linea != Segundo.Posicion.Linea) {
                    return Primero.Posicion.Linea < Segundo.Posicion.Linea;
                }
                return Primero.Posicion.Columna < Segundo.Posicion.Columna;
            });

        for (const auto& Error : Traza.Errores) {
            const char* Fase = Error.Fase == "Léxico" ? "LEXICO" :
                               Error.Fase == "Sintáctico" ? "SINTACTICO" : "SEMANTICO";
            std::cout << "[ERROR " << Fase << "]";
            if (Error.Posicion.Linea > 0) {
                std::cout << " En linea " << Error.Posicion.Linea << ", columna " << Error.Posicion.Columna;
            }
            std::cout << ": " << Error.Mensaje << '\n';
        }

        fs::path Reporte = Directorio_Proyecto / "Reporte_Compilador.html";
        Guardar_Reporte(Reporte, sourceFile.filename().u8string(), Estado, Arbol, Traza, Tablas);
        std::cout << "[REPORTE] " << Reporte.string() << '\n';
        std::cout << Estado << '\n';
        return Codigo_Salida;

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}
