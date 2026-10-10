# ADR-0007 — MoobFS como camada empilhada sobre um filesystem maduro

Data: 2026-10-10. Estado: aceito pelo usuário como direção; substitui a ADR-0006. Nada
implementado.

## Contexto

A ADR-0006 especificava o M2 como volume completo (alocador de blocos, tabela de inodes,
diretórios, journal via jbd2). O usuário fixou o princípio registrado em
[`docs/history/0009`](../history/0009-differentiation-and-principle.md): bibliotecas e
filesystems maduros não são concorrência mas base; a inovação cabe nos recursos que uma memória
de LLM usaria; "concorrer com o git é loucura". Um volume completo reconstrói o que ext4, XFS e
btrfs resolveram em vinte anos e atrasa a única pergunta que importa: se os recursos de memória
têm valor mensurável.

## Decisão

1. **MoobFS passa a ser um filesystem empilhado nativo** (padrão de overlayfs e ecryptfs, ambos
   in-tree): um módulo do kernel montado sobre um diretório de um filesystem maduro
   ("inferior"). Continua sendo mount nativo, sem FUSE; a ADR-0001 permanece válida.
2. **O inferior fornece** blocos, journal, diretórios, mmap, locks, reflink/dedup, checksums de
   dados e snapshots de volume. **A camada fornece** só o que é específico de memória:
   atributos tipados validados, índice de atributos, `QUERY`, identidade e proveniência de
   memórias (M3), consulta temporal (M3) e verificação do que é específico de memória.
3. **Os dados permanecem íntegros e legíveis sem a camada.** Conteúdo são arquivos comuns do
   inferior; atributos são xattr reais do inferior com codificação versionada. Desmontar o
   MoobFS não prende nada: `grep`, `getfattr`, `git` e `rsync` continuam funcionando no
   diretório inferior. Isso é requisito, não efeito colateral.
4. **Interface explícita com o inferior**, por capacidades anunciadas na montagem (tem reflink?
   tem checksums de dados? tem snapshots?). A camada degrada com honestidade sobre ext4 e usa
   o máximo sobre btrfs ou XFS. Alvos de teste: btrfs, XFS e ext4.
5. **Git permanece a ferramenta de versionamento de conteúdo.** A camada não implementa
   histórico de conteúdo. Revisões por memória no M3, se adotadas, complementam o git com
   identidade estável, atributos por revisão e consulta temporal; não o substituem.
6. **O formato M1 fica congelado** como experimento concluído de compartilhamento físico. O
   módulo, as ferramentas e os testes do M1 permanecem no repositório e no histórico. Um
   filesystem próprio como inferior só volta como implementação alternativa da interface do
   item 4, se as medições do M4 mostrarem que o inferior é gargalo ou que o layout limita algo
   específico de memória.
7. **Reuso de infraestrutura verificado no v6.12:** `fs/backing-file.c`
   (`backing_file_open`, `backing_file_read_iter`, `backing_file_write_iter`,
   `backing_file_mmap`, `backing_file_splice_*`, usados pelo overlayfs e pelo FUSE passthrough)
   para delegar I/O; `vfs_clone_file_range` e `vfs_dedupe_file_range_one` para reflink;
   `vfs_getxattr`/`vfs_setxattr` para atributos; `override_creds` com as credenciais de quem
   montou, como o overlayfs. xfstests já sabe testar filesystems empilhados (`./check --fs
   overlay` é o modelo).

## Nota (2026-10-10): critério de aceitação sem projetos externos

O critério de aceitação herdado da ADR-0006 citava o ai-memory. Decisão do usuário: o MoobFS
não depende de nenhum projeto externo de memória de agentes, nos dois sentidos. A aceitação do
M2 usa git, SQLite e um cliente de memória próprio; ai-memory e os demais sistemas do
levantamento (`docs/ecosystem.md`) entram no M4 como objetos de benchmark, não como requisito.

## Alternativas consideradas

- Volume completo (ADR-0006): rejeitado pelo princípio acima; documento preservado como
  referência da forma "ext3 pequeno" caso o item 6 um dia se aplique.
- Dois caminhos em paralelo (camada e FS próprio): rejeitado porque divide o esforço e adia a
  medição; substituído pelo item 6, que condiciona o FS próprio a evidência.
- Biblioteca ou daemon em espaço de usuário com xattr e SQLite: atende aos recursos, mas não é
  filesystem montável, o que contradiz a ADR-0001 e remove a propriedade de que qualquer
  programa consulta memórias por interface do kernel.
- Patch no btrfs ou XFS para índice de xattr: dependeria de aceitação upstream de um recurso
  de nicho; descartado.

## Consequências

- O tamanho do projeto passa a ser o tamanho da inovação: referência de escala são overlayfs
  (cerca de 12,5 mil linhas) e ecryptfs (cerca de 8,2 mil), não ext4 ou btrfs.
- Filesystems empilhados têm armadilhas próprias (inode aliasing, mmap, consistência de
  dentries, modificação do inferior por fora). A especificação do M2 adota as regras do
  overlayfs: o inferior não deve ser modificado enquanto montado; mudanças por fora são
  detectadas na próxima montagem pela reconstrução do índice.
- Atomicidade entre conteúdo e atributos é a do inferior (não há transação cobrindo os dois);
  a camada documenta a ordem e os clientes usam `fsync`.
- A especificação do M2 é reescrita ([`docs/spec-m2.md`](../spec-m2.md)). O roadmap e o site
  mudam de acordo. A ADR-0006 recebe nota de substituição.
