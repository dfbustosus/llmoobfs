# Continuidade do projeto

Leia `README.md`, `docs/history/README.md`, o registro de sessão mais recente e as ADRs relevantes antes de alterar o projeto.

## Histórico obrigatório

- Registre pedidos e resultados em `docs/history/`, como sínteses técnicas factuais: objetivo, decisões, alternativas, justificativas resumidas, mudanças, evidências, limitações e próximos passos.
- Registre decisões, inclusive as que nascem de perguntas do usuário, mas não as perguntas em si nem as respostas de esclarecimento. Erros encontrados com sua correção, hipóteses descartadas e experimentos que falharam continuam sendo registrados. Uma investigação em andamento tem registro próprio, marcado como em andamento e atualizado a cada rodada.
- Documente decisões arquiteturais em `docs/decisions/` com IDs sequenciais. Preserve decisões antigas; uma mudança deve apontar para a decisão que substitui.
- Preserve especificações anteriores. Identifique claramente proposta, implementação e comportamento efetivamente testado.
- Atualize `CHANGELOG.md` para alterações relevantes. Guarde comandos reproduzíveis e resumos de resultados; nunca apresente teste não executado como aprovado.
- Faça commits locais pequenos e legíveis quando houver uma unidade concluída. Não reescreva história compartilhada, não faça force-push e não apague registros para esconder tentativas ou limitações. Exceção única e registrada: em 2026-10-10 a história foi reescrita, com autorização explícita do usuário, para remover perguntas registradas por engano e dados pessoais (registro 0018).
- Não armazene credenciais, tokens, dumps de ambiente ou dados pessoais desnecessários. Históricos registram justificativas técnicas resumidas, não raciocínio interno privado do assistente.
- Antes de terminar uma tarefa, deixe claro o estado atual, os testes, as limitações e o próximo passo. Corrija registros factualmente errados com uma nota datada.

## Documentação técnica

Use `npx ctx7@latest library <nome> "<consulta>"` e depois `npx ctx7@latest docs <id> "<consulta>"` para bibliotecas, APIs, frameworks e CLIs. Máximo de três comandos por pergunta. Consulte o código/headers do kernel exato usado no build. Não exponha segredos em consultas. Em erro de quota, informe e sugira `npx ctx7@latest login` ou `CONTEXT7_API_KEY`.

## Escopo e validação

- O projeto é um filesystem nativo montável no Linux. Não substituir por FUSE ou uma API em espaço de usuário.
- Teste módulos em VM dedicada. As ferramentas de criação devem evitar sobrescrever arquivos/dispositivos existentes por padrão.
- Mudanças no formato exigem especificação, testes de entradas inválidas e decisão explícita de compatibilidade.
- GitHub é uma réplica remota do histórico Git; sua criação e publicação devem seguir as instruções do usuário na sessão.
