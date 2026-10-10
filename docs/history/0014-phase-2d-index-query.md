# 0014 — Fase 2d: índice em memória e consulta por atributo (`QUERY`)

Data: 2026-10-10, America/Sao_Paulo. Estado: implementado e validado na VM de teste.

## Pedido

Continuar para a fase 2d da especificação do M2: índice em RAM construído na montagem,
atualizado nas operações de escrita, `QUERY` e `GET_INFO` por ioctl, e um oráculo em espaço
de usuário para comparar respostas.

## Implementação

- `include/moobfs_ioctl.h`: UAPI versão 1. `MOOB_IOC_QUERY` recebe até 4 termos
  `(nome, operador ∈ {eq, lt, le, gt, ge, prefix, exists}, tipo, valor)` em conjunção, um
  limite de até 4.096 resultados e um ponteiro para o vetor de saída; devolve
  `(inode, caminho relativo ao mount, flags)` por acerto, com `truncated` quando há mais.
  `MOOB_IOC_GET_INFO` devolve versões, limites, capacidades anunciadas pelo tipo do inferior
  (reflink em btrfs/XFS, checksums e snapshots em btrfs, xattr), flags (`nolinks`, índice
  desligado, caminhos desatualizados, índice incompleto) e uso do orçamento. Campos reservados
  devem ser zero; comando desconhecido retorna `ENOTTY`; os ioctls só respondem no diretório
  raiz do mount.
- `layer/index.c` (novo, ~640 linhas) e `layer/moobl.h`: por nome de atributo, uma rbtree de
  entradas ordenadas por `(tipo, valor, inode)`; por inode, um registro com a lista dos seus
  atributos, o caminho registrado na varredura ou na última escrita e um ponteiro para o inode
  da camada quando está em cache. Orçamento de memória contabilizado por alocação (padrão
  128 MiB, opção `index_budget=`; `noindex` desliga o índice e `QUERY` responde `ENOSYS`).
  Varredura na montagem: percurso iterativo do inferior com `iterate_dir` e
  `lookup_one_len_unlocked`, lendo `user.moob.*` de arquivos e diretórios com as credenciais do
  montador; codificações inválidas e `raw` não entram no índice. Atualizações: `setxattr`
  (substitui a entrada e registra o caminho), `removexattr`, `unlink` quando o último link cai,
  `rmdir`, `rename` (arquivo: caminho novo; diretório: marca caminhos desatualizados e passa a
  resolver pelo cache de dentries). Consulta: varredura ordenada das entradas do primeiro termo
  sob lock de leitura, verificação dos demais termos pela lista do inode. `double` ordena por
  chave IEEE 754 monotônica; NaN é recusado. Falha de memória ao atualizar o índice depois de
  uma escrita bem-sucedida no inferior não desfaz a escrita: marca o índice como incompleto e o
  `GET_INFO` expõe isso.
- `tools/moobfs-index.c` + `tools/query-common.h`: oráculo que percorre o diretório inferior
  e responde as mesmas consultas pela API de xattr; `tests/query.c`: cliente do ioctl que
  imprime no mesmo formato, mais `info` e `invalid` (treze pedidos malformados que devem
  retornar `EINVAL`, comando desconhecido `ENOTTY`, ioctl fora da raiz `ENOTTY`).
- `vm/init`: em cada inferior, monta uma árvore de seis inodes com atributos de todos os tipos
  indexáveis e compara kernel com oráculo em dez consultas (igualdade, conjunção, prefixo,
  intervalo, vazia, existência, booleano, `double` negativo, atributo em diretório, `raw` não
  indexado); verifica `GET_INFO`; exercita rename de arquivo e de diretório, unlink e
  removexattr; desmonta e remonta (índice reconstruído pela varredura), monta com `noindex` e
  com `index_budget=1M`.

## Evidências executadas

`make vm-test`, kernel Debian 6.12.111, primeira execução da fase 2d sem correções no módulo:

```text
LAYER_LOWER_PASS: ext4 reflink=no
LAYER_LOWER_PASS: btrfs reflink=yes
LAYER_LOWER_PASS: xfs reflink=yes
MOOBFS_LAYER_PASS: … index and QUERY equal to the oracle …
```

Sem `BUG`, `Oops` ou `WARNING`. Todas as consultas do kernel foram idênticas, byte a byte, às
do oráculo em espaço de usuário, nos três inferiores, antes e depois de renomear, remover e
remontar. Única correção durante a sessão: um `#include` faltando no cliente de teste.

Resultado para o projeto: pela primeira vez um mount Linux responde "quais arquivos têm
`kind = decision` e `score ≥ 40`" por uma chamada ao kernel, sem banco de dados ao lado. É o
item 1 da ADR-0005 funcionando sobre btrfs, XFS e ext4 sem formato próprio em disco.

## Limitações

- Índice não persistente: a montagem percorre o inferior inteiro; custo proporcional ao número
  de inodes, não medido ainda para árvores grandes.
- Consulta por varredura linear das entradas do primeiro termo; sem uso de intervalo na
  rbtree ainda. Suficiente para o M2; medir no M4.
- Caminhos: após renomear um diretório, caminhos de descendentes dependem do cache de
  dentries; sem cache a resposta traz só o inode e a flag `PATH_UNKNOWN`.
- Modificar o inferior por fora enquanto montado não atualiza o índice (regra do overlayfs).
- Mesmas pendências anteriores: KASAN/lockdep, xfstests completo, dm-log-writes, revisão de
  segurança das credenciais.

## Próximo passo

Fase 2e: aceitação com cliente real (git, SQLite em WAL e o ai-memory sobre o mount),
`dm-log-writes` e `dm-flakey`. Antes disso, decidir como levar git e SQLite estáticos para a
VM (pacotes Debian no container, extraídos para o initramfs).
