# 0016 — Harness com KASAN e lockdep em kernel próprio; diagrama de fluxo no site

Data: 2026-10-10, America/Sao_Paulo. Estado: **em andamento**; fechado quando o primeiro run
do workflow concluir.

## Pedido

"Seguir" após o fechamento da fase 2e. Itens escolhidos: a pendência pequena do site (diagrama
do fluxo do dado) e a pendência mais pesada do M2, validação com KASAN e lockdep.

## Mudanças

- `site/plan.html`: diagrama SVG do caminho de uma memória (escrita à esquerda, consulta e
  leitura à direita), com descrição acessível; validado por `scripts/check-site.py`.
- `vm/suite.sh` (novo): a suíte da camada extraída de `vm/init`, parametrizada por diretório
  de binários, diretório de ferramentas, módulos opcionais e cinco dispositivos de bloco.
  `vm/init` passa a defini-los e a incluir a suíte; o harness Debian foi reexecutado após a
  refatoração e passou inteiro (`MOOBFS_VM_PASS`, `MOOBFS_ACCEPTANCE_PASS`, `MOOBFS_LAYER_PASS`).
- `vm/kasan.config`: fragmento com KASAN genérico inline, UBSAN, `PROVE_LOCKING`,
  `DEBUG_ATOMIC_SLEEP`, `DEBUG_LIST`, `DEBUG_VM`, ext4/btrfs/XFS embutidos, device mapper
  com `dm-log-writes` e `dm-flakey`, virtio-blk.
- `vm/kasan-guest.sh`: executa a suíte dentro do guest do virtme-ng (raiz = checkout do host,
  ferramentas do próprio Ubuntu do runner por symlink) e falha em qualquer diagnóstico de
  kernel no `dmesg`.
- `.github/workflows/kasan.yml`: `workflow_dispatch` e semanal; kernel `linux-6.12.112` do
  kernel.org com SHA-256 fixado (`164dc9d1…ed03`), `virtme-ng 1.41` fixado, timeout de 150
  minutos, artefatos com console, `dmesg` e `.config`.

## Decisões

- Kernel próprio só neste workflow; o harness Debian continua sendo a referência de
  reprodutibilidade e roda a cada push. O workflow KASAN é mais pesado e não bloqueia pushes.
- O kernel upstream 6.12.112 difere do Debian 6.12.111 (que traz backports, como a assinatura
  de `backing_file_open`); o módulo é compilado contra os cabeçalhos de cada kernel, então
  ambos são alvos de teste legítimos. Divergências de API entre os dois serão registradas.

## Evidências

Primeiro run: `gh workflow run kasan.yml` → run 38071014790. Resultado anexado abaixo.
