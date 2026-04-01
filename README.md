# Bar do Bode — Sistema de Gestão de Estoque (GUI)
**Banco de Dados I — 2026.1 (Evoluído para Aplicação Nativa Windows)**

Sistema de gerenciamento de estoque com Interface Gráfica (Win32 API) desenvolvido em C++ puro. Conectado a um banco de dados PostgreSQL via `libpqxx`, oferecendo recursos avançados como Tabela Profissional (DataGrid), Modo Claro/Escuro, Exportação de Relatórios e Autenticação Administrativa.

---

##  Estrutura do Projeto

```text
bar_crud/
├── include/              # Arquivos de cabeçalho (Headers)
│   ├── crud_manager.h    # Operações CRUD estruturadas
│   ├── db_conexao.h      # Gerenciamento da conexão PostgreSQL
│   └── produto.h         # Classe Produto (entidade)
├── src/                  # Código fonte principal
│   ├── interface_gui.cpp # Interface Gráfica Principal (Win32 API)
│   └── main.cpp          # Verificador e Inicializador do Banco de Dados
├── sql/                  # Scripts de Banco de Dados
│   ├── repor_produtos.sql# Script para inserção dos produtos iniciais
│   └── setup.sql         # Cria tabelas, índices, views e procedures
├── Makefile              # Automação de compilação (MinGW/GCC)
└── README.md             # Este arquivo
```

---

##  Pré-requisitos (Ambiente Windows)

1. **PostgreSQL + DBeaver:**
   - Ter o PostgreSQL instalado localmente (usuário e senha configurados em `db_conexao.h`).
   - Recomenda-se o **DBeaver** para visualização e administração do banco.
2. **Compilador C++ (MinGW-w64 / MSYS2):**
   - O ambiente deve suportar a compilação com a biblioteca `<windows.h>`.
3. **Bibliotecas Dinâmicas (DLLs):**
   - As DLLs do `libpq` e `libpqxx` devem estar na raiz do projeto para a correta execução do `.exe`.

---

##  Configuração do Banco de Dados

Neste projeto, utilizamos o banco padrão **`postgres`**.

1. Abra o **DBeaver** e conecte-se ao seu servidor PostgreSQL local.
2. Navegue até o banco `postgres` e abra um novo Editor SQL (`F3`).
3. Copie o conteúdo de `sql/setup.sql` e execute (`Alt + X`) para criar a tabela `produto` e as dependências.
4. (Opcional) Execute `sql/repor_produtos.sql` para preencher o estoque inicial.

---

##  Como Compilar e Rodar

Abra o terminal (PowerShell ou VS Code) na raiz do projeto e utilize o `Make`:

```bash
# 1. (Opcional) Rodar o inicializador do backend para checar a conexão
make
./bar_crud.exe

# 2. Compilar a Interface Gráfica (GUI)
make gui

# 3. Executar o sistema
./bar_crud_gui.exe
```

---

##  Funcionalidades da Interface Gráfica

| Recurso | Descrição | SQL Envolvido |
|---------|-----------|---------------|
| **DataGrid Interativo** | Tabela profissional com colunas redimensionáveis. | `SELECT * FROM produto ORDER BY...` |
| **Baixa de Estoque (PDV)** | Subtrai unidades de forma segura. Bloqueia se o estoque for menor que a quantidade exigida. | `UPDATE produto SET estoque = estoque - qtd WHERE ...` |
| **Exportação CSV** | Gera o arquivo `relatorio.csv` compatível com o Excel, extraindo a tabela atual do banco. | `SELECT id, nome, categoria...` |
| **Reiniciar Dados** | Botão protegido por senha (`1234`) que lê automaticamente um script SQL externo para restaurar produtos. | Lê e executa `sql/repor_produtos.sql` |
| **Filtros e Buscas** | Filtra os itens por categoria e por status de "Estoque Crítico". | `SELECT ... WHERE estoque < 5` / `ILIKE` |
| **Personalização Visual** | Alternância em tempo real entre **Modo Claro** e **Modo Escuro** (Paleta Catppuccin). | *(Manipulação GDI nativa do Windows)* |

---

##  Objetos de Banco Criados

| Objeto | Nome | Descrição |
|--------|------|-----------|
| **Tabela** | `produto` | Entidade principal com colunas ID, Nome, Categoria, Preço, Estoque e Descrição. |
| **Índice** | `idx_produto_nome` | Otimização da performance para buscas por nome. |
| **Índice** | `idx_produto_categoria` | Otimização para filtros na ComboBox de categoria. |
| **View** | `vw_estoque_baixo` | Visualização rápida de produtos com menos de 5 unidades. |
| **Procedure** | `fn_relatorio_estoque()` | Função armazenada para gerar relatórios agrupados de valores em estoque. |

---

##  Arquitetura do Sistema

```text
┌──────────────────────┐        usa        ┌────────────────┐
│ interface_gui.cpp    │ ────────────────▶ │  CRUDManager   │
│ (Win32 GUI + Events) │                   │                │
└──────────────────────┘                   │ inserir()      │
           │                               │ alterar()      │
           │                               │ pesquisar()    │
           │ Lê arquivos                   │ remover()      │
           ▼                               │ listarTodos()  │
┌──────────────────────┐                   └───────┬────────┘
│ sql/repor_produtos.sql│                          │ usa
│ relatorio.csv        │                   ┌──────▼──────┐
└──────────────────────┘                   │ DBConexao   │
                                           │ (libpqxx)   │
                                           └──────┬──────┘
                                                  │ (TCP/IP local)
                                           ┌──────▼──────────────┐
                                           │  PostgreSQL (DB)    │
                                           │  banco: postgres    │
                                           │  tabela: produto    │
                                           └─────────────────────┘
```

---
