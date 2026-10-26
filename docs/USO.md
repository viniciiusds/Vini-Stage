# Vini Stage 0.3.0 — versão de teste

Instrumento VST3 com botão **Select Sound** para abrir um arquivo `.npno` diretamente. Os samples são decodificados em memória; não é necessário converter ou importar WAVs.

## Instalar e tocar

1. Feche o Ableton antes de instalar ou atualizar o plugin.
2. Neste M4, as novas compilações já instalam o plugin automaticamente em `~/Library/Audio/Plug-Ins/VST3/`. Para instalar em outro Mac, copie **Vini Stage.vst3** para essa pasta ou execute `Instalar.command`.
3. No Live 11, abra Preferences → Plug-Ins e habilite **Use VST3 Plug-In System Folders**. Use **Rescan** se necessário.
4. Em Plug-Ins → VST3 → Vinizera, coloque **Vini Stage** em uma pista MIDI.
5. Em **Sound 1** ou **Sound 2**, clique em **Select Sound**, escolha seu `.npno` e aguarde o nome aparecer. Basta carregar uma das seções; a segunda é opcional.
6. Cada seção tem seu próprio botão **ON/OFF** e um fader vertical **Volume** em passos de **1 dB**, de **−60 dB a 0 dB**, com uma posição adicional **−∞ dB** que silencia o timbre. Os dois timbres recebem o mesmo MIDI e são somados na saída estéreo. Se a soma ficar alta, reduza os volumes.
7. O visor acima do fader mostra o valor em dB e permite digitá-lo. As setas e cada passo da roda do mouse alteram 1 dB; duplo clique no fader retorna a 0 dB. Os LEDs verdes indicam a posição do fader; o LED vermelho mostra se a seção está ON ou OFF.
8. Toque pelo teclado MIDI ou use o teclado visual. CC64 controla o sustain.

Os caminhos dos dois `.npno`, os volumes e ON/OFF são guardados no projeto do Ableton. Ao reabrir, o plugin recarrega os timbres automaticamente. Cada arquivo `.npno` precisa permanecer no mesmo caminho para reabrir o projeto com o timbre. Se mudar o arquivo de lugar ou trocar de computador, selecione-o novamente. O plugin salva a referência ao arquivo; não inclui os samples no projeto.

## Computadores

O mesmo pacote contém duas arquiteturas:

- **Intel x86_64:** compilado com mínimo macOS **10.13**, para teste no MacBook Pro 2011.
- **Apple Silicon arm64:** mínimo macOS **11.0**, executado e testado neste M4.

A declaração de versão mínima foi conferida no Mach-O. Não foi possível executar o pacote no High Sierra nesta máquina; a compatibilidade real com esse sistema precisa ser confirmada no Mac de 2011. A interface e a integração com o Ableton também precisam do teste manual: o controle de aplicativos não estava autorizado nesta sessão. O teste automatizado usou um host VST3 JUCE e o plugin compilado.

## O que funciona nesta versão

- Seleção de `.npno` por janela nativa do macOS.
- Carregamento fora da thread de áudio, mantendo o timbre anterior em caso de erro.
- Mapa de teclas do arquivo, transposição e ajuste fino por tecla.
- Samples mono/estéreo, 128 vozes por seção, velocity MIDI 1–127 e sustain CC64.
- Seleção automática de camadas e gravações de ressonância quando disponíveis. Na versão 0.1.1, o note-off apenas reduz suavemente a voz que já toca; não dispara samples extras de release.
- Caminho do timbre salvo no estado do plugin.

## Limites musicais atuais

Esta é uma implementação inicial própria, não uma reprodução exata do teclado Nord ou do Sharp. As camadas são ordenadas por suavidade e distribuídas pela faixa de velocity; os pontos de troca exatos ainda não foram validados com o instrumento original. Usa release de 90 ms e toca as gravações até o fim: loops, curva exata de dinâmica, coeficientes de decay, meio-pedal e ressonância por modelagem ainda não foram implementados. O nível bruto dos samples é preservado; o campo de trim não é aplicado porque a interpretação disponível deixou agudos praticamente inaudíveis no Clavinet testado.

Foi validado **Clavinet D6 6.1.npno**, stream `0x0464`: 38 gravações, 17 notas-raiz. As 88 teclas de 21 a 108 responderam em cinco velocities (440 combinações), com áudio finito e não silencioso. Note-off e sustain passaram. Isso não garante suporte a todas as bibliotecas `.npno`.

## Código e dependências

O código do projeto está no repositório Vini-Stage. Foi usada a revisão `29396c22c93392d6738e021b83196283d6e4d850` do JUCE, selecionada pela tag 8.0.12, e `nord-format 0.8.0` (BSD-3-Clause). Licenças e referências estão em `Licenses/`. Nenhum sample Nord está incluído no pacote de instalação.

## Correção 0.1.1

Desativado o disparo de samples de release em cada note-off, que causava uma ressonância extra no Royal Grand 3D XL 6.1. A nota principal mantém a soltura suave de 90 ms; com CC64 acionado, continua até o pedal ser liberado ou a gravação chegar ao fim. A polifonia permanece em 128 vozes.

## Versão 0.2.0

Duas seções independentes e opcionais com seleção de arquivo, ON/OFF e volume horizontal. Desligar uma seção faz um fade curto sem afetar a outra; religar exige uma nova nota e não recupera notas antigas. O estado de ambas é salvo, inclusive se um arquivo ainda está carregando. Projetos antigos com um arquivo continuam abrindo na seção Sound 1.

## Versão 0.3.0

Novo painel vermelho e cinza conforme o layout aprovado, quatro parafusos externos e nenhum interno. Faders verticais em passos de 1 dB, LEDs e visores numéricos. O máximo é 0 dB (ganho unitário), o menor nível audível é −60 dB e a posição inferior é −∞ (mute). A soma de dois timbres pode exigir reduzir seus volumes. Os níveis de projetos antigos são convertidos para o dB inteiro mais próximo; os caminhos, ON/OFF e os IDs de automação foram preservados.
