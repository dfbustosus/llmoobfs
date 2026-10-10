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
