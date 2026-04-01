-- 1. Criação da tabela de Vendedor (Requisito Parte 2)
CREATE TABLE IF NOT EXISTS vendedor (
    id SERIAL PRIMARY KEY,
    nome VARCHAR(100) NOT NULL
);

-- 2. Atualiza Produto (Campo de fabricação em Mari)
ALTER TABLE produto ADD COLUMN IF NOT EXISTS fabricado_em_mari BOOLEAN DEFAULT FALSE;

-- 3. Criação da tabela de Cliente (Com regras de desconto)
CREATE TABLE IF NOT EXISTS cliente (
    id SERIAL PRIMARY KEY,
    nome VARCHAR(100) NOT NULL,
    cpf VARCHAR(20),
    telefone VARCHAR(20),
    torce_flamengo BOOLEAN DEFAULT FALSE,
    assiste_one_piece BOOLEAN DEFAULT FALSE,
    de_sousa BOOLEAN DEFAULT FALSE
);

-- 4. Criação da tabela de Venda (Relacionada a Cliente e Vendedor)
CREATE TABLE IF NOT EXISTS venda (
    id SERIAL PRIMARY KEY,
    cliente_id INTEGER REFERENCES cliente(id),
    vendedor_id INTEGER REFERENCES vendedor(id),
    data_venda TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    valor_total DECIMAL(10,2) DEFAULT 0.00,
    forma_pagamento VARCHAR(50),
    status_pagamento VARCHAR(50)
);

-- 5. Criação da tabela de Itens da Venda (O Carrinho)
CREATE TABLE IF NOT EXISTS item_venda (
    id SERIAL PRIMARY KEY,
    venda_id INTEGER REFERENCES venda(id),
    produto_id INTEGER REFERENCES produto(id) ON DELETE RESTRICT,
    quantidade INTEGER NOT NULL,
    preco_unitario DECIMAL(10,2) NOT NULL,
    desconto_aplicado DECIMAL(10,2) DEFAULT 0.00
);

-- 6. Criação de Índices (Requisito de otimização)
CREATE INDEX IF NOT EXISTS idx_produto_nome ON produto(nome);
CREATE INDEX IF NOT EXISTS idx_produto_categoria ON produto(categoria);
CREATE INDEX IF NOT EXISTS idx_cliente_cpf ON cliente(cpf);