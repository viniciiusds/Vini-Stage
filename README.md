# NordPlayer

Instrumento **NPno Player**: um VST3 simples para carregar arquivos Nord `.npno` diretamente no Ableton Live 11. Clique em **Select Sound**, escolha um arquivo e toque por MIDI.

## Recursos atuais — 0.1.1

- Carregamento de `.npno` em segundo plano, sem conversão manual para WAV.
- Mapeamento de teclas, transposição, ajuste fino e camadas de velocity.
- Samples mono/estéreo, 128 vozes e sustain CC64.
- Caminho do timbre salvo no estado do plugin.
- Note-off com fade suave, sem disparar o sample extra de release que causava ruído no grand piano.
- VST3 e aplicativo standalone para macOS, com interface mínima e teclado visual.

## Compatibilidade

O build Universal contém **Intel x86_64 com mínimo macOS 10.13** e **Apple Silicon arm64 com mínimo 11.0**. O alvo principal de execução é um MacBook Pro 2011 com High Sierra; o desenvolvimento acontece no M4.

A versão mínima e as duas arquiteturas foram verificadas no binário. A execução no High Sierra e a integração real com o Live ainda precisam de validação manual. Os testes automatizados executaram o VST3 em um host JUCE no M4: Clavinet D6 6.1 e Royal Grand 3D XL 6.1, cada um com 440 combinações de nota/velocity, além de note-off e sustain.

## Compilar no Mac de desenvolvimento

Requisitos: Apple Command Line Tools, Git e Rust via rustup (1.87 ou posterior). Se não houver CMake instalado, o script usa Python 3 para instalá-lo em um ambiente local `.build-tools/`. A primeira execução precisa de internet para baixar as ferramentas e dependências.

```bash
bash build.sh
```

O script usa JUCE 8.0.12 e `nord-format 0.8.0`, compila as duas arquiteturas e produz:

```text
build/NpnoPlayer_artefacts/Release/VST3/NPno Player.vst3
build/NpnoPlayer_artefacts/Release/Standalone/NPno Player.app
```

**Cada compilação bem-sucedida do VST3 assina, verifica e instala automaticamente** em:

```text
~/Library/Audio/Plug-Ins/VST3/NPno Player.vst3
```

Feche o Live antes de atualizar o plugin. Reabra e use Rescan se necessário. Para um build apenas de empacotamento, sem instalar, configure `-DNPNO_INSTALL_AFTER_BUILD=OFF`.

Para reutilizar uma cópia local do JUCE, defina `JUCE_PATH` antes de executar `build.sh`. O CMake também aceita `-DJUCE_PATH=/caminho/do/JUCE`. A revisão usada inicialmente foi `29396c22c93392d6738e021b83196283d6e4d850`.

## Testar o VST3

```bash
cmake --build build --target NpnoSmoke --config Release -j 4
build/NpnoSmoke_artefacts/Release/NpnoSmoke \
  "$PWD/build/NpnoPlayer_artefacts/Release/VST3/NPno Player.vst3" \
  "/caminho/para/piano.npno"
```

O teste de 88 teclas pressupõe um arquivo que cubra MIDI 21–108. Bibliotecas com outra extensão de teclado podem ter notas descobertas legítimas.

Se o CMake foi instalado pelo script, substitua `cmake` por `.build-tools/bin/cmake` no comando de teste.

## Estrutura

- `Source/Plugin.cpp`: motor de reprodução, estado e interface.
- `bridge/src/lib.rs`: leitor/decodificador Rust e interface C.
- `Source/Smoke.cpp`: teste usando o VST3 compilado em um host.
- `docs/USO.md`: instalação, funcionamento e limites musicais.

## Limites desta versão

A velocity usa camadas ordenadas e uma curva própria; os pontos exatos de troca ainda não foram validados com a Nord. Loops, decay original, meio-pedal e modelagem exata de ressonância não estão implementados. O campo de trim por gravação não é aplicado, pois sua interpretação disponível silenciava praticamente os agudos no Clavinet testado. O projeto não garante reprodução idêntica à Nord ou ao Sharp.

Nenhum sample ou biblioteca Nord é incluído neste repositório. O código não utiliza implementação ou recursos do Sharp Stage Pianos.

## Licenças

Este projeto é disponibilizado sob AGPL-3.0; veja `LICENSE`. JUCE tem licenciamento AGPL/comercial e `nord-format` usa BSD-3-Clause; seus avisos estão em `Licenses/`.

- JUCE: https://github.com/juce-framework/JUCE
- nord-format: https://docs.rs/nord-format/0.8.0/nord_format/
