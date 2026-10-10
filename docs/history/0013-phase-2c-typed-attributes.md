# 0013 — Fase 2c: atributos tipados validados no kernel, ferramentas e fuzzing

Data: 2026-10-10, America/Sao_Paulo. Estado: implementado e validado na VM de teste.

## Pedido

Continuar para a fase 2c da especificação do M2: codificação tipada de `user.moob.*` com
validação no `setxattr`, ferramentas `moobfs-check` e `moobfs-attr`, fuzzing da codificação.

## Implementação

- `include/moobfs_attr.h` e `common/attr.c`: codificação compartilhada (um byte de tipo mais
  payload; tipos `string` UTF-8 ≤ 1 KiB, `int64`, `double` sem NaN, `bool`, `time`, `raw`
  ≤ 4 KiB), validador de UTF-8 estrito (rejeita NUL, sobrelongos, substitutos, > U+10FFFF),
  regras de nome (`[a-z0-9_.]`, 1–64 bytes, sem pontos nas bordas ou duplicados), nome
  reservado `format`, e parser do marcador de versão. Funções puras, sem alocação; o módulo as
  inclui por `layer/attr.c` como o M1 faz com `common/format.c`.
- `layer/main.c`: `setxattr` em `user.moob.*` passa por `moobl_typed_check` antes de tocar o
  inferior: `EPERM` para o nome reservado, `EINVAL` para nome ou codificação inválidos,
  `ENOSPC` ao criar o 65.º atributo tipado (contagem por `vfs_listxattr` só quando o atributo é
  novo). Namespaces opacos continuam passando sem validação. Na montagem, a camada lê
  `user.moob.format` na raiz inferior e recusa versões desconhecidas ou maiores que 1; ausência
  é aceita; inferior sem xattr gera aviso.
- `tools/moobfs-attr.c`: `get`, `set`, `rm`, `list` tipados sobre a API de xattr, mais `rawset`
  para testes plantarem bytes inválidos. `tools/moobfs-check.c`: percorre uma árvore
  (`nftw`, sem seguir symlinks, sem cruzar mounts), valida cada atributo tipado e o marcador de
  formato, emite JSON e retorna 2 quando há problemas; não repara. Ambos Linux-only, alvo
  `make attr-tools`.
- `tests/attr_test.c`: 60 verificações da codificação, executadas em `make test` no host.
  `tests/fuzz_attr.c` + `tests/fuzz-corpus/attr/`: alvo libFuzzer para validador, nomes e
  marcador; `make fuzz` (clang LLVM com ASan/UBSan); passo no CI com 30 s de orçamento.
- `tests/layer.c`: seção de atributos tipados (codificações válidas e inválidas, nome
  reservado, limite de 64 com verificação de que escritas rejeitadas não tocam o inferior,
  namespace opaco intocado). `vm/init`: exercita `moobfs-attr` e `moobfs-check` em cada
  inferior, inclusive plantando um `bool` inválido direto no inferior e exigindo que o
  verificador o reporte com o motivo.

## Evidências executadas

- Host (macOS arm64): `make test` → `attr encoding tests: 60 checks passed` e os 9 testes do M1.
- VM (`make vm-test`, kernel Debian 6.12.111): `MOOBFS_VM_PASS`, `LAYER_LOWER_PASS` para
  ext4, btrfs e XFS, `MOOBFS_LAYER_PASS: … typed attributes and tools …`; sem `BUG`, `Oops`
  ou `WARNING`. SHA-256 de `moobfs_layer.ko`: ver `build/vm/environment.txt` da execução.
- Fuzzing: não executado no host (Apple clang não traz libFuzzer). No CI (run 38066427480,
  commit `8ab6ecd`, ubuntu-24.04 com clang): 21.747.211 execuções em 31 s sob ASan e UBSan,
  cobertura 113 arestas, corpus final de 138 entradas, nenhuma falha.

## Tentativas e correções

1. O teste de passagem da fase 2b usava `user.moob.kind` com bytes sem tipo; passou a usar
   `user.other.kind`. A recusa pelo módulo era o comportamento correto.
2. A imagem ext4 de 64 MiB recebia blocos de 1 KiB do `mke2fs`, cujo bloco de xattr comporta
   cerca de 36 entradas; o 37.º atributo falhava com `ENOSPC` vindo do ext4, não da camada.
   A imagem passou a usar `-b 4096`, e a especificação registra que o limite efetivo é o menor
   entre os 64 da camada e a capacidade do inferior.
3. Uma chave sobrando no teste fez o build falhar e o log da VM ser o da rodada anterior; o
   erro foi detectado pelo marcador antigo no console. O initramfs não tinha `/tmp`.

## Limitações

Mesmas da fase 2b (sem KASAN/lockdep, sem xfstests completo, sem dm-log-writes, sem cliente
real na VM). O `moobfs-check` lê valores de até 4 KiB + 1; atributos maiores são reportados
como problema sem leitura. O limite por arquivo é verificado só na criação, com custo de uma
listagem de xattr por criação.

## Próximo passo

Fase 2d: índice em RAM construído na montagem, atualizado em `setxattr`/`removexattr`/`unlink`,
`QUERY` e `GET_INFO` por ioctl, `moobfs-index` em espaço de usuário como oráculo.
