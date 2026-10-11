# MoobFS

**Filesystem nativo experimental para Linux, voltado à memória e ao contexto de LLMs.**

Módulo experimental do kernel Linux (GPL-2.0, sem FUSE): uma camada de filesystem empilhada
sobre ext4, btrfs ou XFS que acrescenta atributos tipados em xattr, um índice em memória e um
ioctl `QUERY` autorizado pelas credenciais POSIX de quem chama. Pensado como armazenamento para
memória de agentes de LLM. Projeto de pesquisa, não para produção. Sem versão lançada.

[English](README.md) · [Site](https://studiomoob.github.io/llmoobfs/) · [FAQ (EN)](docs/faq.md) · [Contribuição](CONTRIBUTING.md) · [Roadmap](ROADMAP.md)

> **Estágio experimental. Sem versão lançada; não use em produção.** Fatos verificados neste
> repositório em 2026-10-10:
> - testado em **uma só série de kernel**, em VM: Debian 6.12.111 no QEMU (`make vm-test`) e
>   upstream 6.12.112 sob KASAN/UBSAN/lockdep (`kasan.yml`); apenas sobre ext4, btrfs e XFS;
> - o índice de atributos vive **somente em RAM**, limitado por `index_budget`, e é reconstruído
>   a cada montagem;
> - montar exige **`CAP_SYS_ADMIN` no namespace de usuário inicial**; montagens em user
>   namespace e montagens idmapped são recusadas;
> - a **UAPI v1** (`QUERY`, `GET_INFO`) pode mudar; não há promessa de compatibilidade;
> - **nenhum número de desempenho foi medido** (M4 pendente); nenhuma alegação de velocidade ou
>   eficiência é feita;
> - os próprios testes do projeto encontraram e corrigiram **três defeitos no lado do kernel**
>   durante o desenvolvimento (registros [0015](docs/history/0015-phase-2e-investigation.md) e
>   [0016](docs/history/0016-kasan-lockdep-harness.md)); **não houve auditoria externa**;
> - reflink, deduplicação e durabilidade vêm do filesystem inferior, não do MoobFS.
>
> Contribuições são bem-vindas: veja [CONTRIBUTING.md](CONTRIBUTING.md). Política de idioma:
> código, commits, issues, pull requests e o README principal em inglês; documentos em `docs/`
> podem estar em português ou inglês, com ADRs e especificações traduzidos sob demanda.

## Em resumo

**O que é.** Um módulo nativo do kernel Linux, `moobfs_layer.ko`, montado como camada
empilhada sobre um diretório de um filesystem maduro (ext4, btrfs ou XFS). Memórias são
arquivos comuns; seus metadados são atributos estendidos tipados (`user.moob.*`) que o módulo
valida na escrita e indexa em RAM. Um ioctl `QUERY` na raiz da montagem responde predicados
conjuntivos sobre atributos a partir do kernel, devolvendo só as entradas que quem chama
poderia abrir pelas permissões POSIX e ACLs. Tudo continua legível com ferramentas Unix comuns
sem a camada. O formato de imagem somente leitura do M1, com conteúdo físico compartilhado, é
mantido como regressão.

**O que não é.** Não é FUSE nem daemon: não há servidor em espaço de usuário, socket ou
processo em segundo plano. Não há busca semântica ou vetorial no kernel: ranking, embeddings e
montagem de contexto são do cliente. Não substitui banco de dados; para qualquer uso além de
uma VM descartável, hoje, SQLite ou arquivos comuns continuam sendo a recomendação
([avaliação de valor](docs/value-assessment.md)). Não há persistência do índice, snapshots nem
transações próprias; durabilidade, reflink e dedup são do filesystem inferior.

**Como verificar qualquer afirmação daqui.** Rode `make vm-test` e leia `build/vm/console.log`
([guia de testes](docs/testing.md)); prepare o ambiente com [docs/development.md](docs/development.md).

## Objetivo do projeto

O MoobFS busca oferecer uma base de armazenamento, no nível de filesystem, para memória
persistente e contexto consumidos por agentes baseados em LLMs: preservar memórias como
observações, decisões e registros de tarefas entre sessões, compartilhar conteúdo repetido e,
futuramente, manter revisões que os agentes possam recuperar para compor seu contexto.

O filesystem armazena e expõe as memórias como arquivos. A aplicação do agente seleciona o
material relevante e monta o contexto enviado à LLM. Recuperação semântica e montagem de
contexto ainda precisam ser desenhadas e integradas; o MoobFS não fornece essas capacidades
hoje nem amplia a janela de contexto do modelo.

## Como funciona

Escrever uma memória é um `write` POSIX comum mais `setxattr`; a camada valida a codificação
(byte de tipo mais carga) antes de o xattr chegar ao inferior e atualiza o índice. Encontrar
memórias é uma chamada `QUERY`: até quatro predicados sobre atributos em conjunção,
respondidos pelo índice, cada resultado verificado contra as permissões POSIX e ACLs de quem
chama antes de ser devolvido; o cliente então abre o arquivo normalmente. Desmontada a camada,
o diretório inferior é uma árvore comum que `grep`, `getfattr`, `git` e `rsync` leem sem o
módulo. Detalhes com citações do código (em inglês):
[visão da arquitetura](docs/architecture-overview.md); o fluxo em diagrama:
[site/plan.html](https://studiomoob.github.io/llmoobfs/plan.html#flow).

## O que está implementado, com evidência

"Testado em" nomeia o kernel e os inferiores que de fato rodaram; nada aqui significa prontidão
para produção. Desde a [ADR-0007](docs/decisions/0007-stacked-memory-layer.md) o MoobFS é uma
camada empilhada nativa sobre um filesystem maduro; o experimento M1 está congelado e roda como
regressão na mesma VM.

| Item | Registro | Testado em |
| --- | --- | --- |
| M1, imagem somente leitura: criador offline, verificador, módulo nativo, dois inodes compartilhando um conteúdo (FIEMAP), 15 imagens malformadas rejeitadas | [0002](docs/history/0002-native-read-milestone.md), [formato](docs/format-m1.md) | Debian 6.12.111 (QEMU), loop |
| Fases 2a–2b: passagem empilhada (`mount -t moobfs_layer -o lower=<dir>[,nolinks]`): arquivos, diretórios, rename, links, xattr, mmap, fsync; reflink/dedup/`copy_file_range` delegados, FIEMAP; `fsx` | [0011](docs/history/0011-phase-2a-stacked-passthrough.md), [0012](docs/history/0012-phase-2b-full-passthrough.md) | Debian 6.12.111; ext4, btrfs, XFS |
| Fase 2c: atributos tipados `user.moob.*` validados no kernel; `moobfs-attr`, `moobfs-check`; testes unitários no host; alvo libFuzzer | [0013](docs/history/0013-phase-2c-typed-attributes.md) | Testes no host (Linux/macOS), VM sobre os três inferiores |
| Fase 2d: índice em memória, `QUERY` e `GET_INFO` (UAPI v1), respostas iguais ao oráculo `moobfs-index`, inclusive após rename, unlink, removexattr e remontagem; `noindex`, `index_budget` | [0014](docs/history/0014-phase-2d-index-query.md) | Debian 6.12.111; ext4, btrfs, XFS |
| Fase 2e: aceitação com git, SQLite em WAL concorrente, cliente de memória próprio; replay de queda de energia com `dm-log-writes`; erros de E/S com `dm-flakey`; dois defeitos da camada encontrados e corrigidos | [0015](docs/history/0015-phase-2e-investigation.md) | Debian 6.12.111; só ext4 |
| Harness KASAN/UBSAN/lockdep com a mesma suíte; terceiro defeito encontrado e corrigido | [0016](docs/history/0016-kasan-lockdep-harness.md) | Upstream 6.12.112, virtme-ng |
| M3 fase 3b: autorização POSIX por resultado no `QUERY`; dois uids, dois namespaces, chmod e ACLs, remontagem, montagens idmapped e em user namespace recusadas, corrida de 5 s de chmod/rename/unlink contra `QUERY` | [0020](docs/history/0020-phase-3b-query-authorization.md), [spec-m3](docs/spec-m3.md) §3 | Debian 6.12.111 e KASAN 6.12.112; ext4, btrfs, XFS |

Não implementado: índice persistente, montagens em user namespace ou idmapped, leituras em
lote, cursor de mudanças, revisões, empacotamento, UAPI estável, CLI, xfstests completo e
qualquer medição (M4). Veja o [ROADMAP.md](ROADMAP.md).

## Limitações

- Uma série de kernel testada (Debian 6.12.111; upstream 6.12.112 sob KASAN), x86-64, em VM;
  APIs do kernel mudam entre versões e o alvo fixado não é promessa de compatibilidade.
- Índice só em RAM, não paginável, limitado por `index_budget` (padrão 128 MiB), reconstruído
  por varredura linear a cada montagem; comportamento perto da saturação não testado.
- Montar exige `CAP_SYS_ADMIN` no namespace inicial; sem contêineres rootless.
- A UAPI v1 pode mudar; a tag `v0.1.0-experimental` é um instantâneo do fonte, sem binários, empacotamento ou suporte.
- Nenhum número de desempenho, latência ou memória; nenhuma comparação com SQLite, FUSE ou o
  inferior puro pode ser afirmada antes do M4.
- Sem auditoria externa, syzkaller, xfstests completo ou documento de modelo de ameaça.
- A autorização do `QUERY` vale no instante da consulta (contrato TOCTOU); possíveis falsos
  negativos após renomear diretório, até remontar ([spec-m3](docs/spec-m3.md) §3.1).
- Atomicidade arquivo × metadados é convenção do cliente; durabilidade, reflink e dedup são do
  inferior. A aceitação rodou só sobre ext4.

## Comparado com arquivos comuns, SQLite e frameworks de memória

**Arquivos comuns.** Mesmo modelo de dados, sem módulo nem root, portabilidade total; a
consulta é `find` mais `getfattr`, varredura linear. Se consultas por atributo são raras,
arquivos ganham.

**SQLite ao lado dos arquivos.** SQL completo, índices compostos, décadas de maturidade, falhas
contidas no processo, transações atômicas dentro do banco; a aplicação faz a autorização e
mantém arquivos e linhas consistentes. Hoje é a recomendação fora de laboratório.

**Frameworks de memória (mem0, Letta, ai-memory).** Decidem o que lembrar e como montar o
prompt; o MoobFS guarda arquivos e responde predicados. São clientes potenciais sobre a
camada, não concorrentes; o plano do M4 os usa como cargas.

**MoobFS.** Um só armazenamento, autorização por resultado feita pelo SO, sem daemon, dados
legíveis sem o módulo; em troca, módulo de kernel de um autor, um kernel testado, índice só em
RAM e nenhuma vantagem medida. Tabela completa e condições em que pode valer avaliar:
[when-not-to-use.md](docs/when-not-to-use.md) (EN) e [value-assessment.md](docs/value-assessment.md).

## Experimentar em VM

Com Docker em execução, a partir da raiz do repositório:

```sh
git clone https://github.com/studiomoob/llmoobfs.git
cd llmoobfs
make vm-test
```

Compila as ferramentas e os dois módulos e inicia uma VM QEMU x86-64 com o kernel Debian
**6.12.111+deb13-amd64**. Não carrega módulos no host nem exige container privilegiado; nunca
carregue os módulos no seu host. No Apple Silicon usa emulação. Ambiente em
[docs/development.md](docs/development.md); o que cada marcador prova e não prova em
[docs/testing.md](docs/testing.md). Uma execução bem-sucedida termina com `MOOBFS_VM_PASS`,
`LAYER_LOWER_PASS` para ext4, btrfs e xfs, `MOOBFS_ACCEPTANCE_PASS` e `MOOBFS_LAYER_PASS`
(texto exato no [README em inglês](README.md#try-it-in-a-vm)).

### Ferramentas de imagem do M1 (regressão)

As ferramentas precisam de compilador C11, Make e Python 3 no Linux ou macOS:

```sh
make test
printf 'Uma memória compartilhada.\n' > build/memory.txt
build/host/mkfs.moobfs build/demo.img alpha.txt=build/memory.txt beta.txt=build/memory.txt
build/host/fsck.moobfs build/demo.img
```

O relatório JSON mostra inodes distintos, mesmo objeto e mesmo offset. A criação recusa
sobrescrita; o verificador não repara imagens. A suíte da VM passa com 9 testes de
ferramentas, sanitizers e montagem real: inodes 2/3, offset físico 12288, extent de 16384
bytes, conteúdo lógico de 12562 bytes; testa leitura, EOF, seek, FIEMAP, remontagem, rejeição
de escrita e 15 imagens malformadas. Limites do M1: diretório plano, até 128 arquivos/objetos,
até 64 KiB por objeto, nomes ASCII imprimível de até 63 bytes; a imagem deve permanecer
imutável durante a montagem; compartilhar disco não garante cache comum. O módulo M1 não rodou
sob KASAN (o harness KASAN roda a suíte da camada) e não há benchmark.

## Mapa da documentação

- [Visão da arquitetura](docs/architecture-overview.md), [comparação e quando não usar](docs/when-not-to-use.md) e [FAQ](docs/faq.md), em inglês.
- [Especificação M2](docs/spec-m2.md), [infraestrutura do kernel](docs/kernel-infrastructure.md), [ADR-0007](docs/decisions/0007-stacked-memory-layer.md); [especificação M3](docs/spec-m3.md); [plano](docs/benchmark-plan.md) e [harness](docs/benchmark-harness.md) do benchmark M4.
- [Formato implementado do M1](docs/format-m1.md) e [proposta original do MVP](docs/native-mvp.md) (interface de escrita substituída pela ADR-0007).
- [Decisões](docs/decisions/), [histórico](docs/history/), [levantamento do ecossistema](docs/ecosystem.md), [anterioridade](docs/research-originality.md) e [changelog](CHANGELOG.md).
- [Regras para Codex, Claude e Cursor](docs/ai-workflow.md).
- [Contribuição](CONTRIBUTING.md), [governança](GOVERNANCE.md), [conduta](CODE_OF_CONDUCT.md) e [segurança](SECURITY.md).

## Anterioridade e posicionamento

O MoobFS se apoia em precedentes que reconhece: o Semantic File System (1991) e o Be File
System (1997) para consultas por atributos, overlayfs e ecryptfs para o modelo empilhado, e o
YoloFS (2026), que já demonstra um módulo de kernel empilhado voltado a agentes. O projeto não
reivindica nenhuma dessas ideias como inédita; o que explora é a combinação delas com
autorização POSIX por resultado numa consulta do kernel, e se isso tem valor mensurável
([research-originality.md](docs/research-originality.md) §4, §5, §9).

## Contribuição e segurança

Relatos reproduzíveis, revisão de formato e de kernel e melhorias de reprodutibilidade são
bem-vindos. Leia [CONTRIBUTING.md](CONTRIBUTING.md) (DCO, requisitos de teste, regras de
revisão). Perguntas vão para o GitHub Discussions ou uma issue. Não coloque detalhes privados
de vulnerabilidade em issues públicas; use o [contato de segurança](SECURITY.md).

## Licença

Todo o projeto usa **GPL-2.0-only**, conforme [LICENSE](LICENSE).
