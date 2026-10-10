# Harness de benchmark do M4 — especificação

Estado: proposta (2026-10-10). Complementa [benchmark-plan.md](benchmark-plan.md), que define
perguntas, baselines, cargas, escalas, métricas e método. Este documento define os programas,
formatos e testes que tornam o plano executável e reproduzível por terceiros. Nada aqui está
implementado.

## 1. Por que um harness próprio

Levantamento das ferramentas existentes (2026-10-10):

- `mdtest` (IOR) e `fs_mark` medem operações de metadados por segundo em árvores de arquivos;
  reportam vazão, não percentis de latência por chamada, e não exercitam xattr nem consultas.
- `filebench` tem linguagem de carga, mas está parado há anos e não cobre xattr.
- `fio` cobre o caminho de dados; serve para calibrar o dispositivo, não para metadados.
- `mdbench` (Tigris) e MDWorkbench reportam latência por operação, mas sem xattr e sem o
  conceito de consulta estruturada.

Conclusão: o harness é código nosso, em C sem dependências, reutilizando o que o repositório
já tem (`tests/query.c`, `tools/moobfs-index`, `tools/moobfs-attr`, `replay-log`). As
ferramentas acima entram só como calibração (`fio`) e como referência de formato de saída.

## 2. Componentes

| Programa | Função | Linguagem |
| --- | --- | --- |
| `bench/gen` | Gera o dataset determinístico a partir de uma semente: lista de memórias com caminho, tamanho, conteúdo pseudoaleatório e atributos tipados conforme o perfil (3, 8 ou 16 atributos) e a distribuição escolhida | C |
| `bench/load` | Executa W1 a W5 e W8 contra um alvo (diretório montado) com N processos, registrando a latência de cada chamada em um arquivo binário por processo | C |
| `bench/mountclock` | Executa W6 e W7: desmonta, descarta caches (`echo 3 > drop_caches`) quando pedido, monta, e mede até o primeiro `QUERY` ou a primeira consulta SQL responder | sh + C |
| `bench/sqlbase` | Mantém o índice de atributos dos baselines A e B em SQLite, com o mesmo esquema e os mesmos predicados; valida planos com `EXPLAIN QUERY PLAN` | C (sqlite3 amalgamation vendorizada ou lib do sistema) |
| `bench/equiv` | Confere que, para cada consulta, kernel (`QUERY`), SQLite e oráculo (`moobfs-index`) devolvem o mesmo conjunto de inodes; falha se divergirem ou se algum resultado veio truncado | sh |
| `bench/report` | Lê os arquivos brutos, calcula p50/p95/p99, vazão, mediana e dispersão entre repetições, e emite CSV e JSON; gráficos são gerados fora (gnuplot ou python), nunca requisitos do harness | Python sem dependências além da biblioteca padrão |
| `bench/run.sh` | Orquestra: prepara alvos A, B, C; aquece; mede; repete; alterna a ordem; coleta `environment.txt`, `/proc/diskstats`, `perf stat`, `GET_INFO`; preserva tudo em `build/bench/<data>/` | sh |

## 3. Dataset

`bench/gen` recebe `--seed`, `--inodes`, `--profile {3,8,16}`, `--depth`, `--size-dist` e
escreve um manifesto texto com uma linha por memória: caminho relativo, tamanho em bytes,
semente do conteúdo, e os atributos como `nome=tipo:valor`. Regras:

- Caminhos em dois níveis (`<ns>/<agente>/m<n>.md`) com 8 namespaces e 32 agentes por
  padrão, para que W3 tenha seletividade variada.
- Atributos fixos em todo perfil: `namespace` (string, 8 valores), `agent` (string, 32),
  `memory_id` (string UUID v7 textual, único), `kind` (string, 6 valores, Zipf), `score`
  (int64 uniforme 0..1000), `created` (time crescente com jitter). Perfis 8 e 16 acrescentam
  `pinned` (bool, 10% verdadeiro), `weight` (double normal), `tag1..tagN` (string, 50 valores,
  Zipf) e `raw` (raw de 256 bytes, só no perfil 16, nunca indexado).
- Tamanhos de conteúdo log-normais com mediana 2 KiB e cauda até 64 KiB; conteúdo
  pseudoaleatório a partir da semente da linha, com 15% das memórias duplicadas byte a byte
  de outra, para W-compartilhar.
- O manifesto é a única fonte: `load` o lê e o reproduz em qualquer alvo; o mesmo manifesto
  alimenta `sqlbase`.
- Cardinalidade, tamanho médio e distribuição de cada atributo são impressos no cabeçalho do
  manifesto e copiados para o relatório.

## 4. Baselines A e B: esquema SQLite e equivalência

Tabela `memories(ino INTEGER PRIMARY KEY, path TEXT UNIQUE, namespace TEXT, agent TEXT,
memory_id TEXT UNIQUE, kind TEXT, score INTEGER, created INTEGER, pinned INTEGER, weight REAL,
tag1 TEXT, …)`, uma coluna por atributo indexável, tipos afins aos tipos do MoobFS. Índices
compostos cobrindo as conjunções de W3, com a coluna de igualdade antes da de intervalo:
`(kind, score)`, `(namespace, agent)`, `(namespace, kind, created)`, `(pinned, score)`, e um
índice simples por atributo; `ANALYZE` após a carga. Gate obrigatório: `EXPLAIN QUERY PLAN`
de cada consulta de W3 deve mostrar `SEARCH` usando um índice; um `SCAN` invalida a corrida e
exige novo índice, porque um baseline sem índice adequado não é equivalente. O arquivo do
banco vive no mesmo filesystem dos arquivos (ext4 em A, btrfs em B), em WAL, com
`synchronous=NORMAL`, e o escritor atualiza índice e arquivo na mesma sequência que o cliente
faria (arquivo primeiro, depois a linha), para que W1 pague o custo real de manter o índice.

Equivalência: `bench/equiv` executa as consultas de W3 nos três sistemas e compara os
conjuntos de inodes depois de mapear caminhos; qualquer diferença aborta a medição. O
oráculo é o árbitro quando kernel e SQLite divergem.

## 5. Coleta de latência

`load` registra por chamada: carga, operação, início e duração em nanossegundos
(`clock_gettime(CLOCK_MONOTONIC)`), processo, e resultado (bytes ou número de resultados).
Formato binário fixo de 32 bytes por registro, um arquivo por processo, convertido por
`report`. Sem agregação durante a medição, para não deformar a distribuição. Chamadas que
falham entram como erro e contam na taxa de erros, nunca nas latências.

## 6. Métricas de sistema

Por repetição: `perf stat -e task-clock,context-switches` do conjunto de processos;
`/proc/meminfo` e RSS dos processos antes e depois; `/proc/diskstats` do dispositivo
inferior (setores escritos) para a amplificação de escrita, com `sync` antes de ler;
`GET_INFO` (`index_used`, `index_entries`, `index_inodes`, flags) em C; `df` do inferior
antes e depois de W-compartilhar; `environment.txt` com kernel, CPU, memória, versões.

## 7. Estatística e relatório

Mediana e intervalo p5–p95 entre repetições para cada métrica; p50/p95/p99 por operação
dentro de cada repetição, reportados como a mediana entre repetições; dispersão relativa
acima de 20% marca "inconclusivo". `report` emite `results.csv` (uma linha por sistema,
carga, operação, métrica) e `results.json` com os metadados. Nenhum número é escrito em
documento do projeto sem o caminho do `results.csv` que o originou.

## 8. Execução

Ambiente alvo: VM Linux com KVM, 4 vCPU, 4 GiB, três discos virtio dedicados (um por
baseline), kernel Debian 6.12 ou o kernel do workflow KASAN sem instrumentação. O harness
Debian atual (TCG) roda só a escala S1 como teste de correção do próprio harness, não para
números. `make bench-smoke` faz isso no CI; `make bench` exige KVM e produz números.

## 9. Testes do próprio harness

- `gen`: mesma semente produz manifesto idêntico (hash); perfis têm o número certo de
  atributos; `memory_id` único; distribuições dentro de tolerância em S1.
- `load`: em um diretório tmpfs, W1 cria exatamente o manifesto e `moobfs-check` valida;
  arquivo de latências tem um registro por chamada; chamadas com erro injetado
  (`dm-flakey`) aparecem na taxa de erros e não nas latências.
- `sqlbase`: esquema criado; `EXPLAIN QUERY PLAN` de todas as consultas de W3 retorna
  `SEARCH`; equivalência com o oráculo em S1 nos três sistemas.
- `report`: percentis conferidos contra um conjunto conhecido; dispersão calculada;
  "inconclusivo" quando a tolerância é ultrapassada.
- `run.sh`: execução completa em S1 com 2 repetições dentro da VM TCG, verificando que todos
  os artefatos existem e que `equiv` passou.

## 10. Entregas

Código em `bench/`, alvo `make bench-smoke` no CI, `docs/testing.md` com a seção do
benchmark, e um primeiro relatório em `docs/history/` com S1 a S3 nos três perfis sobre ext4
e btrfs, inclusive os casos em que a camada perde.
