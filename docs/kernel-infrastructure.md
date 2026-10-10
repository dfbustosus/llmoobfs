# Infraestrutura madura do kernel reutilizável pelo MoobFS

Data: 2026-10-10. Verificado contra o código-fonte do Linux **v6.12** (tag upstream) e contra
`build/vm/kernel.config` do kernel Debian 6.12.111 usado na VM. Objetivo: antes de escrever o
M2, listar o que o kernel já oferece a um módulo de filesystem, para não reescrever o que
ext4, XFS, btrfs, ocfs2 e GFS2 usam há anos. "Disponível na VM" indica a opção de configuração
do kernel alvo (`=y` embutido, `=m` módulo carregável).

## Reutilizar (decisão proposta para o M2)

| Necessidade do M2 | Infraestrutura | Maturidade | Na VM | Observações verificadas |
| --- | --- | --- | --- | --- |
| Journaling de metadados e recuperação após queda | **jbd2** (`fs/jbd2/`) | Desde 2006; usado por ext4 e ocfs2 | `CONFIG_JBD2=m` | `jbd2_journal_init_dev`, `jbd2_journal_load`, `jbd2_journal_start`, `jbd2_journal_flush`, `jbd2_journal_destroy` são `EXPORT_SYMBOL`, logo acessíveis a módulo fora da árvore. ocfs2 é a prova de que um FS que não é ext4 consegue usá-lo. |
| Leitura e escrita bufferizada, mmap gravável, truncamento, writeback | **iomap** (`fs/iomap/`) | Usado por XFS, GFS2, zonefs, erofs, ext4 (DIO) | `CONFIG_FS_IOMAP=y` | Exportados com `_GPL`: `iomap_file_buffered_write`, `iomap_page_mkwrite`, `iomap_truncate_page`, `iomap_zero_range`, `iomap_writepages`, `iomap_dirty_folio`. O M1 já usa `iomap_read_folio` e `iomap_fiemap`. zonefs (cerca de 2.600 linhas) é a menor referência in-tree de FS gravável sobre iomap. |
| Compartilhar bytes entre arquivos por interface padrão | **remap_file_range** (`fs/remap_range.c`) | ioctls `FICLONE`, `FICLONERANGE`, `FIDEDUPERANGE`; usados por XFS, btrfs, ocfs2 | VFS, sempre presente | `generic_remap_file_range_prep`, `vfs_clone_file_range`, `vfs_dedupe_file_range` exportados. Implementar `.remap_file_range` faz `cp --reflink`, `duperemove` e `fsdupes` funcionarem sem ferramenta própria. `REMAP_FILE_DEDUP` exige que o VFS compare os bytes antes de compartilhar, o que é exatamente a regra do M1. |
| Atributos estendidos com namespaces e permissões | **VFS xattr** (`fs/xattr.c`, `struct xattr_handler`) | Universal | VFS | `vfs_getxattr`, `vfs_setxattr`, `vfs_listxattr` cuidam de permissão, prefixos `user.`/`trusted.`/`security.` e chamadas de sistema. O módulo só fornece handlers por prefixo. |
| Deduplicar blocos de xattr idênticos entre inodes | **mbcache** (`fs/mbcache.c`) | Usado por ext2 e ext4 | Embutido com ext4 | `mb_cache_entry_create`, `mb_cache_entry_find_first/next` exportados. Resolve o caso "muitas memórias com o mesmo conjunto de atributos" sem código próprio. |
| Verificação criptográfica de conteúdo imutável | **fs-verity** (`fs/verity/`) | ext4, f2fs, btrfs | `CONFIG_FS_VERITY=y` | Árvore de Merkle por arquivo, verificada na leitura. Candidata a substituir o SHA-256 de payload do M1 para objetos selados, em fase posterior. |
| Cache de páginas, dentries, inodes, locks POSIX (`fcntl`), writeback, freeze | VFS genérico | Universal | VFS | SQLite e git precisam de `fcntl` locks e `fsync`; o VFS entrega ambos a qualquer FS que implemente `fsync` corretamente. |
| Checksums | `lib/crc32c`, `crypto/sha256` | Universal | sim | Já usados pelo M1. |
| Estruturas em memória | `lib/btree.c`, maple tree, xarray, rbtree | Universal | sim | Para o índice de atributos em RAM do M2; B+ tree em disco fica para depois. |

## Testar com (não escrever harness próprio)

| Necessidade | Ferramenta | Na VM |
| --- | --- | --- |
| Consistência após queda de energia | `dm-log-writes` + testes estilo xfstests `generic/482` | `CONFIG_DM_LOG_WRITES=m` |
| Injeção de erro de I/O | `dm-flakey` | `CONFIG_DM_FLAKEY=m` |
| Regressão POSIX | xfstests grupo `generic`; `fsx` e `fsstress` (em `ltp/` do xfstests) | espaço de usuário |
| Kernel instrumentado | virtme-ng com `CONFIG_KASAN` e `CONFIG_PROVE_LOCKING` | kernel próprio |
| Fuzzing do parser | libFuzzer/AFL++ sobre `common/` | host |

## Avaliado e não adotado agora

- **Snapshots em nível de bloco** (`dm-snapshot`, `dm-thin`, ambos `=m` na VM). Dão snapshot de volume inteiro sem código no FS, mas não dão revisões por arquivo nem identidade estável de memória, que é o que o M3 pede. Ficam como baseline de comparação para o M3.
- **Alocador de blocos genérico**: não existe no kernel. ext2 (`balloc.c`, 1.535 linhas) é a referência mais simples de bitmap de blocos e inodes.
- **B+ tree em disco genérica**: não existe. Cada FS tem a sua. `fs/befs/btree.c` (leitura) e o livro do BFS são referência; o M2 começa sem índice persistente.
- **Buffer heads para dados**: ext2/ext3 usam; o M1 já escolheu iomap. Ver questão aberta abaixo.

## Questão aberta que a especificação do M2 precisa testar cedo

Não há FS in-tree que combine **iomap para dados bufferizados** com **jbd2 para metadados**:
ext4 usa jbd2 com buffer heads nos dados bufferizados; XFS e GFS2 usam iomap com log próprio.
As duas combinações conhecidas funcionam; a nossa é inédita e pode exigir ordenação explícita
(dados antes do commit de metadados, equivalente a `data=ordered`) via callbacks de commit do
jbd2. Alternativa segura se o experimento falhar: dados por buffer heads no M2, como ext3,
migrando para iomap depois. Isso deve ser o primeiro protótipo do M2, antes do formato final.

## Referências de código para ler

- `fs/ocfs2/journal.c`: uso de jbd2 por um FS que não é ext4.
- `fs/zonefs/file.c`: escrita bufferizada e direta com iomap em FS pequeno.
- `fs/ext2/{balloc,ialloc,dir,xattr}.c`: alocação por bitmap, diretórios lineares, xattr com mbcache.
- `fs/xfs/xfs_reflink.c` e `fs/btrfs/reflink.c`: implementação de `remap_file_range` com copy-on-write.
- `fs/remap_range.c`: o que o VFS já faz antes de chamar o FS (comparação de bytes no dedupe).
