# 0015 — Fase 2e: aceitação com clientes reais e investigação de perda de linhas no SQLite

Data: 2026-10-10, America/Sao_Paulo. Estado: **em andamento**; este registro é atualizado a
cada rodada até a fase fechar. Nada aqui é afirmado como aprovado.

## Decisões desta etapa
  
  - Nenhum projeto externo de memória entra no critério do M2; todos entram no M4 como objetos
    de benchmark. Especificação e ADR-0007 anotadas (ver registro 0014).
  - Aceitação com git, SQLite em WAL e um cliente de memória próprio (convenção tmp + atributos
    + fsync + rename + fsync do diretório), mais `dm-log-writes` e `dm-flakey`. O SQLite não faz
    parte da arquitetura; é o cliente mais exigente disponível para um filesystem e verifica o
    próprio conteúdo.
  - O diagrama de fluxo do dado vai para o site junto com o plano.
  - O histórico registra decisões, erros e hipóteses descartadas; perguntas e respostas de
    esclarecimento não (regra ajustada em `AGENTS.md` em 2026-10-10).
  
  ## Harness da fase 2e

- `tests/third_party/log-writes/`: `replay-log` do xfstests vendorizado (commit `9b56b94`,
  2019-10-24, GPL-2.0) para reproduzir o log do `dm-log-writes` até uma marca.
- `vm/Dockerfile`: `git`, `sqlite3`, `dmsetup`. `vm/in-container.sh`: função `stage_bin`
  copia binários dinâmicos e suas bibliotecas (`ldd`) para o initramfs; `git`, `sqlite3`,
  `dmsetup`, `mke2fs`, `e2fsck` passam a existir na VM; dois discos virtio a mais (scratch e
  log de 64 MiB); módulos `dm-mod`, `dm-log-writes`, `dm-flakey` na cadeia.
- `vm/init`: seção de aceitação com git (init, três commits de memórias, `fsck --strict`,
  `cat-file`), QUERY sobre o repositório, experimento do SQLite (abaixo), replay de queda de
  energia em cinco marcas e injeção de erro com `dm-flakey`. `tests/ipc.c`: primitivas entre
  processos (locks `fcntl` conflitantes, `F_GETLK`, `F_SETLKW`, mapeamento compartilhado nos
  dois sentidos, truncate e remapeamento).

## Resultados até agora (kernel Debian 6.12.111, VM QEMU)

| Teste | Resultado |
| --- | --- |
| git sobre a camada: init, 3 commits, `fsck --strict`, conteúdo via `cat-file` | passou |
| `QUERY` sobre o repositório git | passou |
| `tests/ipc.c` no ext4 direto e na camada | passou nos dois |
| SQLite WAL, escritor + leitor concorrentes, ext4 direto (controle) | 200/200 |
| SQLite WAL, só escritor, camada | 200/200 |
| SQLite WAL, escritor + leitor concorrentes, camada | 184 a 188 de 200, repetível |
| SQLite rollback (`journal_mode=DELETE`), escritor + leitor, camada | 200/200 |
| `integrity_check` em todos os casos | ok |

Linhas perdidas numa rodada: 13, 27, 42, 45, 49, 58, 73, 80, 87, 130, 141, 145, 181, 188,
193. Isoladas e espalhadas; o escritor recebeu sucesso em todas as inserções; nenhum erro de
lock impresso (a CLI do SQLite retorna 1 em erro, verificado no container).

## Erros encontrados no caminho e correções

1. Primeira rodada: "database is locked" porque a CLI do SQLite não tem `busy_timeout`;
   corrigido com `.timeout 10000`. O erro em si provou que os locks `fcntl` atravessam a
   camada.
2. `alias` dentro do script do initramfs não é expandido pelo shell do busybox; trocado por
   função.
3. Variantes "cruzadas" (escritor pela camada, leitor direto no inferior) são inválidas por
   desenho: cada caminho tem seu próprio domínio de locks POSIX (inode da camada versus inode
   inferior). Descartadas; registradas para não repetir.

## Hipóteses e estado

- Descartado: locks `fcntl` básicos (ipc passa; rollback passa). Descartado: mapeamento
  compartilhado básico (ipc passa). Descartado: perda por erro silencioso do cliente.
- Em teste: variantes com os VFS do próprio SQLite, `unix-excl` (wal-index em heap, sem
  `mmap`) e `unix-dotfile` (locks por arquivo, com `mmap`), para separar o caminho de `mmap`
  do `-shm` do caminho de locks. Resultado será anexado aqui.

## Próximo passo

Fechar a causa do defeito, corrigir na camada, repetir a matriz inteira, e só então validar
`dm-log-writes` e `dm-flakey`, que ainda não chegaram a executar por causa da falha anterior.
