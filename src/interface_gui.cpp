#include <windows.h>
#include <commctrl.h> 
#include <string>
#include <iostream>
#include <fstream>
#include <sstream> 
#include <vector> 
#include <pqxx/pqxx>
#include "db_conexao.h"

// ==========================================
// IDs DA PARTE 1 (GERENCIAMENTO)
// ==========================================
#define ID_BTN_LISTAR      101
#define ID_BTN_ADD         102
#define ID_BTN_DEL         103
#define ID_BTN_BAIXA       104
#define ID_BTN_RESET       105
#define ID_BTN_ADD_ESTOQUE 106
#define ID_BTN_FILTRAR     107
#define ID_BTN_EXPORTAR    108
#define ID_RADIO_ID        109
#define ID_RADIO_PRECO     110
#define ID_RADIO_ESTOQUE   111
#define ID_LISTBOX         200
#define ID_EDIT_NOME       201
#define ID_COMBO_CAT       202 
#define ID_EDIT_PRECO      203
#define ID_EDIT_ESTOQUE    204
#define ID_EDIT_ID_DEL     205
#define ID_EDIT_ID_BAIXA   206
#define ID_EDIT_QTD_BAIXA  207
#define ID_COMBO_FILTRO    208 
#define ID_DASHBOARD       209
#define ID_LISTBOX_LOGS    300 
#define ID_PROGRESS_BAR    301 
#define ID_BTN_EDITAR      302 
#define ID_BTN_CRITICO     303 
#define ID_EDIT_ID_EDIT    304 
#define ID_BTN_RESET_CONFIRM 305 
#define ID_BTN_TEMA        306 
#define ID_EDIT_DESC       307

// ==========================================
// IDs DA PARTE 2 (FILTROS AVANÇADOS E PDV)
// ==========================================
#define ID_EDIT_BUSCA_NOME     308
#define ID_EDIT_PRECO_MIN      309
#define ID_EDIT_PRECO_MAX      310
#define ID_CHK_BUSCA_MARI      311
#define ID_CHK_FUNCIONARIO     312
#define ID_CHK_CAD_MARI        313

#define ID_BTN_ABRIR_PDV       400
#define ID_COMBO_CLIENTE       401
#define ID_COMBO_VENDEDOR      402
#define ID_COMBO_PRODUTO       403
#define ID_EDIT_QTD_VENDA      404
#define ID_BTN_ADD_CARRINHO    405
#define ID_LIST_CARRINHO       406
#define ID_COMBO_PAGAMENTO     407
#define ID_BTN_FINALIZAR_VENDA 408
#define ID_TXT_TOTAL_CARRINHO  409
#define ID_BTN_NOVO_CLIENTE    410 
#define ID_BTN_NOVO_VENDEDOR   411 
#define ID_BTN_HISTORICO_VENDAS 412
#define ID_COMBO_HIST_CLIENTE  413
#define ID_BTN_HIST_BUSCAR     414
#define ID_TXT_HIST_DADOS      415
#define ID_LIST_HIST_ITENS     416 
#define ID_BTN_RELATORIO_MENSAL 417 // NOVO: ID DO RELATORIO MENSAL

// IDs CADASTROS RÁPIDOS
#define ID_EDIT_CLI_NOME       501
#define ID_EDIT_CLI_CPF        502
#define ID_EDIT_CLI_TEL        503
#define ID_CHK_FLAMENGO        504
#define ID_CHK_ONEPIECE        505
#define ID_CHK_SOUSA           506
#define ID_BTN_SALVAR_CLI      507
#define ID_EDIT_VEND_NOME      601
#define ID_BTN_SALVAR_VEND     602

// ==========================================
// VARIÁVEIS GLOBAIS
// ==========================================
HWND hList, hNome, hCat, hPreco, hEstoque, hDesc, hIdDel, hIdBaixa, hQtdBaixa, hFiltroCat, hDash;
HWND hRadioId, hRadioPreco, hRadioEstoque;
HWND hLogs, hProgressBar, hIdEdit;
HWND hBuscaNome, hBuscaPrecoMin, hBuscaPrecoMax, hChkBuscaMari, hChkFuncionario, hChkCadMari, hBtnCritico;
HWND hMainWnd; 
DBConexao db;
int ordemAtual = 1;

HWND hwndPDV = NULL, hwndHistorico = NULL, hwndRelMensal = NULL; // Nova Janela do Relatorio
HWND hComboCliente, hComboVendedor, hComboProduto, hComboPagamento;
HWND hEditQtdVenda, hListCarrinho, hTxtTotal;

struct ItemCarrinhoUI { 
    int produto_id; 
    std::string nome; 
    int quantidade; 
    double preco_base; 
    double subtotal; 
};
std::vector<ItemCarrinhoUI> carrinhoAtual; 
double valorTotalCarrinho = 0.0;

bool modoEscuro = true;
COLORREF corFundo = RGB(30, 30, 46);
COLORREF corCaixa = RGB(49, 50, 68);
COLORREF corTexto = RGB(205, 214, 244);
HBRUSH hbrFundo = CreateSolidBrush(corFundo);
HBRUSH hbrCaixaTexto = CreateSolidBrush(corCaixa);

WNDPROC wpOrigEditProc;
WNDPROC wpOrigListProc;

// ==========================================
// FUNÇÕES UTILITÁRIAS
// ==========================================
void AdicionarLog(const std::string& msg) {
    SYSTEMTIME st; 
    GetLocalTime(&st); 
    char timeBuffer[512]; 
    snprintf(timeBuffer, sizeof(timeBuffer), "[%02d:%02d:%02d] %s", st.wHour, st.wMinute, st.wSecond, msg.c_str());
    int pos = SendMessageA(hLogs, LB_ADDSTRING, 0, (LPARAM)timeBuffer); 
    SendMessage(hLogs, LB_SETTOPINDEX, pos, 0); 
}

std::string ObterTexto(HWND hwnd) { 
    char buffer[512]; 
    GetWindowTextA(hwnd, buffer, 512); 
    return std::string(buffer); 
}

LRESULT CALLBACK EditSubclassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    // 1. TRAVA DO PREÇO (Só aceita número, 1 ponto ou 1 vírgula)
    if (msg == WM_CHAR) {
        int ctrlID = GetWindowLong(hwnd, GWL_ID);
        if (ctrlID == ID_EDIT_PRECO) { // Só aplica essa regra na caixinha de Preço
            char c = (char)wParam;
            
            // Deixa passar o Backspace (apagar)
            if (c == VK_BACK) return CallWindowProc(wpOrigEditProc, hwnd, msg, wParam, lParam);
            
            // Deixa passar números de 0 a 9
            if (c >= '0' && c <= '9') return CallWindowProc(wpOrigEditProc, hwnd, msg, wParam, lParam);
            
            // Tratamento da Vírgula e do Ponto
            if (c == '.' || c == ',') {
                char buf[256];
                GetWindowTextA(hwnd, buf, 256);
                // Se a caixa de texto JÁ TEM um ponto ou vírgula, bloqueia o segundo!
                if (strchr(buf, '.') || strchr(buf, ',')) return 0; 
                
                return CallWindowProc(wpOrigEditProc, hwnd, msg, wParam, lParam);
            }
            
            return 0; 
        }
    }

    if (msg == WM_KEYDOWN && wParam == VK_RETURN) { 
        HWND hParent = GetParent(hwnd); 
        char idBuf[10]; 
        GetWindowTextA(GetDlgItem(hParent, ID_EDIT_ID_EDIT), idBuf, 10);
        
        if (strlen(idBuf) > 0) {
            SendMessage(hParent, WM_COMMAND, MAKEWPARAM(ID_BTN_EDITAR, 0), 0); 
        } else {
            SendMessage(hParent, WM_COMMAND, MAKEWPARAM(ID_BTN_ADD, 0), 0); 
        }
        return 0; 
    } 
    return CallWindowProc(wpOrigEditProc, hwnd, msg, wParam, lParam);
}

LRESULT CALLBACK ListSubclassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_KEYDOWN && wParam == VK_DELETE) { 
        HWND hParent = GetParent(hwnd); 
        int index = ListView_GetNextItem(hwnd, -1, LVNI_SELECTED);
        if (index != -1) { 
            char idBuf[50]; 
            ListView_GetItemText(hwnd, index, 0, idBuf, 50); 
            SetWindowTextA(GetDlgItem(hParent, ID_EDIT_ID_DEL), idBuf); 
            SendMessage(hParent, WM_COMMAND, MAKEWPARAM(ID_BTN_DEL, 0), 0); 
        } 
        return 0; 
    } 
    return CallWindowProc(wpOrigListProc, hwnd, msg, wParam, lParam);
}

// ==========================================
// FUNÇÕES DE BANCO DE DADOS E LISTAGEM
// ==========================================
void AtualizarCategorias() {
    SendMessage(hCat, CB_RESETCONTENT, 0, 0); 
    SendMessage(hFiltroCat, CB_RESETCONTENT, 0, 0); 
    SendMessage(hFiltroCat, CB_ADDSTRING, 0, (LPARAM)"-- TODAS --"); 
    
    if (db.conectar()) { 
        try { 
            pqxx::work txn(*db.getConn()); 
            pqxx::result r = txn.exec("SELECT DISTINCT categoria FROM produto ORDER BY categoria ASC;"); 
            for (auto row : r) { 
                SendMessageA(hCat, CB_ADDSTRING, 0, (LPARAM)row[0].c_str()); 
                SendMessageA(hFiltroCat, CB_ADDSTRING, 0, (LPARAM)row[0].c_str()); 
            } 
        } catch (...) {} 
        db.desconectar(); 
    }
}

void AtualizarLista(HWND hwnd, bool apenasCritico = false) {
    ListView_DeleteAllItems(hList); 
    
    if (db.conectar()) {
        try { 
            pqxx::work txn(*db.getConn()); 
            std::string sql = "SELECT id, nome, categoria, preco, estoque, descricao, fabricado_em_mari FROM produto WHERE 1=1 ";

            std::string buscaNome = ObterTexto(hBuscaNome);
            std::string buscaCat  = ObterTexto(hFiltroCat);
            std::string pMin      = ObterTexto(hBuscaPrecoMin);
            std::string pMax      = ObterTexto(hBuscaPrecoMax);
            bool isMari           = SendMessage(hChkBuscaMari, BM_GETCHECK, 0, 0) == BST_CHECKED;

            if (!buscaNome.empty()) {
                sql += " AND nome ILIKE " + txn.quote("%" + buscaNome + "%");
            }
            if (!buscaCat.empty() && buscaCat != "-- TODAS --") {
                sql += " AND categoria = " + txn.quote(buscaCat);
            }
            if (!pMin.empty()) {
                sql += " AND preco >= " + pMin;
            }
            if (!pMax.empty()) {
                sql += " AND preco <= " + pMax;
            }
            if (isMari) {
                sql += " AND fabricado_em_mari = TRUE";
            }
            if (apenasCritico) {
                sql += " AND estoque < 5";
            }

            if (ordemAtual == 1) {
                sql += " ORDER BY id ASC;"; 
            } else if (ordemAtual == 2) {
                sql += " ORDER BY preco DESC;"; 
            } else if (ordemAtual == 3) {
                sql += " ORDER BY estoque ASC;";
            }
            
            pqxx::result r = txn.exec(sql); 
            int totalProdutos = 0; 
            double valorTotalEstoque = 0.0; 
            int itensBons = 0; 
            int index = 0;
            
            for (auto row : r) { 
                double p = row["preco"].as<double>(); 
                int e = row["estoque"].as<int>(); 
                
                totalProdutos++; 
                valorTotalEstoque += (p * e); 
                if (e >= 5) itensBons++; 
                
                LVITEMA lvi;
                memset(&lvi, 0, sizeof(LVITEMA)); 
                lvi.mask = LVIF_TEXT; 
                lvi.iItem = index; 
                lvi.iSubItem = 0;
                
                std::string idStr = row["id"].c_str(); 
                std::string nomeStr = row["nome"].c_str(); 
                
                if (row["fabricado_em_mari"].as<bool>()) {
                    nomeStr += " (Mari)";
                }

                std::string catStr = row["categoria"].c_str(); 
                std::string precoStr = "R$ " + std::string(row["preco"].c_str());
                std::string estStr = row["estoque"].c_str(); 
                std::string descStr = !row["descricao"].is_null() ? row["descricao"].c_str() : ""; 
                
                lvi.pszText = (LPSTR)idStr.c_str(); 
                ListView_InsertItem(hList, &lvi);
                
                ListView_SetItemText(hList, index, 1, (LPSTR)nomeStr.c_str()); 
                ListView_SetItemText(hList, index, 2, (LPSTR)catStr.c_str()); 
                ListView_SetItemText(hList, index, 3, (LPSTR)precoStr.c_str()); 
                ListView_SetItemText(hList, index, 4, (LPSTR)estStr.c_str()); 
                ListView_SetItemText(hList, index, 5, (LPSTR)descStr.c_str()); 
                index++;
            } 
            txn.commit(); 
            
            char bufferDash[256]; 
            snprintf(bufferDash, sizeof(bufferDash), "Resumo: %d produtos listados | Valor Total: R$ %.2f", totalProdutos, valorTotalEstoque); 
            SetWindowTextA(hDash, bufferDash); 
            
            int porcentagem = (totalProdutos > 0) ? (itensBons * 100) / totalProdutos : 0;
            SendMessage(hProgressBar, PBM_SETPOS, porcentagem, 0);
            
        } catch (const std::exception& e) { 
            MessageBoxA(hwnd, e.what(), "Erro ao Listar", MB_ICONERROR); 
        } 
        db.desconectar();
    }
}

void RecarregarCombosPessoas() {
    SendMessage(hComboCliente, CB_RESETCONTENT, 0, 0); 
    SendMessage(hComboVendedor, CB_RESETCONTENT, 0, 0);
    
    if (db.conectar()) {
        try { 
            pqxx::work txn(*db.getConn());
            
            pqxx::result rc = txn.exec("SELECT id, nome FROM cliente ORDER BY nome;");
            for (auto row : rc) { 
                int idx = SendMessageA(hComboCliente, CB_ADDSTRING, 0, (LPARAM)row["nome"].c_str()); 
                SendMessage(hComboCliente, CB_SETITEMDATA, idx, row["id"].as<int>()); 
            }
            
            pqxx::result rv = txn.exec("SELECT id, nome FROM vendedor ORDER BY nome;");
            for (auto row : rv) { 
                int idx = SendMessageA(hComboVendedor, CB_ADDSTRING, 0, (LPARAM)row["nome"].c_str()); 
                SendMessage(hComboVendedor, CB_SETITEMDATA, idx, row["id"].as<int>()); 
            }
        } catch (...) {} 
        db.desconectar();
    }
}

void AtualizarListaCarrinho() {
    ListView_DeleteAllItems(hListCarrinho); 
    valorTotalCarrinho = 0.0;
    
    double descontoAtual = 0.0;
    int idxCliente = SendMessage(hComboCliente, CB_GETCURSEL, 0, 0); 
    
    if (idxCliente != CB_ERR) {
        int cliente_id = SendMessage(hComboCliente, CB_GETITEMDATA, idxCliente, 0);
        if (db.conectar()) {
            try { 
                pqxx::work txn(*db.getConn());
                pqxx::result r = txn.exec_params("SELECT torce_flamengo, assiste_one_piece, de_sousa FROM cliente WHERE id = $1;", cliente_id);
                if (!r.empty()) {
                    if (r[0]["torce_flamengo"].as<bool>()) descontoAtual += 0.10;
                    if (r[0]["assiste_one_piece"].as<bool>()) descontoAtual += 0.10;
                    if (r[0]["de_sousa"].as<bool>()) descontoAtual += 0.10;
                }
            } catch (...) {} 
            db.desconectar();
        }
    }

    for (size_t i = 0; i < carrinhoAtual.size(); i++) {
        LVITEMA lvi;
        memset(&lvi, 0, sizeof(LVITEMA)); 
        lvi.mask = LVIF_TEXT; 
        lvi.iItem = i; 
        lvi.iSubItem = 0;
        
        double preco_com_desc = carrinhoAtual[i].preco_base * (1.0 - descontoAtual);
        carrinhoAtual[i].subtotal = carrinhoAtual[i].quantidade * preco_com_desc;
        
        char bufBase[50], bufDesc[50], bufSub[50]; 
        snprintf(bufBase, 50, "R$ %.2f", carrinhoAtual[i].preco_base); 
        snprintf(bufDesc, 50, "R$ %.2f", preco_com_desc); 
        snprintf(bufSub, 50, "R$ %.2f", carrinhoAtual[i].subtotal);
        
        lvi.pszText = (LPSTR)carrinhoAtual[i].nome.c_str(); 
        ListView_InsertItem(hListCarrinho, &lvi);
        
        ListView_SetItemText(hListCarrinho, i, 1, (LPSTR)std::to_string(carrinhoAtual[i].quantidade).c_str()); 
        ListView_SetItemText(hListCarrinho, i, 2, bufBase); 
        ListView_SetItemText(hListCarrinho, i, 3, bufDesc); 
        ListView_SetItemText(hListCarrinho, i, 4, bufSub); 
        
        valorTotalCarrinho += carrinhoAtual[i].subtotal;
    } 
    
    char bufTotal[150]; 
    if (descontoAtual > 0.0) {
        snprintf(bufTotal, 150, "TOTAL DA VENDA: R$ %.2f (Desconto Ativo: %.0f%%)", valorTotalCarrinho, descontoAtual * 100); 
    } else {
        snprintf(bufTotal, 150, "TOTAL DA VENDA: R$ %.2f", valorTotalCarrinho); 
    }
    SetWindowTextA(hTxtTotal, bufTotal);
}

// ==========================================
// TELA DO RELATORIO MENSAL (VIEW)
// ==========================================
LRESULT CALLBACK RelatorioMensalWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    static HWND hListRel;
    switch (msg) {
        case WM_CREATE: {
            SetClassLongPtr(hwnd, GCLP_HBRBACKGROUND, (LONG_PTR)hbrFundo);
            HFONT hFontBold = CreateFontA(18, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, "Segoe UI");
            
            CreateWindowExA(0, "STATIC", "RELATORIO MENSAL DE VENDAS", WS_CHILD | WS_VISIBLE, 20, 10, 300, 20, hwnd, NULL, NULL, NULL);

            hListRel = CreateWindowExA(WS_EX_CLIENTEDGE, WC_LISTVIEWA, "", WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL, 20, 40, 440, 300, hwnd, (HMENU)600, NULL, NULL);
            ListView_SetExtendedListViewStyle(hListRel, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);

            LVCOLUMNA lvc; 
            memset(&lvc, 0, sizeof(LVCOLUMNA)); 
            lvc.mask = LVCF_FMT | LVCF_WIDTH | LVCF_TEXT | LVCF_SUBITEM; 
            lvc.fmt = LVCFMT_LEFT;
            
            lvc.iSubItem = 0; lvc.cx = 120; lvc.pszText = (LPSTR)"Mes/Ano";           ListView_InsertColumn(hListRel, 0, &lvc);
            lvc.iSubItem = 1; lvc.cx = 140; lvc.pszText = (LPSTR)"Qtd. Vendas";       ListView_InsertColumn(hListRel, 1, &lvc);
            lvc.iSubItem = 2; lvc.cx = 160; lvc.pszText = (LPSTR)"Faturamento Total"; ListView_InsertColumn(hListRel, 2, &lvc);

            ListView_SetBkColor(hListRel, corCaixa); 
            ListView_SetTextBkColor(hListRel, corCaixa); 
            ListView_SetTextColor(hListRel, corTexto);

            if (db.conectar()) {
                try {
                    pqxx::work txn(*db.getConn());
                    
                    // CRIA A VIEW DO ZERO SE ELA NÃO EXISTIR (Garante a nota do PDF!)
                    txn.exec(R"(
                        CREATE OR REPLACE VIEW vw_relatorio_mensal AS
                        SELECT 
                            TO_CHAR(data_venda, 'YYYY-MM') AS mes_ano,
                            COUNT(id) AS quantidade_de_vendas,
                            SUM(valor_total) AS faturamento_total
                        FROM venda
                        GROUP BY TO_CHAR(data_venda, 'YYYY-MM')
                        ORDER BY mes_ano DESC;
                    )");
                    
                    // AGORA PUXA OS DADOS DA VIEW
                    pqxx::result r = txn.exec("SELECT * FROM vw_relatorio_mensal;");
                    int index = 0;
                    for(auto row : r) {
                        LVITEMA lvi; 
                        memset(&lvi, 0, sizeof(LVITEMA)); 
                        lvi.mask = LVIF_TEXT; 
                        lvi.iItem = index; 
                        lvi.iSubItem = 0;
                        
                        lvi.pszText = (LPSTR)row["mes_ano"].c_str(); 
                        ListView_InsertItem(hListRel, &lvi);
                        
                        ListView_SetItemText(hListRel, index, 1, (LPSTR)row["quantidade_de_vendas"].c_str());
                        
                        char bufT[50]; 
                        snprintf(bufT, 50, "R$ %.2f", row["faturamento_total"].as<double>());
                        ListView_SetItemText(hListRel, index, 2, bufT);
                        index++;
                    }
                    txn.commit();
                } catch(...) {}
                db.desconectar();
            }

            EnumChildWindows(hwnd, [](HWND child, LPARAM fonts) -> BOOL {
                SendMessage(child, WM_SETFONT, fonts, TRUE); return TRUE;
            }, (LPARAM)hFontBold);
            break;
        }
        case WM_CTLCOLORSTATIC: { 
            HDC hdcStatic = (HDC)wParam; 
            SetTextColor(hdcStatic, corTexto); 
            SetBkColor(hdcStatic, corFundo); 
            return (INT_PTR)hbrFundo; 
        }
        case WM_CLOSE: { 
            EnableWindow(hMainWnd, TRUE); 
            DestroyWindow(hwnd); 
            break; 
        }
        default: return DefWindowProcA(hwnd, msg, wParam, lParam);
    }
    return 0;
}

// ==========================================
// TELA DO HISTÓRICO DE VENDAS E DADOS DO CLIENTE
// ==========================================
LRESULT CALLBACK HistoricoWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    static HWND hListHist, hComboHistCliente, hTxtHistDados, hBtnHistBuscar, hListHistItens;
    
    switch (msg) {
        case WM_CREATE: {
            SetClassLongPtr(hwnd, GCLP_HBRBACKGROUND, (LONG_PTR)hbrFundo);
            HFONT hFontBold = CreateFontA(18, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, "Segoe UI");
            HFONT hFontNormal = CreateFontA(16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, "Segoe UI");

            CreateWindowExA(0, "STATIC", "HISTORICO DE VENDAS", WS_CHILD | WS_VISIBLE, 20, 10, 300, 20, hwnd, NULL, NULL, NULL);
            
            CreateWindowExA(0, "STATIC", "Filtrar por Cliente:", WS_CHILD | WS_VISIBLE, 20, 45, 130, 20, hwnd, NULL, NULL, NULL);
            hComboHistCliente = CreateWindowExA(0, "COMBOBOX", "", WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL, 150, 40, 250, 150, hwnd, (HMENU)ID_COMBO_HIST_CLIENTE, NULL, NULL);
            hBtnHistBuscar = CreateWindowExA(0, "BUTTON", "Buscar", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW, 410, 40, 80, 25, hwnd, (HMENU)ID_BTN_HIST_BUSCAR, NULL, NULL);

            hTxtHistDados = CreateWindowExA(0, "STATIC", "Selecione um cliente para ver os dados cadastrais.", WS_CHILD | WS_VISIBLE, 20, 80, 690, 40, hwnd, (HMENU)ID_TXT_HIST_DADOS, NULL, NULL);

            // Tabela 1: Resumo das Vendas
            hListHist = CreateWindowExA(WS_EX_CLIENTEDGE, WC_LISTVIEWA, "", WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS, 20, 130, 690, 170, hwnd, (HMENU)500, NULL, NULL);
            ListView_SetExtendedListViewStyle(hListHist, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);

            LVCOLUMNA lvc;
            memset(&lvc, 0, sizeof(LVCOLUMNA)); 
            lvc.mask = LVCF_FMT | LVCF_WIDTH | LVCF_TEXT | LVCF_SUBITEM; 
            lvc.fmt = LVCFMT_LEFT;
            
            lvc.iSubItem = 0; lvc.cx = 40;  lvc.pszText = (LPSTR)"ID";        ListView_InsertColumn(hListHist, 0, &lvc);
            lvc.iSubItem = 1; lvc.cx = 140; lvc.pszText = (LPSTR)"Data";      ListView_InsertColumn(hListHist, 1, &lvc);
            lvc.iSubItem = 2; lvc.cx = 150; lvc.pszText = (LPSTR)"Cliente";   ListView_InsertColumn(hListHist, 2, &lvc);
            lvc.iSubItem = 3; lvc.cx = 140; lvc.pszText = (LPSTR)"Vendedor";  ListView_InsertColumn(hListHist, 3, &lvc);
            lvc.iSubItem = 4; lvc.cx = 100; lvc.pszText = (LPSTR)"Total";     ListView_InsertColumn(hListHist, 4, &lvc);
            lvc.iSubItem = 5; lvc.cx = 100; lvc.pszText = (LPSTR)"Pagamento"; ListView_InsertColumn(hListHist, 5, &lvc);

            ListView_SetBkColor(hListHist, corCaixa); 
            ListView_SetTextBkColor(hListHist, corCaixa); 
            ListView_SetTextColor(hListHist, corTexto);

            // Título Tabela 2
            CreateWindowExA(0, "STATIC", "ITENS DA VENDA SELECIONADA:", WS_CHILD | WS_VISIBLE, 20, 315, 300, 20, hwnd, NULL, NULL, NULL);

            // Tabela 2: Itens da Venda
            hListHistItens = CreateWindowExA(WS_EX_CLIENTEDGE, WC_LISTVIEWA, "", WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL, 20, 340, 690, 160, hwnd, (HMENU)ID_LIST_HIST_ITENS, NULL, NULL);
            ListView_SetExtendedListViewStyle(hListHistItens, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);

            LVCOLUMNA lvcItens;
            memset(&lvcItens, 0, sizeof(LVCOLUMNA)); 
            lvcItens.mask = LVCF_FMT | LVCF_WIDTH | LVCF_TEXT | LVCF_SUBITEM; 
            lvcItens.fmt = LVCFMT_LEFT;
            
            lvcItens.iSubItem = 0; lvcItens.cx = 280; lvcItens.pszText = (LPSTR)"Produto";     ListView_InsertColumn(hListHistItens, 0, &lvcItens);
            lvcItens.iSubItem = 1; lvcItens.cx = 50;  lvcItens.pszText = (LPSTR)"Qtd";         ListView_InsertColumn(hListHistItens, 1, &lvcItens);
            lvcItens.iSubItem = 2; lvcItens.cx = 110; lvcItens.pszText = (LPSTR)"Preco Base";  ListView_InsertColumn(hListHistItens, 2, &lvcItens);
            lvcItens.iSubItem = 3; lvcItens.cx = 110; lvcItens.pszText = (LPSTR)"Preco Pago";  ListView_InsertColumn(hListHistItens, 3, &lvcItens);
            lvcItens.iSubItem = 4; lvcItens.cx = 120; lvcItens.pszText = (LPSTR)"Subtotal";    ListView_InsertColumn(hListHistItens, 4, &lvcItens);

            ListView_SetBkColor(hListHistItens, corCaixa); 
            ListView_SetTextBkColor(hListHistItens, corCaixa); 
            ListView_SetTextColor(hListHistItens, corTexto);
            
            // Popula Combo de Clientes no Histórico
            SendMessageA(hComboHistCliente, CB_ADDSTRING, 0, (LPARAM)"-- TODOS OS CLIENTES --");
            SendMessage(hComboHistCliente, CB_SETITEMDATA, 0, 0); 
            SendMessage(hComboHistCliente, CB_SETCURSEL, 0, 0);
            
            if (db.conectar()) {
                try { 
                    pqxx::work txn(*db.getConn()); 
                    pqxx::result rc = txn.exec("SELECT id, nome FROM cliente ORDER BY nome;");
                    for (auto row : rc) { 
                        int idx = SendMessageA(hComboHistCliente, CB_ADDSTRING, 0, (LPARAM)row["nome"].c_str()); 
                        SendMessage(hComboHistCliente, CB_SETITEMDATA, idx, row["id"].as<int>()); 
                    }
                } catch (...) {} 
                db.desconectar();
            }

            EnumChildWindows(hwnd, [](HWND child, LPARAM fonts) -> BOOL {
                HFONT* pFonts = (HFONT*)fonts; 
                int id = GetWindowLong(child, GWL_ID);
                if (id == ID_TXT_HIST_DADOS) {
                    SendMessage(child, WM_SETFONT, (WPARAM)pFonts[1], TRUE); 
                } else {
                    SendMessage(child, WM_SETFONT, (WPARAM)pFonts[0], TRUE); 
                }
                return TRUE;
            }, (LPARAM)(HFONT[]){hFontBold, hFontNormal});

            // Dispara busca inicial
            SendMessage(hwnd, WM_COMMAND, MAKEWPARAM(ID_BTN_HIST_BUSCAR, 0), 0); 
            break;
        }
        
        // EVENTO: Quando Clicar em uma Venda na Lista de cima, carregar os itens na lista de baixo!
        case WM_NOTIFY: {
            LPNMHDR lpnmh = (LPNMHDR)lParam;
            if (lpnmh->idFrom == 500 && (lpnmh->code == LVN_ITEMCHANGED || lpnmh->code == NM_CLICK)) {
                LPNMLISTVIEW pnmv = (LPNMLISTVIEW)lParam;
                if (pnmv->uNewState & LVIS_SELECTED) {
                    int index = pnmv->iItem;
                    char idBuf[50];
                    ListView_GetItemText(hListHist, index, 0, idBuf, 50);
                    std::string venda_id = idBuf;
                    
                    ListView_DeleteAllItems(hListHistItens); 
                    
                    if (db.conectar()) {
                        try {
                            pqxx::work txn(*db.getConn());
                            // Nova query! Puxa o preço original do produto e o preço pago gravado no item_venda
                            std::string sql = "SELECT p.nome, iv.quantidade, p.preco as preco_base, iv.preco_unitario as preco_pago, (iv.quantidade * iv.preco_unitario) as subtotal "
                                              "FROM item_venda iv JOIN produto p ON iv.produto_id = p.id "
                                              "WHERE iv.venda_id = " + txn.quote(venda_id) + ";";
                            pqxx::result r = txn.exec(sql);
                            
                            int i = 0;
                            for (auto row : r) {
                                LVITEMA lvi;
                                memset(&lvi, 0, sizeof(LVITEMA)); 
                                lvi.mask = LVIF_TEXT; 
                                lvi.iItem = i; 
                                lvi.iSubItem = 0;
                                
                                lvi.pszText = (LPSTR)row["nome"].c_str(); 
                                ListView_InsertItem(hListHistItens, &lvi);
                                
                                ListView_SetItemText(hListHistItens, i, 1, (LPSTR)row["quantidade"].c_str());
                                
                                char bufBase[50], bufPago[50], bufSub[50];
                                snprintf(bufBase, 50, "R$ %.2f", row["preco_base"].as<double>());
                                snprintf(bufPago, 50, "R$ %.2f", row["preco_pago"].as<double>());
                                snprintf(bufSub, 50, "R$ %.2f", row["subtotal"].as<double>());
                                
                                ListView_SetItemText(hListHistItens, i, 2, bufBase);
                                ListView_SetItemText(hListHistItens, i, 3, bufPago);
                                ListView_SetItemText(hListHistItens, i, 4, bufSub);
                                i++;
                            }
                        } catch (...) {}
                        db.desconectar();
                    }
                }
            }
            break;
        }

        case WM_DRAWITEM: { 
            LPDRAWITEMSTRUCT pdis = (LPDRAWITEMSTRUCT)lParam;
            if (pdis->CtlType == ODT_BUTTON) {
                // Azul pastel no escuro, Azul Royal no claro
                COLORREF corBtn = modoEscuro ? RGB(137, 180, 250) : RGB(30, 102, 245); 
                
                if (pdis->itemState & ODS_SELECTED) {
                    int r = GetRValue(corBtn) - 30; if (r < 0) r = 0;
                    int g = GetGValue(corBtn) - 30; if (g < 0) g = 0;
                    int b = GetBValue(corBtn) - 30; if (b < 0) b = 0;
                    corBtn = RGB(r, g, b);
                }
                
                HBRUSH hBrush = CreateSolidBrush(corBtn); 
                FillRect(pdis->hDC, &pdis->rcItem, hBrush); 
                DeleteObject(hBrush);
                
                SetBkMode(pdis->hDC, TRANSPARENT); 
                SetTextColor(pdis->hDC, modoEscuro ? RGB(17, 17, 27) : RGB(255, 255, 255)); 
                char textoBotao[256]; 
                GetWindowTextA(pdis->hwndItem, textoBotao, 256); 
                DrawTextA(pdis->hDC, textoBotao, -1, &pdis->rcItem, DT_CENTER | DT_VCENTER | DT_SINGLELINE); 
                return TRUE;
            } 
            break;
        }
        case WM_COMMAND: {
            if (LOWORD(wParam) == ID_BTN_HIST_BUSCAR) {
                int comboIdx = SendMessage(hComboHistCliente, CB_GETCURSEL, 0, 0); 
                int cliente_id = 0;
                
                if (comboIdx != CB_ERR) {
                    cliente_id = SendMessage(hComboHistCliente, CB_GETITEMDATA, comboIdx, 0);
                }
                
                ListView_DeleteAllItems(hListHist);
                ListView_DeleteAllItems(hListHistItens); // Limpa os itens ao buscar

                if (db.conectar()) {
                    try {
                        pqxx::work txn(*db.getConn());
                        
                        if (cliente_id > 0) { 
                            // Busca e mostra os dados do cliente selecionado
                            pqxx::result r_cli = txn.exec_params("SELECT * FROM cliente WHERE id = $1;", cliente_id);
                            if (!r_cli.empty()) {
                                std::string nome = r_cli[0]["nome"].c_str(); 
                                std::string cpf = !r_cli[0]["cpf"].is_null() ? r_cli[0]["cpf"].c_str() : "Nao informado"; 
                                std::string tel = !r_cli[0]["telefone"].is_null() ? r_cli[0]["telefone"].c_str() : "Nao informado";
                                std::string fla = r_cli[0]["torce_flamengo"].as<bool>() ? "Sim" : "Nao"; 
                                std::string op = r_cli[0]["assiste_one_piece"].as<bool>() ? "Sim" : "Nao"; 
                                std::string sou = r_cli[0]["de_sousa"].as<bool>() ? "Sim" : "Nao";
                                
                                char buf[512]; 
                                snprintf(buf, 512, "Cliente: %s | CPF: %s | Tel: %s\nBeneficios: Fla [%s] | One Piece [%s] | Sousa [%s]", 
                                         nome.c_str(), cpf.c_str(), tel.c_str(), fla.c_str(), op.c_str(), sou.c_str());
                                SetWindowTextA(hTxtHistDados, buf);
                            }
                        } else { 
                            SetWindowTextA(hTxtHistDados, "Exibindo historico geral. Selecione um cliente para ver seus dados cadastrais e suas compras."); 
                        }

                        // Monta a query das vendas (com filtro opcional de cliente)
                        std::string sql = "SELECT v.id, CAST(v.data_venda AS TEXT) as data_venda, "
                                          "COALESCE(c.nome, 'CLIENTE DELETADO') as cliente, "
                                          "COALESCE(vd.nome, 'VEND. DELETADO') as vendedor, "
                                          "v.valor_total, v.forma_pagamento "
                                          "FROM venda v "
                                          "LEFT JOIN cliente c ON v.cliente_id = c.id "
                                          "LEFT JOIN vendedor vd ON v.vendedor_id = vd.id ";
                                          
                        if (cliente_id > 0) {
                            sql += "WHERE v.cliente_id = " + std::to_string(cliente_id) + " ";
                        }
                        sql += "ORDER BY v.id DESC;";

                        pqxx::result r = txn.exec(sql); 
                        int index = 0;
                        for (auto row : r) {
                            LVITEMA lvi;
                            memset(&lvi, 0, sizeof(LVITEMA)); 
                            lvi.mask = LVIF_TEXT; 
                            lvi.iItem = index; 
                            lvi.iSubItem = 0; 
                            
                            std::string idStr = row["id"].c_str(); 
                            lvi.pszText = (LPSTR)idStr.c_str(); 
                            ListView_InsertItem(hListHist, &lvi);
                            
                            std::string dataStr = row["data_venda"].c_str(); 
                            if(dataStr.length() > 16) {
                                dataStr = dataStr.substr(0, 16); 
                            }
                            
                            ListView_SetItemText(hListHist, index, 1, (LPSTR)dataStr.c_str()); 
                            ListView_SetItemText(hListHist, index, 2, (LPSTR)row["cliente"].c_str()); 
                            ListView_SetItemText(hListHist, index, 3, (LPSTR)row["vendedor"].c_str()); 
                            
                            char bufTotal[50]; 
                            snprintf(bufTotal, 50, "R$ %.2f", row["valor_total"].as<double>()); 
                            
                            ListView_SetItemText(hListHist, index, 4, bufTotal); 
                            ListView_SetItemText(hListHist, index, 5, (LPSTR)row["forma_pagamento"].c_str()); 
                            index++;
                        }
                    } catch (...) {} 
                    db.desconectar();
                }
            } 
            break;
        }
        case WM_CTLCOLORSTATIC: { 
            HDC hdcStatic = (HDC)wParam; 
            SetTextColor(hdcStatic, corTexto); 
            SetBkColor(hdcStatic, corFundo); 
            return (INT_PTR)hbrFundo; 
        }
        case WM_CLOSE: { 
            EnableWindow(hMainWnd, TRUE); 
            DestroyWindow(hwnd); 
            break; 
        }
        default: return DefWindowProcA(hwnd, msg, wParam, lParam);
    } 
    return 0;
}

// ==========================================
// JANELAS DE CADASTRO RÁPIDO (CLIENTE E VENDEDOR)
// ==========================================
LRESULT CALLBACK CadastroClienteWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    static HWND hNomeCli, hCpfCli, hTelCli, hChkFla, hChkOne, hChkSou;
    switch (msg) {
        case WM_CREATE: {
            SetClassLongPtr(hwnd, GCLP_HBRBACKGROUND, (LONG_PTR)hbrFundo);
            CreateWindowExA(0, "STATIC", "Nome:", WS_CHILD | WS_VISIBLE, 20, 20, 50, 20, hwnd, NULL, NULL, NULL);
            hNomeCli = CreateWindowExA(0, "EDIT", "", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL, 80, 20, 180, 25, hwnd, (HMENU)ID_EDIT_CLI_NOME, NULL, NULL);
            
            CreateWindowExA(0, "STATIC", "CPF:", WS_CHILD | WS_VISIBLE, 20, 60, 50, 20, hwnd, NULL, NULL, NULL); 
            hCpfCli = CreateWindowExA(0, "EDIT", "", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL | ES_NUMBER, 80, 60, 180, 25, hwnd, (HMENU)ID_EDIT_CLI_CPF, NULL, NULL);
            SendMessage(hCpfCli, EM_LIMITTEXT, 11, 0);
            
            CreateWindowExA(0, "STATIC", "Tel:", WS_CHILD | WS_VISIBLE, 20, 100, 50, 20, hwnd, NULL, NULL, NULL); 
            hTelCli = CreateWindowExA(0, "EDIT", "", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL | ES_NUMBER, 80, 100, 180, 25, hwnd, (HMENU)ID_EDIT_CLI_TEL, NULL, NULL);
            
            hChkFla = CreateWindowExA(0, "BUTTON", "Torce Flamengo", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 80, 140, 150, 20, hwnd, (HMENU)ID_CHK_FLAMENGO, NULL, NULL);
            hChkOne = CreateWindowExA(0, "BUTTON", "Assiste One Piece", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 80, 170, 150, 20, hwnd, (HMENU)ID_CHK_ONEPIECE, NULL, NULL);
            hChkSou = CreateWindowExA(0, "BUTTON", "Nasceu em Sousa", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 80, 200, 150, 20, hwnd, (HMENU)ID_CHK_SOUSA, NULL, NULL);

            CreateWindowExA(0, "BUTTON", "Salvar Cliente", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 80, 240, 120, 35, hwnd, (HMENU)ID_BTN_SALVAR_CLI, NULL, NULL);
            break;
        }
        case WM_COMMAND: {
            if (LOWORD(wParam) == ID_BTN_SALVAR_CLI) {
                std::string nome = ObterTexto(hNomeCli);
                std::string cpf = ObterTexto(hCpfCli);
                std::string tel = ObterTexto(hTelCli);
                
                bool fla = SendMessage(hChkFla, BM_GETCHECK, 0, 0) == BST_CHECKED;
                bool op = SendMessage(hChkOne, BM_GETCHECK, 0, 0) == BST_CHECKED;
                bool sou = SendMessage(hChkSou, BM_GETCHECK, 0, 0) == BST_CHECKED;

                if (nome.empty()) { 
                    MessageBoxA(hwnd, "O Nome e obrigatorio!", "Aviso", MB_ICONWARNING); 
                    break; 
                }

                if (db.conectar()) {
                    try {
                        pqxx::work txn(*db.getConn());
                        txn.exec_params(
                            "INSERT INTO cliente (nome, cpf, telefone, torce_flamengo, assiste_one_piece, de_sousa) VALUES ($1, $2, $3, $4, $5, $6);", 
                            nome, 
                            cpf.empty() ? nullptr : cpf.c_str(), 
                            tel.empty() ? nullptr : tel.c_str(), 
                            fla, op, sou
                        );
                        txn.commit(); 
                        MessageBoxA(hwnd, "Cliente cadastrado com sucesso!", "OK", MB_OK);
                        RecarregarCombosPessoas(); 
                        SendMessage(hwnd, WM_CLOSE, 0, 0);
                    } catch (const std::exception& e) { 
                        MessageBoxA(hwnd, e.what(), "Erro", MB_ICONERROR); 
                    } 
                    db.desconectar();
                }
            } 
            break;
        }
        case WM_CLOSE: {
            EnableWindow(hwndPDV, TRUE); 
            DestroyWindow(hwnd); 
            break;
        }
        default: return DefWindowProcA(hwnd, msg, wParam, lParam);
    } 
    return 0;
}

LRESULT CALLBACK CadastroVendedorWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    static HWND hNomeVend;
    switch (msg) {
        case WM_CREATE: {
            SetClassLongPtr(hwnd, GCLP_HBRBACKGROUND, (LONG_PTR)hbrFundo);
            CreateWindowExA(0, "STATIC", "Nome:", WS_CHILD | WS_VISIBLE, 20, 20, 50, 20, hwnd, NULL, NULL, NULL);
            hNomeVend = CreateWindowExA(0, "EDIT", "", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL, 80, 20, 180, 25, hwnd, (HMENU)ID_EDIT_VEND_NOME, NULL, NULL);
            
            CreateWindowExA(0, "BUTTON", "Salvar Vendedor", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 80, 60, 120, 35, hwnd, (HMENU)ID_BTN_SALVAR_VEND, NULL, NULL);
            break;
        }
        case WM_COMMAND: {
            if (LOWORD(wParam) == ID_BTN_SALVAR_VEND) {
                std::string nome = ObterTexto(hNomeVend);
                if (nome.empty()) { 
                    MessageBoxA(hwnd, "O Nome e obrigatorio!", "Aviso", MB_ICONWARNING); 
                    break; 
                }
                
                if (db.conectar()) {
                    try { 
                        pqxx::work txn(*db.getConn()); 
                        txn.exec_params("INSERT INTO vendedor (nome) VALUES ($1);", nome); 
                        txn.commit();
                        MessageBoxA(hwnd, "Vendedor cadastrado com sucesso!", "OK", MB_OK); 
                        RecarregarCombosPessoas(); 
                        SendMessage(hwnd, WM_CLOSE, 0, 0);
                    } catch (const std::exception& e) { 
                        MessageBoxA(hwnd, e.what(), "Erro", MB_ICONERROR); 
                    } 
                    db.desconectar();
                }
            } 
            break;
        }
        case WM_CLOSE: {
            EnableWindow(hwndPDV, TRUE); 
            DestroyWindow(hwnd); 
            break;
        }
        default: return DefWindowProcA(hwnd, msg, wParam, lParam);
    } 
    return 0;
}

// ==========================================
// A MÁGICA DO CAIXA: PROCEDIMENTO DO PDV
// ==========================================
LRESULT CALLBACK PDVWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE: {
            SetClassLongPtr(hwnd, GCLP_HBRBACKGROUND, (LONG_PTR)hbrFundo);
            HFONT hFontBold = CreateFontA(18, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, "Segoe UI");
            HFONT hFontNormal = CreateFontA(16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, "Segoe UI");
            HFONT hFontTotal = CreateFontA(24, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, "Segoe UI");

            CreateWindowExA(0, "STATIC", "NOVA VENDA", WS_CHILD | WS_VISIBLE, 20, 10, 200, 20, hwnd, NULL, NULL, NULL);

            CreateWindowExA(0, "STATIC", "Cliente:", WS_CHILD | WS_VISIBLE, 20, 40, 70, 20, hwnd, NULL, NULL, NULL);
            hComboCliente = CreateWindowExA(0, "COMBOBOX", "", WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL, 100, 40, 180, 150, hwnd, (HMENU)ID_COMBO_CLIENTE, NULL, NULL);
            CreateWindowExA(0, "BUTTON", "+", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON, 285, 40, 25, 25, hwnd, (HMENU)ID_BTN_NOVO_CLIENTE, NULL, NULL);

            CreateWindowExA(0, "STATIC", "Vendedor:", WS_CHILD | WS_VISIBLE, 20, 75, 75, 20, hwnd, NULL, NULL, NULL);
            hComboVendedor = CreateWindowExA(0, "COMBOBOX", "", WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL, 100, 75, 180, 150, hwnd, (HMENU)ID_COMBO_VENDEDOR, NULL, NULL);
            CreateWindowExA(0, "BUTTON", "+", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON, 285, 75, 25, 25, hwnd, (HMENU)ID_BTN_NOVO_VENDEDOR, NULL, NULL);

            CreateWindowExA(0, "STATIC", "Produto:", WS_CHILD | WS_VISIBLE, 20, 120, 70, 20, hwnd, NULL, NULL, NULL);
            hComboProduto = CreateWindowExA(0, "COMBOBOX", "", WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL, 100, 120, 210, 200, hwnd, (HMENU)ID_COMBO_PRODUTO, NULL, NULL);

            CreateWindowExA(0, "STATIC", "Qtd:", WS_CHILD | WS_VISIBLE, 20, 155, 40, 20, hwnd, NULL, NULL, NULL);
            hEditQtdVenda = CreateWindowExA(0, "EDIT", "1", WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_NUMBER | WS_BORDER, 100, 155, 60, 25, hwnd, (HMENU)ID_EDIT_QTD_VENDA, NULL, NULL);

            CreateWindowExA(0, "BUTTON", "Adicionar ao Carrinho", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW, 100, 195, 210, 35, hwnd, (HMENU)ID_BTN_ADD_CARRINHO, NULL, NULL);

            CreateWindowExA(0, "STATIC", "CARRINHO DE COMPRAS", WS_CHILD | WS_VISIBLE, 330, 10, 200, 20, hwnd, NULL, NULL, NULL);
            
            hListCarrinho = CreateWindowExA(WS_EX_CLIENTEDGE, WC_LISTVIEWA, "", WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL, 330, 40, 330, 250, hwnd, (HMENU)ID_LIST_CARRINHO, NULL, NULL);
            ListView_SetExtendedListViewStyle(hListCarrinho, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);

            LVCOLUMNA lvc; 
            memset(&lvc, 0, sizeof(LVCOLUMNA)); 
            lvc.mask = LVCF_FMT | LVCF_WIDTH | LVCF_TEXT | LVCF_SUBITEM; 
            lvc.fmt = LVCFMT_LEFT;
            
            lvc.iSubItem = 0; lvc.cx = 100; lvc.pszText = (LPSTR)"Produto";  ListView_InsertColumn(hListCarrinho, 0, &lvc);
            lvc.iSubItem = 1; lvc.cx = 40;  lvc.pszText = (LPSTR)"Qtd";      ListView_InsertColumn(hListCarrinho, 1, &lvc);
            lvc.iSubItem = 2; lvc.cx = 60;  lvc.pszText = (LPSTR)"R$ Base";  ListView_InsertColumn(hListCarrinho, 2, &lvc);
            lvc.iSubItem = 3; lvc.cx = 60;  lvc.pszText = (LPSTR)"R$ Desc";  ListView_InsertColumn(hListCarrinho, 3, &lvc);
            lvc.iSubItem = 4; lvc.cx = 70;  lvc.pszText = (LPSTR)"Subtotal"; ListView_InsertColumn(hListCarrinho, 4, &lvc);

            hTxtTotal = CreateWindowExA(0, "STATIC", "TOTAL DA VENDA: R$ 0.00", WS_CHILD | WS_VISIBLE | SS_RIGHT, 330, 300, 330, 30, hwnd, (HMENU)ID_TXT_TOTAL_CARRINHO, NULL, NULL);

            CreateWindowExA(0, "STATIC", "Pagamento:", WS_CHILD | WS_VISIBLE, 330, 340, 90, 20, hwnd, NULL, NULL, NULL);
            hComboPagamento = CreateWindowExA(0, "COMBOBOX", "", WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL, 430, 340, 230, 150, hwnd, (HMENU)ID_COMBO_PAGAMENTO, NULL, NULL);

            CreateWindowExA(0, "BUTTON", "FINALIZAR VENDA", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW, 330, 380, 330, 45, hwnd, (HMENU)ID_BTN_FINALIZAR_VENDA, NULL, NULL);

            ListView_SetBkColor(hListCarrinho, corCaixa); 
            ListView_SetTextBkColor(hListCarrinho, corCaixa); 
            ListView_SetTextColor(hListCarrinho, corTexto);

            RecarregarCombosPessoas();
            
            if (db.conectar()) {
                try {
                    pqxx::work txn(*db.getConn());
                    pqxx::result rp = txn.exec("SELECT id, nome, preco FROM produto WHERE estoque > 0 ORDER BY nome;");
                    for (auto row : rp) {
                        std::string txtExibicao = std::string(row["nome"].c_str()) + " (R$ " + row["preco"].c_str() + ")";
                        int idx = SendMessageA(hComboProduto, CB_ADDSTRING, 0, (LPARAM)txtExibicao.c_str());
                        SendMessage(hComboProduto, CB_SETITEMDATA, idx, row["id"].as<int>());
                    }
                } catch (...) {}
                db.desconectar();
            }

            SendMessageA(hComboPagamento, CB_ADDSTRING, 0, (LPARAM)"Cartao"); 
            SendMessageA(hComboPagamento, CB_ADDSTRING, 0, (LPARAM)"Boleto"); 
            SendMessageA(hComboPagamento, CB_ADDSTRING, 0, (LPARAM)"Pix"); 
            SendMessageA(hComboPagamento, CB_ADDSTRING, 0, (LPARAM)"Berries");

            EnumChildWindows(hwnd, [](HWND child, LPARAM fonts) -> BOOL {
                HFONT* pFonts = (HFONT*)fonts; 
                char className[256]; 
                GetClassNameA(child, className, 256); 
                int id = GetWindowLong(child, GWL_ID);
                if (id == ID_TXT_TOTAL_CARRINHO) {
                    SendMessage(child, WM_SETFONT, (WPARAM)pFonts[2], TRUE);
                } else if (std::string(className) == "STATIC") {
                    SendMessage(child, WM_SETFONT, (WPARAM)pFonts[0], TRUE); 
                } else {
                    SendMessage(child, WM_SETFONT, (WPARAM)pFonts[1], TRUE);
                }
                return TRUE;
            }, (LPARAM)(HFONT[]){hFontBold, hFontNormal, hFontTotal});

            carrinhoAtual.clear(); 
            AtualizarListaCarrinho(); 
            break;
        }

        case WM_CTLCOLORSTATIC: { 
            HDC hdcStatic = (HDC)wParam; 
            SetTextColor(hdcStatic, corTexto); 
            SetBkColor(hdcStatic, corFundo); 
            return (INT_PTR)hbrFundo; 
        }
        case WM_CTLCOLOREDIT: 
        case WM_CTLCOLORLISTBOX: { 
            HDC hdcEdit = (HDC)wParam; 
            SetTextColor(hdcEdit, corTexto); 
            SetBkColor(hdcEdit, corCaixa); 
            return (INT_PTR)hbrCaixaTexto; 
        }
        case WM_DRAWITEM: {
            LPDRAWITEMSTRUCT pdis = (LPDRAWITEMSTRUCT)lParam;
            if (pdis->CtlType == ODT_BUTTON) {
                COLORREF corBtn;
                if (modoEscuro) {
                    if (pdis->CtlID == ID_BTN_ADD_CARRINHO) corBtn = RGB(137, 180, 250); 
                    else if (pdis->CtlID == ID_BTN_FINALIZAR_VENDA) corBtn = RGB(166, 227, 161); 
                    else corBtn = RGB(147, 153, 178);
                } else {
                    if (pdis->CtlID == ID_BTN_ADD_CARRINHO) corBtn = RGB(30, 102, 245); // Azul Royal
                    else if (pdis->CtlID == ID_BTN_FINALIZAR_VENDA) corBtn = RGB(64, 160, 43); // Verde Escuro
                    else corBtn = RGB(140, 143, 161);
                }

                if (pdis->itemState & ODS_SELECTED) {
                    int r = GetRValue(corBtn) - 30; if (r < 0) r = 0;
                    int g = GetGValue(corBtn) - 30; if (g < 0) g = 0;
                    int b = GetBValue(corBtn) - 30; if (b < 0) b = 0;
                    corBtn = RGB(r, g, b);
                }
                
                HBRUSH hBrush = CreateSolidBrush(corBtn); 
                FillRect(pdis->hDC, &pdis->rcItem, hBrush); 
                DeleteObject(hBrush);
                
                SetBkMode(pdis->hDC, TRANSPARENT); 
                SetTextColor(pdis->hDC, modoEscuro ? RGB(17, 17, 27) : RGB(255, 255, 255)); 
                char textoBotao[256]; 
                GetWindowTextA(pdis->hwndItem, textoBotao, 256); 
                DrawTextA(pdis->hDC, textoBotao, -1, &pdis->rcItem, DT_CENTER | DT_VCENTER | DT_SINGLELINE); 
                return TRUE;
            }
            break;
        }

        case WM_COMMAND: {
            int wmId = LOWORD(wParam);
            int wmEvent = HIWORD(wParam);

            if (wmId == ID_COMBO_CLIENTE && wmEvent == CBN_SELCHANGE) {
                AtualizarListaCarrinho(); 
            }

            if (wmId == ID_BTN_NOVO_CLIENTE) {
                EnableWindow(hwnd, FALSE); 
                CreateWindowExA(WS_EX_DLGMODALFRAME, "CadastroClienteClass", "Novo Cliente", 
                                WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_VISIBLE, 
                                GetSystemMetrics(SM_CXSCREEN)/2 - 150, GetSystemMetrics(SM_CYSCREEN)/2 - 150, 
                                300, 330, hwnd, NULL, GetModuleHandle(NULL), NULL);
            }
            if (wmId == ID_BTN_NOVO_VENDEDOR) {
                EnableWindow(hwnd, FALSE); 
                CreateWindowExA(WS_EX_DLGMODALFRAME, "CadastroVendedorClass", "Novo Vendedor", 
                                WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_VISIBLE, 
                                GetSystemMetrics(SM_CXSCREEN)/2 - 150, GetSystemMetrics(SM_CYSCREEN)/2 - 100, 
                                300, 150, hwnd, NULL, GetModuleHandle(NULL), NULL);
            }
            
            if (wmId == ID_BTN_ADD_CARRINHO) {
                int comboIdx = SendMessage(hComboProduto, CB_GETCURSEL, 0, 0); 
                std::string strQtd = ObterTexto(hEditQtdVenda);
                
                if (comboIdx == CB_ERR || strQtd.empty() || std::stoi(strQtd) <= 0) { 
                    MessageBoxA(hwnd, "Selecione um produto e digite uma quantidade valida!", "Aviso", MB_ICONWARNING); 
                    break; 
                }
                
                int id_prod = SendMessage(hComboProduto, CB_GETITEMDATA, comboIdx, 0); 
                int qtd = std::stoi(strQtd);
                
                if (db.conectar()) {
                    try {
                        pqxx::work txn(*db.getConn()); 
                        pqxx::result r = txn.exec("SELECT nome, preco, estoque FROM produto WHERE id = " + std::to_string(id_prod));
                        if (!r.empty()) { 
                            int est = r[0]["estoque"].as<int>();
                            
                            int qtd_ja_no_carrinho = 0;
                            int index_existente = -1;
                            for (size_t i = 0; i < carrinhoAtual.size(); i++) {
                                if (carrinhoAtual[i].produto_id == id_prod) {
                                    qtd_ja_no_carrinho += carrinhoAtual[i].quantidade;
                                    index_existente = i;
                                }
                            }

                            if (qtd + qtd_ja_no_carrinho > est) { 
                                std::string msgErro = "Estoque insuficiente!\nTemos apenas " + std::to_string(est) + " unidades desse produto.";
                                if (qtd_ja_no_carrinho > 0) {
                                    msgErro += "\n\n(Atenção: Voce ja adicionou " + std::to_string(qtd_ja_no_carrinho) + " unidades dele no carrinho atual).";
                                }
                                MessageBoxA(hwnd, msgErro.c_str(), "Erro de Estoque", MB_ICONERROR); 
                            } else { 
                                if (index_existente != -1) {
                                    carrinhoAtual[index_existente].quantidade += qtd;
                                } else {
                                    ItemCarrinhoUI item; 
                                    item.produto_id = id_prod; 
                                    item.nome = r[0]["nome"].c_str(); 
                                    item.quantidade = qtd; 
                                    item.preco_base = r[0]["preco"].as<double>(); 
                                    carrinhoAtual.push_back(item); 
                                }
                                AtualizarListaCarrinho(); 
                                SetWindowTextA(hEditQtdVenda, "1"); 
                            }
                        }
                    } catch (...) {} 
                    db.desconectar();
                }
            }

            if (wmId == ID_BTN_FINALIZAR_VENDA) {
                int idxCliente = SendMessage(hComboCliente, CB_GETCURSEL, 0, 0); 
                int idxVendedor = SendMessage(hComboVendedor, CB_GETCURSEL, 0, 0); 
                int idxPgto = SendMessage(hComboPagamento, CB_GETCURSEL, 0, 0);
                
                if (idxCliente == CB_ERR || idxVendedor == CB_ERR || idxPgto == CB_ERR) { 
                    MessageBoxA(hwnd, "Selecione Cliente, Vendedor e Forma de Pagamento!", "Aviso", MB_ICONWARNING); 
                    break; 
                }
                if (carrinhoAtual.empty()) { 
                    MessageBoxA(hwnd, "O carrinho esta vazio!", "Aviso", MB_ICONWARNING); 
                    break; 
                }

                int cliente_id = SendMessage(hComboCliente, CB_GETITEMDATA, idxCliente, 0); 
                int vendedor_id = SendMessage(hComboVendedor, CB_GETITEMDATA, idxVendedor, 0); 
                char pagBuf[50]; SendMessageA(hComboPagamento, CB_GETLBTEXT, idxPgto, (LPARAM)pagBuf);
                
                if (db.conectar()) {
                    try {
                        pqxx::work txn(*db.getConn());
                        pqxx::result r_venda = txn.exec_params(
                            "INSERT INTO venda (cliente_id, vendedor_id, forma_pagamento, status_pagamento, valor_total) "
                            "VALUES ($1, $2, $3, 'Confirmado', 0.00) RETURNING id;", 
                            cliente_id, vendedor_id, pagBuf
                        );
                        
                        int venda_id = r_venda[0][0].as<int>();
                        for (const auto& item : carrinhoAtual) { 
                            txn.exec_params("CALL adicionar_item_venda($1, $2, $3);", venda_id, item.produto_id, item.quantidade); 
                        }
                        
                        txn.commit(); 
                        AdicionarLog("Venda #" + std::to_string(venda_id) + " finalizada com sucesso!"); 
                        MessageBoxA(hwnd, "Venda finalizada com sucesso! \nOs descontos foram calculados e o estoque atualizado.", "Sucesso", MB_OK);
                        SendMessage(hwnd, WM_CLOSE, 0, 0);
                    } catch (const std::exception& e) { 
                        MessageBoxA(hwnd, "Falha ao registrar a transacao!\nVerifique se os itens do carrinho ainda possuem estoque disponivel.", "Conflito de Estoque", MB_ICONERROR); 
                    } 
                    db.desconectar();
                }
            } 
            break;
        }
        case WM_CLOSE: { 
            EnableWindow(hMainWnd, TRUE); 
            DestroyWindow(hwnd); 
            AtualizarLista(hMainWnd); 
            break; 
        }
        default: return DefWindowProcA(hwnd, msg, wParam, lParam);
    } 
    return 0;
}

// ==========================================
// TELA PRINCIPAL E LOGIN
// ==========================================
LRESULT CALLBACK PassWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    static HWND hPassEdit;
    switch (msg) {
        case WM_CREATE: { 
            SetClassLongPtr(hwnd, GCLP_HBRBACKGROUND, (LONG_PTR)hbrFundo);
            CreateWindowExA(0, "STATIC", "Autenticacao Administrativa", WS_CHILD | WS_VISIBLE | SS_CENTER, 10, 10, 260, 20, hwnd, NULL, NULL, NULL); 
            CreateWindowExA(0, "STATIC", "Digite a senha para resetar:", WS_CHILD | WS_VISIBLE, 20, 40, 200, 20, hwnd, NULL, NULL, NULL); 
            hPassEdit = CreateWindowExA(0, "EDIT", "", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_PASSWORD | ES_AUTOHSCROLL, 20, 65, 240, 25, hwnd, NULL, NULL, NULL); 
            CreateWindowExA(0, "BUTTON", "Confirmar", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW, 85, 100, 110, 30, hwnd, (HMENU)1, NULL, NULL); 
            break; 
        }
        case WM_CTLCOLORSTATIC: { 
            HDC hdcStatic = (HDC)wParam; 
            SetTextColor(hdcStatic, corTexto); 
            SetBkColor(hdcStatic, corFundo); 
            return (INT_PTR)hbrFundo; 
        }
        case WM_CTLCOLOREDIT: { 
            HDC hdcEdit = (HDC)wParam; 
            SetTextColor(hdcEdit, corTexto); 
            SetBkColor(hdcEdit, corCaixa); 
            return (INT_PTR)hbrCaixaTexto; 
        }
        case WM_DRAWITEM: { 
            LPDRAWITEMSTRUCT pdis = (LPDRAWITEMSTRUCT)lParam; 
            if (pdis->CtlType == ODT_BUTTON) { 
                // Vermelho pastel no escuro, Vermelho forte no claro
                COLORREF corBtn = modoEscuro ? RGB(243, 139, 168) : RGB(210, 15, 57); 
                
                if (pdis->itemState & ODS_SELECTED) {
                    int r = GetRValue(corBtn) - 30; if (r < 0) r = 0;
                    int g = GetGValue(corBtn) - 30; if (g < 0) g = 0;
                    int b = GetBValue(corBtn) - 30; if (b < 0) b = 0;
                    corBtn = RGB(r, g, b);
                }
                
                HBRUSH hBrush = CreateSolidBrush(corBtn); 
                FillRect(pdis->hDC, &pdis->rcItem, hBrush); 
                DeleteObject(hBrush); 
                
                SetBkMode(pdis->hDC, TRANSPARENT); 
                SetTextColor(pdis->hDC, modoEscuro ? RGB(17, 17, 27) : RGB(255, 255, 255)); 
                DrawTextA(pdis->hDC, "Confirmar", -1, &pdis->rcItem, DT_CENTER | DT_VCENTER | DT_SINGLELINE); 
                return TRUE; 
            } 
            break; 
        }
        case WM_COMMAND: { 
            if (LOWORD(wParam) == 1) { 
                char pass[20]; 
                GetWindowTextA(hPassEdit, pass, 20); 
                if (strcmp(pass, "1234") == 0) { 
                    EnableWindow(hMainWnd, TRUE); 
                    SendMessage(hMainWnd, WM_COMMAND, MAKEWPARAM(ID_BTN_RESET_CONFIRM, 0), 0); 
                    DestroyWindow(hwnd); 
                } else { 
                    MessageBoxA(hwnd, "Senha Incorreta! Acesso Negado.", "Seguranca", MB_ICONERROR); 
                    SetWindowTextA(hPassEdit, ""); 
                } 
            } 
            break; 
        }
        case WM_CLOSE: { 
            EnableWindow(hMainWnd, TRUE); 
            DestroyWindow(hwnd); 
            break; 
        }
        default: return DefWindowProcA(hwnd, msg, wParam, lParam);
    } 
    return 0;
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE: {
            hMainWnd = hwnd; 

            HFONT hFont = CreateFontA(18, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, "Segoe UI");
            HFONT hFontNormal = CreateFontA(16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, "Segoe UI");

            // --- LADO ESQUERDO (CADASTRO) ---
            CreateWindowExA(0, "STATIC", "CADASTRAR / EDITAR PRODUTO", WS_CHILD | WS_VISIBLE, 20, 10, 240, 20, hwnd, NULL, NULL, NULL);
            
            CreateWindowExA(0, "STATIC", "ID:", WS_CHILD | WS_VISIBLE, 20, 35, 60, 20, hwnd, NULL, NULL, NULL); 
            hIdEdit = CreateWindowExA(0, "EDIT", "", WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_NUMBER, 90, 35, 60, 25, hwnd, (HMENU)ID_EDIT_ID_EDIT, NULL, NULL);
            
            CreateWindowExA(0, "STATIC", "Nome:", WS_CHILD | WS_VISIBLE, 20, 65, 60, 20, hwnd, NULL, NULL, NULL); 
            hNome = CreateWindowExA(0, "EDIT", "", WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL, 90, 65, 150, 25, hwnd, (HMENU)ID_EDIT_NOME, NULL, NULL);
            
            CreateWindowExA(0, "STATIC", "Categoria:", WS_CHILD | WS_VISIBLE, 20, 95, 70, 20, hwnd, NULL, NULL, NULL); 
            hCat = CreateWindowExA(0, "COMBOBOX", "", WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWN | WS_VSCROLL, 90, 95, 150, 150, hwnd, (HMENU)ID_COMBO_CAT, NULL, NULL);
            
            CreateWindowExA(0, "STATIC", "Preco:", WS_CHILD | WS_VISIBLE, 20, 125, 60, 20, hwnd, NULL, NULL, NULL); 
hPreco = CreateWindowExA(0, "EDIT", "", WS_CHILD | WS_VISIBLE | WS_TABSTOP, 90, 125, 150, 25, hwnd, (HMENU)ID_EDIT_PRECO, NULL, NULL);
            
            CreateWindowExA(0, "STATIC", "Estoque:", WS_CHILD | WS_VISIBLE, 20, 155, 60, 20, hwnd, NULL, NULL, NULL); 
            hEstoque = CreateWindowExA(0, "EDIT", "", WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_NUMBER, 90, 155, 150, 25, hwnd, (HMENU)ID_EDIT_ESTOQUE, NULL, NULL);
            
            CreateWindowExA(0, "STATIC", "Detalhes:", WS_CHILD | WS_VISIBLE, 20, 185, 70, 20, hwnd, NULL, NULL, NULL); 
            hDesc = CreateWindowExA(0, "EDIT", "", WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL, 90, 185, 150, 25, hwnd, (HMENU)ID_EDIT_DESC, NULL, NULL);

            hChkCadMari = CreateWindowExA(0, "BUTTON", "Fabricado em Mari", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 90, 215, 150, 20, hwnd, (HMENU)ID_CHK_CAD_MARI, NULL, NULL);
            
            CreateWindowExA(0, "BUTTON", "Adicionar", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW, 20, 240, 105, 35, hwnd, (HMENU)ID_BTN_ADD, NULL, NULL); 
            CreateWindowExA(0, "BUTTON", "Salvar Edicao", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW, 135, 240, 105, 35, hwnd, (HMENU)ID_BTN_EDITAR, NULL, NULL);

            CreateWindowExA(0, "STATIC", "AJUSTE DE ESTOQUE", WS_CHILD | WS_VISIBLE, 20, 290, 200, 20, hwnd, NULL, NULL, NULL); 
            CreateWindowExA(0, "STATIC", "ID:", WS_CHILD | WS_VISIBLE, 20, 315, 30, 20, hwnd, NULL, NULL, NULL); 
            hIdBaixa = CreateWindowExA(0, "EDIT", "", WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_NUMBER, 60, 315, 180, 25, hwnd, (HMENU)ID_EDIT_ID_BAIXA, NULL, NULL);
            
            CreateWindowExA(0, "STATIC", "Qtd:", WS_CHILD | WS_VISIBLE, 20, 345, 40, 20, hwnd, NULL, NULL, NULL); 
            hQtdBaixa = CreateWindowExA(0, "EDIT", "", WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_NUMBER, 60, 345, 180, 25, hwnd, (HMENU)ID_EDIT_QTD_BAIXA, NULL, NULL); 
            
            CreateWindowExA(0, "BUTTON", "Baixa (-)", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW, 20, 380, 105, 35, hwnd, (HMENU)ID_BTN_BAIXA, NULL, NULL); 
            CreateWindowExA(0, "BUTTON", "Subir (+)", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW, 135, 380, 105, 35, hwnd, (HMENU)ID_BTN_ADD_ESTOQUE, NULL, NULL);

            CreateWindowExA(0, "STATIC", "APAGAR PRODUTO", WS_CHILD | WS_VISIBLE, 20, 430, 200, 20, hwnd, NULL, NULL, NULL); 
            CreateWindowExA(0, "STATIC", "ID:", WS_CHILD | WS_VISIBLE, 20, 455, 30, 20, hwnd, NULL, NULL, NULL); 
            hIdDel = CreateWindowExA(0, "EDIT", "", WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_NUMBER, 60, 455, 180, 25, hwnd, (HMENU)ID_EDIT_ID_DEL, NULL, NULL); 
            
            CreateWindowExA(0, "BUTTON", "Apagar do Sistema", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW, 20, 485, 220, 30, hwnd, (HMENU)ID_BTN_DEL, NULL, NULL);

            // --- LADO DIREITO (FILTROS E TABELA) ---
            CreateWindowExA(0, "STATIC", "Nome:", WS_CHILD | WS_VISIBLE, 270, 12, 45, 20, hwnd, NULL, NULL, NULL);
            hBuscaNome = CreateWindowExA(0, "EDIT", "", WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_BORDER | ES_AUTOHSCROLL, 315, 10, 100, 22, hwnd, (HMENU)ID_EDIT_BUSCA_NOME, NULL, NULL);
            
            CreateWindowExA(0, "STATIC", "Cat:", WS_CHILD | WS_VISIBLE, 425, 12, 30, 20, hwnd, NULL, NULL, NULL); 
            hFiltroCat = CreateWindowExA(0, "COMBOBOX", "", WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL, 455, 10, 90, 150, hwnd, (HMENU)ID_COMBO_FILTRO, NULL, NULL);
            
            hChkBuscaMari = CreateWindowExA(0, "BUTTON", "Mari", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 555, 10, 60, 20, hwnd, (HMENU)ID_CHK_BUSCA_MARI, NULL, NULL);
            
            CreateWindowExA(0, "BUTTON", "Abrir PDV", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW, 660, 10, 100, 30, hwnd, (HMENU)ID_BTN_ABRIR_PDV, NULL, NULL);

            CreateWindowExA(0, "STATIC", "R$ Min:", WS_CHILD | WS_VISIBLE, 270, 42, 50, 20, hwnd, NULL, NULL, NULL);
            hBuscaPrecoMin = CreateWindowExA(0, "EDIT", "", WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_BORDER | ES_NUMBER, 320, 40, 45, 22, hwnd, (HMENU)ID_EDIT_PRECO_MIN, NULL, NULL);
            
            CreateWindowExA(0, "STATIC", "Max:", WS_CHILD | WS_VISIBLE, 375, 42, 35, 20, hwnd, NULL, NULL, NULL);
            hBuscaPrecoMax = CreateWindowExA(0, "EDIT", "", WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_BORDER | ES_NUMBER, 410, 40, 45, 22, hwnd, (HMENU)ID_EDIT_PRECO_MAX, NULL, NULL);
            
            hChkFuncionario = CreateWindowExA(0, "BUTTON", "Sou Func.", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 465, 40, 90, 20, hwnd, (HMENU)ID_CHK_FUNCIONARIO, NULL, NULL);
            hBtnCritico = CreateWindowExA(0, "BUTTON", "Estoque < 5", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW | WS_DISABLED, 560, 38, 90, 25, hwnd, (HMENU)ID_BTN_CRITICO, NULL, NULL);
            
            CreateWindowExA(0, "BUTTON", "Historico", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW, 660, 42, 100, 25, hwnd, (HMENU)ID_BTN_HISTORICO_VENDAS, NULL, NULL); 

            CreateWindowExA(0, "BUTTON", "Buscar", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW, 270, 70, 80, 25, hwnd, (HMENU)ID_BTN_FILTRAR, NULL, NULL); 
            CreateWindowExA(0, "BUTTON", "Limpar", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW, 360, 70, 80, 25, hwnd, (HMENU)ID_BTN_LISTAR, NULL, NULL); 
            
            hRadioId = CreateWindowExA(0, "BUTTON", "Por ID", WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON, 450, 72, 60, 20, hwnd, (HMENU)ID_RADIO_ID, NULL, NULL); 
            hRadioPreco = CreateWindowExA(0, "BUTTON", "Maior Preco", WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON, 515, 72, 90, 20, hwnd, (HMENU)ID_RADIO_PRECO, NULL, NULL); 
            
            // NOVO BOTAO RELATORIO MENSAL
            CreateWindowExA(0, "BUTTON", "Relatorio", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW, 660, 69, 100, 25, hwnd, (HMENU)ID_BTN_RELATORIO_MENSAL, NULL, NULL);

            SendMessage(hRadioId, BM_SETCHECK, BST_CHECKED, 0); 

            hList = CreateWindowExA(WS_EX_CLIENTEDGE, WC_LISTVIEWA, "", WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS, 270, 105, 490, 335, hwnd, (HMENU)ID_LISTBOX, NULL, NULL); 
            ListView_SetExtendedListViewStyle(hList, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);

            LVCOLUMNA lvc;
            memset(&lvc, 0, sizeof(LVCOLUMNA)); 
            lvc.mask = LVCF_FMT | LVCF_WIDTH | LVCF_TEXT | LVCF_SUBITEM; 
            lvc.fmt = LVCFMT_LEFT;
            
            lvc.iSubItem = 0; lvc.cx = 40;  lvc.pszText = (LPSTR)"ID";        ListView_InsertColumn(hList, 0, &lvc); 
            lvc.iSubItem = 1; lvc.cx = 120; lvc.pszText = (LPSTR)"Nome";      ListView_InsertColumn(hList, 1, &lvc); 
            lvc.iSubItem = 2; lvc.cx = 80;  lvc.pszText = (LPSTR)"Categoria"; ListView_InsertColumn(hList, 2, &lvc); 
            lvc.iSubItem = 3; lvc.cx = 60;  lvc.pszText = (LPSTR)"Preco";     ListView_InsertColumn(hList, 3, &lvc); 
            lvc.iSubItem = 4; lvc.cx = 55;  lvc.pszText = (LPSTR)"Estoque";   ListView_InsertColumn(hList, 4, &lvc); 
            lvc.iSubItem = 5; lvc.cx = 130; lvc.pszText = (LPSTR)"Descricao"; ListView_InsertColumn(hList, 5, &lvc); 

            ListView_SetBkColor(hList, corCaixa); 
            ListView_SetTextBkColor(hList, corCaixa); 
            ListView_SetTextColor(hList, corTexto);

            hDash = CreateWindowExA(0, "STATIC", "Resumo: 0 produtos | Valor: R$ 0.00", WS_CHILD | WS_VISIBLE, 270, 450, 490, 20, hwnd, (HMENU)ID_DASHBOARD, NULL, NULL);
            CreateWindowExA(0, "STATIC", "Saude do Estoque:", WS_CHILD | WS_VISIBLE, 270, 475, 120, 20, hwnd, NULL, NULL, NULL); 
            
            hProgressBar = CreateWindowExA(0, PROGRESS_CLASS, NULL, WS_CHILD | WS_VISIBLE | PBS_SMOOTH, 395, 475, 365, 18, hwnd, (HMENU)ID_PROGRESS_BAR, NULL, NULL); 
            SendMessage(hProgressBar, PBM_SETRANGE, 0, MAKELPARAM(0, 100)); 
            
            CreateWindowExA(0, "STATIC", "REGISTRO DE ATIVIDADES:", WS_CHILD | WS_VISIBLE, 20, 540, 200, 20, hwnd, NULL, NULL, NULL); 
            hLogs = CreateWindowExA(0, "LISTBOX", NULL, WS_CHILD | WS_VISIBLE | WS_VSCROLL, 20, 560, 740, 70, hwnd, (HMENU)ID_LISTBOX_LOGS, NULL, NULL);
            
            CreateWindowExA(0, "BUTTON", "Exportar Relatorio (CSV)", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW, 20, 640, 310, 30, hwnd, (HMENU)ID_BTN_EXPORTAR, NULL, NULL); 
            CreateWindowExA(0, "BUTTON", "Reiniciar Dados", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW, 340, 640, 310, 30, hwnd, (HMENU)ID_BTN_RESET, NULL, NULL); 
            CreateWindowExA(0, "BUTTON", "Modo Claro", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW, 660, 640, 100, 30, hwnd, (HMENU)ID_BTN_TEMA, NULL, NULL);

            EnumChildWindows(hwnd, [](HWND child, LPARAM fonts) -> BOOL {
                HFONT* pFonts = (HFONT*)fonts; 
                char className[256]; 
                GetClassNameA(child, className, 256);
                if (std::string(className) == "STATIC") {
                    SendMessage(child, WM_SETFONT, (WPARAM)pFonts[0], TRUE); 
                } else {
                    SendMessage(child, WM_SETFONT, (WPARAM)pFonts[1], TRUE); 
                }
                return TRUE;
            }, (LPARAM)(HFONT[]){hFont, hFontNormal});

            wpOrigEditProc = (WNDPROC)SetWindowLongPtr(hNome, GWLP_WNDPROC, (LONG_PTR)EditSubclassProc); 
            SetWindowLongPtr(hPreco, GWLP_WNDPROC, (LONG_PTR)EditSubclassProc); 
            SetWindowLongPtr(hEstoque, GWLP_WNDPROC, (LONG_PTR)EditSubclassProc); 
            SetWindowLongPtr(hDesc, GWLP_WNDPROC, (LONG_PTR)EditSubclassProc); 
            wpOrigListProc = (WNDPROC)SetWindowLongPtr(hList, GWLP_WNDPROC, (LONG_PTR)ListSubclassProc); 
            
            AdicionarLog("Sistema iniciado e pronto para uso."); 
            break;
        }
        
        case WM_CTLCOLORSTATIC: { 
            HDC hdcStatic = (HDC)wParam; 
            SetTextColor(hdcStatic, corTexto); 
            SetBkColor(hdcStatic, corFundo); 
            return (INT_PTR)hbrFundo; 
        }
        case WM_CTLCOLOREDIT: 
        case WM_CTLCOLORLISTBOX: { 
            HDC hdcEdit = (HDC)wParam; 
            SetTextColor(hdcEdit, corTexto); 
            SetBkColor(hdcEdit, corCaixa); 
            return (INT_PTR)hbrCaixaTexto; 
        }

        case WM_DRAWITEM: {
            LPDRAWITEMSTRUCT pdis = (LPDRAWITEMSTRUCT)lParam;
            if (pdis->CtlType == ODT_BUTTON) {
                COLORREF corBtn;
                
                // SISTEMA INTELIGENTE DE CORES (MODO ESCURO VS CLARO)
                if (modoEscuro) {
                    // Cores do Modo Escuro (Tons Pastel)
                    if (pdis->CtlID == ID_BTN_ADD) corBtn = RGB(166, 227, 161);            
                    else if (pdis->CtlID == ID_BTN_EDITAR) corBtn = RGB(250, 179, 135);
                    else if (pdis->CtlID == ID_BTN_CRITICO) corBtn = RGB(243, 139, 168); 
                    else if (pdis->CtlID == ID_BTN_BAIXA) corBtn = RGB(249, 226, 175);      
                    else if (pdis->CtlID == ID_BTN_ADD_ESTOQUE) corBtn = RGB(137, 180, 250);
                    else if (pdis->CtlID == ID_BTN_DEL) corBtn = RGB(243, 139, 168);        
                    else if (pdis->CtlID == ID_BTN_RESET) corBtn = RGB(203, 166, 247);      
                    else if (pdis->CtlID == ID_BTN_FILTRAR) corBtn = RGB(180, 190, 254);
                    else if (pdis->CtlID == ID_BTN_LISTAR) corBtn = RGB(148, 226, 213); 
                    else if (pdis->CtlID == ID_BTN_EXPORTAR) corBtn = RGB(148, 226, 213); 
                    else if (pdis->CtlID == ID_BTN_ABRIR_PDV) corBtn = RGB(166, 227, 161); 
                    else if (pdis->CtlID == ID_BTN_HISTORICO_VENDAS) corBtn = RGB(203, 166, 247); 
                    else if (pdis->CtlID == ID_BTN_RELATORIO_MENSAL) corBtn = RGB(250, 227, 176); 
                    else if (pdis->CtlID == ID_BTN_ADD_CARRINHO) corBtn = RGB(137, 180, 250); 
                    else if (pdis->CtlID == ID_BTN_FINALIZAR_VENDA) corBtn = RGB(166, 227, 161); 
                    else if (pdis->CtlID == ID_BTN_TEMA) corBtn = RGB(249, 226, 175); 
                    else corBtn = RGB(147, 153, 178); 
                } else {
                    // Cores do Modo Claro (Tons Vivos e Fortes)
                    if (pdis->CtlID == ID_BTN_ADD) corBtn = RGB(64, 160, 43); // Verde Escuro
                    else if (pdis->CtlID == ID_BTN_EDITAR) corBtn = RGB(223, 142, 29); // Laranja Escuro
                    else if (pdis->CtlID == ID_BTN_CRITICO) corBtn = RGB(210, 15, 57); // Vermelho Escuro
                    else if (pdis->CtlID == ID_BTN_BAIXA) corBtn = RGB(220, 138, 120); // Salmão      
                    else if (pdis->CtlID == ID_BTN_ADD_ESTOQUE) corBtn = RGB(30, 102, 245); // Azul Royal
                    else if (pdis->CtlID == ID_BTN_DEL) corBtn = RGB(210, 15, 57); // Vermelho Escuro        
                    else if (pdis->CtlID == ID_BTN_RESET) corBtn = RGB(136, 57, 239); // Roxo Escuro      
                    else if (pdis->CtlID == ID_BTN_FILTRAR) corBtn = RGB(114, 135, 253); // Indigo
                    else if (pdis->CtlID == ID_BTN_LISTAR) corBtn = RGB(23, 146, 153); // Turquesa
                    else if (pdis->CtlID == ID_BTN_EXPORTAR) corBtn = RGB(23, 146, 153); 
                    else if (pdis->CtlID == ID_BTN_ABRIR_PDV) corBtn = RGB(64, 160, 43); 
                    else if (pdis->CtlID == ID_BTN_HISTORICO_VENDAS) corBtn = RGB(136, 57, 239); 
                    else if (pdis->CtlID == ID_BTN_RELATORIO_MENSAL) corBtn = RGB(223, 142, 29); 
                    else if (pdis->CtlID == ID_BTN_ADD_CARRINHO) corBtn = RGB(30, 102, 245); 
                    else if (pdis->CtlID == ID_BTN_FINALIZAR_VENDA) corBtn = RGB(64, 160, 43); 
                    else if (pdis->CtlID == ID_BTN_TEMA) corBtn = RGB(140, 143, 161); // Cinza Chumbo
                    else corBtn = RGB(140, 143, 161);
                }

                // Efeito do botão desativado (Estoque < 5) ou sendo clicado
                if (pdis->itemState & ODS_DISABLED) {
                    corBtn = RGB(160, 160, 160); // Cinza
                } else if (pdis->itemState & ODS_SELECTED) {
                    int r = GetRValue(corBtn) - 30; if (r < 0) r = 0;
                    int g = GetGValue(corBtn) - 30; if (g < 0) g = 0;
                    int b = GetBValue(corBtn) - 30; if (b < 0) b = 0;
                    corBtn = RGB(r, g, b);
                }
                
                HBRUSH hBrush = CreateSolidBrush(corBtn); 
                FillRect(pdis->hDC, &pdis->rcItem, hBrush); 
                DeleteObject(hBrush);
                
                SetBkMode(pdis->hDC, TRANSPARENT); 
                
                // Texto do botão: Preto no escuro, Branco no claro (ou cinza se bloqueado)
                COLORREF corTextoBtn;
                if (pdis->itemState & ODS_DISABLED) {
                    corTextoBtn = RGB(100, 100, 100);
                } else {
                    corTextoBtn = modoEscuro ? RGB(17, 17, 27) : RGB(255, 255, 255);
                }
                
                SetTextColor(pdis->hDC, corTextoBtn); 
                
                char textoBotao[256]; 
                GetWindowTextA(pdis->hwndItem, textoBotao, 256); 
                DrawTextA(pdis->hDC, textoBotao, -1, &pdis->rcItem, DT_CENTER | DT_VCENTER | DT_SINGLELINE); 
                return TRUE;
            }
            break;
        }

        case WM_NOTIFY: {
            LPNMHDR lpnmh = (LPNMHDR)lParam;
            if (lpnmh->idFrom == ID_LISTBOX && lpnmh->code == NM_DBLCLK) {
                int index = ListView_GetNextItem(hList, -1, LVNI_SELECTED);
                if (index != -1) {
                    char idBuf[50], nomeBuf[256], catBuf[100], precoBuf[50], estBuf[50], descBuf[512];
                    
                    ListView_GetItemText(hList, index, 0, idBuf, 50); 
                    ListView_GetItemText(hList, index, 1, nomeBuf, 256);
                    ListView_GetItemText(hList, index, 2, catBuf, 100); 
                    ListView_GetItemText(hList, index, 3, precoBuf, 50);
                    ListView_GetItemText(hList, index, 4, estBuf, 50); 
                    ListView_GetItemText(hList, index, 5, descBuf, 512); 

                    std::string p = precoBuf; 
                    if(p.find("R$ ") == 0) p = p.substr(3);
                    
                    std::string n = nomeBuf;
                    
                    if (n.find(" (Mari)") != std::string::npos) {
                        n = n.substr(0, n.length() - 7); 
                        SendMessage(hChkCadMari, BM_SETCHECK, BST_CHECKED, 0);
                    } else {
                        SendMessage(hChkCadMari, BM_SETCHECK, BST_UNCHECKED, 0);
                    }

                    SetWindowTextA(hIdEdit, idBuf); 
                    SetWindowTextA(hNome, n.c_str()); 
                    SetWindowTextA(hCat, catBuf);
                    SetWindowTextA(hPreco, p.c_str()); 
                    SetWindowTextA(hEstoque, estBuf); 
                    SetWindowTextA(hDesc, descBuf); 
                    SetWindowTextA(hIdBaixa, idBuf); 
                    SetWindowTextA(hIdDel, idBuf); 
                    
                    AdicionarLog("Produto ID " + std::string(idBuf) + " selecionado para edicao.");
                }
            }
            break;
        }

        case WM_COMMAND: {
            int wmId = LOWORD(wParam);
            int wmEvent = HIWORD(wParam);
            
            if (wmId == ID_BTN_ABRIR_PDV) {
                EnableWindow(hwnd, FALSE); 
                hwndPDV = CreateWindowExA(WS_EX_DLGMODALFRAME, "PDVWindowClass", "Ponto de Venda - Carrinho", 
                                          WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_VISIBLE, 
                                          GetSystemMetrics(SM_CXSCREEN)/2 - 350, GetSystemMetrics(SM_CYSCREEN)/2 - 250, 
                                          700, 500, hwnd, NULL, GetModuleHandle(NULL), NULL);
            }
            
            if (wmId == ID_BTN_HISTORICO_VENDAS) { 
                EnableWindow(hwnd, FALSE); 
                hwndHistorico = CreateWindowExA(WS_EX_DLGMODALFRAME, "HistoricoWindowClass", "Historico e Dados de Clientes", 
                                          WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_VISIBLE, 
                                          GetSystemMetrics(SM_CXSCREEN)/2 - 375, GetSystemMetrics(SM_CYSCREEN)/2 - 325, 
                                          750, 650, hwnd, NULL, GetModuleHandle(NULL), NULL);
            }
            
            if (wmId == ID_BTN_RELATORIO_MENSAL) { 
                EnableWindow(hwnd, FALSE); 
                hwndRelMensal = CreateWindowExA(WS_EX_DLGMODALFRAME, "RelatorioMensalClass", "Visao Geral de Faturamento", 
                                          WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_VISIBLE, 
                                          GetSystemMetrics(SM_CXSCREEN)/2 - 250, GetSystemMetrics(SM_CYSCREEN)/2 - 200, 
                                          500, 400, hwnd, NULL, GetModuleHandle(NULL), NULL);
            }

            if (wmId == ID_BTN_TEMA) {
                modoEscuro = !modoEscuro;
                corFundo = modoEscuro ? RGB(30, 30, 46) : RGB(239, 241, 245); 
                corCaixa = modoEscuro ? RGB(49, 50, 68) : RGB(204, 208, 218); 
                corTexto = modoEscuro ? RGB(205, 214, 244) : RGB(76, 79, 105);
                
                DeleteObject(hbrFundo); 
                DeleteObject(hbrCaixaTexto); 
                
                hbrFundo = CreateSolidBrush(corFundo); 
                hbrCaixaTexto = CreateSolidBrush(corCaixa); 
                
                SetClassLongPtr(hwnd, GCLP_HBRBACKGROUND, (LONG_PTR)hbrFundo); 
                SetWindowTextA(GetDlgItem(hwnd, ID_BTN_TEMA), modoEscuro ? "Modo Claro" : "Modo Escuro"); 
                
                ListView_SetBkColor(hList, corCaixa); 
                ListView_SetTextBkColor(hList, corCaixa); 
                ListView_SetTextColor(hList, corTexto); 
                
                InvalidateRect(hwnd, NULL, TRUE); 
                UpdateWindow(hwnd); 
                AdicionarLog(modoEscuro ? "Tema Escuro ativado." : "Tema Claro ativado.");
            }

            if (wmId == ID_CHK_FUNCIONARIO) {
                bool isFunc = SendMessage(hChkFuncionario, BM_GETCHECK, 0, 0) == BST_CHECKED;
                EnableWindow(hBtnCritico, isFunc ? TRUE : FALSE);
                InvalidateRect(hBtnCritico, NULL, TRUE);
            }

            if (wmId == ID_COMBO_FILTRO && (wmEvent == CBN_EDITCHANGE || wmEvent == CBN_SELCHANGE)) { 
                AtualizarLista(hwnd); 
            }
            
            if (wmId == ID_RADIO_ID) { 
                ordemAtual = 1; 
                AtualizarLista(hwnd); 
            }
            if (wmId == ID_RADIO_PRECO) { 
                ordemAtual = 2; 
                AtualizarLista(hwnd); 
            }
            if (wmId == ID_RADIO_ESTOQUE) { 
                ordemAtual = 3; 
                AtualizarLista(hwnd); 
            }
            
            if (wmId == ID_BTN_LISTAR) { 
                SetWindowTextA(hFiltroCat, "-- TODAS --"); 
                SetWindowTextA(hBuscaNome, ""); 
                SetWindowTextA(hBuscaPrecoMin, ""); 
                SetWindowTextA(hBuscaPrecoMax, "");
                SendMessage(hChkBuscaMari, BM_SETCHECK, BST_UNCHECKED, 0); 
                SendMessage(hChkFuncionario, BM_SETCHECK, BST_UNCHECKED, 0); 
                EnableWindow(hBtnCritico, FALSE);
                AtualizarLista(hwnd); 
            }
            
            if (wmId == ID_BTN_FILTRAR) { 
                AtualizarLista(hwnd); 
            }
            
            if (wmId == ID_BTN_CRITICO) { 
                AtualizarLista(hwnd, true); 
                AdicionarLog("Filtro Aplicado: Apenas itens criticos."); 
            }
            
            if (wmId == ID_BTN_ADD) {
                std::string nome = ObterTexto(hNome); 
                std::string cat = ObterTexto(hCat); 
                std::string preco = ObterTexto(hPreco); 
                std::string est = ObterTexto(hEstoque); 
                std::string desc = ObterTexto(hDesc); 
                bool mari = SendMessage(hChkCadMari, BM_GETCHECK, 0, 0) == BST_CHECKED;
                
                if(nome.empty() || preco.empty() || est.empty()) { 
                    MessageBoxA(hwnd, "Preencha Nome, Preco e Estoque!", "Aviso", MB_ICONWARNING); 
                    break; 
                }
                
                if (db.conectar()) {
                    try {
                        pqxx::work txn(*db.getConn()); 
                        txn.exec("INSERT INTO produto (nome, categoria, preco, estoque, descricao, fabricado_em_mari) VALUES (" + txn.quote(nome) + ", " + txn.quote(cat) + ", " + preco + ", " + est + ", " + txn.quote(desc) + ", " + (mari ? "TRUE" : "FALSE") + ");"); 
                        txn.commit(); 
                        
                        SetWindowTextA(hIdEdit, ""); 
                        SetWindowTextA(hNome, ""); 
                        SetWindowTextA(hCat, ""); 
                        SetWindowTextA(hPreco, ""); 
                        SetWindowTextA(hEstoque, ""); 
                        SetWindowTextA(hDesc, ""); 
                        SendMessage(hChkCadMari, BM_SETCHECK, BST_UNCHECKED, 0);
                        
                        AtualizarCategorias(); 
                        AtualizarLista(hwnd); 
                        AdicionarLog("Sucesso: Produto '" + nome + "' inserido."); 
                    } catch (const std::exception& e) { 
                        MessageBoxA(hwnd, e.what(), "Erro ao Adicionar", MB_ICONERROR); 
                    } 
                    db.desconectar();
                }
            }
            
            if (wmId == ID_BTN_EDITAR) {
                std::string id = ObterTexto(hIdEdit); 
                std::string nome = ObterTexto(hNome); 
                std::string cat = ObterTexto(hCat); 
                std::string preco = ObterTexto(hPreco); 
                std::string est = ObterTexto(hEstoque); 
                std::string desc = ObterTexto(hDesc); 
                bool mari = SendMessage(hChkCadMari, BM_GETCHECK, 0, 0) == BST_CHECKED;
                
                if(id.empty() || nome.empty() || preco.empty() || est.empty()) { 
                    MessageBoxA(hwnd, "De um duplo clique num produto ou preencha o ID para editar!", "Aviso", MB_ICONWARNING); 
                    break; 
                }
                
                if (db.conectar()) {
                    try {
                        pqxx::work txn(*db.getConn()); 
                        std::string sql = "UPDATE produto SET nome = " + txn.quote(nome) + ", categoria = " + txn.quote(cat) + ", preco = " + preco + ", estoque = " + est + ", descricao = " + txn.quote(desc) + ", fabricado_em_mari = " + (mari ? "TRUE" : "FALSE") + " WHERE id = " + txn.quote(id) + ";"; 
                        pqxx::result r = txn.exec(sql);
                        
                        if (r.affected_rows() > 0) {    
                            txn.commit(); 
                            SetWindowTextA(hIdEdit, ""); 
                            SetWindowTextA(hNome, ""); 
                            SetWindowTextA(hCat, ""); 
                            SetWindowTextA(hPreco, ""); 
                            SetWindowTextA(hEstoque, ""); 
                            SetWindowTextA(hDesc, ""); 
                            SendMessage(hChkCadMari, BM_SETCHECK, BST_UNCHECKED, 0);
                            
                            AtualizarCategorias(); 
                            AtualizarLista(hwnd); 
                            AdicionarLog("Sucesso: Produto ID " + id + " editado."); 
                            MessageBoxA(hwnd, "Produto editado com sucesso!", "Sucesso", MB_OK);
                        } else { 
                            MessageBoxA(hwnd, "Edicao falhou! Verifique se o ID existe.", "Atencao", MB_ICONWARNING); 
                        }
                    } catch (const std::exception& e) { 
                        MessageBoxA(hwnd, e.what(), "Erro ao Editar", MB_ICONERROR); 
                    } 
                    db.desconectar();
                }
            }

            if (wmId == ID_BTN_BAIXA) {
                std::string id = ObterTexto(hIdBaixa); 
                std::string qtd = ObterTexto(hQtdBaixa); 
                
                if(id.empty() || qtd.empty()) { 
                    MessageBoxA(hwnd, "Preencha o ID e a Quantidade!", "Aviso", MB_ICONWARNING); 
                    break; 
                }
                
                if (db.conectar()) {
                    try {
                        pqxx::work txn(*db.getConn()); 
                        std::string sql = "UPDATE produto SET estoque = estoque - " + txn.quote(qtd) + " WHERE id = " + txn.quote(id) + " AND estoque >= " + txn.quote(qtd) + ";"; 
                        pqxx::result r = txn.exec(sql);
                        
                        if (r.affected_rows() > 0) {
                            std::string check_sql = "SELECT nome, estoque FROM produto WHERE id = " + txn.quote(id) + ";"; 
                            pqxx::result res_check = txn.exec(check_sql); 
                            txn.commit(); 
                            
                            SetWindowTextA(hIdBaixa, ""); 
                            SetWindowTextA(hQtdBaixa, ""); 
                            AtualizarLista(hwnd); 
                            AdicionarLog("Baixa realizada: -" + qtd + " unid. no ID " + id);
                            
                            if (!res_check.empty()) {
                                int estoque_atual = res_check[0]["estoque"].as<int>(); 
                                std::string nome_prod = res_check[0]["nome"].as<std::string>();
                                
                                if (estoque_atual < 5) { 
                                    std::string msg_alerta = "ATENCAO: O produto '" + nome_prod + "' esta com estoque baixo! Restam apenas " + std::to_string(estoque_atual) + " unidades."; 
                                    MessageBoxA(hwnd, msg_alerta.c_str(), "Alerta de Estoque", MB_ICONEXCLAMATION); 
                                }
                            }
                        } else { 
                            MessageBoxA(hwnd, "Baixa nao realizada! Verifique o ID e se ha estoque suficiente.", "Atencao", MB_ICONWARNING); 
                        }
                    } catch (const std::exception& e) { 
                        MessageBoxA(hwnd, e.what(), "Erro na Baixa", MB_ICONERROR); 
                    } 
                    db.desconectar();
                }
            }

            if (wmId == ID_BTN_ADD_ESTOQUE) {
                std::string id = ObterTexto(hIdBaixa); 
                std::string qtd = ObterTexto(hQtdBaixa); 
                
                if(id.empty() || qtd.empty()) { 
                    MessageBoxA(hwnd, "Preencha o ID e a Quantidade!", "Aviso", MB_ICONWARNING); 
                    break; 
                }
                
                if (db.conectar()) {
                    try {
                        pqxx::work txn(*db.getConn()); 
                        pqxx::result r = txn.exec("UPDATE produto SET estoque = estoque + " + txn.quote(qtd) + " WHERE id = " + txn.quote(id) + ";");
                        
                        if (r.affected_rows() > 0) { 
                            txn.commit(); 
                            SetWindowTextA(hIdBaixa, ""); 
                            SetWindowTextA(hQtdBaixa, ""); 
                            AtualizarLista(hwnd); 
                            AdicionarLog("Estoque atualizado: +" + qtd + " unid. no ID " + id); 
                            MessageBoxA(hwnd, "Estoque adicionado com sucesso!", "Sucesso", MB_OK); 
                        } else { 
                            MessageBoxA(hwnd, "Operacao falhou! Verifique se o ID existe.", "Atencao", MB_ICONWARNING); 
                        }
                    } catch (const std::exception& e) { 
                        MessageBoxA(hwnd, e.what(), "Erro ao Adicionar Estoque", MB_ICONERROR); 
                    } 
                    db.desconectar();
                }
            }

            if (wmId == ID_BTN_DEL) {
                std::string id = ObterTexto(hIdDel); 
                
                if(id.empty()) { 
                    MessageBoxA(hwnd, "Digite o ID para apagar!", "Aviso", MB_ICONWARNING); 
                    break; 
                }
                
                if (MessageBoxA(hwnd, ("Tem a certeza que deseja apagar o produto ID " + id + "?").c_str(), "Confirmar Remocao", MB_YESNO | MB_ICONQUESTION) == IDYES) {
                    if (db.conectar()) { 
                        try { 
                            pqxx::work txn(*db.getConn()); 
                            txn.exec("DELETE FROM produto WHERE id = " + id + ";"); 
                            txn.commit(); 
                            
                            SetWindowTextA(hIdDel, ""); 
                            AtualizarCategorias(); 
                            AtualizarLista(hwnd); 
                            AdicionarLog("Aviso: Produto ID " + id + " removido."); 
                        } catch (const std::exception& e) { 
                            MessageBoxA(hwnd, e.what(), "Erro ao Apagar", MB_ICONERROR); 
                        } 
                        db.desconectar(); 
                    }
                }
            }

            if (wmId == ID_BTN_EXPORTAR) {
                if (db.conectar()) {
                    try {
                        pqxx::work txn(*db.getConn());
                        
                        // 1. Exportar Estoque Crítico/Geral
                        std::ofstream fCritico("relatorio_estoque.csv");
                        if (fCritico.is_open()) { 
                            fCritico << "ID;Nome;Categoria;Preco;Estoque\n"; 
                            pqxx::result r = txn.exec("SELECT * FROM produto ORDER BY id ASC;"); 
                            for (auto row : r) { 
                                fCritico << row["id"].c_str() << ";" << row["nome"].c_str() << ";" << row["categoria"].c_str() << ";" << row["preco"].c_str() << ";" << row["estoque"].c_str() << "\n"; 
                            } 
                            fCritico.close(); 
                        }
                        
                        // 2. Exportar Histórico Geral de Vendas
                        std::ofstream fVendas("relatorio_vendas.csv");
                        if (fVendas.is_open()) { 
                            fVendas << "ID Venda;Data;Cliente;Vendedor;Total;Pagamento\n"; 
                            pqxx::result r = txn.exec("SELECT v.id, CAST(v.data_venda AS TEXT) as data_venda, COALESCE(c.nome, 'CLIENTE DELETADO') as cliente, COALESCE(vd.nome, 'VEND. DELETADO') as vendedor, v.valor_total, v.forma_pagamento FROM venda v LEFT JOIN cliente c ON v.cliente_id = c.id LEFT JOIN vendedor vd ON v.vendedor_id = vd.id ORDER BY v.id ASC;"); 
                            for (auto row : r) { 
                                fVendas << row["id"].c_str() << ";" << row["data_venda"].c_str() << ";" << row["cliente"].c_str() << ";" << row["vendedor"].c_str() << ";" << row["valor_total"].c_str() << ";" << row["forma_pagamento"].c_str() << "\n"; 
                            } 
                            fVendas.close(); 
                        }
                        
                        // 3. Exportar Relatório Mensal
                        std::ofstream fMensal("relatorio_mensal.csv");
                        if (fMensal.is_open()) { 
                            fMensal << "Mes/Ano;Qtd. Vendas;Faturamento Total\n"; 
                            try {
                                pqxx::result r = txn.exec("SELECT * FROM vw_relatorio_mensal;"); 
                                for (auto row : r) { 
                                    fMensal << row["mes_ano"].c_str() << ";" << row["quantidade_de_vendas"].c_str() << ";" << row["faturamento_total"].c_str() << "\n"; 
                                } 
                            } catch(...) {} // Fica mudo se a view não existir
                            fMensal.close(); 
                        }
                        
                        AdicionarLog("Exportacao CSV: Tabelas geradas com sucesso.");
                        MessageBoxA(hwnd, "Relatorios exportados como CSV com sucesso! \nForam gerados arquivos separados para Estoque, Vendas e Faturamento na pasta do projeto.", "Sucesso", MB_OK);
                    } catch (const std::exception& e) {
                        MessageBoxA(hwnd, "Erro ao extrair dados para o relatorio.", "Erro", MB_ICONERROR);
                    } 
                    db.desconectar();
                }
            }

            if (wmId == ID_BTN_RESET) {
                EnableWindow(hwnd, FALSE); 
                HWND hPassWnd = CreateWindowExA(WS_EX_TOOLWINDOW, "PassWindowClass", "Autenticacao Exigida", 
                                                WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_VISIBLE, 
                                                GetSystemMetrics(SM_CXSCREEN)/2 - 140, GetSystemMetrics(SM_CYSCREEN)/2 - 80, 
                                                280, 180, hwnd, NULL, GetModuleHandle(NULL), NULL);
            }
            
            if (wmId == ID_BTN_RESET_CONFIRM) {
                if (db.conectar()) {
                    try {
                        std::ifstream sqlFile("sql/repor_produtos.sql"); 
                        if (!sqlFile.is_open()) { 
                            MessageBoxA(hwnd, "Nao foi possivel encontrar o ficheiro 'sql/repor_produtos.sql'.", "Erro de Arquivo", MB_ICONERROR); 
                        } else {
                            std::stringstream buffer; 
                            buffer << sqlFile.rdbuf(); 
                            std::string sql_repor = buffer.str(); 
                            sqlFile.close(); 
                            
                            pqxx::work txn(*db.getConn()); 
                            txn.exec(sql_repor); 
                            txn.commit(); 
                            
                            SetWindowTextA(hFiltroCat, ""); 
                            SendMessage(hRadioId, BM_SETCHECK, BST_CHECKED, 0); 
                            ordemAtual = 1; 
                            
                            AtualizarCategorias(); 
                            AtualizarLista(hwnd); 
                            AdicionarLog("Produtos repostos via script externo."); 
                            MessageBoxA(hwnd, "Produtos repostos com sucesso lendo o seu arquivo .sql!", "Operacao Autorizada", MB_OK);
                        }
                    } catch (const std::exception& e) { 
                        MessageBoxA(hwnd, e.what(), "Erro ao Repor Produtos", MB_ICONERROR); 
                    } 
                    db.desconectar();
                }
            }
            break;
        }
        case WM_DESTROY: { 
            DeleteObject(hbrFundo); 
            DeleteObject(hbrCaixaTexto); 
            PostQuitMessage(0); 
            break; 
        }
        default: return DefWindowProcA(hwnd, msg, wParam, lParam);
    }
    return 0;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    INITCOMMONCONTROLSEX icex; 
    icex.dwSize = sizeof(INITCOMMONCONTROLSEX); 
    icex.dwICC = ICC_PROGRESS_CLASS | ICC_LISTVIEW_CLASSES; 
    InitCommonControlsEx(&icex);

    WNDCLASSA wc; memset(&wc, 0, sizeof(WNDCLASSA));
    wc.lpfnWndProc = WndProc; 
    wc.hInstance = hInstance; 
    wc.lpszClassName = "BarDoBodeGUI"; 
    wc.hbrBackground = hbrFundo; 
    wc.hCursor = LoadCursor(NULL, IDC_ARROW); 
    wc.hIcon = LoadIcon(hInstance, "MAINICON"); 
    RegisterClassA(&wc);

    WNDCLASSA wcPass; memset(&wcPass, 0, sizeof(WNDCLASSA));
    wcPass.lpfnWndProc = PassWndProc; 
    wcPass.hInstance = hInstance; 
    wcPass.lpszClassName = "PassWindowClass"; 
    wcPass.hbrBackground = hbrFundo; 
    wcPass.hCursor = LoadCursor(NULL, IDC_ARROW); 
    RegisterClassA(&wcPass);

    WNDCLASSA wcPDV; memset(&wcPDV, 0, sizeof(WNDCLASSA));
    wcPDV.lpfnWndProc = PDVWndProc; 
    wcPDV.hInstance = hInstance; 
    wcPDV.lpszClassName = "PDVWindowClass"; 
    wcPDV.hbrBackground = hbrFundo; 
    wcPDV.hCursor = LoadCursor(NULL, IDC_ARROW); 
    RegisterClassA(&wcPDV);
    
    WNDCLASSA wcCli; memset(&wcCli, 0, sizeof(WNDCLASSA));
    wcCli.lpfnWndProc = CadastroClienteWndProc; 
    wcCli.hInstance = hInstance; 
    wcCli.lpszClassName = "CadastroClienteClass"; 
    wcCli.hbrBackground = hbrFundo; 
    wcCli.hCursor = LoadCursor(NULL, IDC_ARROW); 
    RegisterClassA(&wcCli);
    
    WNDCLASSA wcVend; memset(&wcVend, 0, sizeof(WNDCLASSA));
    wcVend.lpfnWndProc = CadastroVendedorWndProc; 
    wcVend.hInstance = hInstance; 
    wcVend.lpszClassName = "CadastroVendedorClass"; 
    wcVend.hbrBackground = hbrFundo; 
    wcVend.hCursor = LoadCursor(NULL, IDC_ARROW); 
    RegisterClassA(&wcVend);
    
    WNDCLASSA wcHist; memset(&wcHist, 0, sizeof(WNDCLASSA));
    wcHist.lpfnWndProc = HistoricoWndProc; 
    wcHist.hInstance = hInstance; 
    wcHist.lpszClassName = "HistoricoWindowClass"; 
    wcHist.hbrBackground = hbrFundo; 
    wcHist.hCursor = LoadCursor(NULL, IDC_ARROW); 
    RegisterClassA(&wcHist);

    WNDCLASSA wcRel; memset(&wcRel, 0, sizeof(WNDCLASSA));
    wcRel.lpfnWndProc = RelatorioMensalWndProc; 
    wcRel.hInstance = hInstance; 
    wcRel.lpszClassName = "RelatorioMensalClass"; 
    wcRel.hbrBackground = hbrFundo; 
    wcRel.hCursor = LoadCursor(NULL, IDC_ARROW); 
    RegisterClassA(&wcRel);

    HWND hwnd = CreateWindowExA(0, "BarDoBodeGUI", "Bar do Bode - Sistema de Estoque", 
                                WS_OVERLAPPEDWINDOW ^ WS_THICKFRAME ^ WS_MAXIMIZEBOX, 
                                CW_USEDEFAULT, CW_USEDEFAULT, 800, 740, NULL, NULL, hInstance, NULL);

    if (hwnd == NULL) return 0;
    
    AtualizarCategorias(); 
    ShowWindow(hwnd, nCmdShow); 
    AtualizarLista(hwnd);

    MSG msg; memset(&msg, 0, sizeof(MSG));
    while (GetMessage(&msg, NULL, 0, 0)) { 
        TranslateMessage(&msg); 
        DispatchMessage(&msg); 
    }
    return 0;
}
