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

## W5, W9 e o orquestrador `run.sh` (2026-10-10)

- W5 em `load`: processos 1..N-1 renomeiam cada memória da sua fatia (ida e volta) e depois a
  removem e recriam com os mesmos bytes e atributos, enquanto o processo 0 percorre o arquivo
  de consultas até os mutadores terminarem; registros `RENAME`, `RECREATE` e `QUERY`. Na suíte:
  4 processos sobre a camada; ao fim, 300 arquivos, nenhum `.away`, `moobfs-check` limpo e as
  nove formas de W3 iguais ao oráculo, nos três inferiores.
- W9 (durabilidade e recuperação) não ganha código em `load`: é o bloco de replay
  `dm-log-writes` e `dm-flakey` que a suíte já executa (`POWER_LOSS_REPLAY_PASS`,
  `IO_ERROR_INJECTION_PASS`); o relatório do M4 cita esse bloco como W9.
- `bench/run.sh`: orquestra uma campanha (dataset, `environment.txt`, por repetição e sistema:
  W1, `diskstats` antes e depois, `GET_INFO`, `moobfs-check`, baseline SQLite, equivalência,
  W2, W3, W4, W8, W5, W6 e W7 quando root; ordem alternada entre repetições; relatório ao fim
  se houver Python). Sistemas: `--plain NAME=DIR` (A e B) e `--layer NAME=MOUNT:LOWER` (C).
  Executado em modo de fumaça na suíte (60 memórias, uma repetição, só C) com os artefatos
  conferidos. Um primeiro defeito do script (remontar a camada com `lower=<raiz>/bench` em vez
  da raiz inferior, que quebrava o teste seguinte de fsx) foi corrigido antes do commit.
- A medição de latência do caminho SQLite (W3 nos baselines A e B) continua ausente; `run.sh`
  pula W3 nesses sistemas e o relatório deve dizer isso até existir o cliente em C.

## Cliente SQLite em C (2026-10-10)

`bench/sqlclient.c`: executa, pela biblioteca `libsqlite3` (não pela shell), uma instrução
preparada por linha de um arquivo SQL gerado por `sqlbase.sh sql` para as mesmas formas Q1–Q9,
e grava um registro por instrução no mesmo formato do `QUERY` (carga 3, operação `QUERY`,
resultado = linhas). `run.sh` usa o cliente nos sistemas `--plain` (A e B) e o `QUERY` nos
sistemas `--layer` (C), com o mesmo `queries.sql`/`queries.txt`. A imagem da VM e o workflow
KASAN ganharam `libsqlite3-dev`; os binários são estáticos (`-lsqlite3 -ldl -lpthread`).
Evidência: `make bench-smoke` em contêiner (18 registros do cliente em duas repetições,
`sqlite W3 query: n=18 err=0` no relatório) e `make vm-test` com a imagem reconstruída:
`BENCH_LAYER_PASS` em ext4, btrfs e XFS, `MOOBFS_LAYER_PASS`. Com isto o harness cobre tudo o
que `docs/benchmark-harness.md` pede; o que falta é hardware acelerado para medir.

## Q9 reavaliada e roteiro de convidado acelerado (2026-10-10)

- Q9 (`weight exists`): `bench/gen` passou a emitir `weight` em 30% das memórias (perfis 8 e
  16), tornando a consulta de existência seletiva; checagens de contagem de atributos no smoke
  e na suíte passaram a derivar o total do manifesto. Com 300 linhas o planejador do SQLite
  ainda escolhe `SCAN` para Q9 (85 de 300 linhas), registrado como exceção.
- Defeito do próprio harness achado e corrigido: o smoke carregava o baseline SQLite depois de
  W4 ter somado 1 a cada `score` nos arquivos, de modo que um arquivo no limite de Q7 entrava
  no oráculo e não no SQLite. O baseline passou a ser carregado logo após W1 em todas as
  execuções (o `run.sh` já fazia isso). A igualdade de conjuntos pegou o erro: é a função dela.
- `docs/bench-accelerated-guest.md`: roteiro para medir com aceleração de hardware, nos dois
  caminhos (host Linux x86 com KVM para o relatório; Mac Apple Silicon com Hypervisor.framework
  e Debian arm64 como campanha preliminar, fora da matriz testada), com validação da suíte
  antes de medir, calibração de disco com `fio`, invocação do `run.sh` e o que registrar.
  Proposta não executada.

## Limitações

O bind mount do Docker Desktop no macOS não suporta xattr de usuário; o smoke deve usar alvo no
filesystem do contêiner ou rodar em Linux (o CI usa ext4). `load` não cobre W5 e W9; o relatório não roda na VM (sem Python no initramfs);
`mountclock` virou os workloads W6/W7 do `load`.

## Próximo passo

Reavaliar Q9; roteiro para um convidado Linux com aceleração (x86 com KVM, ou arm64 com
Hypervisor.framework no Mac como campanha preliminar); então números e o relatório do M4.

## Preparação de host de benchmark (2026-10-11)

Decisão do mantenedor: a primeira campanha roda numa VM de nuvem x86_64 (Azure, D4as_v5 ou
similar, Ubuntu 24.04, tipo de segurança Standard) usada como host, com o módulo no kernel da
própria instância, em vez de uma VM aninhada. Adicionados `vm/bench.config` (fragmento sem
sanitizadores nem lockdep sobre o config da distribuição), `bench/kernel-build.sh` (mesmo
tarball 6.12.112 com o mesmo SHA-256 do workflow KASAN, `bindeb-pkg`, instalação) e
`bench/host-setup.sh` (formata três discos dedicados, compila módulo e ferramentas, roda a
suíte inteira no host como portão, `HOST_SETUP_PASS`). Caminho C documentado em
`docs/bench-accelerated-guest.md`. Nenhum dos três foi executado ainda: a primeira execução
será na instância, e o resultado entra aqui ou em registro próprio.

Primeira execução de `bench/kernel-build.sh` na VM da Azure (Standard_D4as_v4, AMD EPYC 7763,
Ubuntu 24.04 com kernel 6.17.0-1022-azure, 2026-10-11): compilação e instalação concluídas a
partir do config do kernel Azure. Três ajustes vieram dela: o sufixo de versão saía duplicado
(`-moobfs-bench-moobfs-bench`, porque o fragmento e o `make` definiam `LOCALVERSION`), o
`dpkg -i` incluía o pacote `-dbg`, que falha sem informação de depuração, e o GRUB mantém o
kernel mais novo como padrão, então o 6.12 precisa de `grub-reboot` (um boot só, com volta
automática ao 6.17 se falhar). Corrigidos o sufixo e o pacote; o `grub-reboot` está no roteiro.
A instância usa Trusted launch com Secure Boot ligado, que precisa ser desligado para o kernel
não assinado. O kernel já instalado na instância mantém o sufixo duplicado; não é recompilado.

## Portão no host da Azure: `HOST_SETUP_PASS` (2026-10-11)

VM Standard_D4as_v4 (4 vCPU AMD EPYC 7763, 16 GB), Ubuntu 24.04, kernel
`6.12.112-moobfs-bench-moobfs-bench` compilado por `bench/kernel-build.sh`, Secure Boot
desligado, três discos de dados Premium SSD P6 de 64 GB com cache "Nenhum", endereçados por
`/dev/disk/azure/scsi1/lun0..2` (os nomes `sdX` mudaram entre boots e não são usados).
`sh bench/host-setup.sh --ext4 .../lun0 --btrfs .../lun1 --xfs .../lun2` terminou com
`HOST_SETUP_PASS`: `ISOLATION_PASS`, `BENCH_LAYER_PASS` com `EQUIV_PASS` nos três inferiores,
`LAYER_LOWER_PASS`, `SQLITE_VARIANTS` sem perdas, `POWER_LOSS_REPLAY_PASS`,
`IO_ERROR_INJECTION_PASS`, `MOOBFS_ACCEPTANCE_PASS`, `MOOBFS_LAYER_PASS`, sem diagnóstico no
`dmesg`. É o segundo ambiente em que a suíte inteira passa e o primeiro com aceleração de
hardware. O teste de concorrência fez 12936 consultas em 5 s, contra cerca de 620 na VM
emulada; é contagem de um teste de correção, não medida, e só ilustra por que tempos sob TCG
não servem. O kernel ficou como padrão do GRUB (`grub-set-default`).

Calibração inicial no host da Azure (P6 com bursting, 2026-10-11), escrita de 4 KiB seguida de
`fsync`, 30 s: ext4 313/s, btrfs 229/s, xfs 418/s (de 2,4 a 4,4 ms por operação). O comando
usado imprimiu a latência de conclusão da escrita (cerca de 10 µs), que com `--fsync=1` não
inclui o `fsync`; `bench/calibrate.sh` passa a extrair a latência do `fsync` do JSON do `fio`.
Decisão: a campanha que alimenta o relatório usa discos Premium SSD v2 com IOPS e vazão fixos,
porque o bursting por créditos dos P1–P20 mudaria a velocidade do dispositivo ao longo de horas;
os P6 atuais valem para o portão e o piloto.

## Piloto no host da Azure e correções do harness (2026-10-11)

Piloto (`run.sh --scale 1000 --profile 8 --reps 3 --procs 4`, seis sistemas: A-ext4,
B-btrfs, X-xfs sem camada e C-ext4, C-btrfs, C-xfs com a camada, cada par no mesmo disco P6
com bursting). Rodou inteiro, sem erros, com equivalência em todas as formas. Serve para validar
o procedimento, não para concluir. Sinais a confirmar na campanha: W1 dominado pelo `fsync` do
disco e praticamente igual com e sem camada; W2 e W4 com acréscimo de alguns microssegundos por
operação na camada; W3 com o `QUERY` cerca de oito vezes mais lento que o SQLite com índice
composto na mediana. Os artefatos estão no host (`build/bench/pilot-s1/`) e ainda não foram
copiados para o repositório; os números acima não são resultado do M4.

O piloto expôs três defeitos de método no W3, corrigidos antes da campanha: cada forma rodava
uma vez por repetição (agora `--repeat`, padrão 50 em `run.sh`); os registros não separavam as
formas (o código de operação agora carrega o número da forma, Q1–Q9, e o relatório mostra uma
linha por forma com a mediana de resultados devolvidos); e a comparação não era equivalente
(o SQLite passa a devolver `ino` e `path`, como o `QUERY`, e o `QUERY` também é medido como
usuário comum, `--uid 65534`, registrado como sistema `<nome>+u`, para separar o custo da
autorização por resultado, como o plano pede). A forma Q8 deixa de ser removida nos perfis sem
`tag1`, para que a numeração fique estável; nesses perfis ela não casa nada em nenhum sistema.
Validado com `make bench-smoke` em contêiner e `make vm-test` (`BENCH_LAYER_PASS` nos três).

## Piloto 2 com o harness corrigido (2026-10-11)

Execução remota pela chave dedicada `moobfs-bench-claude` (revogável no `authorized_keys` do
host). Calibração com `bench/calibrate.sh`: `fsync` p50/p99 de 2,8/7,8 ms no ext4, 3,6/12,3 ms
no btrfs e 2,1/6,8 ms no XFS (P6 com bursting). Piloto: 1000 memórias, perfil 8, 5 repetições,
W3 com 50 execuções por forma, seis sistemas mais os três `+u`. Evidências em
`bench/results/2026-10-11-azure-pilot2-s1/`; pacote bruto na pre-release
`bench-2026-10-11-azure-pilot2-s1`. As 30 equivalências passaram.

Medianas no par ext4 (sem camada / com camada): W1 9,90 / 9,82 ms; W2 9,3 / 12,9 µs; W4 set
10,0 / 24,2 µs; W4 remoção 16,0 / 43,6 µs; W6 e W7 (só camada) 35 e 17 ms para 1000 memórias.

W3 por forma (mediana, ext4; o padrão é igual em btrfs e XFS):

| Forma | Resultados | SQLite | `QUERY` root | `QUERY` usuário comum |
| --- | --- | --- | --- | --- |
| Q8 (nenhum casamento) | 0 | 19 µs | 54 µs | 54 µs |
| Q3 `namespace = AND agent =` | 3 | 16 µs | 141 µs | 142 µs |
| Q4 | 23 | 26 µs | 178 µs | 180 µs |
| Q5 | 70 | 43 µs | 248 µs | 254 µs |
| Q2 | 100 | 56 µs | 323 µs | 328 µs |
| Q6 `kind prefix` | 212 | 99 µs | 502 µs | 517 µs |
| Q9 `weight exists` | 291 | 78 µs | 608 µs | 630 µs |
| Q1 `kind =` | 390 | 91 µs | 843 µs | 874 µs |
| Q7 `score` entre 250 e 750 | 513 | 225 µs | 1136 µs | 1177 µs |

Leitura do código (`layer/index.c`, `do_query`) que explica a diferença:

1. Custo fixo de cerca de 50 µs por chamada, visível em Q8: `kvcalloc(q->limit, ...)` aloca e
   zera um vetor de resultados do tamanho do limite pedido (4096 × 280 bytes, cerca de 1,1 MB)
   em toda chamada, mesmo quando nenhum resultado volta.
2. Varredura linear do primeiro termo: sem chave de retomada, `first_after` começa em
   `rb_first` e percorre todas as entradas do atributo, mesmo em igualdade, intervalo ou
   prefixo, em que a árvore permitiria saltar ao primeiro valor candidato e parar depois do
   último. Em Q3 isso custa cerca de 85 µs para 1000 entradas.
3. Cerca de 2 µs por resultado devolvido, contra cerca de 0,4 µs no SQLite: para cada resultado
   a camada copia o caminho, resolve-o com `vfs_path_lookup` sob as credenciais do chamador e
   verifica a permissão (fase 3b). Como usuário comum o custo sobe só 3%: o peso está na
   resolução do caminho, não na decisão de permissão.

Decisão: 1 e 2 são ineficiências de implementação e serão corrigidas antes da campanha, com os
mesmos testes de equivalência, isolamento e KASAN; 3 é consequência do desenho de autorização
por resultado e fica como custo declarado, a ser medido de novo depois das correções. O SQLite
do baseline prepara a instrução a cada chamada, o que o torna um pouco mais lento do que uma
aplicação que reutiliza instruções preparadas; a comparação, portanto, não favorece o SQLite.
Os pilotos 1 e 2 ficam como registro do estado anterior às correções.

Nota de 2026-10-11: a primeira tentativa do piloto 4 (com a revalidação RCU do registro 0028)
abortou na equivalência do sistema C-ext4: o `QUERY` devolveu 291 resultados a mais na forma
Q9 do que o oráculo. A causa foi um conjunto de 1000 memórias deixado em
`/srv/moob/layer-ext4/prof` pelo perfil do registro 0028; o `QUERY` responde sobre a montagem
inteira, enquanto o oráculo e o SQLite olham só `bench/`. Nenhum número foi produzido. A pasta
foi removida e o piloto refeito; `run.sh` passou a recusar começar se a raiz de uma camada
contiver algo além de `bench/`.
