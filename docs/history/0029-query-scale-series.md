# 0029 — Série de escala do `QUERY` (1000 a 60 000 memórias)

Data: 2026-10-11. Estado: concluída (série em nível de piloto, não resultado do M4).

## Objetivo

Opção 1 decidida pelo mantenedor: medir o `QUERY` em escalas maiores com os discos atuais, já
que consultas rodam da memória e não dependem do bursting dos discos P6. Pergunta: como os custos
por resultado e por candidato se comportam com o tamanho do conjunto, e quanto o índice ocupa.

## Execução

Host da Azure (registros 0025, 0027, 0028), módulo e harness do commit `f4f98b0` (main com a
busca a partir do primeiro candidato e a revalidação RCU, mais o modo só consulta do `run.sh`).
Escalas 1000, 5000, 10 000, 25 000, 40 000 e 60 000 memórias, perfil 8, semente 1, três
repetições, W3 com 50 execuções por forma. Só o par ext4 (btrfs e XFS seguiram o mesmo padrão
nos pilotos). Evidências em `bench/results/2026-10-11-azure-query-scale/`; pacote bruto na
pre-release `bench-2026-10-11-azure-query-scale`.

Mudanças no harness para a série (branch `m4/query-scale`): modo `QUERY_ONLY=1` no `run.sh`;
`equiv.sh` registra resposta truncada pelo limite de 4096 resultados como `EQUIV_TRUNCATED`, sem
abortar, e continua comparando SQLite e oráculo; carga do SQLite numa passada de `find -exec
stat` e `awk` (a versão anterior criava quatro processos por memória; a primeira tentativa com
`find -printf` falhou na VM porque o `find` do busybox não o suporta).

## Resultados (mediana, ext4, como root)

| Memórias | Q3 SQLite | Q3 `QUERY` | Q4 SQLite | Q4 `QUERY` | Q5 SQLite | Q5 `QUERY` | Q8 SQLite | Q8 `QUERY` |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 1 000 | 15 µs | 34 µs | 26 µs | 67 µs | 43 µs | 126 µs | 19 µs | 5 µs |
| 5 000 | 24 µs | 457 µs | 56 µs | 500 µs | 281 µs | 676 µs | 23 µs | 6 µs |
| 10 000 | 33 µs | 1,10 ms | 96 µs | 1,14 ms | 541 µs | 1,50 ms | 25 µs | 8 µs |
| 25 000 | 100 µs | 2,83 ms | 394 µs | 2,90 ms | 1,92 ms | 3,69 ms | 53 µs | 8 µs |
| 40 000 | 205 µs | 4,64 ms | 838 µs | 4,68 ms | 3,40 ms | 6,11 ms | 64 µs | 7 µs |
| 60 000 | 392 µs | 6,95 ms | 1,45 ms | 6,99 ms | 5,18 ms | 9,15 ms | 67 µs | 7 µs |

Q3 é `namespace = X AND agent = Y` (253 resultados com 60 000), Q4 é `namespace = X AND kind = Y
AND created >= T` (1021), Q5 é `pinned = 1 AND score < 500` (2926), Q8 não casa nada porque
`tag1` não existe no perfil 8. As formas amplas (Q1, Q2, Q6, Q7, Q9) passam de 4096 resultados a
partir de 10 000 a 40 000 memórias e são truncadas pelo `QUERY`; deixam de ser comparáveis.

Índice: 600 bytes contabilizados por memória com 8 atributos em todas as escalas (36 MB com
60 000 memórias, 437 882 entradas). Pela contabilidade do próprio índice, o orçamento padrão de
128 MiB comportaria cerca de 220 000 memórias deste perfil; a contabilidade subestima a RAM real
(arredondamento do slab, nós do xarray), então o número real é menor. Reconstrução do índice na
remontagem (W7): 19 ms com 1000 memórias, 89 ms com 5000 e 192 ms com 10 000, cerca de 19 µs por
memória; nas escalas maiores o W7 ficou sem valor (ver limitações).

## Leitura

- O custo das consultas com mais de uma condição cresce com o tamanho do conjunto, não com o
  número de resultados: o `QUERY` busca pela primeira condição e testa as demais em cada
  candidato, a cerca de 0,93 µs por candidato (Q3 com 60 000 memórias: 7500 candidatos, 6,95 ms).
  Com índice composto, o SQLite cresce muito menos; na Q3 a diferença vai de 2 vezes com 1000
  memórias a 18 vezes com 60 000.
- Consulta sem casamento: o `QUERY` é de 4 a 9 vezes mais rápido que o SQLite em todas as
  escalas.
- Onde o planejador do SQLite escolhe varredura (Q5), a diferença fica menor (1,8 vez com 60 000).
- O limite de 4096 resultados da UAPI v1 é uma restrição real nesta escala: a maioria das formas
  amplas não cabe numa resposta, e não há paginação.

Esses resultados sustentam a proposta da ADR-0009 (`docs/decisions/0009-query-role-and-user-space-index.md`)
e a alternativa C' dela (interseção pequena no kernel), que ataca exatamente o crescimento por
candidato medido aqui.

## Limitações

- Série em nível de piloto: um host, um kernel, discos com bursting, três repetições.
- W7 usou como primeira consulta a forma mais ampla; a partir de 25 000 memórias a resposta foi
  truncada e o carregador registrou a remontagem como erro. Corrigido no `bench/load.c` (resposta
  truncada conta como índice pronto); o tempo de reconstrução nessas escalas precisa ser medido de
  novo.
- O SQLite prepara a instrução a cada chamada, o que o deixa um pouco mais lento do que uma
  aplicação com instruções preparadas.

## Próximo passo

Decisão do mantenedor sobre a ADR-0009; se a direção for aceita, medir no M4 as variantes que
ela lista, e medir de novo o W7 nas escalas maiores.
