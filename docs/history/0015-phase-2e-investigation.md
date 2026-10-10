# 0015 — Fase 2e: aceitação com clientes reais e investigação de perda de linhas no SQLite

Data: 2026-10-10, America/Sao_Paulo. Estado: **concluído e validado na VM de teste** após a
sequência de rodadas descrita abaixo; mantido na íntegra como registro da investigação.

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

## Hipóteses, descartes e causa-raiz

- Descartado: locks `fcntl` básicos (ipc passa; rollback passa). Descartado: mapeamento
  compartilhado básico (ipc passa). Descartado: perda por erro silencioso do cliente.
- Variante `unix-excl` (wal-index em heap, locking exclusivo) deu 200/200, mas isso só
  remove a concorrência; não prova nada sobre `mmap`. Variante `unix-dotfile` não suporta WAL
  (sem métodos de shm) e foi descartada como experimento inválido.
- **Causa-raiz (defeito 1, perda de dados):** quando um nome é removido e recriado pela camada
  sem que ninguém o tenha aberto, o dentry superior fica negativo mas o seu `d_fsdata`
  continua apontando para o dentry inferior antigo (mantido vivo pela referência da camada,
  com `nlink` 0). O `create` seguinte criava o arquivo novo no inferior, mas `open` passava a
  abrir o inferior antigo, já apagado. Dois processos podiam usar arquivos `-shm`/`-wal`
  diferentes para o mesmo nome; um checkpoint apagava quadros do outro. O WAL do SQLite apaga
  e recria `-wal` e `-shm` constantemente, por isso só ele expôs o problema; o modo rollback
  também abria o journal antigo, mas isso não altera leituras. Correção em
  `moobl_instantiate`: ao instanciar um dentry negativo, trocar o dentry inferior em
  `d_fsdata` pelo recém-criado. Teste de regressão em `tests/layer.c` (unlink + recreate com e
  sem descritor antigo aberto; o mesmo para diretórios). Resultado após a correção: as seis
  variantes do SQLite com 200/200.
- **Defeito 2 (aviso de kernel, só no CI x86):** `WARNING at fs/inode.c:294 __destroy_inode`,
  descasamento de `s_remove_count`, causado por chamadas concorrentes a `set_nlink` na mesma
  inode superior via `fsstack_copy_attr_all` em caminhos sem lock (`getattr`, `d_revalidate`,
  pós-escrita). Reproduzido no runner do GitHub (2 CPUs, sem emulação), não na VM local sob
  TCG. Correção: `moobl_copy_attr` copia modo, dono, tempos e tamanho mas não `nlink`;
  `nlink` muda só nas operações de nome, que o VFS serializa pelo lock do diretório pai.
  Padrão herdado do ecryptfs, que faz a mesma cópia; vale investigar se o problema existe lá.

## Erros de harness na fase (além dos anteriores)

4. `$5` dentro de uma função de shell é o argumento da função, não do chamador; o VFS do
   SQLite chegou vazio. 5. `busybox --install -s /bin` sobrescreveu o `mke2fs` real com o
   applet do busybox; binários reais passaram a viver em `/tools`, à frente do `PATH`.

## Erros de harness na fase (continuação)

6. O ash do busybox Debian é compilado com `PREFER_APPLETS`: um comando cujo nome coincide com
   um applet (`mke2fs`) executa o applet mesmo com `/tools` à frente do `PATH`; binários reais
   passaram a ser chamados por caminho absoluto. 7. Um `sed` que não casou a linha fez a suíte
   não rodar e o log analisado ser o da execução anterior; detectado pelos timestamps iguais.
8. `dmsetup suspend` em um dispositivo que está devolvendo erro falha ao descarregar; a troca
   de tabela de volta para `linear` usa `--noflush --nolockfs`.

## Achado de durabilidade (não é defeito da camada)

Na primeira replay do `dm-log-writes` o arquivo renomeado não existia na marca gravada logo
após o `rename`. É a regra POSIX: `rename` só é durável após `fsync` do diretório pai ou o
commit do journal. A convenção do cliente na especificação (§6) passou a incluir o `fsync` do
diretório; o cliente de teste sincroniza após o `rename`.

## Resultado final (kernel Debian 6.12.111, QEMU TCG)

```text
LAYER_LOWER_PASS: ext4 reflink=no
LAYER_LOWER_PASS: btrfs reflink=yes
LAYER_LOWER_PASS: xfs reflink=yes
SQLITE_VARIANTS: lower=0 alone=0 layer=0 rollback=0 excl=0 again=0
ACCEPTANCE_CLIENTS_PASS
POWER_LOSS_REPLAY_PASS: marks mkfs m1 m2 m4 end
IO_ERROR_INJECTION_PASS
MOOBFS_ACCEPTANCE_PASS: git, sqlite wal, memory client, dm-log-writes replay, dm-flakey
MOOBFS_LAYER_PASS: … index and QUERY equal to the oracle …
```

- git: repositório na camada, três commits, `fsck --strict` limpo, conteúdo correto.
- SQLite WAL: 200/200 em todas as variantes, inclusive escritor e leitor concorrentes na
  camada; `integrity_check` ok.
- `dm-log-writes`: replay em disco zerado até `mkfs`, `m1`, `m2`, `m4` e `end`; em cada marca,
  `e2fsck -fn` limpo após recuperação do journal, memórias até a marca inteiras (conteúdo e
  atributos), posteriores ausentes, `QUERY` com a contagem certa.
- `dm-flakey` com `error_writes`: erros do ext4 no `dmesg` como esperado, sem `BUG`/`Oops`/
  `WARNING`; após a troca de tabela e `e2fsck`, remontagem limpa, conteúdo anterior intacto,
  `moobfs-check` limpo.
- Nenhum aviso de kernel na VM local com as duas correções. O CI em x86 nativo é a verificação
  da corrida do `nlink`; resultado anotado no adendo abaixo quando disponível.

## O que a fase 2e provou

Clientes que não sabem que o MoobFS existe, inclusive o mais exigente deles, rodam inalterados
sobre a camada, sobre ext4, btrfs e XFS, e sobrevivem a queda de energia e a erros de disco com
o comportamento do inferior. Dois defeitos reais foram encontrados e corrigidos por esses
testes; nenhum dos testes anteriores os teria pego.

## Limitações

Carga do SQLite pequena (200 inserções) e um único padrão de concorrência; sem xfstests
completo; sem KASAN/lockdep; `dm-flakey` só no modo "erros de escrita" sobre ext4; o teste de
I/O com erros não verifica o comportamento sobre btrfs e XFS.
