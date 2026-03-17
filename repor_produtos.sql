-- Script para repor os produtos iniciais do Bar do Bode
-- Só insere os que ainda não existem (pelo nome)

INSERT INTO produto (nome, categoria, preco, estoque, descricao)
SELECT * FROM (VALUES
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
) AS novos(nome, categoria, preco, estoque, descricao)
WHERE NOT EXISTS (
    SELECT 1 FROM produto p WHERE p.nome = novos.nome
);

-- Mostra como ficou
SELECT id, nome, categoria, preco, estoque FROM produto ORDER BY id;
