# Ecossistema de software livre relevante ao MoobFS

Data do levantamento: 2026-10-10. Objetivo: antes de implementar algo, verificar se já existe
solução livre que resolva o problema ou sirva de referência. Organizado por problema do projeto.
Estrelas, licença e data do último push foram lidos da API do GitHub em 2026-10-10; são
indicadores de atividade, não de qualidade. Nada aqui foi executado ou integrado ainda.

## 1. Testar o módulo em kernel com KASAN e lockdep (M1 hardening)

| Projeto | Licença | Atividade | Uso para o MoobFS |
| --- | --- | --- | --- |
| [virtme-ng](https://github.com/arighi/virtme-ng) | GPL-2.0 | ativo (2026-10) | Inicia um kernel recém-compilado em QEMU usando o sistema do host como raiz copy-on-write. Substitui parte do nosso harness em `vm/` para iterações rápidas com kernel próprio compilado com `CONFIG_KASAN` e `CONFIG_PROVE_LOCKING`. |
| [syzkaller](https://github.com/google/syzkaller) | Apache-2.0 | ativo | Fuzzer de syscalls com modo `syz-mount-image` para imagens de filesystem. Pesado, mas é a referência para fuzzing contínuo. |
| [LTP](https://github.com/linux-test-project/ltp) | GPL-2.0 | ativo | Testes de syscalls POSIX sobre um mount; útil quando houver escrita. |

O harness atual (`vm/`, kernel Debian pré-compilado) continua válido para reprodutibilidade;
virtme-ng é complementar, para o kernel instrumentado que o Debian não entrega.

## 2. Fuzzing do parser de imagem

| Projeto | Licença | Atividade | Uso |
| --- | --- | --- | --- |
| [Janus](https://github.com/sslab-gatech/janus) | MIT | parado desde 2019 | Fuzzing conjunto de imagem + syscalls sobre Linux Kernel Library. Referência de método; código antigo. |
| [Hydra](https://github.com/sslab-gatech/hydra) | MIT | parado desde 2022 | Sucessor do Janus. Mesma ressalva. |
| libFuzzer/AFL++ sobre `common/` | — | — | O parser do M1 é compartilhado entre ferramentas e módulo e compila no host. Fuzzing em espaço de usuário desse código é o caminho mais barato e imediato. |

Decisão sugerida: fuzzing em espaço de usuário do parser compartilhado primeiro; syzkaller
com `syz-mount-image` depois; Janus/Hydra apenas como leitura.

## 3. Consistência após queda de energia (M2)

| Projeto | Licença | Atividade | Uso |
| --- | --- | --- | --- |
| `dm-log-writes` (kernel ≥ 4.1) | GPL-2.0 | in-tree | Grava e reproduz escritas de bloco com marcas de FUA; permite restaurar o volume a qualquer ponto e verificar recuperação. É o mecanismo padrão do kernel. |
| [xfstests](https://github.com/kdave/xfstests) (`generic/45x`, `generic/482`) | GPL-2.0 | ativo | Suíte de regressão usada por todos os filesystems in-tree. Os testes de crash usam `dm-log-writes`. Ser executável sob xfstests é um critério de maturidade razoável para o M2. |
| [CrashMonkey/ACE](https://github.com/utsaslab/crashmonkey) | Apache-2.0 | parado desde 2022 | Geração sistemática de cargas e verificação de crash-consistency (OSDI 2018). Método relevante; código precisa de adaptação a kernels atuais. |
| `dm-flakey` | GPL-2.0 | in-tree | Injeção de falhas de I/O. |

Decisão sugerida: `dm-log-writes` + testes próprios no estilo xfstests. Não escrever um
mecanismo de replay próprio.

## 4. Estruturas em disco e em memória no kernel

| Recurso | Uso |
| --- | --- |
| `lib/btree.c` | B+ tree genérica em memória do kernel. Não serve para disco, mas evita escrever índice em memória. |
| `lib/maple_tree.c`, `lib/xarray.c` | Mapas ordenados em memória; alternativas modernas para cache de índice. |
| `fs/erofs` | Filesystem somente leitura, in-tree, baseado em iomap. Referência mais próxima do que o M1 faz hoje; vale comparar o caminho de leitura e a validação de imagem. |
| `fs/befs/btree.c` | Leitura de B+ tree em disco do BFS (GPL-2.0, somente leitura, sem atributos/índices expostos). Referência de layout. |
| `fs/nilfs2` | Log-structured com snapshots contínuos. Referência para M3. |
| `fs/btrfs`, `fs/bcachefs` | Snapshots copy-on-write. Referências de desenho, grandes demais para reuso direto. |
| `lib/crc32c`, `crypto/sha256` | Já usados; manter as APIs do kernel em vez de cópias próprias onde possível. |

## 5. O Be File System em espaço de usuário

[Haiku](https://github.com/haiku/haiku) (MIT em sua maior parte) entrega `bfs_shell`, construído
sobre `fs_shell`: o código real do BFS rodando como programa de linha de comando em Linux, com
`query`, `mkindex`, `ls`, `cp`, `sync`. Isso permite, sem kernel Haiku:

- Observar o comportamento de atributos, índices e consultas do BFS como oráculo ao especificar
  o `QUERY` da ADR-0005.
- Usar o BFS como baseline no M4 para comparar custo de consulta estruturada.
- Estudar `fs_shell` como padrão para rodar o próprio código do MoobFS em espaço de usuário,
  reaproveitando `common/` e evitando reboot de VM a cada teste.

O livro *Practical File System Design with the Be File System* (Giampaolo, 1999) circula em
PDF; o link em nobius.org respondeu 406 ao acesso automatizado e não foi verificado.

## 6. Camada de memória para agentes (M4)

Todos abaixo são Apache-2.0, Python, ativos em 2026-10. São candidatos a **cliente** do
MoobFS, não concorrentes: resolvem seleção semântica e montagem de contexto, que a ADR-0005
mantém fora do kernel.

| Projeto | Abordagem | Relação com o MoobFS |
| --- | --- | --- |
| [mem0](https://github.com/mem0ai/mem0) | SDK de memória adicionado a um agente existente; backends plugáveis | Backend de armazenamento alternativo é o ponto de integração mais simples para o M4. |
| [Letta](https://github.com/letta-ai/letta) | Runtime onde o agente é a memória (MemGPT); camadas core/arquivo | Memória arquival pode ser montada como diretório. |
| [Graphiti](https://github.com/getzep/graphiti) | Grafo de conhecimento temporal (equipe Zep) | Fatos com validade temporal mapeiam bem em revisões + atributo `time`. |
| [cognee](https://github.com/topoteretes/cognee) | Ingestão de documentos variados em grafo consultável | Idem; cliente. |

Comparações publicadas em 2026 são majoritariamente de fornecedores; nenhuma medição
independente foi encontrada. O M4 deve definir suas próprias métricas.

## 7. Busca vetorial local

| Projeto | Licença | Uso |
| --- | --- | --- |
| [sqlite-vec](https://github.com/asg017/sqlite-vec) | Apache-2.0 | Índice vetorial embutido, sem servidor; cabe num arquivo ao lado do mount. |
| [USearch](https://github.com/unum-cloud/usearch) | Apache-2.0 | HNSW em C++ com bindings; índice serializável em arquivo. |

Ambos ficam em espaço de usuário, conforme a ADR-0005. O índice vetorial pode ser ele mesmo
uma memória versionada no volume.

## O que não existe pronto

- Filesystem Linux in-tree com atributos tipados indexados e consulta nativa. O BFS fez isso
  em 1997; o driver Linux não o expõe. É o espaço que o MoobFS ocupa.
- Dedup de conteúdo + revisões + snapshots num formato pequeno e auditável. Btrfs e bcachefs
  têm partes disso em escala muito maior.

## Próximos passos derivados

1. Fuzzing em espaço de usuário de `common/` com libFuzzer (dias, não semanas).
2. Kernel próprio com KASAN/lockdep via virtme-ng rodando a suíte atual.
3. Compilar `bfs_shell` do Haiku em Linux e registrar o comportamento de `query`/`mkindex`
   como oráculo para a especificação do `QUERY`.
4. Especificar M2 já assumindo `dm-log-writes` + xfstests como harness de crash.

## Fontes consultadas

- [CrashMonkey/ACE, OSDI 2018](https://www.usenix.net/system/files/osdi18-mohan.pdf)
- [dm-log-writes em xfstests (lore)](https://lore.kernel.org/all/20170904064214.GI17782@dastard/)
- [Janus](https://github.com/sslab-gatech/janus), [survey de fuzzing de SO](https://arxiv.org/pdf/2502.13163)
- [virtme-ng, Kernel Recipes 2024](https://kernel-recipes.org/en/2024/virtme-ng/)
- [bfs_shell em Linux (Haiku)](https://www.haiku-os.org/documents/dev/copying_files_from_and_to_haiku_from_within_linux_using_bfs_shell/), [overview de desenvolvimento de filesystems](https://git.haiku-os.org/haiku.git/tree/docs/develop/file_systems/overview.rst)
- [Comparação mem0/Letta/Zep/cognee](https://mcp.directory/blog/mem0-vs-letta-vs-zep-vs-cognee-2026)
- [iomap, LWN](https://lwn.net/Articles/935934)
