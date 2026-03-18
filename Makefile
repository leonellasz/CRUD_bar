CXX      = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -Iinclude
LIBS     = -lpqxx -lpq -lcomctl32

TARGET_CLI = bar_crud.exe
TARGET_GUI = bar_crud_gui.exe

SRC_CLI = src/main.cpp
SRC_GUI = src/interface_gui.cpp

all: cli gui

cli: $(SRC_CLI)
	$(CXX) $(CXXFLAGS) -o $(TARGET_CLI) $(SRC_CLI) $(LIBS)
	@echo "Terminal pronto!"

# NOVO: Regra para compilar o icone do Windows
res.o: recursos.rc
	windres recursos.rc -O coff -o res.o

# NOVO: Adicionado res.o na compilacao da GUI
gui: $(SRC_GUI) res.o
	$(CXX) $(CXXFLAGS) -o $(TARGET_GUI) $(SRC_GUI) res.o $(LIBS) -mwindows
	@echo "Interface Grafica pronta com Icone!"

clean:
	del /f $(TARGET_CLI) $(TARGET_GUI) res.o