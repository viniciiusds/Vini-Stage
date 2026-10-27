# Vini Stage 0.5.2 — versão de teste

Instrumento VST3 com botões **Select A** e **Select B** para abrir um arquivo `.npno` diretamente. Os samples são decodificados em memória; não é necessário converter ou importar WAVs.

## Instalar e tocar

### Pelo instalador `.pkg`

Feche o Ableton, abra `Vini-Stage-<versão>-Universal.pkg`, siga o assistente e informe a senha de administrador. O mesmo arquivo serve para Intel (alvo macOS 10.13+) e Apple Silicon (11+). O destino é `/Library/Audio/Plug-Ins/VST3/Vini Stage.vst3`, disponível para todos os usuários. Abra o Live novamente após instalar.

Uma versão nova substitui a anterior na pasta global. Para evitar duplicidade, uma cópia igual ou mais antiga na pasta VST3 do usuário conectado é guardada em `~/Library/Application Support/Vini Stage/Plugin Backups/`. Versões mais novas e plugins com outra identidade são preservados. Timbres e projetos não são alterados.

Este pacote ainda não tem certificado Developer ID Installer nem notarização da Apple; portanto não há garantia de abertura sem alerta no macOS. A instalação real no High Sierra precisa ser conferida no Mac de 2011.

### Instalação manual / desenvolvimento

1. Feche o Ableton antes de instalar ou atualizar o plugin.
2. Neste M4, as novas compilações já instalam o plugin automaticamente em `~/Library/Audio/Plug-Ins/VST3/`. Para instalar em outro Mac, copie **Vini Stage.vst3** para essa pasta ou execute `Instalar.command`.
3. No Live 11, abra Preferences → Plug-Ins e habilite **Use VST3 Plug-In System Folders**. Use **Rescan** se necessário.
4. Em Plug-Ins → VST3 → Vinizera, coloque **Vini Stage** em uma pista MIDI.
5. Clique em **Select A** ou **Select B**, escolha seu `.npno` e acompanhe **A: Loading... X%** ou **B: Loading... X%** no visor Sounds até aparecer o nome. Basta carregar um timbre; o outro é opcional.
6. Após selecionar um arquivo, o botão muda para **Clear A/B**. Clique para remover aquele timbre, inclusive durante o carregamento: a linha fica **A:** ou **B:** e o botão volta a Select. Para trocar de timbre, use Clear e depois Select.
7. Cada timbre tem um fader de **−60 a 0 dB**, em passos de **1 dB**, mais a posição **−∞ dB** para silêncio. O visor permite digitar o valor; setas e roda do mouse alteram 1 dB, duplo clique retorna a 0 dB. Os LEDs à direita indicam o volume configurado, mesmo quando o espaço está vazio; não são medidores de áudio.
8. Toque pelo teclado MIDI ou clique nas 88 teclas (A0–C8) na parte inferior. As notas recebidas por MIDI ficam destacadas em vermelho e apagam ao soltar. Ambos os timbres recebem o mesmo MIDI e são somados na saída estéreo; reduza os volumes se necessário. CC64 controla o sustain.

Não há botões ON/OFF neste layout. Os parâmetros ON/OFF anteriores são preservados no host para compatibilidade com projetos/automação: um estado antigo OFF permanece silencioso. Selecionar um novo arquivo reativa a camada.
Os caminhos dos dois `.npno`, os volumes e ON/OFF são guardados no projeto do Ableton. Ao reabrir, o plugin recarrega os timbres automaticamente. Cada arquivo `.npno` precisa permanecer no mesmo caminho para reabrir o projeto com o timbre. Se mudar o arquivo de lugar ou trocar de computador, selecione-o novamente. O plugin salva a referência ao arquivo; não inclui os samples no projeto.

## Computadores

O mesmo pacote contém duas arquiteturas:

- **Intel x86_64:** compilado com mínimo macOS **10.13**, para teste no MacBook Pro 2011.
- **Apple Silicon arm64:** mínimo macOS **11.0**, executado e testado neste M4.

A declaração de versão mínima foi conferida no Mach-O. O usuário confirmou o funcionamento da versão anterior no MacBook High Sierra com controlador Arturia. Esta nova versão de áudio precisa de audição nesse conjunto. Os testes automatizados usam um host VST3 JUCE no M4 e não substituem a conferência no Ableton/Arturia.

## O que funciona nesta versão

- Seleção de `.npno` por janela nativa do macOS.
- Carregamento fora da thread de áudio, mantendo o timbre anterior em caso de erro.
- Mapa de teclas do arquivo, transposição e ajuste fino por tecla.
- Samples mono/estéreo, 128 vozes por seção, velocity MIDI 1–127 e sustain CC64.
- Seleção automática de camadas e gravações de ressonância quando disponíveis. Na versão 0.5.1, o release toca em 40% de ganho fixo, independente da velocity e da cauda da voz principal, com suavização e respeitando o pedal.
- Caminho do timbre salvo no estado do plugin.

## Limites musicais atuais

Esta é uma implementação inicial própria, não uma reprodução exata do teclado Nord ou do Sharp. Usa curva própria de Drive −0,50 (x²) e valores de camada na escala da biblioteca; os pontos de troca e a resposta exata ainda não foram calibrados com o instrumento original. Loops válidos prolongam as caudas com decaimento, e a voz principal mantém soltura de 500 ms acompanhada do sample de release a 40% fixos. O envelope original completo, meio-pedal e ressonância por modelagem ainda não foram implementados. O nível bruto dos samples é preservado; o campo de trim não é aplicado porque a interpretação disponível deixou agudos praticamente inaudíveis no Clavinet testado.

Foi validado **Clavinet D6 6.1.npno**, stream `0x0464`: 38 gravações, 17 notas-raiz. As 88 teclas de 21 a 108 responderam em cinco velocities (440 combinações), com áudio finito e não silencioso. Note-off e sustain passaram. Isso não garante suporte a todas as bibliotecas `.npno`.

## Código e dependências

O código do projeto está no repositório Vini-Stage. Foi usada a revisão `29396c22c93392d6738e021b83196283d6e4d850` do JUCE, selecionada pela tag 8.0.12, e `nord-format 0.8.0` (BSD-3-Clause). Licenças e referências estão em `Licenses/`. Nenhum sample Nord está incluído no pacote de instalação.

## Correção 0.1.1

Desativado o disparo de samples de release em cada note-off, que causava uma ressonância extra no Royal Grand 3D XL 6.1. A nota principal mantém a soltura suave de 90 ms; com CC64 acionado, continua até o pedal ser liberado ou a gravação chegar ao fim. A polifonia permanece em 128 vozes.

## Versão 0.2.0

Duas seções independentes e opcionais com seleção de arquivo, ON/OFF e volume horizontal. Desligar uma seção faz um fade curto sem afetar a outra; religar exige uma nova nota e não recupera notas antigas. O estado de ambas é salvo, inclusive se um arquivo ainda está carregando. Projetos antigos com um arquivo continuam abrindo na seção Sound 1.

## Versão 0.3.0

Novo painel vermelho e cinza conforme o layout aprovado, quatro parafusos externos e nenhum interno. Faders verticais em passos de 1 dB, LEDs e visores numéricos. O máximo é 0 dB (ganho unitário), o menor nível audível é −60 dB e a posição inferior é −∞ (mute). A soma de dois timbres pode exigir reduzir seus volumes. Os níveis de projetos antigos são convertidos para o dB inteiro mais próximo; os caminhos, ON/OFF e os IDs de automação foram preservados.

## Versão 0.3.1

Progresso real de leitura e decodificação no visor do timbre (Loading... X%). Botões Select Sound e ON/OFF centralizados pelo visor do timbre, sem o texto Volume. Visor em dB com a largura do conjunto fader + LEDs, e coluna de LEDs alinhada aos limites verticais da trilha do fader.

## Versão 0.3.2

Conjunto fader + LEDs reduzido um pouco na altura, mantendo o alinhamento e a base. A alça em 0 dB fica com margem de 20 px abaixo do visor. Sem alterações no volume, nos timbres ou no estado salvo dos projetos.

## Versão 0.4.0

Visor Sounds único com linhas A/B, painel cinza de volumes à esquerda e LEDs à direita dos faders. Logotipo com Vini mais espesso que Stage. Select/Clear no mesmo botão, cancelamento de carregamento e remoção independente; espaços vazios persistem no projeto. Sem botões ON/OFF nem teclado visual. Mantidos os IDs de parâmetros e o estado legado.

## Versão 0.4.1

Topo do painel Levels alinhado ao topo do visor Sounds. Painel e faders 14 px mais curtos, mantendo a base e a margem entre os visores em dB e as alças.

## Versão 0.4.2

Retorno do teclado visual de 88 teclas, com clique e destaque de notas MIDI. Logo em minúsculas à esquerda de Levels, sem parafusos, menor margem superior e botões Select/Clear menores. Faders e LEDs alongados para cima, com margens de 20 px acima e abaixo dos LEDs e separadores sobre as letras A/B. Captura do editor compilado no README.

## Versão 0.4.3

Correção do arraste dos faders A/B: a alça mantém o ponto onde foi segurada e atualiza volume, LEDs e visor em cada passo de 1 dB durante o movimento, antes de soltar o mouse. Preservados o duplo clique para 0 dB e os gestos de automação do host.

## Versão 0.5.0 — audição de teste

Corrige o corte na chegada ao fim da gravação: usa o início e o comprimento de loop do arquivo, validados contra os limites da gravação, para a cauda de notas seguradas ou com sustain. O decaimento continua ao repetir, sem manter um som constante indefinidamente. O note-off sob sustain não inicia release; o sample de release toca ao abafar a nota, a 50% do ganho relativo à voz original, respeitando a atenuação já acumulada.

Drive padrão interno −0,50; a implementação própria usa x² e não é uma cópia da curva do Ableton. Para comparar, comece com o efeito Velocity do Live neutro e depois faça o ajuste fino. Mantidos o layout, os IDs de parâmetros e os caminhos salvos. O teste de 14 segundos no Royal passou; a confirmação no MacBook High Sierra com Arturia ainda depende de audição do usuário.

## Versão 0.5.1 — release fixo

Nesta versão, o sample de release usa 40% de ganho fixo e uma gravação fixa por nota. Esse nível não acompanha a intensidade da nota, o Drive, o tempo segurado ou o decaimento da voz principal. O fader da camada continua controlando a saída total. Mantidos o adiamento por sustain e o bloqueio de disparos repetidos. Substitui o comportamento de 50% relativo do instalador 0.5.0.

## Versão 0.5.2 — soltura da voz principal

Amplia a rampa de note-off de 90 para 500 ms, usando como referência o parâmetro padrão observado na instalação do Sharp. Mantém o release em 40% fixos e o sustain. A hipótese sobre o “ar” inicial e o decaimento com a tecla mantida ainda requer comparação dos WAVs no High Sierra; o Sharp do M4 não está ativado. Consulte o relatório em `docs/comparison/2026-10-26-royal-decay-noise.md`.
