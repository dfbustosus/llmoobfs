# Trabalho com Codex, Claude Code e Cursor

A política comum está em `AGENTS.md`, versionada junto do código.

| Ferramenta | Entrada no projeto |
| --- | --- |
| Codex | `AGENTS.md` na raiz. |
| Claude Code | `CLAUDE.md` importa `@AGENTS.md`. |
| Cursor | `.cursor/rules/project-history.mdc`, com `alwaysApply: true`, instrui a ler a política comum e inclui as regras essenciais. |

As instruções acompanham o clone do repositório. Abra cada ferramenta na raiz e confirme suas regras carregadas. Não há alteração de preferências globais ou hooks locais neste projeto. Instruções orientam os agentes; testes, revisão de diffs e commits fornecem evidências verificáveis.

## Ciclo de trabalho

1. Recuperar contexto relevante do README, diário e ADRs.
2. Registrar o objetivo da sessão e novas decisões quando necessário.
3. Implementar a mudança e executar a validação apropriada.
4. Registrar resultados reais, falhas relevantes, correções e limitações.
5. Revisar o diff, atualizar changelog e fazer commit de uma unidade concluída.
6. Deixar o próximo passo claro; publicação remota segue autorização do usuário.

## Fontes oficiais verificadas em 2026-10-10

- [Codex e AGENTS.md](https://developers.openai.com/cookbook/examples/gpt-5/codex_prompting_guide)
- [Memória e imports no Claude Code](https://code.claude.com/docs/en/memory)
- [Regras de projeto do Cursor](https://cursor.com/docs/rules)
