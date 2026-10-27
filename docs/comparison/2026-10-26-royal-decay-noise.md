# Royal Grand: soltura, decaimento e ruído inicial

## Escopo e condições

Investigação de 26/10/2026, com Royal Grand 3D XL 6.1.npno, Vini Stage 0.5.1 e Sharp Stage Pianos 2.1.0 instalado no Mac M4. O Sharp está ativado apenas no High Sierra, conforme informado pelo usuário. O host nativo enumerou seus parâmetros e seu estado normal, mas não obteve áudio dele nesta máquina. Nenhuma licença foi modificada. Não há comparação de áudio Sharp × Vini concluída.

Os dados abaixo são medições da saída do Vini e do PCM decodificado da biblioteca fornecida pelo usuário. WAVs/PCM da biblioteca não fazem parte do repositório.

## Soltura após note-off: diferença confirmada

O parâmetro público `release_ms` da instância padrão do Sharp indicou 500 ms; `long_release` e reverb estavam desligados. O Vini 0.5.1 usava uma rampa de 90 ms na voz principal. Isso explica uma diferença objetiva de configuração após soltar a tecla, mas não estabelece qual envelope a sessão do usuário usa no outro Mac.

A versão 0.5.2 passa a usar 500 ms na voz principal. O sample de release permanece em 40% fixos, com a mesma gravação por nota, independente de velocity e duração. O comportamento de sustain é preservado. O teste com uma fixture senoidal sem sample de release isola e verifica a rampa de 500 ms.

## Nota mantida e primeiros segundos: limites da conclusão

Na captura a 48 kHz, nota MIDI 60, velocity 127, sem sustain e sem efeitos, os primeiros 2,5 segundos da saída coincidem com a reprodução por interpolação linear do PCM decodificado, aplicando o ganho da biblioteca, headroom e fine tune de −4 unidades (−2,8 cents). Erro máximo observado: aproximadamente 3,9e−8 em amplitude float.

Nesse trecho ainda não atua o decaimento adicional da região de loop. Não se identificou uma fonte separada de ruído branco no motor; isso não prova que o som seja igual ao Sharp nem identifica perceptualmente o “ar” relatado. A conversão de 35.002 Hz para 48 kHz por interpolação linear tem imagens espectrais residuais: na janela de 0,2–1,5 s, energia entre 18–24 kHz de aproximadamente −63,9 dB em relação à energia total. Uma interpolação cúbica simulada reduz essa banda para cerca de −70,7 dB. Esses valores isolados não demonstram que esse seja o ruído ouvido pelo usuário; nenhum filtro ou alteração de resampling foi aplicado por suposição.

O Drive interno −0,50 do Vini transforma velocity 64 em aproximadamente 32. Portanto, igual velocity MIDI na entrada não garante igual gravação/camada nos dois instrumentos. Velocity 127 foi usada para evitar essa diferença de curva nos extremos. A comparação de camadas e decaimento com a tecla mantida exige o áudio do Sharp ativado.

## Comparação necessária no High Sierra

1. Importe `reference-note60-v127.mid` numa pista: uma nota MIDI 60, velocity 127, após 1 s de silêncio, segurada por 8 s e seguida por 4 s para a cauda, a 120 BPM.
2. Carregue o mesmo Royal em um único slot de cada plugin. Desative efeitos Velocity externos, EQ, reverb, compressão, chorus/delay e Long Release; mantenha DYN do Sharp neutro e os mesmos níveis de saída. Não use pedal durante esse clipe.
3. Exporte a mesma região dos dois instrumentos, separadamente, a 48 kHz / 24 bits estéreo, sem normalização. Preserve o silêncio inicial para alinhamento e registre a configuração de Release do Sharp.

As duas gravações permitem alinhar o ataque, compensar diferenças constantes de ganho, comparar envelopes enquanto a nota é mantida e após note-off, além de analisar espectro e ruído sem confundir camada, curva MIDI ou efeitos.
