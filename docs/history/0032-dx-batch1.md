# 0032 — Experiência do desenvolvedor no GitHub: plano e lote 1

Data: 2026-10-11. Branch: `community/dx-batch1` a partir de `main` (`1d23b43`). Estado: arquivos
do lote 1 concluídos; configurações no GitHub pendentes de execução pelo mantenedor (ver
Limitações).

## Objetivo

Tornar o repositório mais fácil de encontrar e de usar por quem chega pela primeira vez,
complementando `community-plan.md` e `seo-geo-plan.md`. O diagnóstico, os três lotes e o que
ficou fora do escopo estão em [`docs/dx-plan.md`](../dx-plan.md). O mantenedor aprovou o plano e
a execução do lote 1.

## Decisões

| Tema | Decisão | Justificativa resumida |
| --- | --- | --- |
| Labels de descoberta | Usar os labels padrão `good first issue` e `help wanted` junto dos labels próprios | `/contribute`, a busca do GitHub e agregadores reconhecem só o nome exato |
| Lint local | `make lint` passa a ser a fonte única; o job `Lint` do CI o chama | O comando local e o CI não podem divergir |
| SPDX | Verificação movida para `scripts/check-spdx.sh`, ainda restrita a C | A issue #15 (shell e Python) fica com quem chegar; o script facilita a tarefa |
| CODEOWNERS | `* @claudiofp`, sem exigir aprovação de code owner | Só encaminha pedidos de revisão; a regra de aprovação segue em `GOVERNANCE.md` |
| Citação | `CITATION.cff` com autor-entidade "Studio Moob" | É um projeto de pesquisa; a entidade evita registrar dados pessoais desnecessários |
| Bot de "stale" | Não adotar | Fecha automaticamente issues de quem chega |

## Mudanças

- Novos: `docs/dx-plan.md`, `.github/CODEOWNERS`, `CITATION.cff`, `scripts/check-spdx.sh` e
  este registro.
- `Makefile`: alvos `help` e `lint`. O `lint` roda `shellcheck --shell=sh --severity=warning`
  nos `vm/*.sh` e `scripts/*.sh` rastreados, e depois `scripts/check-spdx.sh`.
- `.github/workflows/ci.yml`: os passos de shellcheck e SPDX do job `Lint` foram trocados por
  `make lint`. O passo de DCO não mudou. O antigo `ls vm/*.sh build/*.sh` não encontrava nada
  em `build/`, que é ignorado pelo Git; a nova lista usa `git ls-files` e inclui `scripts/`.
- `docs/labels.md`: regras e script para `good first issue` e `help wanted`.
- `docs/development.md`: documenta `make help` e `make lint`, no lugar da frase "There is no
  `help` or `lint` target yet".

## Evidências

Comandos executados nesta sessão, em macOS (Apple Silicon):

- `sh scripts/check-spdx.sh`: passou. O conjunto de arquivos é idêntico ao do antigo `find`
  do CI, conferido com `diff` das duas listas ordenadas.
- `docker run --rm -v "$PWD:/mnt" -w /mnt koalaman/shellcheck:v0.10.0 --shell=sh
  --severity=warning vm/in-container.sh vm/kasan-guest.sh vm/run.sh vm/suite.sh
  scripts/check-spdx.sh`: código de saída 0. O CI usa o `shellcheck` do apt do Ubuntu 24.04
  (versão diferente); a primeira execução real de `make lint` no CI será a do pull request.
- `make lint` local: falhou com a mensagem esperada, porque o `shellcheck` não está instalado
  no host.
- `make help`: imprime a lista de alvos.
- `make test`: `attr_test` e 9 testes de imagem, OK.
- `docker run --rm -v "$PWD:/app" -w /app citationcff/cffconvert:2.0.0 --validate`: "Citation
  metadata are valid according to schema version 1.2.0".
- A API `community/profile` informa `issue_template: null`, mas a página "Community
  standards" no navegador mostra os oito itens como presentes. A API não reconhece issue forms
  YAML; nada foi alterado.

Nenhum teste de kernel foi executado: nada em `layer/`, `kernel/` ou `vm/` mudou.

## Limitações

As escritas no GitHub desta rodada foram negadas pelo controle de permissões da sessão e não
foram aplicadas. Ficam para o mantenedor:

```sh
R=studiomoob/llmoobfs
# secret scanning, push protection e exclusão de branch após merge
gh api -X PATCH repos/$R --input - <<'JSON'
{"delete_branch_on_merge": true,
 "security_and_analysis": {"secret_scanning": {"status": "enabled"},
                           "secret_scanning_push_protection": {"status": "enabled"}}}
JSON
# labels de descoberta
for n in 5 6 8 12 13 15; do gh issue edit $n -R $R --add-label "good first issue,help wanted"; done
for n in 7 9 14 16 17 18; do gh issue edit $n -R $R --add-label "help wanted"; done
# issues fixadas (máximo de três)
for n in 16 17 5; do gh issue pin $n -R $R; done
```

A discussão de boas-vindas foi redigida fora do repositório. Publicá-la e fixá-la (a fixação
só existe na interface web) fica com o mantenedor.

Nota de 2026-10-11: o mantenedor aplicou o comando de configurações. Conferido com
`gh api repos/studiomoob/llmoobfs`: `delete_branch_on_merge: true`, `secret_scanning` e
`secret_scanning_push_protection` como `enabled`. Seguem pendentes os labels nas issues, as
issues fixadas e a discussão de boas-vindas.

Nota de 2026-10-11: no PR #27, o job `Lint` (com `make lint` e o `shellcheck` do apt do Ubuntu
24.04) e o job `Tools and sanitizers` passaram na primeira execução.

Nota de 2026-10-11: renumerado de 0031 para 0032 ao trazer `main` para a branch, porque o
registro 0031 (tempo de remontagem) entrou em `main` antes. Os labels `good first issue` e
`help wanted` e as issues fixadas (#16, #17, #5) foram aplicados pelo mantenedor; conferido com
`gh issue list --label` e a consulta GraphQL `pinnedIssues`. A discussão de boas-vindas continua
pendente.

## Próximo passo

Abrir o pull request, observar o job `Lint` com `make lint` e, depois do merge, publicar a
discussão de boas-vindas. Lote 2: devcontainer e Codespaces com medição de tempo e disco de `make
vm-test`, OpenSSF Scorecard, CodeQL, `checkpatch.pl` como aviso, labeler e mensagem de
primeira contribuição.
