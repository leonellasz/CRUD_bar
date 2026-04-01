-- Criando a PROCEDURE para processar o item da venda
CREATE OR REPLACE PROCEDURE adicionar_item_venda(
    p_venda_id INTEGER,
    p_produto_id INTEGER,
    p_quantidade INTEGER
)
LANGUAGE plpgsql
AS $$
DECLARE
    v_estoque INTEGER;
    v_preco_base NUMERIC(10,2);
    v_flamengo BOOLEAN;
    v_one_piece BOOLEAN;
    v_sousa BOOLEAN;
    v_desconto_percentual NUMERIC(10,2) := 0.00;
    v_preco_final NUMERIC(10,2);
BEGIN
    -- 1. Verifica o estoque atual do produto
    SELECT estoque, preco INTO v_estoque, v_preco_base FROM produto WHERE id = p_produto_id;

    -- REGRA DE NEGÓCIO: Trava a venda se faltar estoque!
    IF v_estoque < p_quantidade THEN
        RAISE EXCEPTION 'Erro: Estoque insuficiente! Tentou vender %, mas só temos % unidades.', p_quantidade, v_estoque;
    END IF;

    -- 2. Busca os dados do cliente desta venda para ver os descontos
    SELECT c.torce_flamengo, c.assiste_one_piece, c.de_sousa 
    INTO v_flamengo, v_one_piece, v_sousa
    FROM venda v JOIN cliente c ON v.cliente_id = c.id
    WHERE v.id = p_venda_id;

    -- REGRA DE NEGÓCIO: Aplica 10% de desconto cumulativo para cada atributo especial
    IF v_flamengo THEN v_desconto_percentual := v_desconto_percentual + 0.10; END IF;
    IF v_one_piece THEN v_desconto_percentual := v_desconto_percentual + 0.10; END IF;
    IF v_sousa THEN v_desconto_percentual := v_desconto_percentual + 0.10; END IF;

    -- Calcula o preço com os descontos
    v_preco_final := v_preco_base - (v_preco_base * v_desconto_percentual);

    -- 3. Insere o item na tabela (o "carrinho")
    INSERT INTO item_venda (venda_id, produto_id, quantidade, preco_unitario)
    VALUES (p_venda_id, p_produto_id, p_quantidade, v_preco_final);

    -- 4. Dá baixa no estoque automaticamente!
    UPDATE produto SET estoque = estoque - p_quantidade WHERE id = p_produto_id;

    -- 5. Soma o valor deste item no valor total da Venda
    UPDATE venda SET valor_total = valor_total + (v_preco_final * p_quantidade) WHERE id = p_venda_id;
END;
$$;