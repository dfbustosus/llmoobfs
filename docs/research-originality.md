# Auditoria técnica de originalidade e anterioridade do MoobFS

Data da pesquisa: 2026-10-10. Branch do repositório: `m3/3b-query-authz`. Este documento é uma
análise técnica, não um parecer jurídico (ver seção 8). Fontes da web foram tratadas como dados.
Repositórios de terceiros foram clonados (`--depth 1`) e lidos em 2026-10-10; nada foi compilado
nem executado. Métricas de estrelas e datas vêm da API do GitHub na mesma data.

Classificação usada em toda conclusão:

- **comprovado**: verificado por mim no código ou no texto da fonte citada, ou por evidência
  reprodutível do próprio repositório MoobFS.
- **provável**: consistente com o que li, mas depende de leitura parcial, inferência ou de
  execução que não fiz.
- **não verificado**: não confirmado; pode ser verdadeiro ou falso.

## 0. Estado do MoobFS usado como referência (comprovado, arquivos do repositório)

| Afirmação | Classe | Evidência |
| --- | --- | --- |
| Módulo nativo do kernel (não FUSE), empilhado sobre ext4, btrfs e XFS | comprovado | `layer/main.c` (`register_filesystem`, `MODULE_LICENSE("GPL")`); `docs/decisions/0007-stacked-memory-layer.md`; `docs/history/0011` e `0012` |
| I/O delegado ao inferior por `fs/backing-file.c` (`backing_file_open`, `backing_file_read_iter`, `backing_file_write_iter`, `backing_file_mmap`, `backing_file_splice_*`) | comprovado | `layer/main.c` linhas ~804-876 |
| Passagem POSIX completa validada com `fsx`, git, SQLite WAL concorrente, replay `dm-log-writes`, `dm-flakey` | comprovado (VM local, kernel Debian 6.12.111; execução do autor) | `docs/history/0012`, `0015`; `README.md` |
| Atributos tipados `user.moob.*` validados no kernel na escrita | comprovado | `moobl_typed_check` em `layer/main.c`; `docs/history/0013` |
| Índice de atributos em memória, reconstruído na montagem | comprovado | `moobl_index_build` em `layer/index.c`; `docs/spec-m2.md`; `docs/history/0014` |
| `QUERY` e `GET_INFO` (UAPI v1) iguais a um oráculo em espaço de usuário | comprovado | `include/moobfs_ioctl.h`; `tools/moobfs-index`; `docs/history/0014` |
| Reflink e dedup delegados ao inferior, sem implementação própria | comprovado | `docs/decisions/0007` item 7; `layer/main.c` (`vfs_clone_file_range`, `vfs_dedupe_file_range_one`); `docs/history/0012` |
| Fase 3b (em andamento): `QUERY` resolve cada caminho com as credenciais do chamador | comprovado como implementação e como teste de isolamento em ext4/btrfs/XFS; fase não encerrada (ACL, idmap/userns e identidade passaram na VM em 2026-10-10; faltam KASAN sobre a branch e o merge por PR) | `caller_may_read` e `do_query` em `layer/index.c`; `docs/history/0020` |
| Identidade `(namespace, memory_id)` é convenção do cliente; o kernel não gera nem interpreta ids | comprovado (decisão); verificador em andamento | `docs/history/0020`, `docs/spec-m3.md` §2 |
| Sem busca semântica no kernel | comprovado | `docs/decisions/0005` item 5; README |
| Nenhum benchmark foi medido; M4 é plano | comprovado | `docs/benchmark-plan.md` ("Nenhum número aqui foi medido"); `ROADMAP.md` |
| Fluxo KASAN/UBSAN/lockdep existe | comprovado (harness e um defeito achado) | `docs/history/0016`, `vm/kasan.config` |
| Dois defeitos reais achados e corrigidos pelos testes de aceitação (reuso de dentry negativo com perda de dados no WAL do SQLite; aviso de `set_nlink` concorrente) | comprovado | `docs/history/0015` |
| Defeito adicional (rwsem não inicializado em montagem falha) achado pelo harness KASAN | comprovado | `docs/history/0016` |
| `moobl_permission` delega a `inode_permission` do inferior; ACLs POSIX delegadas após correção | comprovado (código e testes de ACL na VM, `docs/history/0020`) | `docs/history/0020` (achado da terceira rodada), commit `d0c8911` |

Limites declarados do próprio projeto: sem xfstests completo; índice só em RAM com orçamento
(128 MiB padrão); montagem exige privilégio (sem `FS_USERNS_MOUNT`); sem `FS_ALLOW_IDMAP`;
atomicidade conteúdo+atributos é a do inferior.

## 1. Matriz de anterioridade

| Trabalho | Ano | Camada | Link | O que antecipa | O que não faz |
| --- | --- | --- | --- | --- | --- |
| Semantic File System (Gifford, Jouvelot, Sheldon, O'Toole, MIT) | 1991 | Servidor NFS em espaço de usuário sobre FS UNIX; índice B-tree em arquivos | https://pages.cs.wisc.edu/~remzi/Classes/838/Fall2001/Papers/sfs-sosp91.pdf | Atributos campo-valor extraídos automaticamente; consulta como "diretório virtual"; índice mantido incrementalmente; acesso associativo a FS existente | Não é módulo de kernel; indexa só arquivos publicamente legíveis (§4 do artigo), filtragem por permissão fica como trabalho futuro; atributos vêm de transdutores, não de xattr tipado; sem ioctl |
| Be File System (Giampaolo, Meurillon) / Giampaolo 1999 | 1997 | FS nativo de kernel (BeOS); reimplementado em Haiku (OpenBFS) | https://en.wikipedia.org/wiki/Be_File_System ; https://github.com/haiku/haiku | Atributos tipados por arquivo, índices por atributo em B+ tree, consultas por igualdade/intervalo, consultas vivas, journaling | Formato próprio, não empilhado; BeOS/Haiku, não Linux; no código Haiku lido (`QueryParser.h`) há um TODO sobre filtragem por permissão do usuário |
| Driver `befs` do Linux | 2001- | Kernel Linux, FS próprio (somente leitura) | https://docs.kernel.org/filesystems/befs.html | Leitura de volumes BFS no Linux | Somente leitura; a documentação não menciona atributos nem índices |
| Precedentes de empilhamento: overlayfs, ecryptfs | cerca de 2006 e 2014 (datas de memória, não verificado) | Kernel, empilhados in-tree | https://docs.kernel.org/filesystems/overlayfs.html | Padrão de FS empilhado sobre inferior maduro, credenciais do montador, regra de não alterar o inferior por fora | Não indexam atributos tipados nem consultam |
| TagFS (Bloehdorn et al.) / TMSU / Tagsistant | 2006 / 2011- / 2009- | WebDAV e depois FUSE; bancos RDF/SQLite | https://www.bibsonomy.org/bibtex/ab2f49f3779bb2ff505135917eae8975 ; https://tmsu.org/ | Tags e consultas como caminhos; metadados fora do arquivo | Espaço de usuário (FUSE ou serviço); banco próprio, não xattr no inferior |
| LiFS (Ames et al.) | 2005-2006 | Protótipo em espaço de usuário | https://www.crss.ucsc.edu/pub/ames-msst06.html | Atributos e links atribuídos como metadado de FS | Protótipo, não kernel Linux (resultado de busca; não li o artigo inteiro) |
| US7617250B2 (HP) | 2003 (concedida 2009) | Patente | https://patents.google.com/patent/US7617250B2/en | Modelo de tuplas objeto-relação-objeto para informação semântica em FS | Ver §8 |
| US20230401171A1 (Dell) | 2022 | Pedido de patente | https://patents.google.com/patent/US20230401171A1/en | FS virtual gerado a partir de metadados de uma consulta | Ver §8 |
| YoloFS (Zhong et al., UW-Madison) | 2026 | Módulo de kernel C empilhado sobre FS local + CLI Rust | https://yolofs.github.io/ ; https://github.com/YoloFS/YoloFS | Módulo de kernel empilhado que "empilha em qualquer FS local", ioctl na raiz do mount, sem FUSE, staging, snapshots, permissões `allow/ask/deny` por caminho | Sem atributos tipados, sem índice, sem consulta; política por regra de caminho, não por credenciais POSIX |
| SMFS (Supermemory) | 2026 | Cliente Rust: FUSE (Linux) / NFSv3 local (macOS) + SQLite + nuvem | https://smfs.ai/research/memory-as-a-filesystem/ ; https://github.com/supermemoryai/smfs | Memória como diretório montado; `grep` semântico; perfil gerado; cache SQLite | Espaço de usuário e serviço em nuvem; não é módulo de kernel |
| VexFS (lspecian) | 2025 | README: três camadas (FUSE + API + dashboard); `kernel_module/` em C com FS de blocos próprio | https://github.com/lspecian/vexfs | Busca vetorial no FS (pretendida) | Alfa; módulo com 3,4 mil linhas, `vector_ops.c` de 241 linhas, README avisa de panics; busca "future" |
| Factory VFS | 2026 | Rust; FS virtual em SQLite montável por FUSE ou NFS | https://github.com/Factory-AI/vfs | Sandbox copy-on-write, auditoria de chamadas de ferramenta, sessão selada transportável | Espaço de usuário; sem índice de atributos |
| Agent FS (desplega.ai) | 2026 | TypeScript/Bun, servidor HTTP/MCP, SQLite + S3, helper FUSE em Rust | https://github.com/desplega-ai/agent-fs | Busca semântica, SQL via DuckDB, versionamento, identidade, papéis por drive (viewer/editor/admin) | Serviço e espaço de usuário |
| RAGFS (Venere-Labs) | 2026 | Rust; FUSE + LanceDB + embeddings locais | https://github.com/Venere-Labs/ragfs | Operações estruturadas com JSON, soft-delete/undo, auditoria, busca semântica local | FUSE; sem permissões por credencial própria |
| "Filesystem-Based Memory for LLM Agents" (Zhou et al.) | 2026 | Estudo empírico; árvore de markdown | https://arxiv.org/abs/2607.26637 | Memória de agente como diretório de arquivos manipulado com ferramentas genéricas | Não propõe FS nem módulo de kernel |

## 2. Comparação técnica por característica

Legenda das colunas de MoobFS: estado e arquivo de evidência. Colunas de terceiros: o que li
no código/texto (classe entre parênteses quando incerta).

| Característica | MoobFS | Trabalhos mais próximos |
| --- | --- | --- |
| Kernel nativo vs FUSE | **comprovado**: kernel, sem FUSE (`layer/main.c`, ADR-0001/0007) | BFS/befs: kernel (comprovado, kernel docs). YoloFS: kernel C (comprovado, `kmod/super.c`). SFS: NFS em espaço de usuário. VexFS: FUSE/API no caminho funcional, módulo alfa. SMFS, Factory VFS, RAGFS, Agent FS, TagFS: FUSE/NFS/serviço |
| Camada empilhada vs FS próprio | **comprovado**: empilhada sobre ext4/btrfs/XFS (`layer/main.c`, README) | YoloFS: empilhada (comprovado, `docs/architecture.md`). overlayfs/ecryptfs: empilhados. BFS, befs, VexFS: FS próprio. SFS: guarda índice em FS UNIX comum, servidor intermediário |
| Compatibilidade POSIX | **comprovado** em VM (fsx, git, SQLite WAL, dm-log-writes) (`docs/history/0012`, `0015`) | YoloFS: testes e2e do repositório (não verificado em profundidade). VexFS: README lista symlinks, hard links, xattr completo, flock, mmap, ACL como pendentes (comprovado no `FUSE_STATUS.md`). BFS: POSIX parcial por desenho (não verificado) |
| Atributos tipados | **comprovado**: `user.moob.*` com tipos string/int64/double/bool/time/raw validados no kernel (`docs/history/0013`) | BFS: atributos tipados nativos (comprovado, Wikipedia/Haiku). SFS: campo-valor string/inteiro de transdutor. Demais: metadados em banco ou front matter, não xattr tipado |
| Índice no kernel | **comprovado**: índice em RAM por (nome, tipo, valor, inode), rebuild na montagem (`layer/index.c`) | BFS/OpenBFS: índice persistente B+ tree no kernel (comprovado, Haiku `bfs/Query.cpp`, Index). SFS: índice em espaço de usuário. Demais: SQLite/LanceDB em processo |
| Consultas estruturadas | **comprovado**: até 4 termos conjuntivos, EQ/LT/LE/GT/GE/PREFIX/EXISTS por ioctl, verificado contra oráculo (`include/moobfs_ioctl.h`, `docs/history/0014`) | BFS: igualdade/intervalo/lógicas e consultas vivas (comprovado em parte: flags `B_LIVE_QUERY` no header Haiku). SFS: caminhos de diretório virtual. Agent FS: SQL via DuckDB (README). TagFS: caminho como consulta de tags |
| Isolamento por permissões POSIX | **comprovado** como implementação e teste de dois uids em 3 inferiores; **fase em andamento** (`layer/index.c` `caller_may_read`; `docs/history/0020`) | SFS: indexa só arquivos públicos (artigo §4), filtragem por solicitante "em investigação". Haiku `QueryParser.h`: TODO "check user permissions here" (comprovado, leitura de 2026-10-10). YoloFS: permissões por regras de caminho `allow/ask/deny`, não por uid/gid (comprovado na documentação). Agent FS: papéis por drive em serviço (`packages/core/src/identity/drives.ts`). Se o BFS/Haiku filtra em outro ponto do VFS: **não verificado** |
| Memória persistente entre sessões | **comprovado** como propriedade do armazenamento (arquivos no inferior; sobrevive a queda: `docs/history/0015`) | Todos os FS persistem dados; YoloFS persiste artefato de sessão em `.yolofs/`; Factory VFS sela sessões em arquivo; SMFS sincroniza com nuvem |
| Proveniência e identidade | **provável/parcial**: identidade `(namespace, memory_id)` é convenção do cliente; proveniência por atributos do cliente; verificador `moobfs-check` em andamento (`docs/history/0020`). O kernel não garante nem gera | Factory VFS: auditoria de chamadas de ferramenta (comprovado, `docs/SPEC.md`). Agent FS: identidade de agente e versionamento (README). YoloFS: journal de mudanças. ai-memory: `supersedes` em SQLite (`docs/research-ai-memory.md`) |
| Compartilhamento físico | **comprovado, mas delegado**: reflink/dedup do btrfs/XFS (`docs/history/0012`); M1 próprio (extents compartilhados) congelado | btrfs/XFS/overlayfs fazem isso sem MoobFS; Factory VFS e Agent FS: não verifiquei dedup |
| Reconstrução de índice | **comprovado**: a cada montagem; PATHS_STALE/INDEX_INCOMPLETE em `GET_INFO` (`layer/index.c`, `moobfs_ioctl.h`) | SFS: reindexação periódica e completa (artigo §5, 1 milhão de atributos em 1h36 em 1991). ai-memory: SQLite derivado reconstruível (`docs/research-ai-memory.md`). BFS: índice persistente, `reindex` manual para arquivos existentes (Haiku docs) |
| Separação armazenamento/semântica | **comprovado**: kernel só atributos estruturados; embeddings no cliente (ADR-0005 item 5) | SFS: transdutores (semântica por tipo de arquivo) fora do FS. SMFS, RAGFS, Agent FS, VexFS: semântica/embeddings embutidos no produto |
| Evidência de confiabilidade | **comprovado** (autor, VM, kernel 6.12.111): fsx 9000 ops por inferior, dm-log-writes, dm-flakey, 2+1 defeitos achados, harness KASAN/lockdep (`docs/history/0012`, `0015`, `0016`). Sem xfstests completo | YoloFS: CI e suíte `tests/` (existência comprovada; cobertura não verificada). VexFS: README admite risco de panics (comprovado). Demais: testes de aplicação |
| Benchmarks reproduzíveis | **não verificado / inexistente**: nenhum número medido; plano em `docs/benchmark-plan.md` e `docs/benchmark-harness.md` | SMFS publica xAFS (110 perguntas, 13 corpora) e reporta menos tokens; avaliação feita pelos próprios autores, sem reprodução independente (página do relatório). YoloFS: artigo SOSP 2026 com avaliação (não li). ai-memory: LongMemEval (`docs/research-ai-memory.md`) |

## 3. Análise dos três concorrentes arquiteturalmente mais próximos

Critério: sobreposição de arquitetura verificada em código ou fonte primária, não de marketing.
VexFS e RAGFS ficaram de fora: VexFS promete busca vetorial em kernel mas o código lido é um FS
de blocos próprio em alfa, com busca "futura" (`kernel_module/README.md`); RAGFS é FUSE com
LanceDB.

### 3.1 Be File System (BFS) e OpenBFS/Haiku

- O que é: FS de BeOS (1997), reimplementado em C++ como OpenBFS no Haiku (licença MIT segundo
  `docs/decisions/0005`; **não reverifiquei a licença hoje**). Haiku tem 2.360 estrelas e
  commit de 2026-10-10 (API do GitHub).
- Recursos comprovados no código/documentação: atributos tipados, índices por atributo, consultas
  com flags `B_LIVE_QUERY` e `B_QUERY_NON_INDEXED` (`headers/private/fs_shell/fssh_fs_query.h`),
  `Query::LiveUpdate` em `bfs/Query.cpp`, motor de consulta compartilhado `QueryParser.h`.
- Relação com MoobFS: é a referência conceitual explícita (ADR-0005). BFS integra atributo,
  índice e consulta no formato em disco; o MoobFS reproduz uma parte (atributo tipado, índice
  em RAM, consulta conjuntiva) sobre FS alheio. O MoobFS **não** implementa consultas vivas,
  índice persistente nem B+ tree em disco.
- Limitação de BFS relevante: `QueryParser.h` contém o comentário "TODO: check user permissions
  here - but which one?!" (leitura de master em 2026-10-10). Isso é evidência sobre o código
  compartilhado; **não verificado** se outro ponto do VFS do Haiku filtra os resultados.
- Linux `befs`: somente leitura, alfa (documentação de 2001); não implementa o que interessa.
  Classe: comprovado (kernel docs).

### 3.2 Semantic File System (SFS, 1991)

- O que é: servidor NFS em espaço de usuário, com transdutores por tipo de arquivo que extraem
  pares campo-valor, índice (pacote B-tree + `refer`), diretórios virtuais interpretados como
  consultas (`/sfs/exports:/lookup_fault/ext:/c`). O kernel guarda só o índice compartilhado;
  os autores dizem ter escolhido NFS em espaço de usuário "para simplificar o desenvolvimento".
- Evidência lida no PDF (páginas 1-6): indexa apenas arquivos publicamente legíveis e lista como
  investigação "limitar resultados a entidades que o solicitante possa ler"; indexação completa
  de 1.042.832 atributos em 1h36 (Tabela 2), reindexação incremental a cada dois minutos.
- Relação: SFS é anterioridade direta para "atributos extraídos + índice + consulta sobre FS
  existente, compatível com clientes sem modificação" e para a *pergunta* de permissões nos
  resultados. O MoobFS difere por estar no kernel, usar xattr tipado escrito pelo cliente (sem
  transdutores) e responder por ioctl com checagem por credencial; a diferença é de mecanismo,
  não de ideia.

### 3.3 YoloFS (SOSP 2026)

- O que é: módulo de kernel (C, `kmod/`, cerca de 4,1 mil linhas) empilhado sobre ext4/XFS/btrfs,
  com CLI e testes em Rust (`user/`, `tests/`). GPL-2.0. 13 estrelas, último commit 2026-09-30,
  criado em 2026-02-12.
- Recursos comprovados: staging e commit/abort, snapshots por comando, viagem no tempo, regras
  `allow/ask/deny` por caminho, ioctls na raiz do mount (`YOLO_IOC_*`), restauração por journal
  (`docs/architecture.md`, `kmod/ioctl.c`, `kmod/journal.c`). Passagem ao inferior por
  `vfs_*()` (por exemplo `vfs_truncate`, `vfs_mmap`), não por `fs/backing-file.c`.
- Não encontrei em `kmod/`: atributos tipados, índice, consulta (apenas `listxattr` de
  passagem em `kmod/inode.c`). Classe: comprovado para o commit clonado (`8848c24`).
- Relação: é a anterioridade mais próxima da *forma* do MoobFS (módulo de kernel empilhado,
  ioctl na raiz, sem FUSE, para agentes). Difere no que a camada oferece: controle e
  reversão de efeitos versus memória consultável. O MoobFS não deve afirmar nada sobre
  "agent-native filesystem em kernel" como categoria nova.

Outros precedentes de empilhamento: overlayfs e ecryptfs (kernel docs); o ADR-0007 do projeto já
os cita como modelo. Comprovado pelo kernel docs: overlayfs guarda as credenciais de quem montou
e exige não modificar o inferior por fora.

## 4. O que o MoobFS não pode reivindicar como inédito

Cada item abaixo tem anterioridade identificada nesta pesquisa.

1. **Filesystem consultável por atributos** (SFS 1991, BFS 1997). comprovado.
2. **Atributos tipados por arquivo com índice por atributo e consulta por igualdade/intervalo**
   (BFS). comprovado.
3. **Consulta servida pelo próprio FS sem FUSE**: BFS/OpenBFS no kernel. comprovado.
4. **Módulo de kernel empilhado sobre FS maduro, sem FUSE, com ioctl na raiz do mount, para
   agentes de IA** (YoloFS, 2026). comprovado.
5. **Padrão de empilhamento** (overlayfs, ecryptfs) e uso de `fs/backing-file.c` (overlayfs,
   FUSE passthrough, conforme o ADR-0007). comprovado.
6. **Memória de agente como arquivos em diretório** (arXiv 2607.26637; SMFS; ai-memory;
   Letta). comprovado nas fontes lidas.
7. **Montagem de memória como diretório com busca semântica** (SMFS, RAGFS, Agent FS, VexFS
   pretendido). comprovado.
8. **Reflink, dedup, snapshots, journaling, ACLs**: são do inferior (btrfs, XFS, ext4). comprovado.
9. **Questão de permissões sobre resultados de consulta**: formulada em 1991 (SFS) e anotada como
   pendência no código Haiku. A questão não é nova; o que o MoobFS afirma é uma implementação
   específica. comprovado para a anterioridade da *pergunta*.
10. **Isolamento entre agentes por controle de acesso** como tema: YoloFS (por regra de
    caminho), Agent FS (por papel de drive). comprovado.
11. **Identidade/proveniência de memória** como conceito: ai-memory, Factory VFS, Agent FS.
    comprovado nas fontes lidas.
12. **Ideias de patente descritas nas §8**: não é possível afirmar novidade sobre elas sem
    avaliação especializada. não verificado.

## 5. Características potencialmente diferenciadoras

Diferenciar não é ser inédito: são combinações ou escolhas de implementação que **não encontrei**
nos trabalhos listados, dentro dos limites desta busca. Ausência de achado não prova ausência de
anterioridade (busca limitada a 10 obras indicadas e algumas consultas abertas).

| Candidato | Classe | Base e ressalvas |
| --- | --- | --- |
| Índice de atributos tipados (xattr real no inferior) em kernel + `QUERY` por ioctl, **sobre FS maduros sem alterá-los**, com dados legíveis sem a camada | **provável** | Código do MoobFS: `layer/index.c`, ADR-0007 item 3. Nos repositórios lidos (YoloFS, VexFS, SMFS, Factory VFS, Agent FS, RAGFS) não achei equivalente (YoloFS: só `listxattr`). BFS tem o recurso, mas em FS próprio. Busca da web em 2026-10-10 não retornou outro projeto. |
| Empilhado (não FS próprio), com validação de tipo no kernel e reconstrução do índice na montagem | **provável** | Combinação: YoloFS empilha sem índice; BFS indexa sem empilhar. Não é característica individual nova. |
| Resultados de `QUERY` filtrados pelas credenciais POSIX do chamador via resolução de caminho e `inode_permission` (inclui ACL por delegação ao inferior) | **comprovado** (implementação + teste local de dois uids em 3 inferiores, `docs/history/0020`), **fase aberta** (ACL e idmap/userns testados na VM; KASAN e merge pendentes) | Anterioridade da pergunta: SFS e TODO do Haiku. Nos outros repositórios lidos não achei filtragem por uid/gid. Haiku: demais pontos do VFS não auditados (**não verificado**). Limitação conhecida: falso negativo após rename de diretório; contrato TOCTOU documentado. |
| Propriedade "nada preso": desmontar e ler com `grep`, `getfattr`, `git`, `rsync` | **comprovado** como desenho e para git/fsx na camada (`docs/decisions/0007` item 3; `docs/history/0015`); a leitura direta do inferior após desmontagem específica **provável** | Também vale para qualquer FS empilhado cujo dado fica no inferior (ecryptfs cifra, então não vale). Não é exclusiva. |
| Evidência de aceitação com clientes reais sem modificação (git, SQLite WAL concorrente, replay de queda, `dm-flakey`) e dois defeitos de kernel achados por isso | **comprovado** (execução do autor) | `docs/history/0015`; não há reprodução independente; carga do SQLite pequena (200 inserções, `0015` Limitações). |
| Separação explícita: kernel só estrutura; semântica no cliente | **comprovado** como decisão | Escolha de projeto, também adotada por SFS (transdutores fora). Não é novidade. |

## 6. Evidências adicionais necessárias

Alinhar com `docs/benchmark-plan.md` (M4) e `docs/benchmark-harness.md`. Nada disto foi
executado.

1. **W3 vs baseline SQLite** (A e B do plano): latência p50/p95/p99 de `QUERY` contra índice
   composto equivalente em SQLite no mesmo FS, com gate de `EXPLAIN QUERY PLAN`. Só então
   pode-se falar de custo ou benefício.
2. **Custo do filtro de permissão**: `QUERY` com e sem `caller_may_read` para razões de
   candidatos privados de 1:1 a 100:1; efeito do lote de 256 e da árvore de diretórios profunda.
3. **Saturação do índice** (`GET_INFO`: 50%..100% do orçamento) e tempo de montagem em S1..S6.
4. **xfstests** (generic + o conjunto empilhado como em `./check --fs overlay`) e **KASAN/lockdep**
   sobre a branch 3b, com ACL, idmap/userns e `chmod`/`rename`/`unlink` concorrentes com consultas.
5. **Fuzzing do ioctl e de `setxattr`** (syzkaller), conforme `docs/security-roadmap.md`.
6. **Busca de anterioridade ampliada**: ler o artigo YoloFS (arXiv 2604.13536) e o código
   Haiku de `vfs.cpp` (`query_read`) para fechar a questão de permissão em BFS; ler LiFS inteiro;
   busca em listas de kernel (LKML) e em bases de patentes por "xattr index query filesystem".
7. **Reprodução independente** dos testes de aceitação (outro host, outro kernel LTS) e
   registro do ambiente (`environment.txt`).
8. **Cliente real**: ai-memory/Letta/mem0 em A, B e C do plano, para qualquer afirmação de
   utilidade para agentes.

## 7. Riscos de afirmações exageradas

Frases a evitar, com motivo:

- "O primeiro filesystem de memória para LLMs": existem SMFS, Agent FS, RAGFS, Factory VFS, VexFS e YoloFS.
- "O único filesystem de kernel para agentes": YoloFS é módulo de kernel para agentes.
- "Inventamos consultas por atributo em filesystem": SFS (1991) e BFS (1997).
- "Mais rápido que SQLite/FUSE/ext4" ou qualquer número de desempenho: nada foi medido.
- "Sem overhead" ou "zero-copy": não medido; usa `backing_file`, mas custo não quantificado.
- "Seguro" ou "isolamento comprovado entre agentes": a fase 3b está em andamento; há limitações
  documentadas (TOCTOU, falso negativo após rename de diretório) e não houve auditoria externa.
- "Busca semântica no kernel": não existe; `QUERY` é conjunção de predicados sobre atributos.
- "Memória de longo prazo para LLM resolvida no filesystem": é armazenamento e consulta; a seleção e a montagem de contexto são do cliente.
- "Reflink/dedup do MoobFS": são do btrfs/XFS.
- "Testado no Linux de forma ampla": um kernel (6.12.111, Debian) em VM, três inferiores.
- "Validado por xfstests" ou "à prova de queda de energia": xfstests completo não foi rodado; replay cobre cenários dirigidos.
- "Livre de patentes" ou "não infringe": não há análise de FTO (ver seção 8).
- "Substitui git/SQLite/ai-memory": a camada complementa e roda sob eles.
- "Pronto para produção" ou "estável": é experimental, sem release; UAPI v1 pode mudar.

## 8. Patentes

**Nenhum parecer jurídico sobre validade, infração ou liberdade de operar (FTO) é dado aqui.
Qualquer decisão exige avaliação por especialista em propriedade intelectual.** O que segue é
apenas descrição técnica do que as páginas do Google Patents informam em 2026-10-10.

### US7617250B2: "Semantic file system" (HP)

- Inventores: Xu, Karlsson, Tang, Karamanolis. Depósito/prioridade 2003-09-22; concessão
  2009-11-10. O Google Patents indica estado expirado por falta de pagamento de taxas, com data
  de expiração listada em 2026-11-06 (não verificado em base oficial).
- O que descreve, tecnicamente: modelo de dados em tuplas (identificador do objeto 1, relação,
  identificador do objeto 2) para registrar relações semânticas entre arquivos e dados derivados
  (conteúdo, propriedades, versões, dependências, controle de acesso). Independentes: reivindicação
  1 (estrutura de dados em tupla), 17 (método que dispara ação quando o objeto é acessado e uma
  condição armazenada na tupla é satisfeita), 32 (sistema de arquivos com armazenamento de tuplas
  e tratamento de eventos de acesso). Resumo obtido por leitura automática da página; as
  reivindicações exatas não foram comparadas palavra a palavra.
- Relação técnica superficial com o MoobFS: ambos associam metadado estruturado a arquivos; o
  MoobFS não usa tuplas de relação nem dispara ações no acesso (comprovado no código lido).

### US20230401171A1: "Dynamic filesystem generation based on content metadata" (Dell)

- Inventores: Christner, Lincourt, Bruno. Depósito 2022-06-14; publicação 2023-12-14; a página
  indica pedido abandonado (descontinuação em 2026-01-06).
- O que descreve: um administrador numa rede de computadores recebe uma consulta de conteúdo,
  extrai metadados, localiza conteúdo correspondente num repositório, empacota-o como filesystem
  virtual ad hoc e o expõe a um computador do grupo, com permissões e expiração. Dependentes
  citam repositório central e interação com driver de filesystem.
- Relação técnica superficial: consulta por metadados e visão virtual; o MoobFS retorna
  caminhos por ioctl e não gera filesystems virtuais.

Por que isso não conclui nada: não analisei famílias, citações, continuações, reivindicações em
outras jurisdições, nem outras patentes. Status em páginas de terceiros pode estar defasado.

## 9. Proposta de posicionamento técnico público

### Português

O MoobFS é um módulo experimental do kernel Linux, GPL-2.0, empilhado sobre ext4, btrfs ou XFS,
que repassa as operações POSIX ao filesystem inferior e acrescenta atributos tipados `user.moob.*`
validados no kernel, um índice em memória reconstruído na montagem e consultas estruturadas por
ioctl, verificadas contra um oráculo em espaço de usuário. Ele se apoia em precedentes que
reconhecemos: o Semantic File System (1991) e o Be File System (1997) para consultas por
atributos, e overlayfs e ecryptfs para o modelo de filesystem empilhado; YoloFS (2026) já
demonstra um módulo de kernel empilhado voltado a agentes. Em desenvolvimento, a fase 3b faz o
`QUERY` respeitar as permissões POSIX do chamador resolvendo cada caminho com suas credenciais,
com testes locais de isolamento em três filesystems inferiores. Os dados continuam sendo arquivos
e xattr comuns do inferior; reflink e dedup são do btrfs/XFS. Aceitação em VM local inclui git,
SQLite em WAL, replay de queda de energia e injeção de erros, que revelaram e corrigiram defeitos
reais; ainda não há benchmarks medidos, xfstests completo nem auditoria independente.

### English

MoobFS is an experimental GPL-2.0 Linux kernel module stacked over ext4, btrfs or XFS. It passes
POSIX operations through to the lower filesystem and adds typed `user.moob.*` attributes
validated in the kernel, an in-memory index rebuilt at mount, and structured queries through an
ioctl, checked against a user-space oracle. It builds on prior work we acknowledge: the Semantic
File System (1991) and the Be File System (1997) for attribute queries, overlayfs and ecryptfs
for the stacked-filesystem model, and YoloFS (2026), which already demonstrates a stacked
kernel module aimed at agents. In progress, phase 3b makes `QUERY` honor the caller's POSIX
permissions by resolving each path with the caller's credentials, with local isolation tests on
three lower filesystems. Data remains ordinary files and xattrs on the lower filesystem; reflink
and dedup belong to btrfs/XFS. Acceptance in a local VM covers git, WAL-mode SQLite, power-loss
replay and error injection, which exposed and fixed real defects; there are no measured
benchmarks, no full xfstests run and no independent audit yet.

## Fontes consultadas

Acessadas em 2026-10-10:

- https://yolofs.github.io/
- https://github.com/YoloFS/YoloFS (clone; `README.md`, `docs/architecture.md`, `kmod/`)
- https://smfs.ai/research/memory-as-a-filesystem/
- https://github.com/supermemoryai/smfs (clone; `Cargo.toml`, `crates/`)
- https://github.com/lspecian/vexfs (clone; `README.md`, `FUSE_STATUS.md`, `kernel_module/`)
- https://github.com/Factory-AI/vfs (clone; `README.md`, `docs/SPEC.md`, `Cargo.toml`)
- https://github.com/desplega-ai/agent-fs (clone; `README.md`, `packages/`)
- https://github.com/Venere-Labs/ragfs (clone; `README.md`, `crates/`)
- https://pages.cs.wisc.edu/~remzi/Classes/838/Fall2001/Papers/sfs-sosp91.pdf (PDF digitalizado, páginas 1-3 e 5-6 lidas)
- https://arxiv.org/abs/2607.26637
- https://arxiv.org/abs/2604.13536 (referenciado pela página do YoloFS; não lido)
- https://arxiv.org/abs/2609.10144 (consultado; não é filesystem; não usado na matriz)
- https://patents.google.com/patent/US7617250B2/en
- https://patents.google.com/patent/US20230401171A1/en
- https://en.wikipedia.org/wiki/Be_File_System
- https://docs.kernel.org/filesystems/befs.html
- https://docs.kernel.org/filesystems/overlayfs.html
- https://github.com/haiku/haiku (`src/add-ons/kernel/file_systems/bfs/Query.cpp`, `headers/private/file_systems/QueryParser.h`)
- https://git.haiku-os.org/haiku.git/tree/headers/private/fs_shell/fssh_fs_query.h
- https://haiku-os.org/legacy-docs/BeBook/IndexFunctions_Overview.html
- https://www.bibsonomy.org/bibtex/ab2f49f3779bb2ff505135917eae8975 (TagFS)
- https://tmsu.org/
- https://en.wikipedia.org/wiki/Semantic_file_system
- https://www.crss.ucsc.edu/pub/ames-msst06.html (LiFS)
- Repositório MoobFS: `README.md`, `ROADMAP.md`, `docs/decisions/0001-0007`, `docs/spec-m2.md`, `docs/spec-m3.md`, `docs/ecosystem.md`, `docs/research-ai-memory.md`, `docs/benchmark-plan.md`, `docs/history/0011-0020`, `layer/main.c`, `layer/index.c`, `include/moobfs_ioctl.h`
