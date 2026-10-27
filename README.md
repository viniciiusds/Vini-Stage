# Vini Stage

Instrumento **Vini Stage**: um VST3 simples para carregar arquivos Nord `.npno` diretamente no Ableton Live 11. Clique em **Select A** ou **Select B**, escolha um arquivo e toque por MIDI.

![Editor compilado com 88 teclas, logo em minúsculas e painel sem parafusos](docs/interface-0.5.3.png)

Layout com teclado de **88 teclas (A0–C8)** na parte inferior, clicável com o mouse e com destaque das notas recebidas por MIDI. Logo `vini stage` ao lado de Levels, painel sem parafusos, botões Select/Clear menores e faders/LEDs alongados para cima, com margens iguais acima e abaixo dos LEDs. A imagem é uma captura direta do editor compilado, com um acorde MIDI demonstrativo.

**Correção de arraste incluída no instalador 0.4.3:** os faders preservam o ponto onde a alça foi segurada e atualizam volume, LEDs e visor durante o movimento, antes de soltar o mouse. O teste do editor envia três posições de arraste para cada fader e confere as alterações de parâmetro antes do mouse-up. Duplo clique continua retornando a 0 dB.

**Ajuste 0.5.4:** sample de release proporcional à velocity MIDI original, com ganho máximo de **10%** em velocity 127. A soltura da voz principal volta a **90 ms**, respeitando o sustain.

**Ajuste 0.5.2:** soltura da voz principal ampliada de 90 para 500 ms, com soltura independente do volume do sample de release. A comparação do ruído inicial e do decaimento com a tecla mantida ainda precisa de áudio do Sharp ativado no High Sierra; veja o [relatório e o MIDI de comparação](docs/comparison/2026-10-26-royal-decay-noise.md). Nenhum filtro foi aplicado para mascarar o som sem essa medição.

## Versão 0.5.4 em teste — sustain, dinâmica e release

- Caudas sustentadas usam os pontos de loop do `.npno`, com limites validados e decaimento após entrar na região de repetição. O player não encerra a voz simplesmente por chegar ao fim da gravação.
- Drive interno padrão **−0,50**, com curva própria `x²` sobre a velocity MIDI (arredondada a 1–127). Afeta a camada escolhida e o ganho, sem o piso anterior de 30%. Não reproduz matematicamente o Drive do Ableton ou do Sharp; comece com Velocity do Live neutro e depois faça os ajustes finos.
- Seleção por valores e espaçamento de camada na escala da biblioteca, em vez de redistribuir igualmente apenas a ordem das gravações. Calibração musical ainda aproximada.
- Samples de release em **até 10% de ganho**, proporcional à velocity MIDI original (antes do Drive interno), independente da duração e do decaimento da voz principal. A gravação escolhida também é fixa por nota; o fader A/B continua controlando a saída total. Só disparam ao abafar a nota: são adiados com CC64 e não disparam em note-off sem voz ativa nem em pedal-up repetido. Entrada/saída suavizadas para evitar cliques.
- Teste nativo do Royal Grand 3D XL: caudas de 14 s em 44,1/48 kHz, notas graves/centrais/transpostas, pedal antes/depois do note-on, medida de release proporcional com máximo de 10% e comparação entre velocities 16/64/127, notas curtas/longas e soltura do pedal, e regressões de duas camadas/estado/volume. O teste da versão publicada 0.4.3 reproduziu o corte; a nova passou. A audição no High Sierra com o Arturia ainda é necessária para aprovar estas mudanças.

**[Baixar Vini Stage 0.5.4 Universal.pkg — versão de teste](https://github.com/viniciiusds/Vini-Stage/releases/download/v0.5.4/Vini-Stage-0.5.4-Universal.pkg)**

A versão 0.5.4 está publicada como pré-lançamento para audição no High Sierra/Arturia. As versões anteriores continuam disponíveis em Releases. O usuário confirmou que a versão anterior executa no MacBook High Sierra; isso não substitui a validação auditiva desta nova versão.

## Instalador Universal `.pkg`

[Todas as versões e instaladores](https://github.com/viniciiusds/Vini-Stage/releases)

O instalador contém o mesmo VST3 Universal para **Intel/High Sierra 10.13** e **Apple Silicon/M4**. Feche o Live, abra o `.pkg`, siga o assistente e informe a senha de administrador. Ele instala para todos os usuários em `/Library/Audio/Plug-Ins/VST3/Vini Stage.vst3`.

As próximas versões usam o mesmo identificador de pacote e substituem o bundle antigo nesse caminho. O pacote não é relocável e não sobrescreve uma versão global mais nova. Uma cópia anterior da pasta VST3 do usuário conectado, com a mesma identidade e versão igual ou inferior, é movida para `~/Library/Application Support/Vini Stage/Plugin Backups/` para evitar duplicidade; versões mais novas ou plugins com outra identidade não são movidos. Projetos e arquivos `.npno` não são alterados.

O pacote atual é de teste, **sem assinatura Developer ID Installer e sem notarização**; o macOS pode impedir sua abertura até haver aprovação pelo usuário. A experiência de distribuição sem alertas exige certificados Developer ID e notarização da Apple. O binário declara mínimo 10.13 para Intel, mas a instalação/execução real no High Sierra continua dependendo de validação no Mac de 2011. Os testes de pacote conferem conteúdo e scripts em diretórios temporários, sem instalar no sistema.

`build.sh` também gera o instalador em `dist/`. Para empacotar um VST3 já compilado:

```bash
bash package.sh
python3 packaging/test_package.py dist/Vini-Stage-0.5.4-Universal.pkg
```

O nome e a versão do `.pkg` são obtidos do VST3 compilado. Para assinar o pacote, defina `INSTALLER_SIGN_IDENTITY` com a identidade de um certificado **Developer ID Installer** disponível no chaveiro. Isso não faz a notarização automaticamente; o plugin e o pacote de distribuição ainda precisam do fluxo de assinatura e notarização apropriado.

## Recursos atuais — 0.5.4

- Carregamento de `.npno` em segundo plano, com **Loading... X%** no visor e sem conversão manual para WAV. O percentual acompanha a leitura do arquivo e a decodificação das gravações.
- Mapeamento de teclas, transposição, ajuste fino e camadas de velocity.
- Dois timbres opcionais A/B em um único visor **Sounds**. Sem timbre, a linha fica apenas **A:** ou **B:** e não produz som.
- **Select A/B** abre o arquivo; após a seleção, o mesmo botão passa a **Clear A/B**. Clear remove apenas aquele timbre. Para trocar, use Clear e depois Select. Também é possível cancelar um carregamento usando Clear.
- Painel vermelho, área cinza única à esquerda com faders A/B, LEDs verdes à direita de cada fader e visor maior à direita com fonte reduzida. Sem botões ON/OFF; teclado visual de 88 teclas na parte inferior.
- Visores em dB com a largura do conjunto fader + LEDs, botões Select/Clear alinhados sob o visor Sounds.
- Faders e LEDs mais altos, com 20 px entre o visor em dB e o topo dos LEDs e 20 px entre a base dos LEDs e o separador acima de A/B.
- Volume de −60 a 0 dB em passos de 1 dB, com posição adicional de silêncio (−∞). Visores em dB editáveis; máximo de 0 dB.
- Samples mono/estéreo, 128 vozes por seção e sustain CC64.
- Volumes automatizáveis, com transições suavizadas. Os parâmetros ON/OFF antigos continuam disponíveis no host para preservar automações e estados anteriores, mas não aparecem no painel. Selecionar um novo arquivo reativa essa camada; projetos antigos com OFF continuam respeitando esse estado.
- Os dois caminhos de timbre, os volumes e ON/OFF são salvos no estado do plugin. Volumes de projetos anteriores são arredondados para o dB inteiro mais próximo, preservando os IDs dos parâmetros. Projetos antigos com um único timbre abrem na seção Sound 1.
- Note-off com fade suave da voz principal e sample de release proporcional à velocity, limitado a 10%, com soltura principal de 90 ms, adiado enquanto o sustain segura a nota.
- VST3 e aplicativo standalone para macOS, com entrada MIDI. O `.pkg` instala o VST3; o standalone é gerado na compilação.

## Compatibilidade

O build Universal contém **Intel x86_64 com mínimo macOS 10.13** e **Apple Silicon arm64 com mínimo 11.0**. O alvo principal de execução é um MacBook Pro 2011 com High Sierra; o desenvolvimento acontece no M4.

A versão mínima e as duas arquiteturas foram verificadas no binário. O usuário confirmou que a versão publicada funciona no MacBook High Sierra com um controlador Arturia. As mudanças de áudio da versão 0.5.4 ainda precisam ser ouvidas nesse conjunto. Os testes automatizados executaram o VST3 em um host JUCE no M4: Clavinet D6 6.1 e Royal Grand 3D XL 6.1, cada um com 440 combinações de nota/velocity, além de note-off e sustain.

## Compilar no Mac de desenvolvimento

Requisitos: Apple Command Line Tools, Git e Rust via rustup (1.87 ou posterior). Se não houver CMake instalado, o script usa Python 3 para instalá-lo em um ambiente local `.build-tools/`. A primeira execução precisa de internet para baixar as ferramentas e dependências.

```bash
bash build.sh
```

O script usa JUCE 8.0.12 e `nord-format 0.8.0`, compila as duas arquiteturas e produz:

```text
build/NpnoPlayer_artefacts/Release/VST3/Vini Stage.vst3
build/NpnoPlayer_artefacts/Release/Standalone/Vini Stage.app
```

**Cada compilação bem-sucedida do VST3 assina, verifica e instala automaticamente** em:

```text
~/Library/Audio/Plug-Ins/VST3/Vini Stage.vst3
```

Feche o Live antes de atualizar o plugin. Reabra e use Rescan se necessário. Para um build apenas de empacotamento, sem instalar, configure `-DNPNO_INSTALL_AFTER_BUILD=OFF`.

Para reutilizar uma cópia local do JUCE, defina `JUCE_PATH` antes de executar `build.sh`. O CMake também aceita `-DJUCE_PATH=/caminho/do/JUCE`. A revisão usada inicialmente foi `29396c22c93392d6738e021b83196283d6e4d850`.

## Testar o VST3

```bash
cmake --build build --target NpnoSmoke --config Release -j 4
build/NpnoSmoke_artefacts/Release/NpnoSmoke \
  "$PWD/build/NpnoPlayer_artefacts/Release/VST3/Vini Stage.vst3" \
  "/caminho/para/piano.npno" \
  "/caminho/para/segundo-piano.npno"
```

A prévia compilada verifica as 88 teclas visíveis e o destaque por note-on/note-off em MIDI. A interação por clique usa o componente MidiKeyboardComponent do JUCE e precisa de conferência manual na janela nativa.

O teste verifica também os passos de 1 dB, a atenuação medida no áudio, o teto de 0 dB, o mute em −∞ e a migração dos volumes de projetos antigos.

O segundo arquivo é opcional no comando de teste. Quando informado, o teste também verifica soma dos timbres, volumes independentes, ON/OFF, uso apenas da seção 2, arquivo ausente e salvamento/reabertura em uma nova instância.

O teste de 88 teclas pressupõe um arquivo que cubra MIDI 21–108. Bibliotecas com outra extensão de teclado podem ter notas descobertas legítimas.

Se o CMake foi instalado pelo script, substitua `cmake` por `.build-tools/bin/cmake` no comando de teste.

## Estrutura

- `Source/Plugin.cpp`: motor de reprodução, estado e interface.
- `bridge/src/lib.rs`: leitor/decodificador Rust e interface C.
- `Source/Smoke.cpp` e `Source/LayerSmoke.cpp`: testes usando o VST3 compilado em um host.
- `Source/Preview.cpp`: renderização do editor real do plugin para conferir o layout, sem abrir uma DAW.
- `docs/USO.md`: instalação, funcionamento e limites musicais.

## Limites desta versão

A versão atual usa curva própria de velocity, loops validados e decaimento na cauda. Os pontos exatos de troca de velocity e o envelope original da Nord não estão calibrados; meio-pedal e modelagem exata de ressonância não estão implementados. O campo de trim por gravação não é aplicado, pois sua interpretação disponível silenciava praticamente os agudos no Clavinet testado. O projeto não garante reprodução idêntica à Nord ou ao Sharp.

Nenhum sample ou biblioteca Nord é incluído neste repositório. O código não utiliza implementação ou recursos do Sharp Stage Pianos.

## Licenças

Este projeto é disponibilizado sob AGPL-3.0; veja `LICENSE`. JUCE tem licenciamento AGPL/comercial e `nord-format` usa BSD-3-Clause; seus avisos estão em `Licenses/`.

- JUCE: https://github.com/juce-framework/JUCE
- nord-format: https://docs.rs/nord-format/0.8.0/nord_format/

## Renderizar a interface real

```bash
cmake --build build --target NpnoPreview --config Release -j 4
build/NpnoPreview_artefacts/Release/NpnoPreview "$PWD/interface.png" \
  "/caminho/para/grand.npno" \
  "/caminho/para/clavinet.npno"
```

Os dois arquivos são opcionais na prévia. A ferramenta testa os controles do editor e renderiza o layout real, com níveis demonstrativos de −3 dB e −8 dB. Quando recebe dois arquivos, testa o botão Clear B e captura A carregado e B vazio. A imagem no início deste README vem desse editor, não de um mockup. Adicione `--unity` após os dois caminhos para renderizar ambos os faders em 0 dB e conferir a margem máxima.

O progresso real do leitor pode ser verificado separadamente:

```bash
cmake --build build --target NpnoProgress --config Release -j 4
build/NpnoProgress "/caminho/para/piano.npno"
```

## Testar sustain, Drive e release

```bash
cmake --build build --target NpnoSmoke --config Release -j 4
build/NpnoSmoke_artefacts/Release/NpnoSmoke \
  "$PWD/build/NpnoPlayer_artefacts/Release/VST3/Vini Stage.vst3" \
  "/caminho/para/Royal Grand 3D XL 6.1.npno" --playability "/pasta/para/audios-de-teste"
```

Este modo exige um piano com loops e samples de release, como o Royal testado. Os WAVs gerados contêm áudio da biblioteca fornecida pelo usuário e não devem ser commitados nem distribuídos com o código. A fixture sintética para testes de duas camadas pode ser gerada com `cargo run --manifest-path bridge/Cargo.toml --example test_tone -- /tmp/test-sine.npno`.
