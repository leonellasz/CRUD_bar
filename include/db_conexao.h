#ifndef DB_CONEXAO_H
#define DB_CONEXAO_H

#include <pqxx/pqxx>
#include <iostream>
#include <string>
#include <stdexcept>
#include <cstdlib>

using namespace std;

// ─────────────────────────────────────────────────────────
//  Classe responsável por gerenciar a conexão com o banco
// ─────────────────────────────────────────────────────────
class DBConexao {
private:
    pqxx::connection* conn;

    // String de conexão via variáveis de ambiente
    // No terminal rode: export DB_HOST=localhost DB_USER=postgres DB_PASSWORD=postgres
    string getStringConexao() const {
        const char* senha = getenv("DB_PASSWORD");
        const char* host  = getenv("DB_HOST");
        const char* user  = getenv("DB_USER");

        return string("host=") + (host ? host : "localhost") +
               " port=5432"
               " dbname=postgres"
               " user=" + (user ? user : "postgres") +
               " password=" + (senha ? senha : "Minhamae21321@");
    }

public:
    DBConexao() : conn(nullptr) {}

    ~DBConexao() {
        desconectar();
    }

    // Abre a conexão
    bool conectar() {
        try {
            conn = new pqxx::connection(getStringConexao());
            if (conn->is_open()) {
                cout << "[DB] Conectado ao banco: " << conn->dbname() << endl;
                return true;
            }
            return false;
        } catch (const exception& e) {
            cerr << "[ERRO DB] Falha ao conectar: " << e.what() << endl;
            return false;
        }
    }

    // Fecha a conexão
    void desconectar() {
        if (conn) {
            conn->close();
            delete conn;
            conn = nullptr;
        }
    }

    // Retorna ponteiro para a conexão ativa
    pqxx::connection* getConn() {
        if (!conn || !conn->is_open()) {
            throw runtime_error("[ERRO DB] Conexao nao esta ativa.");
        }
        return conn;
    }

    bool estaConectado() const {
        return conn && conn->is_open();
    }
};

#endif