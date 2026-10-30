# Dyn Comp — atualizado para a versão oficial 1.1.2

Curva própria para tornar notas suaves mais audíveis sem mudar as camadas escolhidas pela velocity. Não representa uma reprodução medida dos níveis do Nord físico.

- Parâmetro global `dynComp` acrescentado após os parâmetros existentes; OFF é o padrão. IDs antigos preservados.
- Para a velocity MIDI v, t = (v − 1) / 126 e peso = 1 − 3t² + 2t³. O ganho extra em dB é peso × máximo, com máximos 4, 8 e 12 dB.
- Velocity 1 recebe o máximo; 64 recebe metade; 127 não recebe aumento. As derivadas nos extremos são zero.
- Ganho aplicado uma vez ao iniciar cada voz principal, depois da seleção do sample. O estado do controle é lido por bloco; a mudança vale para novos note-ons.
- O ganho permanece constante durante a vida daquela voz, preservando sua curva de decaimento. Isso não significa ganho constante entre velocities.
- O seguidor de amplitude já existente observa a nota amplificada; o release utiliza esse seguidor sem reaplicar Dyn Comp.
- OFF multiplica por exatamente 1; mantém o processamento anterior. O ganho geral de +10 dB após A+B permanece aplicado uma vez.
- Botão e parâmetro do host alternam OFF/1/2/3, com automação e estado persistente. Estado antigo sem esse parâmetro restaura OFF, mesmo se o controle estava ligado.
- O aumento pode elevar a soma de acordes. Os faders mantêm máximo 0 dB, sem limitador novo.

Os níveis salvos continuam OFF/1/2/3. Ao atualizar, projetos com Dyn Comp ligado usam os novos ganhos. Na velocity 64, os níveis 1/2/3 acrescentam 2/4/6 dB.

## Verificação

O teste `NpnoSmoke plugin.vst3 sound.npno --dyn-comp` renderiza áudio real do VST3. Confere os três pontos de ganho em 44,1/48 kHz, os três níveis, pedal e release, estabilidade de uma nota durante mudança do parâmetro, soma de notas com velocities diferentes, B isolado e soma A+B, restauração de estado e migração de estado antigo para OFF. A comparação usa a forma de onda inteira escalada, para detectar mudanças de sample ou envelope.

O editor compilado verifica o ciclo do botão, faders e teclado. O instalador é verificado em diretórios temporários. Nova audição no High Sierra fica a cargo do usuário; não foi executada instalação real por esses testes.
