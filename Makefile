CXX = g++
CXXFLAGS = -std=c++17 -O2 -Wall -Wextra
ifeq ($(OS),Windows_NT)
TARGET = Compilador_Chocopy.exe
else
TARGET = chocopy
endif
SRCS = main.cpp parser.cpp scanner.cpp AST.cpp Analisis_Semantico.cpp Reporte_HTML.cpp
HEADERS = parser.hpp scanner.hpp AST.hpp Analisis_Semantico.hpp Reporte_HTML.hpp Seguimiento.hpp

all: $(TARGET)

$(TARGET): $(SRCS) $(HEADERS)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(SRCS)

# Fuerza la regeneracion de la tabla de la gramatica incluida.
regen:
	rm -f tabla_chocopy.dat

clean:
	rm -f $(TARGET) $(TARGET).exe
