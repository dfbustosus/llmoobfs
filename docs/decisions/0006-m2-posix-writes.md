# ADR-0006 — M2 com escrita POSIX sobre infraestrutura do kernel

Data: 2026-10-10. Estado: **substituída pela [ADR-0007](0007-stacked-memory-layer.md)** em
2026-10-10, no mesmo dia, antes de qualquer implementação. Preservada como registro da forma
"volume completo"; os requisitos de escrita POSIX, aceitação com cliente real e reuso de
infraestrutura do kernel continuam válidos e foram transportados para a ADR-0007.

## Contexto

A proposta original (`docs/native-mvp.md` §5) previa escrita apenas por ioctl `PUT` em um
arquivo de controle, sem `write(2)`, `rename`, `unlink` ou `mmap` gravável. A leitura técnica
do ai-memory (`docs/research-ai-memory.md`) mostrou que um cliente real de memória para agentes
grava arquivos com `std::fs` (tmp + rename + fsync), mantém um repositório git (`.git/` com
mmap) e um SQLite em WAL (locks `fcntl`, `-shm` com `MAP_SHARED` gravável). Nenhum desses
caminhos funciona com escrita só por ioctl. O usuário pediu para não reescrever o que o kernel
já tem e para especificar o M2 com escrita POSIX.

## Decisão

1. **O M2 é um filesystem POSIX gravável**: `create`, `write`, `truncate`, `rename`, `unlink`,
   `mkdir`, `rmdir`, `fsync`, `mmap` compartilhado gravável, locks `fcntl`, xattr.
   Hard links e symlinks ficam fora do M2 (retornam `EPERM`), por não serem exigidos pelos
   clientes-alvo e simplificarem contagem de referências e confinamento.
2. **Journaling via jbd2**, modo metadados com ordenação de dados antes do commit
   (semântica equivalente a `data=ordered`). Não escrever log transacional próprio.
3. **Dados via iomap**, mantendo a escolha do M1. A combinação iomap + jbd2 é inédita
   in-tree; o primeiro protótipo do M2 valida essa combinação e, se falhar, o M2 usa buffer
   heads para dados, como ext3, sem mudar o formato em disco.
4. **Compartilhamento de bytes pela interface padrão** `remap_file_range` (`FICLONE`,
   `FIDEDUPERANGE`) com copy-on-write em extents compartilhados. Dedup automático na escrita
   não faz parte do M2; a deduplicação é explícita (ioctl padrão) ou offline (`mkfs` importando,
   ferramenta de varredura). Isso preserva a regra do M1: só bytes comparados são compartilhados.
5. **Atributos tipados da ADR-0005 expostos como xattr** `user.moob.*` via handlers do VFS;
   blocos de xattr idênticos compartilhados por `mbcache`.
6. **Índice de atributos em RAM** reconstruído na montagem (volume limitado a 16 GiB e 65.536
   inodes); `QUERY` por ioctl no `.control` consulta esse índice. Índice persistente em disco é
   decisão posterior, com medição.
7. **Formato novo com magic próprio** (`MOOBFSW2`). Imagens M1 não são montáveis pelo código
   do M2 nem convertidas in-place; `mkfs.moobfs --import-m1` cria volume M2 a partir de uma
   imagem M1. O módulo M1 permanece no histórico; não há compatibilidade binária prometida.
8. **Critério de aceitação com cliente real**: montar um volume M2, inicializar um repositório
   git, criar um SQLite em WAL e apontar o `data_dir` do ai-memory para ele, executando uma
   sessão capturada. Sem isso o M2 não é considerado concluído.

## Alternativas consideradas

- Manter escrita só por ioctl (proposta original): mais simples de tornar durável, mas inútil
  para qualquer cliente existente. Rejeitada; a interface `PUT`/`GET_REQUEST` idempotente
  pode voltar como otimização por cima do POSIX.
- Log transacional próprio (native-mvp §6–7): bem desenhado, mas duplica o jbd2 e exigiria
  anos de testes que o jbd2 já tem. Rejeitado como implementação; preservado como documento.
- Snapshots por dm-thin em vez de revisões no FS: avaliado para o M3, não para o M2.
- Rust for Linux: abstrações de FS ainda não estáveis em 6.12. Não adotado.

## Consequências

- O M2 fica, deliberadamente, com a forma de um "ext3 pequeno" (bitmaps, tabela de inodes,
  diretórios lineares, jbd2) acrescido de três coisas que ext3 não tem: extents compartilháveis
  com copy-on-write, atributos tipados indexados e `QUERY`. A justificativa do projeto passa a
  depender de medir essas três coisas, não de reinventar o resto.
- O parser compartilhado em `common/` cresce para cobrir bitmaps, inodes, diretórios, xattr e
  journal; o fuzzing dele passa a ser pré-requisito.
- xfstests, fsx, fsstress e dm-log-writes tornam-se o harness oficial; `make vm-test` passa a
  executá-los.
- A proposta `native-mvp.md` §5–7 fica superada por esta ADR; §3 (modelo de dados) fica
  refinada pela ADR-0005; o restante permanece como referência histórica.
