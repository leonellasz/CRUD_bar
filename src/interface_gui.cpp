#include <windows.h>
#include <commctrl.h> 
#include <string>
#include <iostream>
#include <fstream>
#include <sstream> 
#include <pqxx/pqxx>
#include "db_conexao.h"

// IDs dos elementos
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
#define ID_EDIT_DESC       307 // NOVO: ID para o campo de Descricao

HWND hList, hNome, hCat, hPreco, hEstoque, hDesc, hIdDel, hIdBaixa, hQtdBaixa, hFiltroCat, hDash;
HWND hRadioId, hRadioPreco, hRadioEstoque;
HWND hLogs, hProgressBar, hIdEdit;
HWND hMainWnd; 
DBConexao db;
int ordemAtual = 1;

bool modoEscuro = true;
COLORREF corFundo = RGB(30, 30, 46);
COLORREF corCaixa = RGB(49, 50, 68);
COLORREF corTexto = RGB(205, 214, 244);
HBRUSH hbrFundo = CreateSolidBrush(corFundo);
HBRUSH hbrCaixaTexto = CreateSolidBrush(corCaixa);

WNDPROC wpOrigEditProc;
WNDPROC wpOrigListProc;

LRESULT CALLBACK EditSubclassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_KEYDOWN && wParam == VK_RETURN) {
        HWND hParent = GetParent(hwnd);
        char idBuf[10];
        GetWindowTextA(GetDlgItem(hParent, ID_EDIT_ID_EDIT), idBuf, 10);
        if (strlen(idBuf) > 0) SendMessage(hParent, WM_COMMAND, MAKEWPARAM(ID_BTN_EDITAR, 0), 0);
        else SendMessage(hParent, WM_COMMAND, MAKEWPARAM(ID_BTN_ADD, 0), 0);
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

void AdicionarLog(const std::string& msg) {
    SYSTEMTIME st; GetLocalTime(&st);
    char timeBuffer[512];
    snprintf(timeBuffer, sizeof(timeBuffer), "[%02d:%02d:%02d] %s", st.wHour, st.wMinute, st.wSecond, msg.c_str());
    int pos = SendMessageA(hLogs, LB_ADDSTRING, 0, (LPARAM)timeBuffer);
    SendMessage(hLogs, LB_SETTOPINDEX, pos, 0); 
}

std::string ObterTexto(HWND hwnd) {
    char buffer[512]; // Aumentado para suportar descricoes maiores
    GetWindowTextA(hwnd, buffer, 512);
    return std::string(buffer);
}

void AtualizarCategorias() {
    SendMessage(hCat, CB_RESETCONTENT, 0, 0);
    SendMessage(hFiltroCat, CB_RESETCONTENT, 0, 0);
    SendMessage(hFiltroCat, CB_ADDSTRING, 0, (LPARAM)""); 
    
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

// NOVO: Adaptado para ler a descricao do banco
void AtualizarLista(HWND hwnd, std::string filtroCategoria = "", bool apenasCritico = false) {
    ListView_DeleteAllItems(hList); 
    if (db.conectar()) {
        try {
            pqxx::work txn(*db.getConn());
            std::string sql;
            std::string ordemStr = " ORDER BY id ASC;";
            
            if (ordemAtual == 2) ordemStr = " ORDER BY preco DESC;";
            else if (ordemAtual == 3) ordemStr = " ORDER BY estoque ASC;";
            
            // Adicionado a coluna 'descricao' no SELECT
            if (apenasCritico) {
                sql = "SELECT id, nome, categoria, preco, estoque, descricao FROM produto WHERE estoque < 5" + ordemStr;
            } else if (filtroCategoria.empty()) {
                sql = "SELECT id, nome, categoria, preco, estoque, descricao FROM produto" + ordemStr;
            } else {
                sql = "SELECT id, nome, categoria, preco, estoque, descricao FROM produto WHERE categoria ILIKE " + txn.quote("%" + filtroCategoria + "%") + ordemStr;
            }
            
            pqxx::result r = txn.exec(sql);
            
            int totalProdutos = 0; double valorTotalEstoque = 0.0; int itensBons = 0; 
            int index = 0;
            
            for (auto row : r) {
                double p = row["preco"].as<double>();
                int e = row["estoque"].as<int>();
                totalProdutos++; valorTotalEstoque += (p * e);
                if (e >= 5) itensBons++; 
                
                LVITEMA lvi = {0};
                lvi.mask = LVIF_TEXT;
                lvi.iItem = index;
                lvi.iSubItem = 0;
                
                std::string idStr = row["id"].c_str();
                std::string nomeStr = row["nome"].c_str();
                std::string catStr = row["categoria"].c_str();
                std::string precoStr = "R$ " + std::string(row["preco"].c_str());
                std::string estStr = row["estoque"].c_str();
                // Verifica se a descricao é nula
                std::string descStr = !row["descricao"].is_null() ? row["descricao"].c_str() : ""; 

                lvi.pszText = (LPSTR)idStr.c_str();
                ListView_InsertItem(hList, &lvi);
                ListView_SetItemText(hList, index, 1, (LPSTR)nomeStr.c_str());
                ListView_SetItemText(hList, index, 2, (LPSTR)catStr.c_str());
                ListView_SetItemText(hList, index, 3, (LPSTR)precoStr.c_str());
                ListView_SetItemText(hList, index, 4, (LPSTR)estStr.c_str());
                ListView_SetItemText(hList, index, 5, (LPSTR)descStr.c_str()); // 6a Coluna
                
                index++;
            }
            txn.commit();
            
            char bufferDash[256];
            snprintf(bufferDash, sizeof(bufferDash), "Resumo: %d produtos listados | Valor Total: R$ %.2f", totalProdutos, valorTotalEstoque);
            SetWindowTextA(hDash, bufferDash);
            
            int porcentagem = (totalProdutos > 0) ? (itensBons * 100) / totalProdutos : 0;
            SendMessage(hProgressBar, PBM_SETPOS, porcentagem, 0);
            
        } catch (const std::exception& e) { MessageBoxA(hwnd, e.what(), "Erro ao Listar", MB_ICONERROR); }
        db.desconectar();
    }
}

LRESULT CALLBACK PassWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    static HWND hPassEdit;
    switch (msg) {
        case WM_CREATE: {
            CreateWindowExA(0, "STATIC", "Autenticacao Administrativa", WS_CHILD | WS_VISIBLE | SS_CENTER, 10, 10, 260, 20, hwnd, NULL, NULL, NULL);
            CreateWindowExA(0, "STATIC", "Digite a senha para resetar:", WS_CHILD | WS_VISIBLE, 20, 40, 200, 20, hwnd, NULL, NULL, NULL);
            hPassEdit = CreateWindowExA(0, "EDIT", "", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_PASSWORD | ES_AUTOHSCROLL, 20, 65, 240, 25, hwnd, NULL, NULL, NULL);
            CreateWindowExA(0, "BUTTON", "Confirmar", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW, 85, 100, 110, 30, hwnd, (HMENU)1, NULL, NULL);
            break;
        }
        case WM_CTLCOLORSTATIC: {
            HDC hdcStatic = (HDC)wParam; SetTextColor(hdcStatic, corTexto); SetBkColor(hdcStatic, corFundo); return (INT_PTR)hbrFundo;
        }
        case WM_CTLCOLOREDIT: {
            HDC hdcEdit = (HDC)wParam; SetTextColor(hdcEdit, corTexto); SetBkColor(hdcEdit, corCaixa); return (INT_PTR)hbrCaixaTexto;
        }
        case WM_DRAWITEM: {
            LPDRAWITEMSTRUCT pdis = (LPDRAWITEMSTRUCT)lParam;
            if (pdis->CtlType == ODT_BUTTON) {
                HBRUSH hBrush = CreateSolidBrush(RGB(243, 139, 168)); 
                FillRect(pdis->hDC, &pdis->rcItem, hBrush); DeleteObject(hBrush);
                SetBkMode(pdis->hDC, TRANSPARENT); SetTextColor(pdis->hDC, RGB(17, 17, 27)); 
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

            // --- LADO ESQUERDO: Ajustado verticalmente para caber o campo Descricao ---
            CreateWindowExA(0, "STATIC", "CADASTRAR / EDITAR PRODUTO", WS_CHILD | WS_VISIBLE, 20, 10, 240, 20, hwnd, NULL, NULL, NULL);
            
            CreateWindowExA(0, "STATIC", "ID:", WS_CHILD | WS_VISIBLE, 20, 35, 60, 20, hwnd, NULL, NULL, NULL);
            hIdEdit = CreateWindowExA(0, "EDIT", "", WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_NUMBER, 90, 35, 60, 25, hwnd, (HMENU)ID_EDIT_ID_EDIT, NULL, NULL);
            
            CreateWindowExA(0, "STATIC", "Nome:", WS_CHILD | WS_VISIBLE, 20, 65, 60, 20, hwnd, NULL, NULL, NULL);
            hNome = CreateWindowExA(0, "EDIT", "", WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL, 90, 65, 150, 25, hwnd, (HMENU)ID_EDIT_NOME, NULL, NULL);
            
            CreateWindowExA(0, "STATIC", "Categoria:", WS_CHILD | WS_VISIBLE, 20, 95, 70, 20, hwnd, NULL, NULL, NULL);
            hCat = CreateWindowExA(0, "COMBOBOX", "", WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWN | WS_VSCROLL, 90, 95, 150, 150, hwnd, (HMENU)ID_COMBO_CAT, NULL, NULL);
            
            CreateWindowExA(0, "STATIC", "Preco:", WS_CHILD | WS_VISIBLE, 20, 125, 60, 20, hwnd, NULL, NULL, NULL);
            hPreco = CreateWindowExA(0, "EDIT", "", WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_NUMBER, 90, 125, 150, 25, hwnd, (HMENU)ID_EDIT_PRECO, NULL, NULL);
            
            CreateWindowExA(0, "STATIC", "Estoque:", WS_CHILD | WS_VISIBLE, 20, 155, 60, 20, hwnd, NULL, NULL, NULL);
            hEstoque = CreateWindowExA(0, "EDIT", "", WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_NUMBER, 90, 155, 150, 25, hwnd, (HMENU)ID_EDIT_ESTOQUE, NULL, NULL);
            
            // NOVO: Campo Descricao
            CreateWindowExA(0, "STATIC", "Detalhes:", WS_CHILD | WS_VISIBLE, 20, 185, 70, 20, hwnd, NULL, NULL, NULL);
            hDesc = CreateWindowExA(0, "EDIT", "", WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL, 90, 185, 150, 25, hwnd, (HMENU)ID_EDIT_DESC, NULL, NULL);
            
            CreateWindowExA(0, "BUTTON", "Adicionar", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW, 20, 220, 105, 35, hwnd, (HMENU)ID_BTN_ADD, NULL, NULL);
            CreateWindowExA(0, "BUTTON", "Salvar Edicao", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW, 135, 220, 105, 35, hwnd, (HMENU)ID_BTN_EDITAR, NULL, NULL);

            CreateWindowExA(0, "STATIC", "AJUSTE DE ESTOQUE", WS_CHILD | WS_VISIBLE, 20, 270, 200, 20, hwnd, NULL, NULL, NULL);
            CreateWindowExA(0, "STATIC", "ID:", WS_CHILD | WS_VISIBLE, 20, 295, 30, 20, hwnd, NULL, NULL, NULL);
            hIdBaixa = CreateWindowExA(0, "EDIT", "", WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_NUMBER, 60, 295, 180, 25, hwnd, (HMENU)ID_EDIT_ID_BAIXA, NULL, NULL);
            CreateWindowExA(0, "STATIC", "Qtd:", WS_CHILD | WS_VISIBLE, 20, 325, 40, 20, hwnd, NULL, NULL, NULL);
            hQtdBaixa = CreateWindowExA(0, "EDIT", "", WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_NUMBER, 60, 325, 180, 25, hwnd, (HMENU)ID_EDIT_QTD_BAIXA, NULL, NULL);
            CreateWindowExA(0, "BUTTON", "Baixa (-)", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW, 20, 360, 105, 35, hwnd, (HMENU)ID_BTN_BAIXA, NULL, NULL);
            CreateWindowExA(0, "BUTTON", "Subir (+)", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW, 135, 360, 105, 35, hwnd, (HMENU)ID_BTN_ADD_ESTOQUE, NULL, NULL);

            CreateWindowExA(0, "STATIC", "APAGAR PRODUTO", WS_CHILD | WS_VISIBLE, 20, 410, 200, 20, hwnd, NULL, NULL, NULL);
            CreateWindowExA(0, "STATIC", "ID:", WS_CHILD | WS_VISIBLE, 20, 435, 30, 20, hwnd, NULL, NULL, NULL);
            hIdDel = CreateWindowExA(0, "EDIT", "", WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_NUMBER, 60, 435, 180, 25, hwnd, (HMENU)ID_EDIT_ID_DEL, NULL, NULL);
            CreateWindowExA(0, "BUTTON", "Apagar do Sistema", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW, 20, 465, 220, 30, hwnd, (HMENU)ID_BTN_DEL, NULL, NULL);

            // --- LADO DIREITO: Largura ampliada para caber a nova coluna Descricao ---
            CreateWindowExA(0, "STATIC", "Cat:", WS_CHILD | WS_VISIBLE, 270, 15, 35, 20, hwnd, NULL, NULL, NULL);
            hFiltroCat = CreateWindowExA(0, "COMBOBOX", "", WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWN | WS_VSCROLL, 305, 12, 80, 150, hwnd, (HMENU)ID_COMBO_FILTRO, NULL, NULL);
            
            CreateWindowExA(0, "BUTTON", "Buscar", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW, 390, 10, 60, 30, hwnd, (HMENU)ID_BTN_FILTRAR, NULL, NULL);
            CreateWindowExA(0, "BUTTON", "Atualizar", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW, 455, 10, 80, 30, hwnd, (HMENU)ID_BTN_LISTAR, NULL, NULL);
            CreateWindowExA(0, "BUTTON", "Estoque Critico", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW, 540, 10, 110, 30, hwnd, (HMENU)ID_BTN_CRITICO, NULL, NULL);
            
            CreateWindowExA(0, "STATIC", "Ordenar:", WS_CHILD | WS_VISIBLE, 270, 45, 65, 20, hwnd, NULL, NULL, NULL);
            hRadioId = CreateWindowExA(0, "BUTTON", "Por ID", WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON, 340, 45, 70, 20, hwnd, (HMENU)ID_RADIO_ID, NULL, NULL);
            hRadioPreco = CreateWindowExA(0, "BUTTON", "Maior Preco", WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON, 415, 45, 100, 20, hwnd, (HMENU)ID_RADIO_PRECO, NULL, NULL);
            hRadioEstoque = CreateWindowExA(0, "BUTTON", "Menor Estoque", WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON, 520, 45, 120, 20, hwnd, (HMENU)ID_RADIO_ESTOQUE, NULL, NULL);
            SendMessage(hRadioId, BM_SETCHECK, BST_CHECKED, 0); 

            // Tabela agora tem largura de 490px
            hList = CreateWindowExA(WS_EX_CLIENTEDGE, WC_LISTVIEWA, "", WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS, 270, 70, 490, 380, hwnd, (HMENU)ID_LISTBOX, NULL, NULL);
            ListView_SetExtendedListViewStyle(hList, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);

            // Adicionando as Colunas (incluindo Descricao)
            LVCOLUMNA lvc = {0};
            lvc.mask = LVCF_FMT | LVCF_WIDTH | LVCF_TEXT | LVCF_SUBITEM;
            lvc.fmt = LVCFMT_LEFT;

            lvc.iSubItem = 0; lvc.cx = 40;  lvc.pszText = (LPSTR)"ID";        ListView_InsertColumn(hList, 0, &lvc);
            lvc.iSubItem = 1; lvc.cx = 120; lvc.pszText = (LPSTR)"Nome";      ListView_InsertColumn(hList, 1, &lvc);
            lvc.iSubItem = 2; lvc.cx = 80;  lvc.pszText = (LPSTR)"Categoria"; ListView_InsertColumn(hList, 2, &lvc);
            lvc.iSubItem = 3; lvc.cx = 60;  lvc.pszText = (LPSTR)"Preco";     ListView_InsertColumn(hList, 3, &lvc);
            lvc.iSubItem = 4; lvc.cx = 55;  lvc.pszText = (LPSTR)"Estoque";   ListView_InsertColumn(hList, 4, &lvc);
            lvc.iSubItem = 5; lvc.cx = 130; lvc.pszText = (LPSTR)"Descricao"; ListView_InsertColumn(hList, 5, &lvc); // NOVO

            ListView_SetBkColor(hList, corCaixa);
            ListView_SetTextBkColor(hList, corCaixa);
            ListView_SetTextColor(hList, corTexto);

            hDash = CreateWindowExA(0, "STATIC", "Resumo: 0 produtos | Valor: R$ 0.00", WS_CHILD | WS_VISIBLE, 270, 460, 490, 20, hwnd, (HMENU)ID_DASHBOARD, NULL, NULL);
            
            CreateWindowExA(0, "STATIC", "Saude do Estoque:", WS_CHILD | WS_VISIBLE, 270, 485, 120, 20, hwnd, NULL, NULL, NULL);
            hProgressBar = CreateWindowExA(0, PROGRESS_CLASS, NULL, WS_CHILD | WS_VISIBLE | PBS_SMOOTH, 395, 485, 365, 18, hwnd, (HMENU)ID_PROGRESS_BAR, NULL, NULL);
            SendMessage(hProgressBar, PBM_SETRANGE, 0, MAKELPARAM(0, 100)); 
            
            CreateWindowExA(0, "STATIC", "REGISTRO DE ATIVIDADES:", WS_CHILD | WS_VISIBLE, 20, 520, 200, 20, hwnd, NULL, NULL, NULL);
            hLogs = CreateWindowExA(0, "LISTBOX", NULL, WS_CHILD | WS_VISIBLE | WS_VSCROLL, 20, 540, 740, 90, hwnd, (HMENU)ID_LISTBOX_LOGS, NULL, NULL);

            CreateWindowExA(0, "BUTTON", "Exportar Relatorio (CSV)", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW, 20, 640, 310, 30, hwnd, (HMENU)ID_BTN_EXPORTAR, NULL, NULL);
            CreateWindowExA(0, "BUTTON", "Reiniciar Dados", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW, 340, 640, 310, 30, hwnd, (HMENU)ID_BTN_RESET, NULL, NULL);

            // Sem Emojis como pedido
            CreateWindowExA(0, "BUTTON", "Modo Claro", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW, 660, 640, 100, 30, hwnd, (HMENU)ID_BTN_TEMA, NULL, NULL);

            EnumChildWindows(hwnd, [](HWND child, LPARAM fonts) -> BOOL {
                HFONT* pFonts = (HFONT*)fonts;
                char className[256]; GetClassNameA(child, className, 256);
                if (std::string(className) == "STATIC") SendMessage(child, WM_SETFONT, (WPARAM)pFonts[0], TRUE);
                else SendMessage(child, WM_SETFONT, (WPARAM)pFonts[1], TRUE);
                return TRUE;
            }, (LPARAM)(HFONT[]){hFont, hFontNormal});

            wpOrigEditProc = (WNDPROC)SetWindowLongPtr(hNome, GWLP_WNDPROC, (LONG_PTR)EditSubclassProc);
            SetWindowLongPtr(hPreco, GWLP_WNDPROC, (LONG_PTR)EditSubclassProc);
            SetWindowLongPtr(hEstoque, GWLP_WNDPROC, (LONG_PTR)EditSubclassProc);
            SetWindowLongPtr(hDesc, GWLP_WNDPROC, (LONG_PTR)EditSubclassProc); // Aplica a Descricao
            wpOrigListProc = (WNDPROC)SetWindowLongPtr(hList, GWLP_WNDPROC, (LONG_PTR)ListSubclassProc);

            AdicionarLog("Sistema iniciado e pronto para uso.");
            break;
        }
        
        case WM_CTLCOLORSTATIC: {
            HDC hdcStatic = (HDC)wParam; SetTextColor(hdcStatic, corTexto); SetBkColor(hdcStatic, corFundo); return (INT_PTR)hbrFundo;
        }
        case WM_CTLCOLOREDIT:
        case WM_CTLCOLORLISTBOX: {
            HDC hdcEdit = (HDC)wParam; SetTextColor(hdcEdit, corTexto); SetBkColor(hdcEdit, corCaixa); return (INT_PTR)hbrCaixaTexto;
        }

        case WM_DRAWITEM: {
            LPDRAWITEMSTRUCT pdis = (LPDRAWITEMSTRUCT)lParam;
            if (pdis->CtlType == ODT_BUTTON) {
                COLORREF corBtn;
                if (pdis->CtlID == ID_BTN_ADD) corBtn = RGB(166, 227, 161);            
                else if (pdis->CtlID == ID_BTN_EDITAR) corBtn = RGB(250, 179, 135);
                else if (pdis->CtlID == ID_BTN_CRITICO) corBtn = RGB(243, 139, 168); 
                else if (pdis->CtlID == ID_BTN_BAIXA) corBtn = RGB(249, 226, 175);      
                else if (pdis->CtlID == ID_BTN_ADD_ESTOQUE) corBtn = RGB(137, 180, 250);
                else if (pdis->CtlID == ID_BTN_DEL) corBtn = RGB(243, 139, 168);        
                else if (pdis->CtlID == ID_BTN_RESET) corBtn = RGB(203, 166, 247);      
                else if (pdis->CtlID == ID_BTN_FILTRAR) corBtn = RGB(180, 190, 254);
                else if (pdis->CtlID == ID_BTN_EXPORTAR) corBtn = RGB(148, 226, 213); 
                else if (pdis->CtlID == ID_BTN_TEMA) corBtn = modoEscuro ? RGB(249, 226, 175) : RGB(147, 153, 178); 
                else corBtn = RGB(147, 153, 178); 

                if (pdis->itemState & ODS_SELECTED) corBtn = RGB(GetRValue(corBtn) - 30, GetGValue(corBtn) - 30, GetBValue(corBtn) - 30);

                HBRUSH hBrush = CreateSolidBrush(corBtn);
                FillRect(pdis->hDC, &pdis->rcItem, hBrush);
                DeleteObject(hBrush);

                SetBkMode(pdis->hDC, TRANSPARENT); SetTextColor(pdis->hDC, RGB(17, 17, 27)); 
                char textoBotao[256]; GetWindowTextA(pdis->hwndItem, textoBotao, 256);
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
                    ListView_GetItemText(hList, index, 5, descBuf, 512); // Pega a descricao

                    std::string p = precoBuf;
                    if(p.find("R$ ") == 0) p = p.substr(3);

                    SetWindowTextA(hIdEdit, idBuf); SetWindowTextA(hNome, nomeBuf); SetWindowTextA(hCat, catBuf);
                    SetWindowTextA(hPreco, p.c_str()); SetWindowTextA(hEstoque, estBuf);
                    SetWindowTextA(hDesc, descBuf); // Preenche a descricao na caixa de texto
                    SetWindowTextA(hIdBaixa, idBuf); SetWindowTextA(hIdDel, idBuf);
                    AdicionarLog("Produto ID " + std::string(idBuf) + " selecionado para edicao.");
                }
            }
            break;
        }

        case WM_COMMAND: {
            int wmId = LOWORD(wParam);
            int wmEvent = HIWORD(wParam);
            
            if (wmId == ID_BTN_TEMA) {
                modoEscuro = !modoEscuro;
                
                corFundo = modoEscuro ? RGB(30, 30, 46) : RGB(239, 241, 245);
                corCaixa = modoEscuro ? RGB(49, 50, 68) : RGB(204, 208, 218);
                corTexto = modoEscuro ? RGB(205, 214, 244) : RGB(76, 79, 105);

                DeleteObject(hbrFundo); DeleteObject(hbrCaixaTexto);
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

            if (wmId == ID_COMBO_FILTRO && (wmEvent == CBN_EDITCHANGE || wmEvent == CBN_SELCHANGE)) {
                std::string busca = ObterTexto(hFiltroCat); AtualizarLista(hwnd, busca);
            }
            
            if (wmId == ID_RADIO_ID)      { ordemAtual = 1; AtualizarLista(hwnd, ObterTexto(hFiltroCat)); }
            if (wmId == ID_RADIO_PRECO)   { ordemAtual = 2; AtualizarLista(hwnd, ObterTexto(hFiltroCat)); }
            if (wmId == ID_RADIO_ESTOQUE) { ordemAtual = 3; AtualizarLista(hwnd, ObterTexto(hFiltroCat)); }
            
            if (wmId == ID_BTN_LISTAR) { SetWindowTextA(hFiltroCat, ""); AtualizarLista(hwnd, ""); }
            if (wmId == ID_BTN_FILTRAR) { AtualizarLista(hwnd, ObterTexto(hFiltroCat)); }
            if (wmId == ID_BTN_CRITICO) { AtualizarLista(hwnd, "", true); AdicionarLog("Filtro Aplicado: Apenas itens criticos."); }
            
            if (wmId == ID_BTN_ADD) {
                std::string nome = ObterTexto(hNome); std::string cat = ObterTexto(hCat);
                std::string preco = ObterTexto(hPreco); std::string est = ObterTexto(hEstoque);
                std::string desc = ObterTexto(hDesc); // Pega a descricao

                if(nome.empty() || preco.empty() || est.empty()) { MessageBoxA(hwnd, "Preencha Nome, Preco e Estoque!", "Aviso", MB_ICONWARNING); break; }

                if (db.conectar()) {
                    try {
                        pqxx::work txn(*db.getConn());
                        txn.exec("INSERT INTO produto (nome, categoria, preco, estoque, descricao) VALUES (" + txn.quote(nome) + ", " + txn.quote(cat) + ", " + preco + ", " + est + ", " + txn.quote(desc) + ");"); 
                        txn.commit();
                        
                        SetWindowTextA(hIdEdit, ""); SetWindowTextA(hNome, ""); SetWindowTextA(hCat, ""); 
                        SetWindowTextA(hPreco, ""); SetWindowTextA(hEstoque, ""); SetWindowTextA(hDesc, "");
                        AtualizarCategorias(); AtualizarLista(hwnd, ObterTexto(hFiltroCat)); AdicionarLog("Sucesso: Produto '" + nome + "' inserido."); 
                    } catch (const std::exception& e) { MessageBoxA(hwnd, e.what(), "Erro ao Adicionar", MB_ICONERROR); }
                    db.desconectar();
                }
            }
            
            if (wmId == ID_BTN_EDITAR) {
                std::string id = ObterTexto(hIdEdit); std::string nome = ObterTexto(hNome); std::string cat = ObterTexto(hCat);
                std::string preco = ObterTexto(hPreco); std::string est = ObterTexto(hEstoque);
                std::string desc = ObterTexto(hDesc); // Pega a descricao editada

                if(id.empty() || nome.empty() || preco.empty() || est.empty()) { MessageBoxA(hwnd, "De um duplo clique num produto ou preencha o ID para editar!", "Aviso", MB_ICONWARNING); break; }

                if (db.conectar()) {
                    try {
                        pqxx::work txn(*db.getConn());
                        std::string sql = "UPDATE produto SET nome = " + txn.quote(nome) + ", categoria = " + txn.quote(cat) + ", preco = " + preco + ", estoque = " + est + ", descricao = " + txn.quote(desc) + " WHERE id = " + txn.quote(id) + ";";
                        pqxx::result r = txn.exec(sql);
                        if (r.affected_rows() > 0) {
                            txn.commit();
                            SetWindowTextA(hIdEdit, ""); SetWindowTextA(hNome, ""); SetWindowTextA(hCat, ""); 
                            SetWindowTextA(hPreco, ""); SetWindowTextA(hEstoque, ""); SetWindowTextA(hDesc, "");
                            AtualizarCategorias(); AtualizarLista(hwnd, ObterTexto(hFiltroCat)); AdicionarLog("Sucesso: Produto ID " + id + " editado."); MessageBoxA(hwnd, "Produto editado com sucesso!", "Sucesso", MB_OK);
                        } else { MessageBoxA(hwnd, "Edicao falhou! Verifique se o ID existe.", "Atencao", MB_ICONWARNING); }
                    } catch (const std::exception& e) { MessageBoxA(hwnd, e.what(), "Erro ao Editar", MB_ICONERROR); }
                    db.desconectar();
                }
            }

            if (wmId == ID_BTN_BAIXA) {
                std::string id = ObterTexto(hIdBaixa); std::string qtd = ObterTexto(hQtdBaixa);

                if(id.empty() || qtd.empty()) { MessageBoxA(hwnd, "Preencha o ID e a Quantidade!", "Aviso", MB_ICONWARNING); break; }

                if (db.conectar()) {
                    try {
                        pqxx::work txn(*db.getConn());
                        std::string sql = "UPDATE produto SET estoque = estoque - " + txn.quote(qtd) + " WHERE id = " + txn.quote(id) + " AND estoque >= " + txn.quote(qtd) + ";";
                        pqxx::result r = txn.exec(sql);
                        
                        if (r.affected_rows() > 0) {
                            std::string check_sql = "SELECT nome, estoque FROM produto WHERE id = " + txn.quote(id) + ";";
                            pqxx::result res_check = txn.exec(check_sql); txn.commit();
                            SetWindowTextA(hIdBaixa, ""); SetWindowTextA(hQtdBaixa, ""); AtualizarLista(hwnd, ObterTexto(hFiltroCat)); AdicionarLog("Baixa realizada: -" + qtd + " unid. no ID " + id);

                            if (!res_check.empty()) {
                                int estoque_atual = res_check[0]["estoque"].as<int>(); std::string nome_prod = res_check[0]["nome"].as<std::string>();
                                if (estoque_atual < 5) {
                                    std::string msg_alerta = "ATENCAO: O produto '" + nome_prod + "' esta com estoque baixo! Restam apenas " + std::to_string(estoque_atual) + " unidades.";
                                    MessageBoxA(hwnd, msg_alerta.c_str(), "Alerta de Estoque", MB_ICONEXCLAMATION);
                                }
                            }
                        } else { MessageBoxA(hwnd, "Baixa nao realizada! Verifique o ID e se ha estoque suficiente.", "Atencao", MB_ICONWARNING); }
                    } catch (const std::exception& e) { MessageBoxA(hwnd, e.what(), "Erro na Baixa", MB_ICONERROR); }
                    db.desconectar();
                }
            }

            if (wmId == ID_BTN_ADD_ESTOQUE) {
                std::string id = ObterTexto(hIdBaixa); std::string qtd = ObterTexto(hQtdBaixa);
                if(id.empty() || qtd.empty()) { MessageBoxA(hwnd, "Preencha o ID e a Quantidade!", "Aviso", MB_ICONWARNING); break; }

                if (db.conectar()) {
                    try {
                        pqxx::work txn(*db.getConn());
                        pqxx::result r = txn.exec("UPDATE produto SET estoque = estoque + " + txn.quote(qtd) + " WHERE id = " + txn.quote(id) + ";");
                        if (r.affected_rows() > 0) {
                            txn.commit();
                            SetWindowTextA(hIdBaixa, ""); SetWindowTextA(hQtdBaixa, ""); AtualizarLista(hwnd, ObterTexto(hFiltroCat)); AdicionarLog("Estoque atualizado: +" + qtd + " unid. no ID " + id); MessageBoxA(hwnd, "Estoque adicionado com sucesso!", "Sucesso", MB_OK);
                        } else { MessageBoxA(hwnd, "Operacao falhou! Verifique se o ID existe.", "Atencao", MB_ICONWARNING); }
                    } catch (const std::exception& e) { MessageBoxA(hwnd, e.what(), "Erro ao Adicionar Estoque", MB_ICONERROR); }
                    db.desconectar();
                }
            }

            if (wmId == ID_BTN_DEL) {
                std::string id = ObterTexto(hIdDel);
                if(id.empty()) { MessageBoxA(hwnd, "Digite o ID para apagar!", "Aviso", MB_ICONWARNING); break; }
                if (MessageBoxA(hwnd, ("Tem a certeza que deseja apagar o produto ID " + id + "?").c_str(), "Confirmar Remocao", MB_YESNO | MB_ICONQUESTION) == IDYES) {
                    if (db.conectar()) {
                        try {
                            pqxx::work txn(*db.getConn()); txn.exec("DELETE FROM produto WHERE id = " + id + ";"); txn.commit();
                            SetWindowTextA(hIdDel, ""); AtualizarCategorias(); AtualizarLista(hwnd, ObterTexto(hFiltroCat)); AdicionarLog("Aviso: Produto ID " + id + " removido.");
                        } catch (const std::exception& e) { MessageBoxA(hwnd, e.what(), "Erro ao Apagar", MB_ICONERROR); }
                        db.desconectar();
                    }
                }
            }

            if (wmId == ID_BTN_EXPORTAR) {
                if (db.conectar()) {
                    std::ofstream file("relatorio.csv");
                    if (file.is_open()) {
                        file << "ID;Nome;Categoria;Preco;Estoque;Descricao\n"; // Adicionado no CSV
                        try {
                            pqxx::work txn(*db.getConn());
                            pqxx::result r = txn.exec("SELECT id, nome, categoria, preco, estoque, descricao FROM produto ORDER BY id ASC;");
                            for (auto row : r) { 
                                std::string descStr = !row["descricao"].is_null() ? row["descricao"].c_str() : "";
                                file << row["id"].c_str() << ";" << row["nome"].c_str() << ";" << row["categoria"].c_str() << ";" << row["preco"].c_str() << ";" << row["estoque"].c_str() << ";" << descStr << "\n"; 
                            }
                            file.close(); AdicionarLog("Exportacao: 'relatorio.csv' gerado na pasta do projeto."); MessageBoxA(hwnd, "Relatorio exportado como CSV! \nAbra no Excel para ver as colunas formatadas.", "Sucesso", MB_OK);
                        } catch (const std::exception& e) { MessageBoxA(hwnd, "Erro ao extrair dados para o relatorio.", "Erro", MB_ICONERROR); }
                    } else { MessageBoxA(hwnd, "Erro ao criar o ficheiro CSV.", "Erro", MB_ICONERROR); }
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
                            std::stringstream buffer; buffer << sqlFile.rdbuf(); std::string sql_repor = buffer.str(); sqlFile.close();
                            pqxx::work txn(*db.getConn()); txn.exec(sql_repor); txn.commit();
                            
                            SetWindowTextA(hFiltroCat, ""); SendMessage(hRadioId, BM_SETCHECK, BST_CHECKED, 0); ordemAtual = 1;
                            AtualizarCategorias(); AtualizarLista(hwnd, ""); AdicionarLog("Produtos repostos via script externo.");
                            MessageBoxA(hwnd, "Produtos repostos com sucesso lendo o seu arquivo .sql!", "Operacao Autorizada", MB_OK);
                        }
                    } catch (const std::exception& e) { MessageBoxA(hwnd, e.what(), "Erro ao Repor Produtos", MB_ICONERROR); }
                    db.desconectar();
                }
            }
            break;
        }
        case WM_DESTROY: {
            DeleteObject(hbrFundo); DeleteObject(hbrCaixaTexto); PostQuitMessage(0); break;
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

    WNDCLASSA wc = {0};
    wc.lpfnWndProc = WndProc; wc.hInstance = hInstance; wc.lpszClassName = "BarDoBodeGUI";
    wc.hbrBackground = hbrFundo; wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hIcon = LoadIcon(hInstance, "MAINICON");

    if (!RegisterClassA(&wc)) return 0;

    WNDCLASSA wcPass = {0};
    wcPass.lpfnWndProc = PassWndProc; wcPass.hInstance = hInstance; wcPass.lpszClassName = "PassWindowClass";
    wcPass.hbrBackground = hbrFundo; wcPass.hCursor = LoadCursor(NULL, IDC_ARROW);
    RegisterClassA(&wcPass);

    // Janela alargada para 800px para as colunas da tabela caberem melhor!
    HWND hwnd = CreateWindowExA(0, "BarDoBodeGUI", "Bar do Bode - Sistema de Estoque",
        WS_OVERLAPPEDWINDOW ^ WS_THICKFRAME ^ WS_MAXIMIZEBOX, 
        CW_USEDEFAULT, CW_USEDEFAULT, 800, 740, NULL, NULL, hInstance, NULL);

    if (hwnd == NULL) return 0;
    
    AtualizarCategorias();
    ShowWindow(hwnd, nCmdShow); AtualizarLista(hwnd, "");

    MSG msg = {0};
    while (GetMessage(&msg, NULL, 0, 0)) { TranslateMessage(&msg); DispatchMessage(&msg); }
    return 0;
}