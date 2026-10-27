# Componentes do instalador

`package.sh` requer os builds Universal VST3 e Standalone da mesma versão. Produz um distribution package com `customize="always"` e escolhas independentes inicialmente selecionadas.

| Choice | Receipt | Destino |
|---|---|---|
| vst3 | audio.vinizera.vinistage.vst3 | /Library/Audio/Plug-Ins/VST3/Vini Stage.vst3 |
| standalone | audio.vinizera.vinistage.standalone | /Applications/Vini Stage.app |
| resources (oculto) | audio.vinizera.vinistage.resources | /Library/Application Support/Vini Stage/ |

O receipt VST3 existente permanece estável para upgrade dos pacotes antigos. O receipt do aplicativo é separado. Os bundles não são relocáveis e usam checagem de versão, identidade estrita e ação de upgrade. O payload standalone não contém plugin nem scripts de migração; o payload VST3 não contém aplicativo. Licenças/documentação são instaladas quando pelo menos um dos dois é selecionado, por expressão `choices.vst3.selected || choices.standalone.selected`.

Atributos e expressões seguem a [referência de Distribution XML da Apple](https://developer.apple.com/library/archive/documentation/DeveloperTools/Reference/DistributionDefinitionRef/Chapters/Distribution_XML_Ref.html). As escolhas visíveis usam `start_selected`, sem expressão dinâmica `selected` que sobrescreveria o clique do usuário.

`test_package.py` expande o pacote em diretório temporário e verifica conteúdo, isolamento das escolhas, metadados, arquiteturas, destinos, assinaturas e migração de VST3. Não chama instalação real. Isso não equivale a executar o instalador em um Mac High Sierra.
