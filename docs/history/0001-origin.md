# 0001 — Origem e definição do projeto

Data do registro: 2026-10-10, America/Sao_Paulo. Registro retrospectivo resumido da conversa.

## Objetivo que emergiu

O usuário propôs compartilhar endereços de armazenamento entre arquivos. A conversa distinguiu hard links, reflinks, deduplicação e copy-on-write. O uso pretendido foi então esclarecido: memória persistente de LLM, no nível de filesystem.

## Evolução e decisões

1. Diferenciou-se economia de espaço de economia de contexto do modelo: compartilhar bytes em disco não reduz automaticamente tokens entregues à LLM.
2. Uma proposta inicial usava FUSE com motor de memória e SQLite. O usuário pediu uma alternativa sem FUSE.
3. Foi considerada uma biblioteca/motor local. O usuário esclareceu que montar como filesystem é requisito; essa alternativa não satisfaz o projeto.
4. Foi escolhido módulo nativo do Linux, formato próprio em dispositivo de blocos e leitura convencional pelo VFS.
5. A proposta completa foi salva em `docs/native-mvp.md`, com revisões, snapshots, deduplicação, transações e recuperação. É arquitetura proposta, não implementação concluída.
6. O usuário autorizou implementar o primeiro marco: imagem MoobFS e módulo que monta e lê dois arquivos compartilhando conteúdo.
7. Antes da implementação, pediu histórico das decisões, alterações e construção do projeto. Informou que criará um repositório no GitHub; a captura mostra uma tela de criação, não comprova criação ou publicação.

## Estado ao iniciar implementação

Diretório local continha somente README e proposta do MVP, sem Git inicializado ou código. Ambiente de trabalho é macOS arm64, com Docker Desktop Linux arm64 disponível. Validação do módulo exige Linux separado.

## Referências

- [ADR-0001](../decisions/0001-native-filesystem.md)
- [ADR-0002](../decisions/0002-project-history.md)
- [Proposta original](../native-mvp.md)
