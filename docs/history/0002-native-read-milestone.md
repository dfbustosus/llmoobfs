# 0002 — Primeiro marco nativo

Data: 2026-10-10, America/Sao_Paulo. Estado: implementado e validado localmente.

## Pedido e decisões

Criar imagem MoobFS e módulo nativo que monte no Linux e leia dois arquivos
independentes com conteúdo físico compartilhado. Registrar a construção e criar
regras de continuidade para Codex, Claude e Cursor. ADRs 0001–0003 documentam o
escopo; M1 é somente leitura e separado da proposta transacional original.
O usuário confirmou GPL-2.0-only para todo o projeto.

## Implementação

- `include/` e `common/`: formato little-endian, CRC32C, SHA-256 e parser
  compartilhado com validação de limites, padding, nomes e referências.
- `tools/`: criação offline determinística com deduplicação por igualdade de bytes
  e recusa de sobrescrita; verificador JSON sem reparos.
- `kernel/`: módulo VFS/iomap, inodes separados, leitura, diretório plano,
  FIEMAP, somente leitura e validação integral na montagem.
- `tests/` e `vm/`: ferramentas e VM QEMU isolada, kernel Debian
  `6.12.111+deb13-amd64`, pacote `6.12.111-1`. O módulo não é carregado no host
  nem no kernel do Docker Desktop. Docker fornece ferramentas para iniciar a VM.
- `docs/format-m1.md`: especificação do formato implementado.

## Evidências executadas

- `make test`: 9 testes no macOS arm64; os mesmos testes passaram no Linux.
- `make test BUILD=build/asan CFLAGS='-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer' LDFLAGS='-fsanitize=address,undefined'`:
  9 testes passaram no macOS, sem diagnóstico de sanitizers.
- `make vm-test`: build do módulo e execução real no Linux x86-64 passaram.
  GCC 14.2.0, QEMU 10.0.13; imagem base Debian fixada por digest no Dockerfile.
- FIEMAP: inodes **2 e 3**, ambos link count 1, mesmo offset físico **12288**,
  extent **16384** bytes e flag SHARED. Conteúdo lógico: 12562 bytes.
- Verificados leitura integral, pread em partes, leitura cruzando página, seek,
  EOF, enumeração/rewind de diretório, arquivo ausente, rejeição de escrita,
  criação, remoção, mount rw e remount rw, desmontagem e nova montagem.
- Objetos vazio, de um byte e de 64 KiB; 15 imagens malformadas rejeitadas pelas
  ferramentas e pelo módulo. Nenhum BUG/Oops/WARNING observado no log.
- SHA-256 da imagem compartilhada reproduzível:
  `6bbb9b0b08d4bfda24c3a7dbd9708792e1e3ce96807cad49ac83d9bfeac7c5cc`.

Console, versões, configuração do kernel, imagem e módulo ficam em `build/vm/`,
regenerados pelo comando. Versionamos scripts e resumo, não os binários.

## Tentativas e correções

A preparação inicial procurou `loop.ko`, mas Debian entrega `loop.ko.xz`;
o harness passou a descompactá-lo. A primeira verificação de mount rw usava
BusyBox, que tenta novamente em modo ro. A montagem já funcionava; o falso
positivo no teste foi corrigido usando `mount(2)` e exigindo `EROFS`. A execução
completa posterior passou. Nomes foram limitados a ASCII imprimível e a suíte
foi repetida após essa alteração.

## Limitações e continuidade

O kernel usado não habilita KASAN nem PROVE_LOCKING; não alegamos validação com
esses recursos. Dependências apt auxiliares não são todas fixadas; não prometemos
build integralmente bit a bit. Não medimos ganhos de latência, memória ou utilidade
para LLMs. Não há escrita nem durabilidade transacional. Próximo trabalho:
consolidar a base open source, documentação e CI antes de escrita e recuperação.
