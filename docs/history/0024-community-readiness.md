# 0024 — Preparação para contribuições externas: políticas, templates, documentação e CI

Data: 2026-10-10. Branch: `community/readiness`. Estado: concluído na parte de documentos e
configuração (ações 1 a 14 do plano); validação do CI pendente até o pull request.

## Objetivo

Executar a parte de documentos e configuração do plano aprovado em
[`docs/community-plan.md`](../community-plan.md), com as decisões do mantenedor da seção 13,
sem tocar código do kernel, ferramentas ou testes. Nota: o plano previa este registro como 0023,
mas esse número já estava ocupado pela revisão de implementação; o registro recebeu o próximo
id livre, 0024.

## Decisões (seção 13 do plano, aplicadas aqui)

| Tema | Decisão |
| --- | --- |
| Contato de segurança e conduta | `claudio@studiomoob.com`, mais o canal privado do GitHub Security Advisories |
| DCO | Adotado: `Signed-off-by` obrigatório em cada commit, verificado no CI (job `Lint`, só em pull requests); não retroativo |
| Código de conduta | Contributor Covenant 2.1, texto integral com atribuição e fonte |
| Idioma | Inglês em código, commits, issues, PRs e README; `docs/` em português ou inglês; ADRs e especificações traduzidos sob demanda externa |
| Prazos em `SECURITY.md` | Confirmação em 7 dias, avaliação em 30 dias, atualização ao relator a cada 30 dias; correção sem prazo fixo; sem recompensa |
| Escopo de segurança | Módulo da camada, `common/`, ferramentas, harness da VM, módulo M1; fora: filesystems inferiores, kernel, QEMU |
| KASAN | Roda em pull requests que alterem `layer/`, `vm/` ou o próprio workflow; mantidos o semanal e o `workflow_dispatch` |
| Licença | Confirmada GPL-2.0-only; verificação de cabeçalho SPDX nos fontes C em CI |
| Revisão de `layer/` | Uma aprovação do mantenedor mais uma revisão externa registrada no PR, até haver segundo mantenedor |
| Execução | Ações 1 a 14 nesta rodada; 15 a 18 após o merge |

Decisão adicional desta sessão: o novo CI `Lint` verifica SPDX apenas em `*.c` e `*.h` sob
`layer/ common/ tools/ tests/ include/` (excluindo `tests/third_party/`), conforme o plano;
scripts shell e Python ficam para a ação de `check-spdx` (ação 13 da seção 4). Arquivos `*.mod.c`
gerados pelo kbuild (ignorados pelo Git) são excluídos para que o laço também passe numa árvore
local com módulos compilados.

## Mudanças

Arquivos novos:

- `.github/ISSUE_TEMPLATE/test_failure.yml`, `.github/ISSUE_TEMPLATE/review_request.yml`
- `docs/labels.md` (esquema de labels com cores e script `gh label create`)
- `docs/development.md` (ambiente reproduzível, harness, parametrização de `vm/suite.sh`, como
  adicionar teste e binário, KASAN local passo a passo, estilo)
- `docs/history/0024-community-readiness.md` (este registro)

Arquivos reescritos ou alterados:

- `CODE_OF_CONDUCT.md` (Contributor Covenant 2.1), `SECURITY.md`, `CONTRIBUTING.md`
- `.github/pull_request_template.md` (checklist da seção 6 do plano, caixas de UAPI/formato)
- `.github/ISSUE_TEMPLATE/bug_report.yml` (kernel, inferior, opções de montagem, `dmesg`,
  `moobfs-check`), `proposal.yml` (ADR relacionada, verificação de escopo, testes planejados),
  `config.yml` (Security Advisories primeiro, e-mail como alternativa, issues em branco desativadas)
- `docs/testing.md`: reestruturado conforme a seção 5 do plano (pré-requisitos, alvos, tabela de
  marcadores com o que cada um prova e não prova, leitura de logs, teste isolado, KASAN,
  adicionar inferior, não coberto). Correção factual: o timeout do QEMU em `vm/in-container.sh` é
  de 600 s, não 180 s como o texto anterior dizia.
- `README.md` (banner de estágio experimental, seção "At a glance", política de idioma) e
  `README.pt-BR.md` (banner equivalente em português)
- `ROADMAP.md` em três estados (concluído com evidência, planejado, dependente de validação),
  cada item com link para registro ou especificação
- `.github/workflows/ci.yml`: job `Lint` (shellcheck em `vm/*.sh` e `build/*.sh`, SPDX nos
  fontes C, DCO no intervalo `origin/<base>..HEAD` em pull requests); jobs existentes intactos
- `.github/workflows/kasan.yml`: gatilho `pull_request` com `paths` para `layer/**`, `vm/**` e o
  próprio workflow; grupo de concorrência mantido
- `CHANGELOG.md` (entrada em Unreleased) e `docs/history/README.md` (linha 0024)

## Evidências

Nenhum teste de kernel ou de ferramenta foi executado: a mudança é só de documentos e de
configuração de CI. Verificações feitas localmente nesta sessão:

- leitura de `vm/in-container.sh`, `vm/run.sh`, `vm/init`, `vm/suite.sh`, `vm/kasan-guest.sh`,
  `Makefile` e `kasan.yml` para que `docs/development.md` e `docs/testing.md` descrevam o que os
  scripts fazem (timeout 600 s, `-accel tcg -m 512M -smp 2`, cinco discos virtio, variáveis
  `BIN TOOLS MODS LOWER LAYER DEV_*`);
- lista de marcadores extraída por `grep` de `vm/suite.sh`, `vm/kasan-guest.sh`, `vm/init` e
  `tests/*.c`; os marcadores `LAYER_PASSTHROUGH_TESTS_PASS`, `QUERY_INVALID_REQUESTS_REFUSED`,
  `IDMAP_REFUSED_EINVAL` e `USERNS_MOUNT_REFUSED_EPERM` vêm dos binários de teste, não da suíte;
- todos os `*.c` e `*.h` rastreados fora de `tests/third_party/` começam com linha SPDX
  (verificado com `git ls-files` e `head -1`), logo o novo passo de SPDX deve passar;
- `shellcheck` e PyYAML não estão instalados nesta máquina: a sintaxe YAML dos workflows e dos
  templates foi conferida por inspeção, e o `shellcheck` dos scripts `vm/*.sh` será executado
  pela primeira vez pelo próprio job `Lint` no pull request. Se ele apontar problemas, a correção
  é uma mudança separada em `vm/` (que, pela nova regra, dispara o KASAN).

O CI do pull request desta branch é a primeira execução real dos passos de lint, SPDX e DCO.

## Limitações

- Labels no GitHub, Security Advisories, proteção do `main` e o status obrigatório do KASAN não
  foram configurados nem verificados; `docs/labels.md` traz o script, a configuração é ação
  separada do mantenedor.
- Tempos de execução continuam não medidos; os documentos citam só os timeouts do CI (35 min
  para a VM, 150 min para o KASAN, 600 s para a fase QEMU).
- `tests/ipc.c` é compilado e copiado para o guest mas não é chamado por `vm/suite.sh`; registrado
  em `docs/testing.md` como observação, sem alteração.
- `vm/*.sh` e `vm/init` não têm linha SPDX; a verificação em CI cobre só fontes C, como o plano
  pedia. Acrescentar SPDX aos scripts é tarefa inicial (seção 4, item 13).
- `site/index.html` não recebeu o banner (a seção 9 do plano o prevê); fica para a rodada do site.
- Os commits desta branch anteriores à adoção do DCO não têm `Signed-off-by`; a verificação só
  vale no intervalo do PR e a regra não é retroativa. Se o PR incluir commits antigos sem
  sign-off, o job falhará e o mantenedor decide entre rebase com `-s` ou merge após `main`.

## Próximo passo

Abrir o pull request de `community/readiness` para `main`, observar os jobs `Lint` e `KASAN and
lockdep` (este dispara porque `.github/workflows/kasan.yml` mudou), criar os labels com o
script de `docs/labels.md` e, após o merge, executar as ações 15 a 18 do plano: pedidos de
revisão (seção 8), ADR do modelo de ameaça (S1), execução isolada em `vm/suite.sh` e KASAN
como check obrigatório.
