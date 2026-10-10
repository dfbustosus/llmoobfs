# Memória do projeto

Este diretório guarda um diário técnico versionado. O Git preserva as alterações exatas dos arquivos; as ADRs explicam decisões e o diário conecta pedidos, mudanças e resultados.

## Onde encontrar cada informação

- `docs/decisions/`: decisões arquiteturais, alternativas e motivos resumidos.
- `docs/history/`: registros cronológicos de trabalho e evidências de validação.
- `CHANGELOG.md`: mudanças relevantes para quem usa/contribui.
- `docs/native-mvp.md`: proposta original do MVP completo.
- `AGENTS.md`: instruções de continuidade para próximas sessões.

Um registro deve conter data, objetivo, decisões/ADRs, arquivos ou commits relacionados, testes realmente executados, limitações e próximo passo. Use novos registros ou adendos datados para mudanças posteriores. Registros da sessão em andamento podem ser completados antes do commit.

Os registros iniciais reconstituem a conversa em forma resumida; não são uma transcrição literal ou exaustiva. Histórico de decisões registra o que foi decidido e suas justificativas técnicas, sem alegar acesso a pensamentos internos privados.

Preservação depende de commits e cópias: nenhum disco ou serviço garante retenção para sempre. Manter o repositório remoto e uma cópia independente permite recuperar o histórico. Saídas grandes, imagens de teste e binários ficam em `build/`; versionar os scripts que os reproduzem e resumos verificáveis dos resultados.

## Índice

- [0001 — Origem e definição do projeto](0001-origin.md)
- [0002 — Primeiro marco nativo](0002-native-read-milestone.md)
- [0003 — Base open source e GitHub](0003-open-source-github.md)
- [0004 — Memória e contexto de LLMs como objetivo explícito](0004-llm-memory-context.md)
- [0005 — Modelo de memória e o Be File System como referência](0005-bfs-reference-memory-model.md)
- [0006 — Publicação da ADR-0005 e levantamento do ecossistema livre](0006-ecosystem-survey.md)
- [0007 — Leitura técnica do ai-memory e posição relativa](0007-ai-memory-reading.md)
- [0008 — Infraestrutura do kernel reutilizável e especificação do M2 com escrita POSIX](0008-m2-spec-kernel-infra.md)
- [0009 — O que o MoobFS traria de diferente, e o princípio de não concorrer com o maduro](0009-differentiation-and-principle.md)
- [0010 — ADR-0007: camada empilhada sobre filesystem maduro; M2 reescrito](0010-adr-0007-stacked-layer.md)
- [0011 — Fase 2a: camada empilhada mínima sobre ext4, validada em VM](0011-phase-2a-stacked-passthrough.md)
- [0012 — Fase 2b: passagem completa sobre ext4, btrfs e XFS, com reflink delegado](0012-phase-2b-full-passthrough.md)
- [0013 — Fase 2c: atributos tipados validados no kernel, ferramentas e fuzzing](0013-phase-2c-typed-attributes.md)
- [0014 — Fase 2d: índice em memória e consulta por atributo (`QUERY`)](0014-phase-2d-index-query.md)
- [0015 — Fase 2e: aceitação com clientes reais e investigação de perda de linhas no SQLite](0015-phase-2e-investigation.md)
- [0016 — Harness com KASAN e lockdep em kernel próprio; diagrama de fluxo no site](0016-kasan-lockdep-harness.md)
- [0017 — Uso no macOS e postura de segurança comparada ao mercado](0017-macos-and-security-posture.md)
- [0018 — Reescrita da história do Git: remoção de perguntas e de dados pessoais](0018-history-rewrite.md)
- [0019 — Decisões a partir de uma revisão externa da arquitetura](0019-external-review-decisions.md)
- [0020 — Fase 3b: autorização POSIX no `QUERY` (em andamento)](0020-phase-3b-query-authorization.md)
