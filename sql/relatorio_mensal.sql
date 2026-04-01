-- Criando a VIEW para o Relatório Mensal
CREATE OR REPLACE VIEW relatorio_mensal_vendedores AS
SELECT 
    v.nome AS vendedor,
    EXTRACT(MONTH FROM vd.data_venda) AS mes,
    EXTRACT(YEAR FROM vd.data_venda) AS ano,
    COUNT(vd.id) AS total_vendas_realizadas,
    COALESCE(SUM(vd.valor_total), 0.00) AS total_arrecadado
FROM vendedor v
LEFT JOIN venda vd ON v.id = vd.vendedor_id
GROUP BY v.nome, EXTRACT(MONTH FROM vd.data_venda), EXTRACT(YEAR FROM vd.data_venda);