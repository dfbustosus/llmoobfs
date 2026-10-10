# 0006 — Publicação da ADR-0005 e levantamento do ecossistema livre

Data: 2026-10-10, America/Sao_Paulo.

## Pedido

Publicar no GitHub o commit da ADR-0005 e levantar ferramentas, bibliotecas, módulos e
projetos de software livre que ajudem o MoobFS, para não resolver problemas que já têm solução.

## Resultado

- `git push origin main` publicou `b2fb037` (ADR-0005); os dois checks obrigatórios foram
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

## Adendo (2026-10-10): página interativa do plano no site

Pedido: transformar o desenho de entrada/processamento/saída numa página do site e publicar.
Criada `site/plan.html` (sem dependências, JavaScript mínimo, funciona sem JS com todas as
caixas visíveis), estilos em `site/style.css`, links de navegação em `index.html` e `guide.html`.
Validação: `python3 scripts/check-site.py` (3 páginas), `git diff --check`, prévia local em
1280 px (três colunas, filtro por marco, painel de detalhe) e 375 px (uma coluna), ambas sem
overflow horizontal medido. A página marca só o M1 como validado; o restante é proposta.
