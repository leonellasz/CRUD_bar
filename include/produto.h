#ifndef PRODUTO_H
#define PRODUTO_H

#include <string>
#include <iostream>
#include <iomanip>

using namespace std;

class Produto {
private:
    int id;
    string nome;
    string categoria;   // Bebida, Petisco, Prato, etc.
    double preco;
    int estoque;
    string descricao;

public:
    // Construtores
    Produto() : id(0), nome(""), categoria(""), preco(0.0), estoque(0), descricao("") {}

    Produto(int id, string nome, string categoria, double preco, int estoque, string descricao)
        : id(id), nome(nome), categoria(categoria), preco(preco), estoque(estoque), descricao(descricao) {}

    // Getters
    int getId() const { return id; }
    string getNome() const { return nome; }
    string getCategoria() const { return categoria; }
    double getPreco() const { return preco; }
    int getEstoque() const { return estoque; }
    string getDescricao() const { return descricao; }

    // Setters
    void setId(int id) { this->id = id; }
    void setNome(string nome) { this->nome = nome; }
    void setCategoria(string categoria) { this->categoria = categoria; }
    void setPreco(double preco) { this->preco = preco; }
    void setEstoque(int estoque) { this->estoque = estoque; }
    void setDescricao(string descricao) { this->descricao = descricao; }

    // Métodos utilitários
    void diminuirEstoque(int qtd) {
        if (estoque >= qtd) estoque -= qtd;
    }

    void aumentarEstoque(int qtd) {
        estoque += qtd;
    }

    bool temEstoque() const {
        return estoque > 0;
    }

    double calcularValorTotalEstoque() const {
        return preco * estoque;
    }

    void exibir() const {
        cout << "+-------------------------------------------------+" << endl;
        cout << "| ID       : " << left << setw(37) << id << "|" << endl;
        cout << "| Nome     : " << left << setw(37) << nome << "|" << endl;
        cout << "| Categoria: " << left << setw(37) << categoria << "|" << endl;
        cout << "| Preco    : R$ " << left << setw(34) << fixed << setprecision(2) << preco << "|" << endl;
        cout << "| Estoque  : " << left << setw(37) << estoque << "|" << endl;
        cout << "| Descricao: " << left << setw(37) << descricao << "|" << endl;
        cout << "+-------------------------------------------------+" << endl;
    }

    void exibirResumido() const {
        cout << left << setw(5) << id
             << setw(25) << nome
             << setw(15) << categoria
             << "R$ " << setw(10) << fixed << setprecision(2) << preco
             << setw(8) << estoque << endl;
    }
};

#endif
