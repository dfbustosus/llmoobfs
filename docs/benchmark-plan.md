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
grava os mesmos bytes e os mesmos atributos em A, B e C.

| Carga | Operações | O que mede |
| --- | --- | --- |
| criar | tmp + atributos + fsync + rename + fsync do diretório | latência de criação, amplificação de escrita |
| ler por caminho | open + read de memórias conhecidas | custo da passagem |
| consultar | `QUERY` em C; SQL equivalente em A e B; mesmos predicados, mesmos resultados obrigatórios | pergunta 1 |
| atualizar atributo | `setxattr` em memórias existentes | custo de validação e de índice |
| escrita concorrente | N processos criando em diretórios distintos e no mesmo diretório | locks e índice sob contenção |
| leitura concorrente | N processos consultando e lendo | lock de leitura do índice |
| compartilhar | `FICLONE` de memórias idênticas; `duperemove` | pergunta 3, só em B e C sobre btrfs/XFS |
| remover | `unlink` com e sem descritores abertos | custo de retirada do índice |
| remontar | `umount` + `mount`; medir até o primeiro `QUERY` responder | tempo de varredura e memória do índice |
| recuperar | `dm-log-writes` com marcas; replay; verificação | integridade, sem métrica de tempo |

Cliente real: a mesma sessão do ai-memory capturada e reproduzida sobre A, B e C.

## Escalas e concorrência

Escalas iniciais compatíveis com o orçamento atual do índice: 1 mil, 10 mil e 65 mil inodes
com 8 atributos tipados cada. Acima disso, medir o ponto em que a montagem é recusada por
orçamento e o custo de subi-lo (`index_budget=`); 1 milhão e 10 milhões são hipóteses a testar
só depois desses números, e podem simplesmente mostrar o limite da abordagem em memória.
Concorrência: 1, 10 e 100 processos; 1.000 só se 100 não saturar a VM.

## Métricas

Por operação: latência p50, p95 e p99 e vazão. Por sistema: CPU, memória residente (incluindo
`index_used` do `GET_INFO`), espaço em disco antes e depois de dedup, amplificação de escrita
(bytes gravados no dispositivo por byte útil, via `dm-log-writes` ou `/proc/diskstats`), tempo
de montagem, tempo até o índice responder, integridade após queda.

## Método

- Hardware, kernel, versões de ferramentas e imagens registrados em `environment.txt`.
- Aquecimento de uma execução descartada; no mínimo cinco repetições; relatar mediana e
  dispersão (intervalo p5–p95 entre repetições). Resultados com dispersão acima de 20% são
  reportados como inconclusivos.
- Mesmo dataset, gerado por semente, para A, B e C; validação de que os resultados das
  consultas são idênticos entre sistemas antes de medir tempo.
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
