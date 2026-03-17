-- ============================================================
--  SCRIPT DE SETUP - BAR DO BODE
--  Execute este script como superusuário (postgres) antes
--  de rodar o programa C++.
--
--  Como executar:
--    psql -U postgres -f setup.sql
-- ============================================================

-- 1. Criar o banco de dados (execute separado se necessário)
-- CREATE DATABASE bar_do_bode;
-- \c bar_do_bode

-- ─── Tabela principal ───────────────────────────────────────
CREATE TABLE IF NOT EXISTS produto (
    id        SERIAL PRIMARY KEY,
    nome      VARCHAR(100)  NOT NULL,
    categoria VARCHAR(50)   NOT NULL,
    preco     NUMERIC(10,2) NOT NULL CHECK (preco >= 0),
    estoque   INTEGER       NOT NULL DEFAULT 0 CHECK (estoque >= 0),
    descricao TEXT
);

-- ─── Índices ────────────────────────────────────────────────
-- Acelera buscas por nome (ILIKE usa índice com pg_trgm, mas
-- um índice simples já ajuda ORDER BY e buscas exatas)
CREATE INDEX IF NOT EXISTS idx_produto_nome
    ON produto(nome);

CREATE INDEX IF NOT EXISTS idx_produto_categoria
    ON produto(categoria);

-- ─── View: produtos com estoque crítico ─────────────────────
CREATE OR REPLACE VIEW vw_estoque_baixo AS
    SELECT id, nome, categoria, preco, estoque
    FROM produto
    WHERE estoque < 5
    ORDER BY estoque ASC;

-- ─── Stored Procedure: relatório por categoria ──────────────
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
        COUNT(*)::BIGINT          AS qtd_produtos,
        SUM(p.estoque)::BIGINT    AS total_unidades,
        SUM(p.preco * p.estoque)  AS valor_total
    FROM produto p
    GROUP BY p.categoria
    ORDER BY valor_total DESC;
END;
$$ LANGUAGE plpgsql;

-- ─── Dados iniciais ─────────────────────────────────────────
INSERT INTO produto (nome, categoria, preco, estoque, descricao) VALUES
    ('Cerveja Heineken 600ml',  'Bebida',  12.00, 48, 'Cerveja long neck gelada'),
    ('Cerveja Brahma 350ml',    'Bebida',   6.50, 60, 'Latinha gelada'),
    ('Caipirinha',              'Drink',   15.00, 30, 'Com limao e cachaca artesanal'),
    ('Agua Mineral 500ml',      'Bebida',   4.00, 40, 'Gelada sem gas'),
    ('Refrigerante Coca 350ml', 'Bebida',   7.00, 35, 'Latinha gelada'),
    ('Porcao de Batata Frita',  'Petisco', 22.00, 20, 'Crocante com sal e tempero'),
    ('Calabresa Acebolada',     'Petisco', 28.00, 15, 'Porcao 300g'),
    ('Torresmo',                'Petisco', 25.00,  4, 'Porcao 250g crocante'),
    ('Frango na Brasa',         'Prato',   45.00, 10, 'Meio frango temperado'),
    ('Vodka Smirnoff 51ml',     'Drink',   12.00, 25, 'Dose simples')
ON CONFLICT DO NOTHING;

-- ─── Verificar ──────────────────────────────────────────────
SELECT 'Tabela produto criada com ' || COUNT(*) || ' registros.' AS status
FROM produto;

SELECT 'View vw_estoque_baixo OK' AS status;
SELECT * FROM fn_relatorio_estoque();
