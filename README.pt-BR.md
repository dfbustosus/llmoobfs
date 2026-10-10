# MoobFS

**Filesystem nativo experimental para Linux, voltado à memória e ao contexto de LLMs.**

[English](README.md) · [Site](https://studiomoob.github.io/llmoobfs/) · [Contribuição](CONTRIBUTING.md) · [Roadmap](ROADMAP.md)

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

## Objetivo do projeto

O MoobFS busca oferecer uma base de armazenamento, no nível de filesystem, para
memória persistente e contexto consumidos por agentes baseados em LLMs. A intenção
é preservar memórias como observações, decisões e registros de tarefas entre
sessões, compartilhar conteúdo repetido e, futuramente, manter revisões que os
agentes possam recuperar para compor seu contexto.

O filesystem armazena e expõe as memórias como arquivos. A aplicação do agente
seleciona o material relevante e monta o contexto enviado à LLM. Recuperação
semântica e montagem de contexto ainda precisam ser desenhadas e integradas;
o MoobFS não fornece essas capacidades hoje nem amplia a janela de contexto do modelo.

## Implementado hoje: o marco M1 somente leitura

O primeiro marco valida uma base técnica para esse objetivo: arquivos independentes
podem compartilhar os mesmos bytes imutáveis. Uma ferramenta offline cria a imagem;
o módulo nativo monta no Linux e lê dois arquivos com inodes diferentes apontando
para o mesmo conteúdo físico. Não usa FUSE. O exemplo abaixo demonstra
compartilhamento físico, não um fluxo completo de memória para agentes.

```text
alpha.txt → inode 2 ─┐
                    ├─→ objeto 1 → conteúdo armazenado uma vez
beta.txt  → inode 3 ─┘
```

**Estado: M1 somente leitura validado em VM Linux. Sem versão estável.** Escrita,
transações, snapshots, busca semântica e integração com agentes ainda não existem.
Não há alegação de ganho de desempenho em relação a FUSE ou outros filesystems.

## Experimentar

As ferramentas precisam de compilador C11, Make e Python 3 no Linux ou macOS:

```sh
git clone https://github.com/studiomoob/llmoobfs.git
cd llmoobfs
make test
printf 'Uma memória compartilhada.\n' > build/memory.txt
build/host/mkfs.moobfs build/demo.img alpha.txt=build/memory.txt beta.txt=build/memory.txt
build/host/fsck.moobfs build/demo.img
```

O relatório JSON mostra inodes distintos, mesmo objeto e mesmo offset. A criação
recusa sobrescrita. O verificador não repara imagens.

Com Docker em execução, `make vm-test` compila o módulo e inicia uma VM QEMU com
Linux **6.12.111+deb13-amd64**. Não carrega módulos no host nem exige container
privilegiado. No Apple Silicon usa emulação. Veja [como reproduzir](docs/testing.md).

A suíte passou com 9 testes de ferramentas, sanitizers nas ferramentas e montagem
real: inodes 2/3, offset físico 12288, extent de 16384 bytes, conteúdo lógico de
12562 bytes. Também testa leitura, EOF, seek, FIEMAP, remontagem, rejeição de escrita
e 15 imagens malformadas. KASAN/lockdep e benchmarks de kernel ainda estão pendentes.

## Limites e documentos

M1: diretório plano, até 128 arquivos/objetos, até 64 KiB por objeto, nomes ASCII
imprimível de até 63 bytes. Conteúdo pode conter qualquer byte. A imagem deve
permanecer imutável durante a montagem. Compartilhar disco não garante cache comum.

- [Formato implementado](docs/format-m1.md) e [proposta transacional futura](docs/native-mvp.md).
- [Decisões](docs/decisions/), [histórico](docs/history/) e [changelog](CHANGELOG.md).
- [Regras para Codex, Claude e Cursor](docs/ai-workflow.md).
- [Contribuição](CONTRIBUTING.md), [governança](GOVERNANCE.md), [conduta](CODE_OF_CONDUCT.md) e [segurança](SECURITY.md).

Todo o projeto usa **GPL-2.0-only**, conforme [LICENSE](LICENSE).
