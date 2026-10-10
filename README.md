# MoobFS

Projeto de um filesystem montável, nativo do Linux, para memória persistente de agentes de LLM.

O MVP usa um módulo de kernel, formato próprio em um dispositivo de blocos, conteúdo imutável compartilhado e revisões acessíveis como arquivos. A leitura não depende de um processo em espaço de usuário. Gravações são publicadas por operações transacionais do módulo.

**Estado: arquitetura proposta; módulo e ferramentas ainda não implementados.**

O [desenho do MVP nativo](docs/native-mvp.md) define arquitetura, formato em disco, interface, persistência, limites, etapas de implementação e critérios de validação.

O objetivo do experimento é medir três coisas separadamente: correção e durabilidade, custo de armazenamento/acesso e utilidade da memória para o agente. Ganhos de desempenho ou economia de espaço ainda precisam ser demonstrados.
