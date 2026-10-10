# 0003 — Base open source e GitHub

Data: 2026-10-10. Estado: base preparada; publicação e CI remoto em verificação.

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
