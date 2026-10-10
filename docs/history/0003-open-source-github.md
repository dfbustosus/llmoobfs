# 0003 — Base open source e GitHub

Data: 2026-10-10. Estado: publicado; CI remoto e Pages validados.

## Pedido e escolhas

O usuário pediu preparar GitHub, README e Pages antes de avançar o projeto. Confirmou
GPL-2.0-only em todo o repositório e claudio@studiomoob.com como contato público.
Para o site, pediu combinar apresentação visual e documentação técnica. ADR-0004
registra política e fontes oficiais pesquisadas. PRODUCT.md e DESIGN.md registram
a direção do site; a skill Impeccable orientou a apresentação.

## Mudanças

Criados README em inglês e português, contribuição, conduta, segurança, governança,
roadmap, guia de reprodução e formato M1, templates de issues/PR e Dependabot para
Actions. CI executa ferramentas, sanitizers e a VM. Actions externas fixadas por
commit. Pages publica somente `site/`, com permissões de implantação no job próprio.
O site estático tem explicação visual do compartilhamento e um guia reproduzível.

Configurado o remote `origin` para `https://github.com/studiomoob/llmoobfs.git`,
verificado como repositório existente vazio e privado, com acesso administrativo.
Descrição, homepage e tópicos foram configurados; issues habilitadas e wiki desativada.

## Validação local

Links e âncoras das duas páginas passaram em `python3 scripts/check-site.py`.
Prévia inspecionada no navegador; navegação para o guia verificada. Um overflow de
coluna em tela estreita foi corrigido com `min-width: 0`; a medição posterior não
mostrou overflow horizontal. O diagrama ganhou composição própria para celular.
A porta 8080 estava ocupada; a prévia foi iniciada em 127.0.0.1:8879.
Testes nativos e evidências estão no registro 0002. Não há repetição de testes de
kernel para mudanças apenas documentais. CI remoto e Pages serão registrados em
adendo com os resultados reais, sem presumir sucesso pela presença dos workflows.

## Adendo — publicação autorizada e verificada

O primeiro pedido de ativação do Pages retornou HTTP 422: o plano da organização
não permite Pages nesse repositório privado. O usuário autorizou explicitamente
“Sim, tornar público e publicar o Pages”. O repositório foi tornado público, Pages
configurado para GitHub Actions com HTTPS e relatos privados de vulnerabilidade
ativados (API confirmou `enabled: true`). Nenhum plano pago foi contratado.

A primeira tentativa de workflow Pages falhou enquanto o site ainda não estava
habilitado. Após a configuração, a reexecução passou. Resultados no commit `ed4b997`:

- [CI 38058154595](https://github.com/studiomoob/llmoobfs/actions/runs/38058154595):
  sucesso em ferramentas/sanitizers e VM nativa Linux.
- [Pages 38058154618](https://github.com/studiomoob/llmoobfs/actions/runs/38058154618):
  sucesso na reexecução, com site em https://studiomoob.github.io/llmoobfs/.
- [Repositório público](https://github.com/studiomoob/llmoobfs): README, licença,
  políticas, histórico, código, workflows e site enviados à branch `main`.

A API de perfil comunitário reportou 100% dos critérios considerados por ela;
isso não é certificação de segurança ou qualidade técnica. Os resumos permanentes
ficam no Git; artifacts do CI têm retenção de 30 dias. O contato de segurança foi
atualizado para incluir o canal privado do GitHub agora habilitado.

## Adendo — proteção da branch e verificação pública

A API confirmou `main` protegida com os checks `Tools and sanitizers` e
`Native Linux VM`, branch atualizada antes de merge, PR obrigatório para quem não
pode ignorar a regra e resolução de conversas. Force-push e exclusão desabilitados.
Zero aprovações obrigatórias evita bloquear um projeto com um único mantenedor;
a política de revisão continua em CONTRIBUTING.md. Administradores mantêm bypass
(`enforce_admins: false`), uma exceção explícita, não proteção absoluta.

A homepage, o guia e o CSS publicados responderam HTTP 200. A página pública foi
aberta e verificada no navegador. A configuração de Pages exige HTTPS. Nenhuma
mudança no código nativo ocorreu após o CI bem-sucedido mencionado acima.
