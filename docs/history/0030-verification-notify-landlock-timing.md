# 0030 — Verificações: fluxo de mudanças, Landlock e canal de tempo do `QUERY`

Data: 2026-10-11. Estado: concluído (experimentos no host da Azure).

## Objetivo

Verificar na prática três pontos que a pesquisa (`docs/research-metadata-search.md`) deixou
como não verificados ou inferidos, todos relevantes para a ADR-0009: se um índice externo pode
ser alimentado por notificações do kernel para mudanças feitas pela camada; se processos
confinados pelo Landlock veem resultados que não podem abrir; e se o tempo de resposta do
`QUERY` revela entradas que o chamador não pode ver.

## Ferramentas

`tests/notify.c` (observador `fanotify` com marca de filesystem e `FAN_REPORT_DFID_NAME`, e
`inotify` por diretório) e `tests/landlock_run.c` (executa um comando com Landlock: listagem
permitida sob uma raiz, leitura de arquivos só sob uma pasta e a pasta do executável). Host da
Azure, kernel `6.12.112-moobfs-bench`, LSMs ativos
`lockdown,capability,landlock,yama,apparmor,ima,evm`, camada sobre ext4. Binários estáticos
em `/opt/moob-test` para poderem rodar como uid 1001.

## 1. Fluxo de mudanças

Operações pela camada: criar `f1`, gravar dois atributos, renomear para `f2`, criar pasta
`d1`, remover `f2`.

| Observador | Resultado |
| --- | --- |
| `fanotify`, marca de filesystem na camada | recusado: `Operation not supported` |
| `fanotify`, marca de filesystem no ext4 inferior | 7 eventos: CREATE f1, ATTRIB f1 (2), MOVED_FROM f1, MOVED_TO f2, CREATE d1, DELETE f2 |
| `inotify` na pasta da camada | os mesmos 7 |
| `inotify` na pasta correspondente do inferior | os mesmos 7 |

Conclusão: o fluxo de mudanças de que a ADR-0009 precisa já existe sem código novo no kernel,
observando o filesystem inferior com `fanotify` (uma marca, exige `CAP_SYS_ADMIN`) ou cada pasta
da camada com `inotify`. A camada recusa o `fanotify` com identificadores de arquivo porque não
implementa as operações de exportação (`encode_fh`); o overlayfs implementou essas operações para
permitir isso, e seria uma mudança contida se for necessária.

## 2. Landlock

Seis memórias com `kind = lt`, três em `lt/allowed` e três em `lt/denied`, legíveis por todos.
Como uid 1001:

- sem Landlock: `QUERY` devolve as 6, e todas abrem;
- com Landlock (listagem sob a raiz da montagem, leitura de arquivos só em `lt/allowed`): o
  `QUERY` devolve as mesmas 6; `open` de `lt/allowed/m1.md` funciona e `open` de
  `lt/denied/m1.md` falha com `EACCES` (código 3 do cliente);
- com o mesmo Landlock, `moobfs-attr get` (`getxattr`) do atributo `kind` de um arquivo negado
  funciona.

Conclusão: a lacuna existe (resultados que o processo não pode abrir), mas é estreita. O Landlock
(ABI 6) não controla leitura de atributos estendidos, e usar o `QUERY` exige poder listar a raiz
da montagem, o que no Landlock vale para toda a árvore abaixo dela. O `QUERY` revela, portanto,
nomes e valores de atributos que esse processo já obteria listando pastas e lendo atributos; o
conteúdo continua protegido. AppArmor, que pode negar subpastas por caminho, não foi testado.

## 3. Canal de tempo

Uid 1001 tem 10 memórias `kind = tc` numa pasta sua; uid 1002 tem N memórias `kind = tc` numa
pasta 0700. Uid 1001 executa `kind eq string tc` 400 vezes (`bench-load --uid 1001`).

| N (privadas do uid 1002) | Resultados vistos pelo uid 1001 | p50 | p10 | p90 |
| --- | --- | --- | --- | --- |
| 0 | 10 | 18,2 µs | 18,1 | 18,3 |
| 1 000 | 10 | 1,15 ms | 1,14 | 1,17 |
| 5 000 | 10 | 5,88 ms | 5,75 | 6,13 |
| 20 000 | 10 | 28,4 ms | 28,2 | 28,6 |

Conclusão: o tempo cresce cerca de 1,15 µs por candidato invisível, com dispersão abaixo de 3%.
Um agente estima quantas memórias de outros donos casam com um predicado e testa a existência de
um valor em qualquer lugar da montagem. A promessa da fase 3b de não revelar nada "nem em
contagens" vale para os resultados e contagens devolvidos, não para o tempo. Documentos
corrigidos: `docs/spec-m3.md` §3.1, `docs/faq.md`, `docs/architecture-overview.md`,
`docs/testing.md`; nota no S1 de `docs/security-roadmap.md`.

## Implicações

- Fluxo de mudanças para um índice externo: disponível hoje (`fanotify` no inferior ou
  `inotify` na camada); não justifica cursor próprio agora.
- O canal de tempo só se fecha estruturalmente quando a consulta não percorre entradas de outros
  domínios de permissão: índice particionado por domínio, no kernel ou fora dele (modelo GUFI da
  ADR-0009). Atenuações (reaproveitar a autorização por diretório, ADR-0008) reduzem o custo por
  candidato, mas não o eliminam.
- Preenchimento de tempo constante foi descartado como mitigação: custaria o pior caso em toda
  consulta.

## Limitações

Um host, ext4, uma execução de cada experimento; AppArmor não testado; o Landlock testado na ABI
6 do kernel 6.12.
