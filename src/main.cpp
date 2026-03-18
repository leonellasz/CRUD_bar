#include "db_conexao.h"
#include "crud_manager.h"
#include <iostream>

using namespace std;

int main() {
    cout << "##############################################" << endl;
    cout << "#    BAR DO BODE - VERIFICADOR DE BANCO      #" << endl;
    cout << "##############################################" << endl << endl;

    cout << "[INFO] Conectando ao PostgreSQL..." << endl;

    // Conectar ao banco
    DBConexao db;
    if (!db.conectar()) {
        cerr << "[FATAL] Nao foi possivel conectar ao banco." << endl;
        cerr << "Verifique se o PostgreSQL esta rodando e as credenciais em db_conexao.h" << endl;
        return 1;
    }

    // Inicializa tabelas, índices, view, procedure (Garante que tudo exista para a GUI funcionar)
    CRUDManager mgr(db);
    cout << "[INFO] Verificando estrutura de tabelas e dependencias..." << endl;
    mgr.inicializarBanco();

    cout << "[SUCESSO] Banco de dados pronto e atualizado!" << endl;
    cout << "[INFO] Total: " << mgr.getTotalProdutos() << " produtos registrados." << endl;
    cout << endl;
    cout << "=> Tudo certo! Voce ja pode fechar este terminal e rodar a INTERFACE GRAFICA." << endl;

    return 0;
}