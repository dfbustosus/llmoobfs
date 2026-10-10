# ADR-0005 — Modelo de memória para agentes com o Be File System como referência

Data: 2026-10-10. Estado: proposto; aceito pelo usuário como base de desenho para M2/M3. Nada aqui está implementado.

## Contexto

O registro [0004](../history/0004-llm-memory-context.md) fixou o objetivo: um filesystem nativo
para memória persistente e contexto consumidos por agentes LLM. O M1 (ADR-0003) valida apenas
o compartilhamento físico de bytes entre inodes. Faltava um modelo que ligasse o objetivo ao
mecanismo: o que é uma memória como arquivo, quais metadados ela carrega, como um agente a
encontra sem recuperação semântica, e qual parte disso justifica estar no kernel.

A proposta original (`docs/native-mvp.md`, §3) trata metadados de revisão como um bloco opaco
de até 4 KiB, interpretado como JSON pela CLI. O kernel não os entende e não os indexa; toda
busca estruturada ficaria em espaço de usuário, o que enfraquece a razão para um filesystem
nativo em vez de um diretório comum com um banco ao lado.

O Be File System (BFS, 1997, Dominic Giampaolo e Cyril Meurillon) é o precedente mais próximo
de "filesystem como camada de memória consultável": atributos estendidos tipados por arquivo,
índices automáticos por atributo em B+ trees, consultas por igualdade e intervalo servidas pelo
próprio filesystem, journaling de metadados e diretórios em B+ tree. O desenho está documentado
em *Practical File System Design with the Be File System* (Giampaolo, 1999).

## Decisão

1. **Memória = arquivo; revisão = conteúdo imutável + atributos tipados.** Os metadados de uma
   revisão deixam de ser um bloco opaco e passam a ser um conjunto limitado de atributos com
   nome e tipo. Tipos previstos: `string` (UTF-8), `int64`, `double`, `bool`, `time` (ns desde
   epoch) e `raw` (bytes, não indexável). Limite total por revisão permanece 4 KiB; o número
   máximo de atributos e o tamanho de nome serão fixados na especificação do formato M2.
2. **Exposição pela API padrão do Linux.** Atributos são lidos por `getxattr`/`listxattr` no
   namespace `user.moob.*`. A CLI continua podendo renderizá-los como JSON. No M2, a escrita
   de atributos ocorre dentro do `PUT` transacional, não por `setxattr` avulso.
3. **Índices por atributo mantidos pelo módulo.** Atributos marcados como indexáveis na criação
   do volume ganham um índice persistente por igualdade e intervalo. A estrutura inicial pode
   ser simples e sequencial; B+ tree em disco é o alvo quando os limites do M1 forem removidos.
   A atualização do índice é parte da mesma transação que publica a revisão.
4. **Interface de consulta por ioctl, não por diretório virtual.** O VFS não tem chamada de
   consulta por atributo; o BeOS tinha uma syscall própria. Adota-se um comando `QUERY` no
   arquivo `.control` (mesma UAPI proposta em `native-mvp.md` §5), que recebe um predicado
   limitado (atributo, operador, valor, conjunção) e devolve pares `(memory_id, revision)`.
   Um diretório virtual de consultas ao estilo BeOS fica como alternativa futura, não adotada.
5. **Recuperação semântica permanece em espaço de usuário.** O kernel não calcula embeddings,
   não compara similaridade e não interpreta linguagem natural. A aplicação do agente usa as
   consultas estruturais para reduzir candidatos e monta o contexto por conta própria.
6. **Journaling de metadados como modelo de durabilidade do M2.** O desenho de log de
   transações do BFS (metadados no journal, dados fora dele) é a referência de partida para
   a especificação de escrita, falha e recuperação, a ser detalhada em ADR própria com testes
   de queda de energia.
7. **Diretórios em B+ tree substituem o namespace plano** quando o limite de 128 entradas do
   M1 for removido. Isso é consequência, não meta imediata.

Os itens 1 a 5 refinam a seção 3 da proposta original; a proposta é preservada e esta ADR
aponta para ela. ADRs 0001 a 0004 continuam válidas.

## Reuso de código do BFS

Avaliado e decidido como **não reutilizar código**, apenas o desenho:

- `fs/befs` no Linux upstream (GPL-2.0-only, mantido por Luis de Bethencourt e Salah Triki)
  é um driver **somente leitura** e, segundo seu próprio Kconfig, não disponibiliza atributos
  nem índices. Ou seja, não implementa exatamente a parte que interessa. Sua licença é
  compatível com a do MoobFS, e `fs/befs/btree.c` serve como referência de leitura de B+ tree
  em disco com a API de buffer heads, mas o MoobFS usa iomap e um formato próprio.
- A implementação do Haiku (OpenBFS, licença MIT, C++) é completa, com escrita, atributos,
  índices e consultas, mas depende da API de kernel do Haiku. Portar para o kernel Linux
  seria reescrever; é a melhor referência de comportamento e de casos de teste.
- Usar o formato BFS em disco como formato do MoobFS foi descartado: ele não tem dedup de
  conteúdo, revisões nem snapshots, e herdaria limites e decisões de 1997 sem a transação
  idempotente com `request_id` que a proposta exige.

## Alternativas consideradas

- Manter metadados opacos e indexar em espaço de usuário (SQLite ao lado). Simples, mas torna
  o filesystem nativo um mero repositório de bytes; rejeitado por contradizer ADR-0001.
- Expor consultas como diretório virtual (`/mnt/moob/queries/<predicado>/`). Elegante para
  ferramentas POSIX, mas cria entradas de diretório dinâmicas com semântica difícil de
  manter sob concorrência e snapshots. Adiado.
- Indexar conteúdo (texto completo) no kernel. Fora de escopo: é processamento semântico.

## Consequências

- A especificação do formato M2 precisa definir: codificação dos atributos, lista de
  atributos indexáveis, estrutura do índice, layout da UAPI de `QUERY` e regras de validação
  de entradas inválidas (nomes, tipos, tamanhos, predicados).
- Os testes de M2 devem cobrir consultas por igualdade e intervalo, índices após queda de
  energia e rejeição de predicados malformados, além do que já exige a proposta original.
- O roadmap ganha uma dependência explícita: M2 inclui atributos e índice, não só escrita.
- Nenhum ganho de desempenho ou utilidade para LLMs é afirmado até M4 medir contra baselines.

## Referências

- Giampaolo, D. *Practical File System Design with the Be File System*. Morgan Kaufmann, 1999.
- [Be File System — Wikipedia](https://en.wikipedia.org/wiki/Be_File_System)
- [BeOS filesystem for Linux — documentação do kernel](https://docs.kernel.org/filesystems/befs.html)
- `fs/befs/Kconfig` e `MAINTAINERS` no Linux upstream, consultados em 2026-10-10.
- [Haiku BFS](https://github.com/haiku/haiku/tree/master/src/add-ons/kernel/file_systems/bfs)
