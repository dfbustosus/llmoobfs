# ADR-0003 — Formato experimental de leitura para M1

Data: 2026-10-10. Estado: adotado para o primeiro marco.

## Contexto

A proposta completa inclui log transacional, checkpoints, snapshots e namespace de revisões. O primeiro marco autorizado é menor: criar imagem, montar e ler dois arquivos independentes que compartilham conteúdo.

## Decisão

Criar um formato experimental de imagem somente para leitura, com superbloco, tabela de objetos, tabela de arquivos e payloads alinhados. O magic identifica esse formato M1 separadamente. Não será apresentado como implementação do formato transacional completo.

Cada arquivo tem inode próprio; entradas distintas podem apontar para o mesmo objeto físico. A ferramenta offline agrupa somente bytes idênticos. O módulo verifica estrutura, checksums e conteúdo durante a montagem, e serve leituras pelo cache de páginas/iomap.

Começar com namespace plano, até 128 arquivos/objetos, nomes de até 63 bytes e objetos de até 64 KiB. Uma imagem publicada é imutável e só pode ser montada em modo de leitura. Não implementar escrita, ioctls, checkpoints ou snapshots neste marco.

## Consequências

O marco permite validar o caminho real VFS → módulo → blocos e o compartilhamento, com testes reproduzíveis. O formato futuro exigirá conversão offline ou nova imagem, sem prometer compatibilidade binária. A proposta original permanece em `docs/native-mvp.md`.
