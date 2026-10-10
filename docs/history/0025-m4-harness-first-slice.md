# 0025 — M4: primeira fatia do harness de benchmark

Data: 2026-10-10. Branch: `m4/bench-harness`. Estado: em andamento (harness; nenhum número).

## Objetivo

Começar o M4 pelo harness em modo de correção, conforme `docs/benchmark-harness.md`, antes de
qualquer medição: gerador determinístico, carregador com latência por chamada e relatório.

## Mudanças

- `bench/common.h`: PRNG splitmix64, formato do manifesto, registro de latência de 32 bytes.
- `bench/gen.c`: manifesto determinístico (semente, inodes, perfil 6/8/16, namespaces, agentes);
  caminhos `ns<i>/a<j>/m<n>.md`, `memory_id` textual único, `kind` Zipf, `score` uniforme,
  `created` crescente com jitter, tamanhos log-normais com mediana 2 KiB, 15% de duplicatas byte
  a byte (mesma semente e mesmo tamanho da origem), perfis 8 e 16 com `pinned`, `weight`,
  `tag1..tag7` e `raw`.
- `bench/load.c` (Linux): W1 (tmp, atributos tipados, fsync, rename, fsync do diretório), W2
  (leitura por caminho), W3 (`MOOB_IOC_QUERY` por linha de um arquivo de consultas, `--root`),
  W4 (atualização de `score`, set e remoção de `scratch`); N processos por módulo do índice da
  linha; um arquivo `.rec` por processo; erros gravados como `-errno`, nunca como latência.
- `bench/report.py`: p50/p95/p99 por sistema, carga e operação; vazão; mediana e p5–p95 entre
  repetições; "inconclusivo" acima de 20% de dispersão; `results.csv` e `results.json`.
- `bench/smoke.sh` e alvos `bench-tools`/`bench-smoke` no `Makefile`; passo no CI.

## Decisões

1. Perfis são 6, 8 e 16 atributos (a especificação dizia 3 para o menor, mas fixa seis
   atributos em todo perfil; 6 é a leitura consistente). Anotado em `benchmark-harness.md`.
2. Duplicatas herdam tamanho e semente de conteúdo da origem, para que W-compartilhar meça
   dedup real.
3. O `load` não calcula nada durante a medição; só grava registros.

## Evidências

Execução em contêiner Linux (`gcc:13`, Docker Desktop) com alvo em filesystem do contêiner:
`BENCH_SMOKE_PASS` com `gen` determinístico (mesma semente, bytes iguais; semente diferente,
bytes diferentes; 300 `memory_id` únicos; 16/8/6 atributos por perfil), W1 com 3 processos
criando 300 arquivos sem `.tmp` sobrando e `moobfs-check` limpo com 4800 atributos, W2 e W4,
1200 registros, erros reportados (`n=0 err=300` contra árvore ausente), relatório com duas
repetições. Os tempos impressos pelo smoke não são medidas do projeto: ambiente emulado,
diretório comum, sem camada. W3 foi exercitado em seguida na suíte da VM (abaixo).

Bloco `bench` em `vm/suite.sh` (`make vm-test`, kernel Debian 6.12.111, `build/vm-test-bench.log`):
sobre a camada montada em ext4, btrfs e XFS, W1 com 2 processos criou 300 memórias do perfil 16
(`moobfs-check` no inferior: 4800 atributos, sem problemas), W2 leu todas, cada uma das nove
formas de W3 (`bench/queries-w3.txt`, Q1–Q9) devolveu pelo `QUERY` exatamente o conjunto do
oráculo `moobfs-index`, W3 e W4 gravaram um registro por chamada (1209 registros no total);
`BENCH_LAYER_PASS` nos três, `MOOBFS_LAYER_PASS` e `MOOBFS_ACCEPTANCE_PASS` inalterados. Os
binários do harness são compilados estáticos e preparados em `vm/in-container.sh` e no workflow
KASAN. Uma primeira execução falhou em `attr-set-ext4` por binários dinâmicos reaproveitados de
um `build/linux` deixado por um contêiner diferente; `rm -rf build/linux` resolveu (anotado em
`docs/development.md` como cuidado ao alternar contêineres).

## Fatia SQLite: `sqlbase` e `equiv` (2026-10-10)

`bench/sqlbase.sh` cria o esquema e os índices compostos do §4 do harness, carrega o manifesto
com o inode real de cada arquivo (`stat`), gera o SQL de cada forma Q1–Q9 (prefixo como
intervalo binário, existência como `IS NOT NULL`, `raw` sem coluna) e expõe `EXPLAIN QUERY
PLAN`. `bench/equiv.sh` compara, por forma, os conjuntos de inodes do `QUERY`, do SQLite e do
oráculo e falha em qualquer diferença ou truncamento. Ambos em shell sobre a CLI `sqlite3`, para
rodarem no initramfs (sem Python); decisão registrada no harness.

Evidência: `make bench-smoke` em contêiner Linux (`BENCH_SMOKE_PASS`, SQLite igual ao oráculo
nas 9 formas) e `make vm-test` (`build/vm-test-bench.log`): `BENCH_LAYER_PASS` em ext4, btrfs e
XFS com `EQUIV_PASS: 9 shapes (kernel, SQLite, oracle)` dentro do bloco; `MOOBFS_LAYER_PASS`.

Achado: com 300 linhas o SQLite planeja `SCAN` para Q1, Q5 e Q9 por baixa seletividade, apesar
dos índices e do `ANALYZE`. Decisão: o portão registra `SCAN` como exceção a justificar na
medição em vez de falhar, e Q9 (existência de atributo presente em 100% das linhas) é marcada
para reavaliação na especificação antes de medir. Fecha as issues 10 e 11 ao mesclar.

## W6 e W7: remontagem com índice frio e quente (2026-10-10)

`bench/load --workload W6|W7 --target MOUNT --lower DIR --queries FILE --repeat N` (root):
desmonta, para W6 faz `sync` e escreve 3 em `/proc/sys/vm/drop_caches`, monta `moobfs_layer`
e emite a primeira consulta do arquivo; o registro cobre `mount()` (que reconstrói o índice) e
esse primeiro `QUERY`. Na suíte da VM, duas repetições de cada sobre o dataset de 300 memórias,
com a camada remontada e `GET_INFO` sem flags ao final. `make vm-test`: `BENCH_LAYER_PASS` em
ext4, btrfs e XFS; a equivalência tríplice registrou um plano `SCAN` a justificar por inferior
com a semente 11 (contra três com a semente 7 no contêiner: a escolha do planejador depende da
distribuição gerada). Tempos não são medidas: emulação TCG.

## W8: carga mista concorrente (2026-10-10)

`bench/load --workload W8 --procs N [--queries FILE --root MOUNT]`: cada processo percorre sua
fatia do manifesto e, com PRNG semeado pelo número do processo, escolhe leitura (70%),
atualização de atributo (20%) ou um `QUERY` sorteado do arquivo de consultas (10%, só com
`--queries` e `--root`). Suíte da VM: 10 processos sobre a camada nos três inferiores, todas as
chamadas registradas, nenhuma falha; `make bench-smoke` cobre W8 sem raiz de montagem.
`BENCH_LAYER_PASS` em ext4, btrfs e XFS; `MOOBFS_LAYER_PASS`.

## Limitações

O bind mount do Docker Desktop no macOS não suporta xattr de usuário; o smoke deve usar alvo no
filesystem do contêiner ou rodar em Linux (o CI usa ext4). `load` não cobre W5 e W9; o relatório não roda na VM (sem Python no initramfs);
`run.sh` e a medição de latência do caminho SQLite (em C) não existem; `mountclock` virou os workloads W6/W7 do `load`.

## Próximo passo

W5 e W9 em `load` (W9 reutiliza o replay `dm-log-writes` da suíte); medição do baseline SQLite em C; `run.sh`; reavaliar Q9; então números
em VM com KVM.
