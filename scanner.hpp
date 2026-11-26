#ifndef SCANNER_HPP
#define SCANNER_HPP

#include "parser.hpp"
#include <string>
#include <vector>

struct Diagnostico_Lexico
{
    std::string Mensaje;
    int Linea;
    int Columna;
};

class Scanner
{
  public:
    Scanner(const std::string& filename);
    ~Scanner();
    std::vector<Token> scan();
    bool hasError() const;

    const std::vector<Diagnostico_Lexico>& Errores() const
    {
        return Diagnosticos;
    }

  private:
    std::string filename;
    char* text_Arr;
    int index_pos;
    int sizee;
    int line_global;
    int col_global;
    std::vector<Diagnostico_Lexico> Diagnosticos;
    std::vector<int> indent_stack;
    int bracket_depth;
    bool at_line_start;

    char get_char();
    char peek_actual_char();
    void report_error(const std::string& message, int line, int col);
    void process_indentation(std::vector<Token>& tokens);
    void scan_next_tokens(std::vector<Token>& tokens);
};

#endif
