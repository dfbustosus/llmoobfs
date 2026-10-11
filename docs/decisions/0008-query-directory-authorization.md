# ADR-0008 — Autorização do `QUERY` com reaproveitamento por diretório dentro de uma consulta

Data: 2026-10-11. Estado: **proposta em espera**. Decisão do mantenedor em 2026-10-11: manter
a autorização como está e reavaliar esta proposta depois que o M4 mostrar se consultas que
devolvem centenas de resultados fazem parte do uso real. Nada implementado.
Não substitui decisão anterior; refina como o contrato de autorização da fase 3b (registro 0020,
`docs/spec-m3.md` §3.1) é cumprido.

## Contexto

Desde a fase 3b, cada resultado do `QUERY` é autorizado resolvendo o caminho relativo à raiz da
montagem com `vfs_path_lookup`, sob as credenciais do chamador, e verificando `MAY_READ` no
arquivo com `inode_permission`. O resultado só entra na resposta, e na contagem, se o chamador
poderia abri-lo naquele instante; a janela entre a resposta e a abertura é aceita no contrato.

Os pilotos no host da Azure (registros 0025, 0027 e 0028) mediram o custo restante em cerca de
1,7 µs por resultado, contra 0,2 a 0,4 µs por linha no SQLite com índice composto. O perfil com
`perf` mostra que esse custo vem de refazer, para cada resultado, o caminho inteiro: montar o
caminho como texto, copiá-lo de novo em `getname_kernel`, percorrer cada componente (com
`moobl_permission` repetindo `inode_permission` no inode inferior) e verificar o arquivo. Num
conjunto típico, centenas de resultados compartilham poucos diretórios (no piloto, 1000
memórias em 256 diretórios), e a verificação de cada diretório é repetida a cada resultado.

## Proposta

Dentro de **uma única chamada** de `QUERY`, guardar a decisão de travessia de cada diretório já
verificado e reaproveitá-la para os resultados seguintes:

1. Para cada candidato, subir pela cadeia de dentries a partir do pai do arquivo até a raiz da
   montagem, sob `rcu_read_lock` e com o `seqlock` de renomeação (`rename_lock`) para detectar
   mudanças concorrentes, verificando `MAY_EXEC` em cada diretório com `inode_permission`.
2. Cada diretório aprovado entra num conjunto local à chamada (endereço do dentry); ao encontrar
   um diretório já aprovado, a subida para ali. Diretórios recusados também são lembrados.
3. Verificar `MAY_READ` no arquivo, como hoje.
4. Se o `seqlock` indicar renomeação durante a subida, ou qualquer passo não puder ser feito sem
   bloquear, o candidato cai no caminho atual (`vfs_path_lookup` completo). O caminho atual
   continua sendo a referência de correção.
5. O conjunto é descartado ao fim da chamada; nada é guardado entre chamadas nem entre
   processos.

## Invariantes que a mudança precisa manter

- Nenhum resultado, nem contagem, que o chamador não pudesse abrir naquele instante (fora da
  janela já aceita entre resposta e abertura).
- Mesmas credenciais: as do chamador no momento da chamada, com `nop_mnt_idmap`.
- Mesmo escopo: só caminhos alcançáveis a partir da raiz desta montagem; um dentry fora dela é
  recusado.
- Mudança de permissão de um diretório durante a consulta não pode conceder acesso que a
  verificação atual negaria em nenhum instante da chamada. Reaproveitar uma decisão positiva
  tomada antes de um `chmod` concorrente dentro da mesma chamada é equivalente à janela já
  aceita; isso precisa ser escrito no contrato e testado.

## Alternativas

- **Manter como está.** Correto e simples; custo por resultado documentado. É a opção se o M4
  mostrar que consultas com centenas de resultados não são o uso real.
- **Cache entre chamadas.** Descartado: exigiria invalidação a cada `chmod`, `rename` e mudança
  de ACL, e abriria espaço para decisões vencidas.
- **Autorizar só o arquivo.** Descartado: ignora a travessia dos diretórios e quebraria o
  isolamento por diretório testado na fase 3b.
- **Tirar os caminhos da resposta.** Fora do escopo: mudaria a UAPI v1.

## Testes exigidos antes de aceitar

- Todos os blocos atuais da suíte (isolamento com dois usuários, ACLs, 600 candidatos em vários
  lotes, concorrência com `chmod`, `rename` e remoção) passando sem mudança.
- Novo teste de concorrência focado: `chmod` e renomeação de diretórios intermediários em laço
  enquanto um usuário comum consulta, verificando a cada resposta que todo caminho devolvido
  abre ou falha só pela janela aceita, e que nenhum resultado de diretório negado aparece.
- Equivalência com o oráculo rodando como o mesmo usuário, em todas as formas do W3.
- KASAN e lockdep sem diagnóstico.
- Medição no host da Azure, por forma, contra o piloto 4.

## Lacuna relacionada, fora desta decisão

Módulos de segurança que decidem pelo caminho na abertura (AppArmor, Landlock) não são
consultados pela verificação atual nem pela proposta. Fica para a ADR do modelo de ameaça (S1
do roteiro de segurança).
