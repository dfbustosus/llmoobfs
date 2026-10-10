# Plano de benchmark do M4

Estado: proposta (2026-10-10). Nenhum número aqui foi medido. O M4 só existe para produzir
números; até lá toda afirmação de desempenho do projeto é hipótese.

## O que o benchmark deve responder

1. O `QUERY` da camada responde consultas por atributo mais rápido, ou mais barato, do que um
   índice equivalente mantido pela aplicação sobre um filesystem comum?
2. O que custa a camada: latência adicional por operação POSIX, tempo de montagem com a
   varredura do índice, memória do índice, e a partir de quantos inodes o orçamento padrão
   (128 MiB) deixa de bastar?
3. O compartilhamento físico delegado (reflink e dedup em btrfs e XFS) reduz espaço em cargas
   reais de memória de agentes, e quanto?
4. Um cliente real de memória de agentes roda inalterado sobre a camada com custo aceitável?

Busca semântica não entra nestas perguntas: embeddings, ranking e síntese ficam no cliente
(ADR-0005). Comparar `QUERY` com busca vetorial seria comparar operações diferentes.

## Sistemas comparados

Baselines de armazenamento, funcionalmente equivalentes ao que a camada faz:

| Rótulo | Configuração | Papel |
| --- | --- | --- |
| A | ext4, arquivos com xattr, índice de atributos em SQLite mantido pela aplicação | baseline sem camada, sem reflink |
| B | btrfs, idem | baseline sem camada, com reflink e dedup do próprio FS |
| C | MoobFS sobre ext4, btrfs e XFS, `QUERY` nativo | o objeto do experimento |

Clientes reais, usados como geradores de carga e como teste de compatibilidade, não como
concorrentes do `QUERY`:

| Cliente | Por que entra | Como entra |
| --- | --- | --- |
| [ai-memory](https://github.com/akitaonrails/ai-memory) | Fonte de verdade em arquivos com front matter; já tem harness de benchmark (LongMemEval) e teto de escrita medido (~700 observações/s); o mais próximo do nosso modelo | `data_dir` sobre A, B e C; mesma sessão capturada; comparar latência de escrita de páginas, `reindex`, espaço |
| [mem0](https://github.com/mem0ai/mem0) | Backend de armazenamento plugável | backend de arquivos sobre A, B e C |
| [Letta](https://github.com/letta-ai/letta) e [letta-code](https://github.com/letta-ai/letta-code) (memória em arquivos, integração com git) | Forma realista de memória de agente como arquivos versionados | diretório de memória sobre A, B e C; git intacto |
| [Graphiti](https://github.com/getzep/graphiti), [cognee](https://github.com/topoteretes/cognee) | Fatos com validade temporal e grafos; referência para o modelo de atributos do M3 | referência de modelo de dados; carga só se houver backend de arquivos |
| [Agent VFS](https://github.com/johannesmichalke/agent-vfs) | Filesystem virtual em espaço de usuário com persistência em banco | referência de interface; não é concorrente de módulo de kernel |
| [Microsoft GraphRAG](https://github.com/microsoft/graphrag) | Pipeline de conhecimento; produz entidades e relações | fonte de dataset de atributos (entidades, relações) para cargas de `QUERY`; não é armazenamento |

PostgreSQL com pgvector fica fora da comparação do `QUERY`. Entra, se entrar, apenas no
cenário "cliente semântico inteiro" sobre A, B e C, onde o banco é parte do cliente.

## Cargas

Cada carga é um programa determinístico com semente fixa, versionado no repositório, que
grava os mesmos bytes e os mesmos atributos em A, B e C, e cujos resultados de consulta são
validados contra o oráculo (`tools/moobfs-index`) antes de qualquer medição de tempo. Uma
consulta truncada não conta como completa.

| ID | Operação | O que mede |
| --- | --- | --- |
| W1 | criação: tmp + atributos + fsync + rename + fsync do diretório | latência de criação, amplificação de escrita |
| W2 | leitura por caminho conhecido | custo da passagem |
| W3 | consulta estruturada por 1 a 4 atributos (`QUERY` em C; SQL com índices equivalentes em A e B) | pergunta 1; base para decidir `QUERY_BATCH` |
| W4 | atualização e remoção de atributos | validação e índice |
| W5 | renomeação e remoção concorrentes com consultas | locks e consistência do índice |
| W6 | montagem com índice frio (caches de página descartados) | tempo de varredura |
| W7 | remontagem e reconstrução do índice com cache quente | idem, separando I/O de CPU |
| W8 | cargas mistas concorrentes (1, 10, 100 processos) | contenção |
| W9 | fsync, durabilidade e recuperação (`dm-log-writes`, marcas, replay) | integridade; sem métrica de tempo |

Cliente real: a mesma sessão do ai-memory capturada e reproduzida sobre A, B e C.

## Escalas e perfis

S1 1.000, S2 5.000, S3 10.000, S4 25.000, S5 40.000, S6 50.000 a 65.000 inodes, conforme a
capacidade observada; perfis de 3, 8 e 16 atributos tipados por inode, com cardinalidade,
tamanho e distribuição dos valores registrados. Esses números acompanham o orçamento atual do
índice (128 MiB) e não são limites definitivos.

Saturação do índice: usando `GET_INFO` (`index_budget`, `index_used`, `index_entries`,
`index_inodes`, flags), medir `U = index_used / index_budget` em 50%, 70%, 85%, 95% e 100%,
registrando inodes reais, atributos, RAM, tempo de montagem e de reconstrução, p99 de W3,
e o comportamento quando a montagem é recusada por orçamento ou o índice fica incompleto.
Nenhuma persistência de índice é implementada antes de esses números existirem.

## Métricas

Por operação: latência p50, p95 e p99 e vazão. Por sistema: CPU, memória residente (incluindo
`index_used` do `GET_INFO`), espaço em disco antes e depois de dedup, amplificação de escrita
(bytes gravados no dispositivo por byte útil, via `dm-log-writes` ou `/proc/diskstats`), tempo
de montagem, tempo até o índice responder, integridade após queda.

## Método

- Hardware, kernel, versões de ferramentas e imagens registrados em `environment.txt`.
- Dataset determinístico; aquecimento de 60 s; medição de 180 s; no mínimo 7 repetições;
  ordem dos baselines alternada entre repetições; resultados brutos preservados; mediana e
  dispersão (p5–p95 entre repetições) reportadas; dispersão acima de 20% marca o resultado
  como inconclusivo. Durações e repetições são ajustadas pela estabilidade observada.
- Latências separadas por tipo de operação; nunca uma distribuição única misturando
  leitura, escrita e consulta.
- Amplificação de escrita: `WA_logical = bytes gravados no dispositivo / bytes úteis
  alterados` e `WA_relative = bytes gravados com a camada / bytes gravados no baseline
  equivalente`, com journal, writeback, cache e flushes controlados e sem outros processos
  gravando; qualquer diferença entre C e seu baseline é investigada na origem, já que a
  persistência é delegada ao inferior.
- Montagem e reconstrução medidas em cache frio e quente, com as condições explícitas.
- VM dedicada com KVM; emulação (TCG) serve para correção, não para números.
- Nenhum número estimado é publicado como medido; cada tabela cita o commit, o comando e o
  arquivo de evidência.

## Ferramentas

- Harness próprio em C para as cargas (determinístico, sem dependências), reutilizando
  `tests/query.c` e `tools/moobfs-index`.
- `fio` apenas para calibrar o dispositivo da VM antes das cargas.
- `dm-log-writes` para amplificação de escrita e recuperação; `duperemove` para dedup.
- `perf` e `/proc/lock_stat` (kernel com `LOCK_STAT`) para localizar gargalos.
- O harness de benchmark do ai-memory (`evals/`) para o cenário de cliente real.

## Critério de saída do M4

Relatório em `docs/history/` com as quatro perguntas respondidas por números, incluindo os
casos em que a camada perde. Se a camada não mostrar vantagem mensurável em nenhuma das
quatro, isso é um resultado válido e muda o rumo do projeto, conforme ADR-0007.
