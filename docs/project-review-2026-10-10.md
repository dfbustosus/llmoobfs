# Revisão do projeto — 2026-10-10

Base examinada: `36eb64b023bf2a3fdeaf99e0cf070843d036e4e1`, branch
`m3/3b-query-authz`. Revisão de arquitetura, implementação, testes, planos e proposta
de valor. Recomendações abaixo são propostas, não novas decisões arquiteturais.
O arquivo não versionado `docs/community-plan.md` foi preservado.

## Parecer

O projeto já ultrapassou a prova de leitura compartilhada. Existe uma camada nativa
montável, escrita POSIX delegada, atributos tipados, índice e consulta estruturada,
com uma infraestrutura de testes substancial. A ADR-0007 é a decisão mais importante:
reutilizar um filesystem maduro concentra o esforço na interface de memória.

Ainda é um protótipo de pesquisa. A base não demonstra vantagem de desempenho, nem
melhoria da qualidade da memória de um agente. Há defeitos de consistência de consulta
identificados por leitura e configurações de identidade/permissão que precisam de
reprodução específica antes de ampliar o uso.

Recomendação: fechar correção e contrato do QUERY, executar um M4 pequeno e produzir
uma demonstração de memória entre sessões. Não iniciar um novo formato de disco,
busca vetorial no kernel ou várias integrações simultaneamente.

## O que existe e o que ainda é plano

| Área | Evidência atual | Limite |
| --- | --- | --- |
| Base open source | Licença GPL-2.0-only, README bilíngue, site, workflows, guias, ADRs, histórico e instruções para agentes | Os documentos de status já divergem entre si |
| M1 | Imagem offline, fsck, módulo nativo somente leitura, dois inodes com conteúdo físico compartilhado | Experimento congelado; não é o backend atual da camada |
| M2 | `layer/main.c`: filesystem empilhado sobre diretório inferior; operações POSIX e delegação de I/O/reflink | Não torna ext4 capaz de reflink nem oferece deduplicação automática universal |
| Atributos | Validador compartilhado entre kernel/ferramentas, `user.moob.*`, tipos e limites | Atualização de conteúdo e vários atributos não forma uma transação única |
| Índice | RAM, árvores por nome de atributo e registros por inode; reconstrução ao montar | Custo de montagem, orçamento e consistência sob falhas ainda exigem trabalho |
| QUERY | Predicados conjuntivos, UAPI v1, comparação com oráculo | Busca estruturada, sem embeddings ou ranking semântico |
| M3 | Convenções de identidade/proveniência, checker; autorização POSIX implementada nesta branch | Não confundir branch com release; batch e cursor de mudanças continuam propostas adiadas |
| Validação Linux | Registros de ext4/Btrfs/XFS, fsx, git, SQLite WAL, cliente próprio, dm-log-writes/flakey e concorrência | Cobertura ampla para um protótipo, sem equivaler a certificação POSIX, auditoria de segurança ou todo fstests |
| M4 | Plano e desenho de harness | Sem resultados de benchmark apresentados nesta revisão |
| Memória de LLM | Arquivos e metadados utilizáveis por um cliente | Seleção, ranking, atualização de conhecimento e montagem de contexto pertencem ao cliente |

Fluxo atual: cliente → VFS/QUERY → camada MoobFS → filesystem inferior → disco.
O conteúdo e os xattrs persistem no inferior; o índice é uma estrutura derivada.
Essa separação é boa para recuperação e inspeção, mas exige tornar explícita qualquer
resposta incompleta quando o índice falhar.

## Achados de código prioritários

Os três primeiros achados decorrem diretamente do fluxo do código. Não foram
reproduzidos em VM nesta revisão. P1 significa corrigir/investigar antes de tratar
a funcionalidade como confiável; P2 significa corrigir no próximo ciclo.

### R1 — P1: mudar um atributo indexado para RAW conserva o valor anterior

Em `layer/index.c:238`, `index_put()` retorna para `MOOB_T_RAW` antes de localizar
e remover a entrada anterior. `moobl_xattr_set()` escreve o novo xattr e chama
`moobl_index_set()` sem remoção prévia (`layer/main.c:655`).

Consequência prevista: depois de transformar `kind=string:note` em RAW, uma consulta
por `kind=note` continua encontrando o arquivo até reconstruir o índice. O disco e
o índice passam a discordar sem necessariamente marcar o índice como incompleto.

Correção sugerida: remover a entrada anterior também na transição para RAW,
preservando a regra de não indexar o novo payload. Testar string→RAW, RAW→string,
EXISTS, remoção e igualdade com o oráculo antes/depois da remontagem.

### R2 — P1: truncamento de caminho antes da autorização causa falsos negativos

`fill_result()` (`layer/index.c:735`) limita o caminho a 255 bytes e sinaliza
truncamento. `do_query()` copia esse caminho para o candidato (`:891`) e depois
`caller_may_read()` tenta resolvê-lo. O indicador de truncamento não é preservado.

Um caminho relativo válido com mais de 255 bytes normalmente deixa de resolver,
e a consulta omite silenciosamente um arquivo legível. O limite da representação
da UAPI está alterando a decisão de acesso.

Correção sugerida: autorizar usando o caminho completo e definir separadamente
como a UAPI representa resultados longos. Enquanto não houver representação
adequada, retornar erro explícito/estado de incompletude documentado é preferível
a uma resposta aparentemente completa. Testar caminhos próximos e acima do limite.

### R3 — P2: erros operacionais podem virar ausência de resultados

`caller_may_read()` (`layer/index.c:798`) retorna booleano e converte qualquer erro
de `vfs_path_lookup()` em `false`; erros de `inode_permission()` também são
indistintos. Isso inclui falhas diferentes de acesso negado ou objeto removido.
Contradiz o contrato que promete propagar falhas operacionais.

Correção sugerida: retorno com erro/permitido/omitido e classificação explícita de
errno. Injetar falhas e verificar que uma falha operacional não produz sucesso com
zero resultados. EACCES/ENOENT durante concorrência exigem tratamento próprio.

### R4 — P1, hipótese a reproduzir: identidade de inode entre subvolumes Btrfs

O índice usa apenas `ino` como chave do xarray (`layer/index.c:172`); o inode superior
copia `lower->i_ino` (`layer/main.c:98`). A varredura percorre diretórios e não contém
uma barreira explícita de subvolume. A validação do candidato compara novamente o
número do inode (`layer/index.c:808`).

Btrfs possui um espaço de números de inode por subvolume. Portanto, inodes distintos
em subvolumes diferentes podem colidir nessa representação. A documentação oficial
recomenda o par subvolume/inode para identidade no filesystem. Isso sustenta o risco;
não prova, sozinho, a manifestação exata ou um vazamento no MoobFS.

Teste prioritário: dois subvolumes aninhados, inodes numericamente iguais, atributos
e permissões diferentes; comparar resultado, conteúdo aberto e oráculo. Até haver
identidade interna adequada, considerar recusar cruzar subvolumes. A solução deve
tratar também a identidade exposta pela camada, não apenas acrescentar um campo na
árvore. [Fonte Btrfs](https://btrfs.readthedocs.io/en/latest/Subvolumes.html).

### R5 — P1, configuração a reproduzir: inferior com IDs remapeados

`moobl_fill_super()` resolve o inferior (`layer/main.c:1107`) sem recusar explicitamente
uma montagem idmapped; operações delegadas e permissões usam `nop_mnt_idmap`.
Recusar idmap sobre a montagem superior não cobre automaticamente selecionar um
inferior já remapeado.

Teste: montar um inferior idmapped e usá-lo em `lower=`, verificando UID/GID, criação,
ACL e QUERY. Para o primeiro release experimental, recusar a configuração pode ser
mais simples que suportá-la. Não foi demonstrada exploração nesta revisão.

### R6 — P2: o lote limita resultados candidatos, não o trabalho de varredura

Em `layer/index.c:816`, a busca inicial começa em `rb_first()`. O laço a partir de
`:881` percorre entradas até reunir candidatos que satisfaçam todos os termos.
Uma consulta muito seletiva pode visitar a árvore inteira sob o lock de leitura.
Ter uma árvore balanceada não torna essa consulta automaticamente logarítmica.

Medir número de entradas examinadas, duração do lock e latência dos escritores.
Depois avaliar posicionamento no início do intervalo pesquisado, parada no fim,
escolha do primeiro termo e lotes limitados por entradas examinadas. Essa limitação
de trabalho é relevante mesmo antes de otimizações finas de desempenho.

### R7 — P2: checker pode transmitir confiança indevida

`tools/moobfs-check.c:48` ignora `FTW_NS` e `FTW_DNR`; um diretório não inspecionado
pode não aparecer como problema. O nome do atributo é interpolado sem escape no
JSON (`:36`), embora nomes inválidos provenientes do inferior sejam justamente um
dos casos que a ferramenta deve reportar.

Sugestão: distinguir árvore limpa de inspeção incompleta, sinalizar erro na saída e
escapar todas as strings. Testar diretório inacessível e xattr com aspas no nome.

## Outros pontos de robustez

- O índice pode ficar incompleto após uma escrita bem-sucedida no inferior. O cliente
  precisa observar esse estado; respostas não devem parecer completas inadvertidamente.
- `index_used` não é a RAM real: alocador, xarray e buffers transitórios precisam de
  medição. A lista temporária de nomes de um diretório merece teste separado de limite.
- Os testes `--uid` usam setgid/setuid sem limpar grupos suplementares. Preparar a
  identidade completa evita que os testes herdem acesso não intencional por grupo.
- A corrida QUERY/open admite ENOENT/EACCES por contrato. Esse teste é útil para
  estabilidade, mas não basta para provar ausência de resultados indevidos: adicionar
  fases controladas de revogação, com barreiras e resultados esperados conhecidos.
- Revisar especificamente readonly no inferior e congelamento do filesystem nas
  operações de metadados delegadas. É uma área de revisão pendente, não um bug
  confirmado aqui.
- Reflink compartilha dados; não assumir que o ioctl FICLONE transfere xattrs. O
  contrato do cliente deve dizer como copiar atributos e quando gerar nova identidade.
- Git versiona os arquivos do projeto, mas não preserva automaticamente os xattrs das
  memórias. Propor exportação/importação de conteúdo + metadados e teste de ida/volta.
- `(namespace, memory_id)` é identidade lógica; `namespace` como atributo não é uma
  barreira de segurança. O isolamento depende das credenciais e permissões POSIX.

## Planos e comunicação

README anuncia no título apenas fase 2a e descreve 2a–2e no corpo. ROADMAP ainda
lista KASAN como pendente genericamente, embora o histórico registre aprovação em
main; a branch atual tem outra execução. A abertura da especificação M3 diz que nada
foi implementado, embora já exista código parcial. O roadmap ainda cita cliente
externo obrigatório e batch/cursor como se o escopo não tivesse sido refinado.

Não apagar o histórico: acrescentar notas datadas e manter um quadro curto de
estado atual com commit, evidência e pendência. ADRs descrevem a decisão no momento
em que foi tomada; precisam de ligação para o estado atual, não reescrita retrospectiva.

O benefício potencial mais específico é: arquivos e atributos consultáveis por
vários processos, com autorização POSIX e persistência num filesystem comum. A
ausência de daemon de banco não diferencia o projeto de SQLite, que também roda
no processo da aplicação. [Documentação SQLite](https://www.sqlite.org/serverless.html).

O fato de estar no kernel não prova menor latência de ponta a ponta. Travessia VFS,
checagem de cada caminho, reconstrução e manutenção do índice podem dominar a carga.
O custo de instalação privilegiada e compatibilidade com kernels também entra na
avaliação de adoção. Manter Linux 6.12 como alvo inicial explícito e expandir a matriz
depois de estabilizar o contrato é mais defensável que prometer suporte Linux amplo.

Dois ajustes concretos no desenho do benchmark: `docs/benchmark-harness.md:59`
declara `memory_id TEXT UNIQUE`, enquanto a identidade proposta é o par
`(namespace, memory_id)`; alinhar os contratos antes de gerar o dataset. O uso de
SQLite com `synchronous=NORMAL` (`:67`) também exige explicitar qual recuperação
após perda de energia é esperada e tornar equivalente a política de sincronização
dos sistemas comparados. Não interpretar configurações diferentes de durabilidade
como vantagem intrínseca de desempenho.

## Caminho recomendado

1. **Correção do QUERY:** regressões R1–R3; reproduções R4/R5; estado de índice
   incompleto; identidade dos testes. Concluir KASAN da branch e atualizar status.
2. **M4 mínimo:** comparar inferior puro, camada com `noindex`, camada indexada e
   arquivos + SQLite com índices adequados. Começar por leitura de caminho conhecido,
   consulta seletiva, atualização concorrente e montagem/reconstrução.
3. **Mesma semântica:** mesmo conteúdo e atributos, autorização equivalente,
   durabilidade equivalente e resultados conferidos antes de cronometrar. Comparar
   identidade lógica, não números físicos de inode entre backends. Medir p50/p95/p99,
   CPU, RAM real, tempo de montagem e entradas examinadas. Usar Linux nativo/KVM;
   não usar emulação TCG como evidência de desempenho para usuários Linux.
4. **Demonstração de memória entre sessões:** agente A registra uma decisão;
   agente B autorizado recupera após reinício; versão nova substitui uma informação;
   outro UID não a recebe; exportação/importação mantém identidade e proveniência.
   O cliente pequeno pode ter adaptadores MoobFS/SQLite para o experimento sem alterar
   a decisão de que o produto é um filesystem nativo.
5. **Só então ampliar:** integração de agente, empacotamento e matriz de kernel.
   Adiar batch, cursor de revisão, índice persistente e ranking até que medições
   identifiquem a necessidade concreta. Um cursor de mudanças precisa definir
   persistência e reinício; um contador volátil não resolve o problema sozinho.

Definir critérios antes do benchmark: correção obrigatória; limites aceitáveis de
overhead, memória e tempo de montagem; uma carga relevante com benefício mensurável.
Não é necessário vencer SQLite em tudo, mas é necessário justificar o custo da camada.

## Artigos e implicações para o projeto

As aplicações abaixo são inferências de arquitetura, não resultados experimentais
sobre MoobFS. Foram consultadas as páginas e os resumos dos artigos; não foi feita
reprodução dos experimentos nem revisão exaustiva da literatura.

- [MemGPT](https://arxiv.org/abs/2310.08560): trata gestão de contexto por níveis de
  memória. Reforça a separação entre armazenamento persistente e a política que
  decide o que entra na janela de contexto. Essa política permanece no cliente.
- [LongMemEval](https://arxiv.org/abs/2410.10813): avalia extração, raciocínio entre
  sessões, tempo, atualização de conhecimento e abstenção. Pode avaliar o fluxo de
  memória depois do benchmark de armazenamento. Manter modelo, prompt e estratégia
  de recuperação iguais ao comparar backends, para não atribuir ao FS ganhos do agente.
- [A-MEM](https://arxiv.org/abs/2502.12110): explora organização e relações entre
  memórias. Sugere valor em proveniência e relações explícitas; não implica implementar
  um grafo semântico ou execução de LLM no kernel.
- [Modelo de permissões do OverlayFS](https://www.kernel.org/doc/html/latest/filesystems/overlayfs.html):
  referência para distinguir credenciais do chamador e do acesso ao inferior.
  Documentação atual não substitui verificar as APIs no kernel 6.12 usado pelo projeto.

## Evidência desta revisão

- Executado `make test BUILD=build/review-20261010`: 60 verificações de encoding e
  9 testes Python de imagem aprovados. Isso não executa o módulo nem QUERY em Linux.
- Consultados código, especificações, ADRs e registros de validação; os testes Linux
  citados acima são evidências registradas pelo projeto, não reexecuções desta sessão.
- O histórico 0016 registra `MOOBFS_KASAN_PASS` em main na execução
  [38074631812](https://github.com/studiomoob/llmoobfs/actions/runs/38074631812).
- A execução KASAN da branch
  [38079499011](https://github.com/studiomoob/llmoobfs/actions/runs/38079499011)
  estava `in_progress`, sem conclusão, na consulta realizada nesta revisão.
- Nenhum módulo foi carregado no host macOS; nenhuma nova execução de VM foi feita.
  As reproduções propostas ainda precisam ser implementadas/executadas.
- Nenhuma correção de código, alteração no site ou publicação remota integra esta
  entrega. Os achados não equivalem a uma auditoria completa de segurança do kernel.
