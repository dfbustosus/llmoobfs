# 0010 — ADR-0007: camada empilhada sobre filesystem maduro; M2 reescrito

Data: 2026-10-10, America/Sao_Paulo.

## Pedido

Após o princípio registrado em 0009, o usuário perguntou se empilhar sobre um FS maduro perde
novidade e se valeria manter dois caminhos (camada e FS próprio). Resposta registrada: a
novidade (atributos tipados, índice, `QUERY`, proveniência) fica toda na camada; perde-se
controle do layout em disco e há um teto de desempenho por dupla camada; dois caminhos em
paralelo dividem o esforço e adiam a medição. O usuário concordou com um caminho só, com o FS
próprio condicionado a evidência do M4, e pediu a ADR-0007 e a reescrita da especificação.

## Mudanças

- `docs/decisions/0007-stacked-memory-layer.md` (nova; substitui a ADR-0006, que recebe nota
  datada e é preservada).
- `docs/spec-m2.md` reescrita como camada empilhada; a versão anterior preservada em
  `docs/spec-m2-full-volume-superseded.md`.
- `docs/kernel-infrastructure.md`: adendo com `fs/backing-file.c`, overlayfs, ecryptfs e
  reflink delegado, verificados no v6.12 e na configuração da VM (`OVERLAY_FS=m`,
  `ECRYPT_FS=m`).
- `ROADMAP.md`, `README.md`, índice do histórico, `CHANGELOG.md`.

## Evidências

Símbolos exportados de `fs/backing-file.c` e uso pelo overlayfs lidos do código v6.12; contagem
de linhas de overlayfs e ecryptfs por `wc -l` nos arquivos principais; `README.overlay` do
xfstests confirma suporte a FS empilhados. Nenhum código compilado ou executado.

## Limitações

Especificação é proposta. O M1 fica congelado como experimento concluído e regressão; não é
mais a base do produto. Riscos próprios de FS empilhado (aliasing de inodes, mmap, modificação do
inferior por fora) estão listados, não resolvidos.

## Próximo passo

Fase 2a: passagem mínima estilo wrapfs com `backing_file_*` sobre ext4, `fsx` e KASAN, em VM.
