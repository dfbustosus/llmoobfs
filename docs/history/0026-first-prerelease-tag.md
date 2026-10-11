# 0026 — Primeira tag de pre-release: v0.1.0-experimental

Data: 2026-10-10. Branch: `release/v0.1.0-experimental` sobre `main` (commit `455f1c2`).

## Decisão

Criar a tag anotada `v0.1.0-experimental` e a release correspondente no GitHub marcada como
pre-release, sem binários, sem empacotamento e sem suporte, conforme decidido no plano de SEO
(registro 0024, decisões do mantenedor). A tag é um instantâneo do fonte com M1, M2 (fases 2a
a 2e), M3 fase 3b e o harness do M4, mais os arquivos de comunidade e SEO.

## Mudanças

- `CHANGELOG.md`: a seção "Unreleased" passa a chamar-se `v0.1.0-experimental — 2026-10-10`,
  com um parágrafo de abertura; as entradas não mudam.
- Textos que diziam "sem release" (README em inglês e português, FAQ em `docs/` e no site,
  `when-not-to-use.md`, `SECURITY.md`, `CONTRIBUTING.md`, `site/index.html`) passam a dizer
  "apenas tag de pre-release do fonte", mantendo o caráter experimental e o "não use em
  produção".

## Evidências

Os checks do PR desta branch (ferramentas, Lint, suíte da VM) são a validação; a tag é criada
sobre o commit de merge em `main`, com as notas da release derivadas do `CHANGELOG.md`.

## Próximo passo

Medições do M4 em hardware acelerado (`docs/bench-accelerated-guest.md`); nenhuma nova tag
antes do relatório do M4.
