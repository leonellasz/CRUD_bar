#ifndef PRODUTO_H
#define PRODUTO_H

#include <string>

class Produto {
public:
    int id;
    std::string nome;
    std::string categoria;
    double preco;
    int estoque;
    std::string descricao;
    bool fabricado_em_mari;

    // Construtor Vazio
    Produto() : id(0), nome(""), categoria(""), preco(0.0), estoque(0), descricao(""), fabricado_em_mari(false) {}

    // Construtor Completo
    Produto(int id, std::string nome, std::string categoria, double preco, int estoque, std::string descricao, bool fabricado_em_mari) {
        this->id = id;
        this->nome = nome;
        this->categoria = categoria;
        this->preco = preco;
        this->estoque = estoque;
        this->descricao = descricao;
        this->fabricado_em_mari = fabricado_em_mari;
    }
};

#endif