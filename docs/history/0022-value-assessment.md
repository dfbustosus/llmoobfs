# 0022 — Avaliação de utilidade, proposta de valor e maturidade como critério de adoção

Data: 2026-10-10. Branch: `m3/3b-query-authz`. Estado: concluído (análise; nada implementado).

## Objetivo

Responder, antes de qualquer divulgação, por que alguém usaria o MoobFS em vez de arquivos
convencionais ou SQLite, tratando a maturidade das alternativas como vantagem concreta delas e
o módulo de kernel como custo e risco a superar.

## Resultado

[`docs/value-assessment.md`](../value-assessment.md): desempenho por operação mapeado para W1–W9,
estimativa de memória por inode e atributo derivada das estruturas de `layer/index.c`
(marcada como estimativa), segurança separando o que é herdado do VFS do que a camada
acrescenta, confiabilidade e custo de manutenção de módulo, agilidade de desenvolvimento,
três casos de uso com métrica de comprovação e condição de não uso, matriz de proposta de
valor, resultados negativos do M4 que derrubam cada argumento, critérios de sucesso em quatro
níveis, seção de maturidade com matriz de decisão para laboratório, produção controlada e
produção crítica, e a entrega final (cinco benefícios, cinco limitações, comparação com
arquivos e SQLite, conclusão).

## Decisões

1. Nenhum ganho de desempenho ou produtividade é afirmado; tudo que depende de número fica
   "potencial" ou "não demonstrado" até o M4.
2. Hoje a recomendação para produção controlada e produção crítica é SQLite ou arquivos
   convencionais; o MoobFS é aceitável só em laboratório e VM descartável.
3. As capacidades que podem justificar adoção sem vencer o SQLite em todo benchmark são:
   autorização POSIX por resultado de consulta entre muitos uids sem camada de autorização na
   aplicação; dados legíveis por qualquer ferramenta Unix sem a camada; ausência de processo de
   banco por host. Qualquer uma delas exige requisito real do usuário, não preferência.
4. Achados de código registrados para o M3/M4: `QUERY` varre as entradas do primeiro termo e
   filtra os demais pela lista do inode (provável desvantagem frente a índice composto; a
   escolha do termo mais seletivo é candidata a melhoria, sem decisão agora); `index_used`
   subestima a RAM real (não conta arredondamento de slab, nós do xarray nem buffers
   transitórios); falha de orçamento ao registrar caminho é silenciosa; a escala S6 do plano
   pode não saturar 128 MiB com 3 a 8 atributos, logo a medição de saturação deve reduzir
   `index_budget` ou aumentar a escala; um só rwsem por mount serializa escritores de atributo;
   canal de tempo pela quantidade de candidatos privados não testado.
5. Critérios para sair de experimental: xfstests completo, ciclos de KASAN e syzkaller, ao
   menos duas séries de kernel, doze meses sem defeito de perda de dados, reprodução
   independente do M4, itens do roteiro de segurança, revisão externa.

## Limitações

Estimativas de memória não medidas; comparação com SQLite assume configuração adequada (WAL,
índices compostos, `ANALYZE`) sem execução; documento com 669 linhas, acima do alvo.

## Próximo passo

Resultado do KASAN na branch e PR para `main`; o M4 passa a ser o próximo marco, com as
medições indispensáveis listadas na entrega do documento.
