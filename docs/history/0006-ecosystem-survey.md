# 0006 — Publicação da ADR-0005 e levantamento do ecossistema livre

Data: 2026-10-10, America/Sao_Paulo.

## Pedido

Publicar no GitHub o commit da ADR-0005 e levantar ferramentas, bibliotecas, módulos e
projetos de software livre que ajudem o MoobFS, para não resolver problemas que já têm solução.

## Resultado

- `git push origin main` publicou `ae16320` (ADR-0005); os dois checks obrigatórios foram
  acionados no GitHub.
- `docs/ecosystem.md` (novo) organiza o levantamento por problema: VM instrumentada, fuzzing,
  crash-consistency, estruturas de kernel, BFS em espaço de usuário, camadas de memória para
  agentes e busca vetorial. Cada entrada traz licença, atividade e uso previsto.

## Decisões derivadas (propostas, não implementadas)

Preferir `dm-log-writes` + xfstests a um replay próprio; fuzzing em espaço de usuário do
parser compartilhado antes de syzkaller; virtme-ng para kernel com KASAN/lockdep; `bfs_shell`
do Haiku como oráculo para o `QUERY` da ADR-0005; mem0/Letta/Graphiti/cognee tratados como
clientes, não concorrentes.

## Evidências

Buscas na web e consultas à API do GitHub (licença, estrelas, último push) em 2026-10-10.
Nenhuma ferramenta foi executada; nenhuma integração foi feita.

## Limitações e próximo passo

Comparações de camadas de memória vêm de fornecedores. O PDF do livro de Giampaolo não foi
verificado. Próximo passo: iniciar o item 1 de `docs/ecosystem.md` (fuzzing de `common/`).
