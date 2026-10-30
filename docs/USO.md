# Vini Stage 1.1.1 — versão de teste

Dyn Comp: clique para alternar OFF, 1, 2 e 3. O LED de baixo indica 1; o de cima, 2; ambos, 3. Em OFF, nenhum acende. Máximos de +2/+3/+4 dB para notas suaves, diminuindo até zero na velocity 127. O ajuste vale para A/B, é salvo com o projeto e afeta novas notas. Projetos anteriores abrem em OFF. A versão oficial 1.0.0 permanece sem Dyn Comp.

A versão compilada aparece no canto superior direito da interface.

## Instalação

Feche o Ableton e o Vini Stage standalone. Execute o PKG Universal e, na etapa Tipo de instalação, marque **VST3**, **Standalone** ou ambos; informe a senha de administrador. O VST3 é instalado em `/Library/Audio/Plug-Ins/VST3/Vini Stage.vst3` e o aplicativo em `/Applications/Vini Stage.app`. Um componente desmarcado não é removido nem atualizado. O mesmo pacote serve para Intel/High Sierra 10.13 e Apple Silicon/11+. As versões antigas do plugin são atualizadas; cópias antigas na pasta do usuário são guardadas em backup para evitar duplicidade. O pacote ainda não possui assinatura Developer ID Installer nem notarização.

O standalone usa o ícone vermelho com VS na parte superior e teclas de piano embaixo. Para tocar sem DAW, abra Aplicativos → Vini Stage e configure a entrada MIDI e a saída de áudio nos ajustes do aplicativo. Para o plugin no Ableton Live 11, habilite Use VST3 Plug-In System Folders e carregue VST3 → Vinizera → Vini Stage. Após atualizar, reabra o Live e faça Rescan se necessário.

## Timbres e controles

Select A e Select B abrem arquivos `.npno`. O visor mostra a leitura/decodificação como Loading... X% e depois o nome do timbre. Não é obrigatório preencher os dois slots. Após selecionar, o botão vira Clear A/B: remove apenas aquele timbre e também permite cancelar uma carga em andamento.

Os dois timbres recebem MIDI e são somados. Cada fader tem passos de 1 dB entre −60 e 0 dB, mais −∞. Duplo clique retorna a 0 dB. É possível editar o número no visor. Os LEDs indicam a posição do volume, não a amplitude do áudio.

O teclado de 88 teclas responde ao mouse e destaca notas MIDI recebidas. CC64 controla o sustain. Cada seção possui 128 vozes, usadas também pelos samples de release.

## Motor de áudio 0.6.5

Os coeficientes antes e durante o loop foram corrigidos; a associação anterior fazia a nota perder corpo cedo demais e manter uma cauda muito baixa por mais tempo. Cada fase preserva sua regra de transposição.

A saída recebe +10 dB fixos após a soma A+B, inclusive ao reabrir projetos antigos. Os faders mantêm o máximo de 0 dB. Não há limitação automática da soma; reduza os faders se a saída exceder 0 dBFS.

A entrada de velocity é neutra: foi removido o Drive interno −0,50. Comece com o efeito Velocity do Live neutro e depois ajuste conforme o controlador. Seleção da camada e ganho da voz são calculados separadamente. Trim por sample, correção por tecla, filtro por voz e decaimento por metadados substituem aproximações anteriores.

O release não usa mais 2% da velocity inicial. Ele acompanha a amplitude recente da nota, então uma nota que já decaiu tende a produzir menos release. O sustain adia o disparo até o abafamento. Note-offs sem nota ativa, repetições de pedal-up e comandos de panic não devem iniciar samples extras.

A soltura da voz principal é exponencial, com referência de 500 ms ajustada por sample; esse tempo corresponde à queda de −80 dB. Substitui os antigos 90 ms lineares por solicitação do usuário para aproximar o comportamento observado no Sharp.

Para comparar volume com o Sharp, alinhe os níveis de saída. O Vini não tem master separado; o padrão observado do master do Sharp era −6 dB. Desative reverb, EQ, compressão e efeitos MIDI compensatórios durante o teste. A curva foi comparada aos WAVs do Royal fornecidos pelo usuário; isso não comprova equivalência sonora em todas as notas/timbres ou ganho original igual entre os plugins.

## Memória e carregamento

O áudio decodificado é mantido em PCM16, preservando sua precisão. O buffer do Royal Grand 3D XL 6.1 caiu de aproximadamente 1,52 GB para 762 MB. O mesmo timbre pode ser compartilhado entre A/B e instâncias do mesmo módulo sem duplicar o buffer.

O cache reconhece caminho, tamanho e data do arquivo. Pode reter até 128 MiB adicionais de timbres ociosos. Um XL maior que esse limite só é reaproveitado enquanto alguma seção/instância ainda o mantém vivo. Não há cache de áudio persistente em disco nem streaming.

Cancelar verifica a solicitação durante a leitura e entre gravações; uma gravação já em decodificação termina antes de interromper. Os ganhos de velocidade no MacBook de 2011 ainda precisam ser medidos.

## Salvar projetos

O Ableton salva os caminhos dos dois `.npno`, os volumes e o estado legado ON/OFF. Ao reabrir, o plugin carrega os timbres novamente. Os arquivos precisam continuar no mesmo caminho; os samples não são embutidos no projeto. Um arquivo ausente em A não deve interromper B.

## Desenvolvimento

O build gera e instala uma cópia do VST3 em `~/Library/Audio/Plug-Ins/VST3/`. O standalone fica em `build/NpnoPlayer_artefacts/Release/Standalone/Vini Stage.app`; o PKG permite escolher VST3, standalone ou ambos. Não é necessário converter os timbres para WAV.

Veja [o README](../README.md) e [as notas da correção e validação](ENGINE-0.6.5.md) para validações e limites desta versão.
