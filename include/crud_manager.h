#ifndef CRUD_MANAGER_H
#define CRUD_MANAGER_H

#include "produto.h"
#include "db_conexao.h"
#include <pqxx/pqxx>
#include <vector>
#include <map>
#include <algorithm>
#include <iomanip>
#include <iostream>

using namespace std;

// ==========================================
// ESTRUTURAS DE DADOS (PARTE 1 + PARTE 2)
// ==========================================

// 2. Cliente (Com os perfis de desconto malucos do professor!)
struct Cliente {
    int id;
    string nome;
    string cpf;
    string telefone;
    bool torce_flamengo;
    bool assiste_one_piece;
    bool de_sousa;
};

// 3. Vendedor
struct Vendedor {
    int id;
    string nome;
};

// 4. Venda (O Recibo / Cabeçalho)
struct Venda {
    int id;
    int cliente_id;
    int vendedor_id;
    string data_venda;
    double valor_total;
    string forma_pagamento; // Cartao, Boleto, Pix, Berries
    string status_pagamento; // Confirmado, Pendente
};

// 5. Item da Venda (O Carrinho)
struct ItemVenda {
    int id;
    int venda_id;
    int produto_id;
    int quantidade;
    double preco_unitario;
};

// ─────────────────────────────────────────────────────────
//  Classe que gerencia todas as operações CRUD no PostgreSQL
// ─────────────────────────────────────────────────────────
class CRUDManager {
private:
    DBConexao& db;

    // Converte uma linha do resultado pqxx em objeto Produto
    Produto rowParaProduto(const pqxx::row& row) const {
        return Produto(
            row["id"].as<int>(),
            row["nome"].c_str(),
            row["categoria"].c_str(),
            row["preco"].as<double>(),
            row["estoque"].as<int>(),
            row["descricao"].is_null() ? "" : row["descricao"].c_str(),
            row["fabricado_em_mari"].is_null() ? false : row["fabricado_em_mari"].as<bool>()
        );
    }

public:
    CRUDManager(DBConexao& conexao) : db(conexao) {}

    // ─── Inicialização do banco ───────────────────────────

    void inicializarBanco() {
        try {
            pqxx::work txn(*db.getConn());

            // Cria tabela se não existir
            txn.exec(R"(
                CREATE TABLE IF NOT EXISTS produto (
                    id        SERIAL PRIMARY KEY,
                    nome      VARCHAR(100) NOT NULL,
                    categoria VARCHAR(50)  NOT NULL,
                    preco     NUMERIC(10,2) NOT NULL CHECK (preco >= 0),
                    estoque   INTEGER NOT NULL DEFAULT 0 CHECK (estoque >= 0),
                    descricao TEXT
                );
            )");

            // Índice para busca por nome (exigência da Parte 2)
            txn.exec(R"(
                CREATE INDEX IF NOT EXISTS idx_produto_nome
                ON produto(nome);
            )");

            // Índice para busca por categoria
            txn.exec(R"(
                CREATE INDEX IF NOT EXISTS idx_produto_categoria
                ON produto(categoria);
            )");

            // View: produtos com estoque baixo (exigência da Parte 2)
            txn.exec(R"(
                CREATE OR REPLACE VIEW vw_estoque_baixo AS
                SELECT id, nome, categoria, preco, estoque
                FROM produto
                WHERE estoque < 5
                ORDER BY estoque ASC;
            )");

            // Stored procedure: relatório de estoque por categoria
            txn.exec(R"(
                CREATE OR REPLACE FUNCTION fn_relatorio_estoque()
                RETURNS TABLE(
                    categoria      TEXT,
                    qtd_produtos   BIGINT,
                    total_unidades BIGINT,
                    valor_total    NUMERIC
                ) AS $$
                BEGIN
                    RETURN QUERY
                    SELECT
                        p.categoria::TEXT,
                        COUNT(*)::BIGINT,
                        SUM(p.estoque)::BIGINT,
                        SUM(p.preco * p.estoque)::NUMERIC
                    FROM produto p
                    GROUP BY p.categoria
                    ORDER BY valor_total DESC;
                END;
                $$ LANGUAGE plpgsql;
            )");

            txn.commit();
            cout << "[DB] Tabelas, indices, view e procedure criados." << endl;

            // Insere dados iniciais se a tabela estiver vazia
            pqxx::work txn2(*db.getConn());
            pqxx::result r = txn2.exec("SELECT COUNT(*) FROM produto;");
            txn2.commit();

            if (r[0][0].as<int>() == 0) {
                carregarDadosIniciais();
            }

        } catch (const exception& e) {
            cerr << "[ERRO] inicializarBanco: " << e.what() << endl;
        }
    }

    void carregarDadosIniciais() {
        inserir("Cerveja Heineken 600ml",  "Bebida",  12.00, 48, "Cerveja long neck gelada");
        inserir("Cerveja Brahma 350ml",    "Bebida",   6.50, 60, "Latinha gelada");
        inserir("Caipirinha",              "Drink",   15.00, 30, "Com limao e cachaca artesanal");
        inserir("Agua Mineral 500ml",      "Bebida",   4.00, 40, "Gelada sem gas");
        inserir("Refrigerante Coca 350ml", "Bebida",   7.00, 35, "Latinha gelada");
        inserir("Porcao de Batata Frita",  "Petisco", 22.00, 20, "Crocante com sal e tempero");
        inserir("Calabresa Acebolada",     "Petisco", 28.00, 15, "Porcao 300g");
        inserir("Torresmo",                "Petisco", 25.00,  4, "Porcao 250g crocante");
        inserir("Frango na Brasa",         "Prato",   45.00, 10, "Meio frango temperado");
        inserir("Vodka Smirnoff 51ml",     "Drink",   12.00, 25, "Dose simples");
        cout << "[DB] Dados iniciais inseridos." << endl;
    }

    // ─── 1. INSERIR ──────────────────────────────────────

    bool inserir(const string& nome, const string& categoria,
                 double preco, int estoque, const string& descricao) {
        if (nome.empty() || categoria.empty() || preco < 0 || estoque < 0) {
            cout << "[ERRO] Dados invalidos para insercao." << endl;
            return false;
        }
        try {
            pqxx::work txn(*db.getConn());

            pqxx::result r = txn.exec_params(
                "INSERT INTO produto (nome, categoria, preco, estoque, descricao) "
                "VALUES ($1, $2, $3, $4, $5) RETURNING id;",
                nome, categoria, preco, estoque, descricao
            );
            txn.commit();

            int novoId = r[0][0].as<int>();
            cout << "[OK] Produto \"" << nome << "\" inserido com ID " << novoId << "." << endl;
            return true;

        } catch (const exception& e) {
            cerr << "[ERRO] inserir: " << e.what() << endl;
            return false;
        }
    }

    // ─── 2. ALTERAR ──────────────────────────────────────

    bool alterar(int id, const string& novoNome, const string& novaCategoria,
                 double novoPreco, int novoEstoque, const string& novaDescricao) {
        try {
            // Busca produto atual
            pqxx::work txn(*db.getConn());
            pqxx::result r = txn.exec_params(
                "SELECT * FROM produto WHERE id = $1;", id
            );
            if (r.empty()) {
                cout << "[ERRO] Produto com ID " << id << " nao encontrado." << endl;
                txn.commit();
                return false;
            }

            Produto atual = rowParaProduto(r[0]);

            // Usa valor atual se não informado
            string nome      = novoNome.empty()      ? atual.nome      : novoNome;
            string categoria = novaCategoria.empty() ? atual.categoria : novaCategoria;
            double preco     = novoPreco < 0         ? atual.preco     : novoPreco;
            int    estoque   = novoEstoque < 0       ? atual.estoque   : novoEstoque;
            string descricao = novaDescricao.empty() ? atual.descricao : novaDescricao;

            txn.exec_params(
                "UPDATE produto SET nome=$1, categoria=$2, preco=$3, estoque=$4, descricao=$5 "
                "WHERE id=$6;",
                nome, categoria, preco, estoque, descricao, id
            );
            txn.commit();

            cout << "[OK] Produto ID " << id << " atualizado com sucesso." << endl;
            return true;

        } catch (const exception& e) {
            cerr << "[ERRO] alterar: " << e.what() << endl;
            return false;
        }
    }

    // ─── 3. PESQUISAR POR NOME ───────────────────────────

    vector<Produto> pesquisarPorNome(const string& termo) {
        vector<Produto> resultado;
        try {
            pqxx::work txn(*db.getConn());
            // ILIKE = case-insensitive no PostgreSQL
            pqxx::result r = txn.exec_params(
                "SELECT * FROM produto WHERE nome ILIKE $1 ORDER BY nome;",
                "%" + termo + "%"
            );
            txn.commit();

            for (const auto& row : r) {
                resultado.push_back(rowParaProduto(row));
            }
        } catch (const exception& e) {
            cerr << "[ERRO] pesquisarPorNome: " << e.what() << endl;
        }
        return resultado;
    }

    // ─── 4. REMOVER ──────────────────────────────────────

    bool remover(int id) {
        try {
            pqxx::work txn(*db.getConn());

            pqxx::result check = txn.exec_params(
                "SELECT nome, estoque FROM produto WHERE id = $1;", id
            );
            if (check.empty()) {
                cout << "[ERRO] Produto com ID " << id << " nao encontrado." << endl;
                txn.commit();
                return false;
            }

            string nome      = check[0]["nome"].as<string>();
            int estoqueAtual = check[0]["estoque"].as<int>();
            txn.commit();

            cout << endl;
            cout << "  Produto  : " << nome << endl;
            cout << "  Estoque  : " << estoqueAtual << " unidades" << endl;
            cout << endl;
            cout << "  O que deseja fazer?" << endl;
            cout << "  1. Retirar unidades do estoque" << endl;
            cout << "  2. Excluir produto do cadastro" << endl;
            cout << "  0. Cancelar" << endl;
            cout << "  Opcao: ";

            int opcao;
            cin >> opcao;
            cin.ignore();

            if (opcao == 0) {
                cout << "[INFO] Operacao cancelada." << endl;
                return false;
            }

            if (opcao == 2) {
                // Excluir o produto inteiro
                cout << "  Tem certeza que deseja EXCLUIR \"" << nome << "\" do cadastro? (1=Sim / 0=Nao): ";
                int confirm;
                cin >> confirm;
                cin.ignore();
                if (confirm != 1) {
                    cout << "[INFO] Operacao cancelada." << endl;
                    return false;
                }
                pqxx::work txn2(*db.getConn());
                txn2.exec_params("DELETE FROM produto WHERE id = $1;", id);
                txn2.commit();
                cout << "[OK] Produto \"" << nome << "\" excluido do cadastro." << endl;
                return true;
            }

            if (opcao == 1) {
                if (estoqueAtual <= 0) {
                    cout << "[ERRO] Produto ja esta sem estoque." << endl;
                    return false;
                }

                cout << "  Quantas unidades deseja retirar? (max: " << estoqueAtual << "): ";
                int qtd;
                cin >> qtd;
                cin.ignore();

                if (qtd <= 0) {
                    cout << "[ERRO] Quantidade invalida." << endl;
                    return false;
                }
                if (qtd > estoqueAtual) {
                    cout << "[ERRO] Quantidade maior que o estoque disponivel (" << estoqueAtual << ")." << endl;
                    return false;
                }

                pqxx::work txn2(*db.getConn());

                if (qtd == estoqueAtual) {
                    // Quantidade igual ao estoque — exclui o produto direto
                    txn2.exec_params("DELETE FROM produto WHERE id = $1;", id);
                    txn2.commit();
                    cout << "[OK] Estoque zerado. Produto \"" << nome << "\" excluido do cadastro." << endl;
                } else {
                    // Só diminui o estoque
                    txn2.exec_params(
                        "UPDATE produto SET estoque = estoque - $1 WHERE id = $2;",
                        qtd, id
                    );
                    txn2.commit();
                    cout << "[OK] " << qtd << " unidade(s) retirada(s) de \"" << nome << "\"." << endl;
                    cout << "     Estoque restante: " << (estoqueAtual - qtd) << " unidades." << endl;
                }
                return true;
            }

            cout << "[ERRO] Opcao invalida." << endl;
            return false;

        } catch (const exception& e) {
            cerr << "[ERRO] remover: " << e.what() << endl;
            return false;
        }
    }

    // ─── 5. LISTAR TODOS ─────────────────────────────────

    void listarTodos() {
        try {
            pqxx::work txn(*db.getConn());
            pqxx::result r = txn.exec(
                "SELECT * FROM produto ORDER BY id;"
            );
            txn.commit();

            if (r.empty()) {
                cout << "[INFO] Nenhum produto cadastrado." << endl;
                return;
            }

            cout << endl;
            cout << left << setw(5)  << "ID"
                 << setw(25) << "NOME"
                 << setw(15) << "CATEGORIA"
                 << setw(13) << "PRECO"
                 << setw(8)  << "ESTOQUE" << endl;
            cout << string(66, '-') << endl;

            for (const auto& row : r) {
                // Aqui você pode implementar um exibirResumido se a sua struct Produto suportar, 
                // ou simplesmente dar um cout manual. (Mantive a lógica original do seu código).
                // rowParaProduto(row).exibirResumido();
            }

            cout << string(66, '-') << endl;
            cout << "Total de produtos cadastrados: " << r.size() << endl;

        } catch (const exception& e) {
            cerr << "[ERRO] listarTodos: " << e.what() << endl;
        }
    }

    // ─── 6. EXIBIR UM ────────────────────────────────────

    bool exibirUm(int id) {
        try {
            pqxx::work txn(*db.getConn());
            pqxx::result r = txn.exec_params(
                "SELECT * FROM produto WHERE id = $1;", id
            );
            txn.commit();

            if (r.empty()) {
                cout << "[ERRO] Produto com ID " << id << " nao encontrado." << endl;
                return false;
            }

            // rowParaProduto(r[0]).exibir();
            return true;

        } catch (const exception& e) {
            cerr << "[ERRO] exibirUm: " << e.what() << endl;
            return false;
        }
    }

    // ─── 7. RELATÓRIO ────────────────────────────────────

    void gerarRelatorio() {
        try {
            pqxx::work txn(*db.getConn());

            // Resumo geral
            pqxx::result geral = txn.exec(R"(
                SELECT
                    COUNT(*)                        AS qtd_produtos,
                    SUM(estoque)                    AS total_unidades,
                    SUM(preco * estoque)            AS valor_total,
                    AVG(preco)                      AS preco_medio,
                    MAX(preco)                      AS maior_preco,
                    MIN(preco)                      AS menor_preco,
                    SUM(CASE WHEN estoque=0 THEN 1 ELSE 0 END) AS sem_estoque
                FROM produto;
            )");

            // Produto mais caro / mais barato
            pqxx::result maisCaro = txn.exec(
                "SELECT nome, preco FROM produto ORDER BY preco DESC LIMIT 1;"
            );
            pqxx::result maisBarato = txn.exec(
                "SELECT nome, preco FROM produto ORDER BY preco ASC LIMIT 1;"
            );

            // Por categoria (usando a stored procedure)
            pqxx::result porCat = txn.exec(
                "SELECT * FROM fn_relatorio_estoque();"
            );

            // Estoque baixo (usando a view)
            pqxx::result baixo = txn.exec(
                "SELECT nome, estoque FROM vw_estoque_baixo;"
            );

            txn.commit();

            cout << fixed << setprecision(2);
            cout << endl;
            cout << "==========================================" << endl;
            cout << "      RELATORIO DE ESTOQUE - BAR          " << endl;
            cout << "==========================================" << endl;

            if (!geral.empty() && !geral[0]["qtd_produtos"].is_null()) {
                cout << endl << "--- RESUMO GERAL ---" << endl;
                cout << "  Qtd. de produtos diferentes : " << geral[0]["qtd_produtos"].as<int>() << endl;
                cout << "  Total de unidades em estoque: " << geral[0]["total_unidades"].as<int>() << endl;
                cout << "  Valor total do estoque      : R$ " << geral[0]["valor_total"].as<double>() << endl;
                cout << "  Preco medio dos produtos    : R$ " << geral[0]["preco_medio"].as<double>() << endl;
                cout << "  Produto mais caro           : " << maisCaro[0]["nome"].as<string>()
                     << " (R$ " << maisCaro[0]["preco"].as<double>() << ")" << endl;
                cout << "  Produto mais barato         : " << maisBarato[0]["nome"].as<string>()
                     << " (R$ " << maisBarato[0]["preco"].as<double>() << ")" << endl;
                cout << "  Produtos sem estoque        : " << geral[0]["sem_estoque"].as<int>() << endl;
            }

            cout << endl << "--- POR CATEGORIA (via stored procedure) ---" << endl;
            cout << left << setw(15) << "Categoria"
                 << setw(12) << "Produtos"
                 << setw(12) << "Unidades"
                 << "Valor Total" << endl;
            cout << string(52, '-') << endl;
            for (const auto& row : porCat) {
                cout << left << setw(15) << row["categoria"].as<string>()
                     << setw(12) << row["qtd_produtos"].as<int>()
                     << setw(12) << row["total_unidades"].as<int>()
                     << "R$ " << row["valor_total"].as<double>() << endl;
            }

            cout << endl << "--- ESTOQUE BAIXO (< 5 unidades) - via VIEW ---" << endl;
            if (baixo.empty()) {
                cout << "  Nenhum produto com estoque critico." << endl;
            } else {
                for (const auto& row : baixo) {
                    cout << "  [ATENCAO] " << row["nome"].as<string>()
                         << " - Estoque: " << row["estoque"].as<int>() << endl;
                }
            }

            cout << endl << "==========================================" << endl;

        } catch (const exception& e) {
            cerr << "[ERRO] gerarRelatorio: " << e.what() << endl;
        }
    }

    // ─── Utilitários ─────────────────────────────────────

    int getTotalProdutos() {
        try {
            pqxx::work txn(*db.getConn());
            pqxx::result r = txn.exec("SELECT COUNT(*) FROM produto;");
            txn.commit();
            return r[0][0].as<int>();
        } catch (...) {
            return 0;
        }
    }

    // ==========================================
    // FUNÇÕES DA PARTE 2: SISTEMA DE VENDAS
    // ==========================================

    // 1. Busca todos os clientes cadastrados para o Dropdown
    std::vector<Cliente> obterClientes() {
        std::vector<Cliente> lista;
        if (db.conectar()) {
            try {
                pqxx::work txn(*db.getConn());
                pqxx::result r = txn.exec("SELECT id, nome, cpf, telefone, torce_flamengo, assiste_one_piece, de_sousa FROM cliente ORDER BY nome ASC;");
                
                for (auto row : r) {
                    Cliente c;
                    c.id = row["id"].as<int>();
                    c.nome = row["nome"].c_str();
                    c.cpf = !row["cpf"].is_null() ? row["cpf"].c_str() : "";
                    c.telefone = !row["telefone"].is_null() ? row["telefone"].c_str() : "";
                    c.torce_flamengo = row["torce_flamengo"].as<bool>();
                    c.assiste_one_piece = row["assiste_one_piece"].as<bool>();
                    c.de_sousa = row["de_sousa"].as<bool>();
                    lista.push_back(c);
                }
            } catch (const std::exception& e) {
                std::cerr << "Erro ao buscar clientes: " << e.what() << std::endl;
            }
            db.desconectar();
        }
        return lista;
    }

    // 2. Busca todos os vendedores para o Dropdown
    std::vector<Vendedor> obterVendedores() {
        std::vector<Vendedor> lista;
        if (db.conectar()) {
            try {
                pqxx::work txn(*db.getConn());
                pqxx::result r = txn.exec("SELECT id, nome FROM vendedor ORDER BY nome ASC;");
                
                for (auto row : r) {
                    Vendedor v;
                    v.id = row["id"].as<int>();
                    v.nome = row["nome"].c_str();
                    lista.push_back(v);
                }
            } catch (const std::exception& e) {
                std::cerr << "Erro ao buscar vendedores: " << e.what() << std::endl;
            }
            db.desconectar();
        }
        return lista;
    }

}; // <-- Só esta chave fecha a classe de verdade no final do arquivo!

#endif