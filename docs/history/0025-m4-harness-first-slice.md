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
diretório comum, sem camada. W3 não foi exercitado (precisa da camada montada; entra na suíte
da VM na próxima fatia).

## Limitações

O bind mount do Docker Desktop no macOS não suporta xattr de usuário; o smoke deve usar alvo no
filesystem do contêiner ou rodar em Linux (o CI usa ext4). `load` não cobre W5, W8 e W9;
`sqlbase`, `equiv`, `mountclock` e `run.sh` não existem.

## Próximo passo

W3 na suíte da VM contra a camada com equivalência ao oráculo; `sqlbase` com `EXPLAIN QUERY
PLAN`; `equiv`; `mountclock`; então números em VM com KVM.
