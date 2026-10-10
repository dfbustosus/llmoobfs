# Plano para receber contribuições externas

Estado: **proposta** (2026-10-10). Nada neste documento está implementado; é um plano. Ele não
cria compromisso de compatibilidade, de prazo ou de suporte. Origem: inventário do repositório
na branch `m3/3b-query-authz`. Onde a informação não foi verificada, o texto diz "a confirmar".

Objetivo: preparar o MoobFS, camada nativa empilhada e experimental para memória de agentes de
LLM, para receber contribuições de desenvolvedores de Linux, filesystems, segurança e
desempenho, sem exagerar a maturidade e sem alegar vantagens não provadas. Restrições do projeto
que valem para tudo abaixo: todo trabalho é registrado em `docs/history/`, decisões
arquiteturais em `docs/decisions/`, e o projeto não é substituído por FUSE nem por API em espaço
de usuário (ADR-0001, `AGENTS.md`).

## 0. Estado atual

O inventário mostrou que **vários arquivos esperados já existem**. O plano foca nas lacunas e em
alinhar o que existe, não em recriar.

| Item | Existe? | Onde | Lacuna |
| --- | --- | --- | --- |
| Licença | Sim | `LICENSE` (GPL v2); `SPDX GPL-2.0-only` nos fontes (ex.: `layer/main.c`) | Confirmar que todo arquivo novo (scripts, testes) tem SPDX; nenhuma verificação automática |
| README | Sim, EN e PT-BR | `README.md`, `README.pt-BR.md` | Sem banner de estágio experimental no topo; descreve M1 primeiro e a camada (M2/3b) depois; sem seção "como contribuir por área" |
| ROADMAP | Sim, 17 linhas | `ROADMAP.md` | Tabela única de marcos; sem separar "concluído com evidência" de "dependente de validação"; sem links por item para diário/spec |
| AGENTS/CLAUDE | Sim | `AGENTS.md`, `CLAUDE.md` (importa AGENTS) | Escritos para agentes; contribuidor humano precisa de resumo (está em `CONTRIBUTING.md`) |
| CHANGELOG | Sim | `CHANGELOG.md`, seção "Unreleased" | Sem política de formato por entrada |
| CONTRIBUTING | Sim, 49 linhas | `CONTRIBUTING.md` | Diz que **não há DCO nem CLA**; não pede VM para mudança em `layer/` de forma explícita, não pede KASAN, não define revisão dupla nem política de squash; inglês e português aceitos |
| CODE_OF_CONDUCT | Sim, 23 linhas | `CODE_OF_CONDUCT.md` | Texto próprio, **não** é o Contributor Covenant (declara isso); contato `claudio@studiomoob.com`; sem prazo de resposta |
| SECURITY | Sim, 24 linhas | `SECURITY.md` | Já cita GitHub private vulnerability reporting (habilitado, segundo o texto; a confirmar na configuração) e e-mail; faltam escopo, metas de resposta, CVE, embargo, crédito |
| GOVERNANCE | Sim | `GOVERNANCE.md` | Mantenedor único; descreve proteção do `main`; sem plano de segundo mantenedor |
| Templates de issue | Parcial | `.github/ISSUE_TEMPLATE/bug_report.yml`, `proposal.yml`, `config.yml` | Falta template de falha de teste e de pedido de revisão; bug não pede kernel, FS inferior nem `dmesg` (a confirmar o conteúdo dos campos); sem labels definidos |
| Template de PR | Sim | `.github/pull_request_template.md` | Sem itens de KASAN, UAPI/formato, VM, evidência de números |
| CI | Sim | `.github/workflows/ci.yml` | Ver abaixo |
| KASAN/lockdep | Sim, sob demanda e semanal | `.github/workflows/kasan.yml` | Não roda por PR; não é check obrigatório |
| Pages | Sim | `.github/workflows/pages.yml`, `site/` (index, guide, plan) | Sem fluxo de contribuição na página; `plan.html` sem auditoria de acessibilidade registrada |
| Makefile | Sim | `Makefile` | Alvos: `all tools attr-tools test fuzz module layer vm-test`; sem `help`, sem `lint`, sem alvo para um teste só |
| Harness de VM | Sim | `vm/Dockerfile`, `in-container.sh`, `run.sh`, `init`, `suite.sh` (381 linhas), `kasan.config`, `kasan-guest.sh` | Sem como rodar um teste isolado nem como adicionar um inferior |
| Testes | Sim | `tests/` (`attr_test.c`, `layer.c`, `query.c`, `ipc.c`, `mounted.c`, `idmap.c`, `test_images.py`, `fuzz_attr.c`, `fuzz-corpus/attr`, `third_party`) | Corpus de fuzz pequeno; sem cobertura medida |
| Guia de testes | Sim | `docs/testing.md` (138 linhas) | Narrativa por fase, não referência; sem tempos esperados; sem tabela de marcadores |
| Roadmap de segurança | Sim | `docs/security-roadmap.md` (S1 a S11) | Tudo "a fazer" exceto a tabela de estado; S1 (ADR) ainda não escrita |
| Diário e ADRs | Sim | `docs/history/` (0001 a 0021 + README), `docs/decisions/` (0001 a 0007) | ADRs 0001-0007 só; modelo de ameaça sem ADR |
| Estratégia de distribuição | Sim | `docs/distribution-strategy.md`, §8 (três etapas) | Proposta, nada implementado |
| Originalidade | Sim | `docs/research-originality.md`, §7 (frases a evitar) | Não é referenciada por `CONTRIBUTING.md` |
| Avaliação de valor | Sim | `docs/value-assessment.md` (669 linhas, commit 01e76f7), §9 | Não é referenciada por `CONTRIBUTING.md` |
| Harness M4 | Só especificação | `docs/benchmark-harness.md`; `bench/` **não existe** | Nada implementado |

Fatos de CI (`ci.yml`): job `Tools and sanitizers` (`make test`, ASan/UBSan com clang, `make
fuzz FUZZ_TIME=30`, `scripts/check-site.py`, 10 min) e job `Native Linux VM` (`make vm-test`, 35
min, evidência anexada por 30 dias). `kasan.yml`: kernel 6.12.112 com SHA-256 verificado,
virtme-ng 1.41, 150 min, segunda-feira e manual.

Proteção do `main` (segundo `GOVERNANCE.md`; **a confirmar** em Settings > Branches): PR
obrigatório, checks `Tools and sanitizers` e `Native Linux VM`, branch atualizada, conversas
resolvidas, sem force-push, sem deleção, sem número mínimo de aprovações, administradores com
bypass. A história foi reescrita uma única vez (diário 0018), com autorização do usuário. Há 57
commits em todas as refs; `main` e `origin/main` existem.

Maturidade (fatos de `README.md`, `docs/research-originality.md` §0/§7 e
`docs/value-assessment.md` §9): um kernel testado (Debian 6.12.111, KASAN em 6.12.112), três
inferiores (ext4, btrfs,
XFS), índice só em RAM, sem números medidos, UAPI v1 sujeita a mudança, três defeitos de kernel
achados pelos próprios testes, um autor, nenhum usuário externo.

## 1. Ambiente de desenvolvimento reproduzível

**O que existe.** `make vm-test` (`vm/run.sh`) constrói a imagem Docker (base Debian trixie fixada por digest, kernel 6.12.111+deb13 fixado), compila módulos e boota QEMU x86-64 TCG com
timeout de 180 s, sem contêiner privilegiado e sem KVM. KASAN/UBSAN/lockdep usam o tarball
upstream `linux-6.12.112` com hash verificado e virtme-ng (`vm/kasan.config`,
`vm/kasan-guest.sh`).
O mesmo `vm/suite.sh` roda nos dois ambientes.

**O que falta**, em ordem de valor para um novo contribuidor:

1. Um documento de bootstrap de um comando (`docs/development.md`) com: requisitos do host
   (Docker; macOS Apple Silicon usa emulação e é lento; Linux x86-64 com `/dev/kvm` pode
   acelerar o
   KASAN, a confirmar se o alvo `vm-test` aproveita KVM, pois o texto atual diz TCG), espaço em disco, memória e rede para o primeiro download.
2. Tempos esperados medidos em pelo menos duas máquinas (Mac ARM, Linux x86). **A confirmar:**
   nenhum tempo está registrado fora do timeout de CI; o plano é medir e registrar, não estimar.
3. Como rodar um teste só. Hoje `vm/suite.sh` é sequencial. Proposta: variável de ambiente
   `MOOB_ONLY=<etapa>` ou funções nomeadas, via mudança pequena revisada (ver §5, item 5).
4. Como adicionar um teste: onde colocar o binário (`tests/*.c`), como `in-container.sh` o
   compila e copia, onde `suite.sh` o chama, qual marcador `*_PASS` imprime e como falha.
5. `make help` listando alvos; `make lint` (shellcheck, ver §4); `make check-spdx`.
6. Opcional, só com voluntário mantenedor: `.devcontainer/` reaproveitando `vm/Dockerfile`, ou
   `flake.nix`. Decisão do mantenedor.
7. Repetir no topo do guia a diretriz de `AGENTS.md`: nunca carregar o módulo no host; sempre VM
   descartável.

## 2. CONTRIBUTING, CODE_OF_CONDUCT e SECURITY

Os três arquivos existem. A proposta é **revisar**, não criar.

### 2.1 CONTRIBUTING.md (revisão)

Manter o que há e acrescentar, nesta ordem:
- Estágio experimental e o que não esperar (link para o banner, §9).
- Como começar (links para `docs/development.md`, `docs/testing.md`, a lista de issues
  `good-first-issue`).
- Estilo: C no estilo do kernel (tabulações, 80 colunas como guia, `checkpatch.pl` quando S5
  existir), limites e endianness explícitos, SPDX `GPL-2.0-only` em todo arquivo novo, nenhuma
  verificação desabilitada para passar o build.
- Testes: mudança em `layer/`, `include/moobfs_ioctl.h`, `common/` ou formato exige `make
  vm-test` verde e teste novo na VM; mudança em `layer/` pede também o workflow KASAN (`gh
  workflow
  run kasan.yml` na branch) com link da execução no PR.
- **Sem números de desempenho sem arquivo de evidência** em `build/bench/<data>/` ou anexo ao
  diário, com comando, kernel, hardware e repetições (M4, `docs/benchmark-plan.md`).
- Registro obrigatório: entrada em `docs/history/` e `CHANGELOG.md`; ADR para decisão
  arquitetural, sem editar ADRs antigas (apontar a substituta).
- Sem force-push em `main`; em branches de PR, rebase permitido antes da revisão e merge sem
  reescrever história depois que houver revisão.
- Linguagem (ver §4, item 12): decisão pendente.
- **DCO:** hoje o texto diz que não há DCO nem CLA. Proposta: adotar DCO (`Signed-off-by`, `git
  commit -s`) a partir de uma data, sem CLA, e verificar com uma GitHub Action. Justificativa:
  prática do kernel Linux e licença GPL-2.0-only. **Decisão do mantenedor**; a mudança não é retroativa e deve respeitar contribuições existentes (`GOVERNANCE.md`).
- Contribuições assistidas por IA: manter a seção atual.

### 2.2 CODE_OF_CONDUCT.md

Hoje é um texto próprio de 23 linhas, que declara não ser um covenant de terceiros. Proposta:
adotar **Contributor Covenant 2.1** (declarar versão e atribuição CC BY 4.0), porque é
reconhecido por contribuidores externos, mantendo o contato de denúncia. Pontos a decidir: (a)
contato de aplicação, hoje `claudio@studiomoob.com`, enquanto o e-mail do mantenedor no
ambiente é outro; **a confirmar** qual endereço publicar; (b) alternativa a um mantenedor único
(o texto atual remete a denúncia à plataforma GitHub quando o alvo é o mantenedor); (c) prazo
de resposta. Registrar a troca em diário e `CHANGELOG.md`.

### 2.3 SECURITY.md

Estrutura proposta: escopo; canais (GitHub Security Advisories primeiro, e-mail como
alternativa); o que enviar; metas de resposta; embargo; CVE; crédito; limites atuais. Detalhes
em §7.
Manter a seção "Current boundaries" existente e ampliá-la com o modelo de ameaça quando S1 existir.

## 3. Organização de issues

**Labels** (criar via `gh label create`, listar em `docs/labels.md` ou em `CONTRIBUTING.md`):

| Grupo | Labels |
| --- | --- |
| Área | `kernel-layer`, `index-query`, `tools`, `tests-vm`, `kasan`, `docs`, `site`, `bench-m4`, `security` |
| Dificuldade | `good-first-issue`, `intermediate`, `kernel-expert` |
| Prioridade | `p0` (perda de dados, crash, vazamento), `p1`, `p2` |
| Estado | `needs-evidence`, `blocked-on-m4`, `needs-adr`, `needs-triage` |

Regras: toda issue de desempenho nasce com `needs-evidence` e `bench-m4`; `blocked-on-m4` marca
decisões que `docs/distribution-strategy.md` §8 e `docs/value-assessment.md` §9 condicionam ao
M4. Existe hoje o label `bug` (aplicado por `bug_report.yml`).

**Templates** (em `.github/ISSUE_TEMPLATE/`):
- `bug_report.yml` (revisar): commit; versão do kernel (`uname -r`); host ou VM; **sistema de
  arquivos inferior** e opções de montagem (`lower=`, `nolinks`, `index_budget=`); `dmesg` com
  BUG/WARNING/KASAN; passos mínimos; se rodou no harness (`make vm-test`) ou fora.
- `test_failure.yml` (novo): qual alvo, marcador esperado e obtido, trecho de
  `build/vm/console.log`, se reproduz em segundo run, host e Docker.
- `proposal.yml` (existe): acrescentar campo "ADR relacionada" (link ou "nova ADR necessária") e
  "impacto em UAPI ou formato".
- `review_request.yml` (novo, para os pedidos de §8, uso de mantenedor).
- Segurança: **sem template público**. `config.yml` já aponta `mailto:`; trocar/ampliar para o
  link de Security Advisories (`/security/advisories/new`) e deixar `blank_issues_enabled:
  false`.

## 4. Contribuições iniciais sem kernel

Candidatos concretos, todos verificáveis no host ou em CI comum. Cada um vira issue com label de
área, critério de pronto e arquivos tocados. Nenhum exige carregar módulo.

1. **Lacunas de `docs/testing.md`**: tabela de marcadores `*_PASS`, tempos, como ler
   `build/vm/*.log` (§5). `docs`, `good-first-issue`.
2. **`moobfs-attr` e `moobfs-check`**: mensagens de erro e códigos de saída documentados,
   `--help` consistente. Arquivos `tools/moobfs-attr.c`, `tools/moobfs-check.c`. `tools`.
3. **Saída JSON** opcional em `moobfs-check` e `moobfs-index` (hoje `fsck.moobfs` já emite
   JSON), com esquema documentado e teste em `tests/`. `tools`, `intermediate`.
4. **Páginas de manual** (`man/moobfs-attr.1`, `moobfs-check.1`, `moobfs-index.1`,
   `mkfs.moobfs.8`), verificadas com `mandoc -Tlint`. `docs`, `good-first-issue`.
5. **Corpus de fuzz** para `tests/fuzz_attr.c`: hoje `tests/fuzz-corpus/attr` tem poucos
   arquivos; gerar sementes por tipo de atributo e limites (comprimento 0, máximo, UTF-8
   inválido),
   minimizar com `-merge=1`. `tests-vm`/`tools`, `intermediate`.
6. **Peças do harness `bench/`** conforme `docs/benchmark-harness.md`: `bench/gen` (dataset
   determinístico a partir de semente) e `bench/report` (p50/p95/p99, CSV/JSON) em C ou Python;
   ambos
   rodam no host sem módulo. `bench-m4`, `intermediate`. **Nenhum número publicado** até haver execução em VM com evidência.
7. **`bench/equiv`** (comparar `QUERY`, SQLite e oráculo): contra o oráculo, que já existe;
   parte depende de VM. `bench-m4`, `intermediate`.
8. **`site/plan.html` e demais páginas**: auditoria de acessibilidade (contraste, foco, `lang`,
   landmarks, texto alternativo do diagrama), `scripts/check-site.py` estendido. `site`,
   `good-first-issue`.
9. **`moobfs-index` (oráculo)**: mensagens de erro, tratamento de árvores grandes, testes de
   predicados limite. É a referência de corretude do `QUERY`; qualquer mudança exige comparação
   com
   o kernel na VM (então `intermediate`).
10. **Matriz de avisos de compilador no CI**: gcc e clang, versões distintas, `-Wall -Wextra
    -Wconversion -Wshadow -Werror` já usados em `Makefile` para ferramentas. `tools`,
    `good-first-issue`.
11. **`shellcheck`** em `vm/*.sh`, `vm/init`, `scripts/`: corrigir ou suprimir com
    justificativa; novo passo no job `Tools and sanitizers`. `tests-vm`, `good-first-issue`.
12. **Reprodutibilidade dos hashes de kernel**: script que verifica o digest da imagem Debian, o
    pacote 6.12.111 e o SHA-256 do `linux-6.12.112` (hoje em `kasan.yml`); documentar como
    atualizar em conjunto (como `docs/testing.md` já manda). `docs`/`tests-vm`.
13. **Verificação de SPDX** (`scripts/check-spdx.py`) em fontes `.c/.h/.py/.sh`. `tools`,
    `good-first-issue`.
14. **Tradução e política de idioma.** O README tem EN e PT-BR e o site está em inglês, mas
    `docs/` (specs, ADRs, diário, roadmap de segurança) está em português, e o público-alvo é
    internacional. Propor: (a) seção "At a glance" em inglês no README, com estágio, limites e como validar; (b) política: código, commits, issues e PRs em inglês ou português; documentos
    normativos novos (ADRs, specs) em inglês ou bilíngues a partir de uma data; diário permanece no idioma do autor; (c) traduções de `docs/testing.md`, `security-roadmap.md` e `spec-m3.md`
    primeiro, sinalizando "tradução de cortesia, o original prevalece". **Decisão do mantenedor.**

## 5. Documentar a execução dos testes em VM

Reestruturar `docs/testing.md` (preservando o conteúdo atual, movido) em:

1. **Pré-requisitos**: Docker, plataforma, espaço, rede; aviso para nunca usar `insmod` no host.
2. **Alvos do Makefile**: tabela `make test` / `make fuzz` / `make attr-tools` / `make module` /
   `make layer` / `make vm-test`, o que cada um compila e roda, onde escreve (`build/`), tempo
   observado (a medir).
3. **Marcadores e significado**: `MOOBFS_VM_PASS`, `SHARED:`, `MOUNTED_READ_TESTS_PASS`,
   `WRITABLE_MOUNT_REJECTED`, `LAYER_LOWER_PASS: <fs>`, `ISOLATION_PASS: <fs>` (`suite.sh`),
   `ACCEPTANCE_CLIENTS_PASS`, `POWER_LOSS_REPLAY_PASS`, `IO_ERROR_INJECTION_PASS`, `MOOBFS_ACCEPTANCE_PASS`, `MOOBFS_LAYER_PASS`, `MOOBFS_KASAN_PASS`; e `MOOBFS_VM_FAIL:`. Para cada um: o
   que prova e o que **não** prova.
4. **Ler os logs**: `build/vm/console.log`, `environment.txt`, `kernel.config`,
   `image-report.json`; em KASAN, `build/vng/dmesg.txt` e o padrão que reprova (`BUG: KASAN`,
   `WARNING:`,
   lockdep, UBSAN). Como distinguir uma rejeição **esperada** (imagem malformada, escrita em M1) de uma falha.
5. **Rodar um teste isolado**: ver §1, item 3.
6. **KASAN localmente**: replicar os passos de `kasan.yml` com `vng` (virtme-ng 1.41), comando
   por comando, com espaço e tempo esperados, e como baixar o artefato do workflow no lugar de
   rodar local.
7. **Adicionar um inferior** (por exemplo F2FS): `vm/Dockerfile` (pacotes), `in-container.sh`
   (módulos e imagem), `suite.sh` (`run_lower <fs> <dev> <reflink>`), `kasan.config`,
   `kasan-guest.sh` (disco extra) e `kasan.yml`; avisar que cada inferior novo exige registro e não amplia o escopo sem ADR.
8. **O que não é coberto**: xfstests completo, syzkaller, outros kernels (S2, S3, §0).

## 6. Critérios de revisão e aceitação de PRs

Checklist a anexar ao template de PR e a `CONTRIBUTING.md`:

- [ ] O escopo corresponde a uma ADR aceita ou a uma issue discutida; formato ou UAPI sem issue
  prévia não é aceito.
- [ ] Testes adicionados na VM (`vm/suite.sh` ou `tests/`) para mudança de comportamento;
  entradas inválidas incluídas quando há parser ou ioctl.
- [ ] CI verde, incluindo `Tools and sanitizers` e `Native Linux VM`.
- [ ] Para `layer/`: execução do workflow KASAN na branch, com link, sem `BUG`, `WARNING`,
  lockdep ou UBSAN.
- [ ] Entrada em `docs/history/` (objetivo, decisões, testes realmente executados, limitações,
  próximo passo) e linha em `CHANGELOG.md`.
- [ ] Nenhuma alegação sem evidência: nada de desempenho, segurança, "testado no Linux" ou
  "pronto para produção"; conferir `docs/research-originality.md` §7.
- [ ] Mudança de UAPI (`include/moobfs_ioctl.h`): especificação, incremento de versão, testes de
  entradas inválidas e decisão explícita de compatibilidade.
- [ ] Mudança de formato (M1 em `docs/format-m1.md` ou atributos): decisão de compatibilidade (ADR).
- [ ] SPDX; sem credenciais, sem memórias reais; declaração de DCO (se adotado).
- [ ] Declaração de assistência de IA e o que foi revisado por humano.

Política de revisão:
- **Duas revisões** para `layer/`, `include/` e `common/` enquanto houver mantenedor único? Isso
  é impossível hoje (um mantenedor e administrador com bypass). Proposta honesta: exigir uma
  aprovação do mantenedor mais **uma revisão externa registrada** (comentário no PR, com nome) para mudanças em `layer/`; ativar a regra de duas aprovações quando existir um segundo
  mantenedor. Decisão pendente.
- **Squash**: merge por squash com mensagem final escrita pelo mantenedor, para manter `main`
  legível; commits intermediários continuam no PR. Alternativa: merge com histórico quando a
  série
  for bisectável. A definir.
- **Mensagem de commit**: assunto imperativo curto com prefixo de área (`layer:`, `tests:`,
  `docs:`, `tools:`, `vm:`, `ci:`), linha em branco, o porquê, `Signed-off-by` se DCO,
  referência a
  issue/ADR/diário.
- Reverter é preferível a reescrever; sem force-push em `main` (única exceção histórica: 0018).

## 7. Comunicação responsável de vulnerabilidades

Conteúdo proposto de `SECURITY.md`:

- **Canal:** GitHub Security Advisories (`security/advisories/new`) como principal; e-mail como
  alternativa (endereço **a confirmar**); nunca issue pública.
- **Escopo dentro:** módulo `layer/moobfs_layer.ko` e `kernel/moobfs.ko`, UAPI (`QUERY`,
  `GET_INFO`), `common/` (parser de atributos), ferramentas em `tools/`, scripts do harness em
  `vm/`
  quando o defeito afeta quem executa o harness (por exemplo, escrita fora de `build/`).
- **Escopo fora:** defeitos nos sistemas de arquivos inferiores (ext4, btrfs, XFS), no kernel
  Debian ou no QEMU sem relação com a camada; reporte aos respectivos projetos. Ataques que
  exigem
  `CAP_SYS_ADMIN` para montar não são escalada (a montagem já exige isso; S1).
- **Contam como vulnerabilidade:** crash, travamento ou corrupção disparável por usuário sem
  privilégio (negação de serviço inclusive, conforme `security-roadmap.md`), vazamento entre
  usuários via `QUERY`, escrita fora dos limites, uso após liberação, perda silenciosa de dados.
- **Metas de resposta** (metas, não garantias, dado um mantenedor): confirmação em 7 dias,
  triagem em 14, correção ou plano em 90, divulgação coordenada ao final, ou antes se o relator
  concordar. **Valores a confirmar** pelo mantenedor; o texto atual diz não prometer prazo.
- **CVE:** solicitar via GitHub (CNA do GitHub) quando houver impacto em quem roda o módulo; sem
  expectativa de CVE para código só de teste. Em achados no próprio kernel, acionar a lista de
  segurança do kernel conforme `Documentation/process/security-bugs`.
- **Embargo:** privado até a correção estar em `main`; relator e mantenedor combinam a data.
- **Crédito:** no advisory, no diário e em `CHANGELOG.md`, com consentimento.
- **Sem recompensa financeira (bug bounty).**
- Aviso: não é um release suportado; correções só em `main`.

## 8. Incentivar revisões independentes

Abrir issues de "pedido de revisão" (label `security` ou `kernel-expert`), cada uma com um
pacote para o revisor e uma pergunta delimitada. Nenhuma sugere que a camada foi auditada.

| Pedido | Pergunta | Artefatos |
| --- | --- | --- |
| Modelo de ameaça (S1) | O modelo cobre o adversário certo? `FS_USERNS_MOUNT` desligado, `override_creds`, `nop_mnt_idmap`, autorização no `QUERY` | `docs/security-roadmap.md`, diário 0020, `layer/main.c`, `layer/index.c`, `tests/idmap.c` |
| Concorrência | `rwsem` do índice, chunking do `QUERY`, `rename` e `paths_stale`, corrida com chmod/rename/unlink (commit `c933a74`) | `layer/index.c`, `tests/query.c`, logs KASAN do workflow |
| Integridade | Cenários do `dm-log-writes` e do `dm-flakey`: o que falta (barreiras, rename, fsync de diretório) | `vm/suite.sh`, diário 0015 |
| UAPI | Struct, flags, reservados, `copy_to_user`, `open_by_handle_at` devolvido pelo `QUERY` (S11) | `include/moobfs_ioctl.h`, `docs/spec-m3.md` |
| Parser de atributos | Limites, UTF-8, alfabeto de nomes | `common/attr.c`, `tests/fuzz_attr.c` |

O revisor recebe: a seção da especificação, o comando para reproduzir, os logs do `make
vm-test`, o artefato do workflow KASAN da revisão exata (commit) e a lista de limitações
conhecidas
(TOCTOU, falso negativo após rename de diretório, segundo `research-originality.md` §7).
Registro de achados: issue (ou advisory, se for vulnerabilidade) mais entrada de diário; se o
achado
muda o desenho, ADR nova que aponta para a anterior; crédito ao revisor. Não registrar
"revisado" como aprovação: um comentário não prova segurança.

## 9. Estágio experimental e limitações

Texto proposto para o topo de `README.md` (e equivalente em PT-BR e no `site/index.html`):

> **Experimental research project. Not for production use.** MoobFS is a native Linux stacked
> filesystem layer (kernel module over ext4, btrfs or XFS) for storing and querying LLM-agent memory
> as typed extended attributes. Current limitations, all verified in the repository:
> - tested on **one kernel** (Debian 6.12.111; a KASAN/lockdep kernel 6.12.112 on demand) in a VM;
> - the attribute index lives **only in RAM** and is rebuilt at mount;
> - mounting requires **root** in the initial namespace; user namespaces and idmapped mounts are rejected;
> - **no performance numbers have been measured**; no speed or efficiency claim is made;
> - the **UAPI v1 may change**; there is no stable release or compatibility promise;
> - our own tests found **three kernel bugs** during development; there has been **no external audit**;
> - reflink, deduplication and durability come from the lower filesystem, not from MoobFS.

A página `site/index.html` mostra o mesmo bloco acima da dobra. Verificar cada linha contra
`README.md`, `docs/research-originality.md` §0 e §7 e `docs/value-assessment.md` §9 antes de
publicar; "três defeitos" vem de §9.1 e **a confirmar** contra os diários 0011 a 0015.

## 10. Roadmap público

Reestruturar `ROADMAP.md` em três estados, cada item com link para diário ou especificação:

- **Concluído com evidência:** M0 (0001-0003); M1 somente leitura (0002, `docs/format-m1.md`);
  M2 fases 2a-2e (0011-0015, `docs/spec-m2.md`); harness KASAN (0016); fase 3b parcial (0020:
  autorização no `QUERY`, idmap e userns rejeitados, corrida com chmod/rename/unlink). Cada linha cita o que **foi** testado e o que não foi.
- **Planejado:** restante do M3 (`docs/spec-m3.md`); ADR de ameaça S1; S5 lint; S6
  (`SECURITY.md` revisado); fstests (S2); S4 (inferior hostil). Sem datas.
- **Dependente de validação (M4 ou segurança):** `QUERY_BATCH`, `READ_BATCH` e persistência do
  índice (condicionados ao M4); benchmark (M4, `docs/benchmark-plan.md` e
  `benchmark-harness.md`); DKMS, `.deb` e etapa 3 pública (`distribution-strategy.md` §8); syzkaller (S3); fuzzing contínuo (S7); hardening (S8); limites (S9); assinatura (S10).

Mapa de fases para itens de segurança: M2 fecha com S-itens parciais da tabela de estado; M3
depende de S1 e S11; M4 não depende de S-itens, mas a versão pública depende de S1-S6 e S8-S10.
Cada item recebe um rótulo "proposta", "implementado", "testado (kernel, inferior)", como
`AGENTS.md` pede.

## 11. Sequência priorizada de ações

Esforço: S (horas), M (1 a 3 dias), L (semana ou mais). "Decisão" marca o que depende do mantenedor.

| # | Ação | Esforço | Depende de | Decisão |
| --- | --- | --- | --- | --- |
| 1 | Revisar e commitar este plano, com diário e `CHANGELOG.md` | S | revisão do mantenedor | não |
| 2 | Confirmar contatos (conduta e segurança) e se o Security Advisories está ativo | S | 0 | **sim** (e-mail) |
| 3 | Confirmar a licença: `LICENSE` GPL v2, SPDX `GPL-2.0-only`; avaliar "or later" fora de escopo | S | 0 | **sim** (confirmar) |
| 4 | Banner de estágio experimental no README, PT-BR e site (§9) | S | 1 | não |
| 5 | Criar labels e `docs/labels.md` | S | 0 | não |
| 6 | Revisar `SECURITY.md` (§7), metas de resposta | S | 2 | **sim** (prazos) |
| 7 | Decidir DCO vs sem DCO; atualizar `CONTRIBUTING.md` e template de PR | S | 0 | **sim** |
| 8 | Decidir Contributor Covenant 2.1 e contato de aplicação | S | 2 | **sim** |
| 9 | Política de idioma e seção "At a glance" em inglês | M | 0 | **sim** |
| 10 | Templates de issue novos (§3) | S | 5 | não |
| 11 | `docs/development.md` e reestruturar `docs/testing.md` (§1, §5); medir tempos | M | 0 | não |
| 12 | `ROADMAP.md` em três estados (§10) | S | 1 | não |
| 13 | Abrir 10 a 15 issues iniciais (§4) com labels | M | 5, 10 | não |
| 14 | `shellcheck` e `check-spdx` no CI | S | 13 | não |
| 15 | Abrir pedidos de revisão (§8) após ADR S1 | M | S1 | não |
| 16 | ADR de modelo de ameaça (S1) | M | 0020 | **sim** (ADR) |
| 17 | `MOOB_ONLY` ou execução isolada em `vm/suite.sh` (altera código; exige VM e KASAN) | M | 11 | não |
| 18 | KASAN como check obrigatório ou por PR de `layer/` | M | custo de CI (150 min) | **sim** |

As ações 1 a 14 não tocam código do kernel. A 17 e a 18 tocam harness e CI e seguem as regras de §6.

## 12. Arquivos a criar ou modificar

Este documento cria apenas `docs/community-plan.md`. Se o plano for aprovado, os arquivos abaixo
seriam alterados em PRs separados, cada um com diário e `CHANGELOG.md`.

| Arquivo | Ação |
| --- | --- |
| `README.md`, `README.pt-BR.md`, `site/index.html` | banner experimental; seção "At a glance" em inglês |
| `ROADMAP.md` | três estados com links |
| `CONTRIBUTING.md`, `.github/pull_request_template.md` | checklist de §6, DCO (se aprovado), KASAN, evidência |
| `CODE_OF_CONDUCT.md` | Contributor Covenant 2.1 (se aprovado) |
| `SECURITY.md` | escopo, metas, CVE, embargo, crédito |
| `.github/ISSUE_TEMPLATE/bug_report.yml`, `proposal.yml`, `config.yml` | campos de kernel/inferior/dmesg; ADR; link de advisory |
| `.github/ISSUE_TEMPLATE/test_failure.yml`, `review_request.yml` | novos |
| `.github/CODEOWNERS` | novo (quando houver mais de um mantenedor) |
| `.github/workflows/ci.yml` | `shellcheck`, `check-spdx`, matriz de avisos, DCO (se aprovado) |
| `docs/development.md` (novo), `docs/testing.md` | bootstrap, estrutura de §5 |
| `docs/labels.md` (novo) | labels e uso |
| `docs/decisions/0008-*.md` (novo, ID a confirmar) | modelo de ameaça (S1) |
| `docs/history/NNNN-*.md`, `docs/history/README.md`, `CHANGELOG.md` | registros de cada etapa |
| `vm/suite.sh`, `Makefile` | execução isolada, `help`, `lint` (código; exige VM) |
| `scripts/check-spdx.py` (novo), `man/*` (novos) | tarefas iniciais |
| `.devcontainer/` ou `flake.nix` | opcional, a decidir |

Limitações: o conteúdo de campos dos templates, a configuração real do GitHub (labels, proteção
do `main`, advisories, Pages) e tempos de execução não foram verificados; todos estão marcados
"a confirmar".

## 13. Decisões do mantenedor (2026-10-10)

| Tema | Decisão |
| --- | --- |
| Contato de segurança e conduta | `claudio@studiomoob.com`, mais o canal privado do GitHub Security Advisories |
| DCO | Adotado: `Signed-off-by` obrigatório em cada commit, verificado no CI |
| Código de conduta | Contributor Covenant 2.1, com a fonte citada |
| Idioma | Inglês em código, commits, issues, PRs e README; documentação em `docs/` aceita em português ou inglês; ADRs e especificações traduzidos sob demanda externa |
| Prazos em `SECURITY.md` | Confirmação em 7 dias, avaliação em 30 dias, atualização ao relator a cada 30 dias; correção sem prazo fixo |
| KASAN | Obrigatório em pull requests que alterem `layer/` ou `vm/`; semanal e sob demanda para o resto |
| Licença | Confirmada GPL-2.0-only |
| Revisão de `layer/` | Um mantenedor mais uma revisão externa registrada, até haver segundo mantenedor |
| Execução | Ações 1 a 14 aprovadas; 15 a 18 após o merge da fase 3b |
