# Motor e carregamento — 0.6.0

Esta versão implementa relações de DSP e metadados observadas por análise estática do caminho de piano do Sharp Stage Pianos 2.1.0, cruzada em ARM64 e x86_64. A implementação é própria. O binário, projeto de engenharia reversa e pseudocódigo permanecem fora deste repositório. Não houve comparação auditiva com o Sharp ativado neste M4.

## Reprodução

A velocity MIDI permanece neutra por padrão; o antigo Drive −0,50 deixou de modificar a entrada. A função de seleção converte a intensidade para uma coordenada calibrada. Cada gravação preserva sua fronteira explícita (`record[0x74]`). Somente grupos sem fronteiras usam pontos intermediários entre camadas, na escala da geração do contêiner. Camadas ausentes não são redistribuídas.

O ganho é separado da seleção: região suave com interpolação em dB, região intermediária com potência 2,5 e correções em dB, limite unitário na região forte. A gravação recebe seu próprio trim; a nota recebe a correção da tabela por tecla em `0x10c`. O fator fixo 0,65 e a aplicação indiscriminada do ganho global usados anteriormente saem da cadeia normal de reprodução. O getter legado de ganho permanece no bridge, mas o motor usa o ganho por tecla. Se o metadado de trim não for válido, há normalização pelo pico filtrado.

O contêiner CBIN V0/V1, e não a versão interna CNSP, determina a escala legada de trim e de fronteiras ausentes. Os metadados q23 em `0x2e` e `0x3a` são convertidos para taxas em dB/s na escala observada, com limites; a taxa antes do loop pode ser independente da transposição, enquanto a taxa do loop acompanha a velocidade de leitura. Na ausência de taxa, uma estimativa por energia em janelas antes do loop oferece fallback. Não se altera o PCM original.

A interpolação continua linear. Um filtro de um polo de 3 kHz por canal e voz reproduz o caminho de piano identificado. O coeficiente acompanha a taxa do host e a velocidade de leitura/transposição da gravação. O filtro não é uma prova de que todo ruído percebido tenha a mesma causa; requer audição com os mesmos arquivos. Há uma entrada curta de 32 amostras para a voz principal e proteção curta no fim da gravação de release.

## Release e soltura

O seguidor de amplitude observa o pico estéreo pré-filtro/pré-fader da voz, incluindo ganho da gravação, ganho da velocity, entrada e decaimento. Ataque instantâneo e amortecimento com constante de 50 ms mantêm uma estimativa recente. Na soltura, o release é disparado uma vez com ganho relacionado a esse estado, à normalização do take e ao fator de escala observado. Usa o take de maior camada do banco de release; as gravações desse banco são normalizadas entre si por raiz. O multiplicador isolado não representa percentual de volume final, pois envolve outras escalas de ganho.

Isso substitui o teto anterior de 2% proporcional à velocity inicial. O ganho agora muda com o estado atual da nota: uma nota longa pode gerar release mais discreto que uma curta com a mesma velocity. CC64 adia o disparo até o abafamento. Notas acima da fronteira de abafador seguem soando; note-off sem voz ativa, pedal-up repetido e panic não iniciam releases novos.

A soltura principal usa multiplicação por coeficiente exponencial, tendo −80 dB como referência de fim de tempo. O padrão é 500 ms escalado pelo metadado do sample (`record[0x72]`), com limites de 10–4000 ms. A regra anterior de 90 ms lineares foi substituída por solicitação do usuário. Não se trata de apenas mudar 90 para 500 mantendo a mesma forma de envelope.

## Carregamento e memória

O decoder `nord-format` 0.8.0 já reconstrói áudio int16. O bridge passa a manter esse formato e evita o vetor float32 adicional. Para o Royal Grand 3D XL 6.1 testado:

| Buffer final | 0.5.6 | 0.6.0 |
|---|---:|---:|
| PCM | 1.524.124.752 bytes | 762.062.376 bytes |

Essa conta é do buffer de áudio final, não do RSS total. O arquivo comprimido, buffers transitórios, estruturas do host e outros plugins continuam usando RAM. Dois slots com a mesma biblioteca compartilham o PCM, em vez de manter duas cópias. Cada slot conserva suas vozes, volume e estado MIDI independentes.

O cache é local ao módulo carregado do plugin. A chave usa caminho, tamanho e última modificação. Um mecanismo de single-flight evita duas decodificações simultâneas do mesmo arquivo. As referências de slots/instâncias mantêm bibliotecas vivas; o cache pode reter até 128 MiB adicionais ociosos. Um XL maior que esse limite não é retido quando a última referência é removida. Não há persistência em disco nem streaming. Alterar os metadados do arquivo invalida a chave; conteúdo alterado mantendo tamanho e data não é detectado por hash.

Cancelamento é verificado durante leituras e entre gravações. O decoder de uma gravação individual não foi modificado: uma gravação em curso termina antes de interromper. Clear e solicitações mais recentes também invalidam a publicação de resultados antigos. Criação, compartilhamento, substituição e liberação de bibliotecas ficam nos threads de carregamento; não no callback de áudio.

## Medições e validação

Medições locais no M4, com arquivo possivelmente já no cache do macOS: duas cargas sem cache de áudio do plugin em 0.5.6 levaram 1,98 e 1,96 s. Em 0.6.0, medições com perfil ficaram em 1,88 e 1,87 s; outra execução do benchmark de biblioteca ficou em 1,94 s. Isso demonstra redução de memória e ausência de grande regressão de carga, não uma aceleração relevante garantida. O lookup de uma biblioteca ainda viva ficou abaixo de 1 ms no benchmark; esse número exclui interface gráfica, checagem de arquivo e publicação no slot. Os tempos do High Sierra ainda precisam ser medidos.

O perfil pode ser habilitado em desenvolvimento com `VINI_STAGE_PROFILE_LOAD=1`. O utilitário `NpnoLoadBench` mede uma carga real e uma aquisição com biblioteca ainda viva; `NpnoProgress` verifica PCM16, progresso, cancelamento, single-flight e invalidação por chave. Nenhum deles mede o Sharp.

Os testes incluem:

- Unidades q23, trim, limites de release, loops inválidos e cancelamento sem repetição automática de leitura.
- Seleção de camadas do Royal C4 em velocities 16/50/64/127, take de release e correção por tecla contra valores gravados conhecidos.
- 440 combinações de notas/velocities, sustain de 14 s em 44,1/48 kHz, notas transpostas e pedal antes/depois do ataque.
- Release com diferenças entre notas fortes/fracas e curtas/longas, sem disparo durante o sustain nem retrigger em mensagens repetidas.
- Envelope exponencial isolado com fixture de seno próprio, restauração de projeto, ausência de arquivo, Clear durante carga, mix e passos de volume.
- Compartilhamento real da biblioteca em A/B de outra instância, editor compilado e pacote/upgrade em diretórios temporários.

## Limites para comparação

O Sharp neste M4 permanece sem ativação. A reconstrução do DSP não substitui comparação de áudio com a versão ativada do High Sierra. Sem os mesmos presets e níveis, uma diferença de volume não pode ser atribuída apenas à biblioteca.

O Vini mantém seus dois faders A/B e não tem master independente. O master padrão observado no Sharp era −6 dB; para comparar níveis, alinhe esse ganho externo. Não foram implementados os efeitos, modelagem de ressonância, half-pedal, portamento, programas de quatro slots e toda a matriz de controle do Sharp. Transições de roubo de voz e interpretação de variantes antigas ainda podem diferir. Tampouco foi demonstrada equivalência em todos os arquivos `.npno`.

O layout aprovado e os IDs dos parâmetros existentes foram preservados. Projetos antigos continuam referenciando os arquivos e os volumes; o som será recalculado pelo novo motor, não pelo algoritmo antigo.
