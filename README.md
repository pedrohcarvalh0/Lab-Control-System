# Sistema de Controle de Acesso com FreeRTOS no RP2040 (BitDogLab)

## Descrição do Projeto

Este projeto implementa um sistema embarcado para controle de acesso concorrente de usuários a um espaço físico (como laboratório, biblioteca ou refeitório), utilizando a placa BitDogLab com o microcontrolador RP2040. O sistema utiliza o sistema operacional de tempo real FreeRTOS para gerenciar tarefas concorrentes, sincronização por semáforos e mutex, garantindo segurança e integridade na manipulação dos recursos compartilhados.

---

## Funcionalidades

- **Controle do número de usuários ativos** através de um semáforo de contagem, limitando o acesso simultâneo ao espaço conforme uma capacidade máxima definida.
- **Reset do sistema** via semáforo binário acionado por interrupção de um botão joystick, zerando a contagem de usuários.
- **Proteção do acesso ao display OLED** com mutex para evitar condições de corrida.
- **Feedback visual** por meio do LED RGB que indica a ocupação atual:
  - Azul: Nenhum usuário
  - Verde: Usuários ativos (de 0 até MAX-2)
  - Amarelo: Apenas 1 vaga restante (MAX-1)
  - Vermelho: Capacidade máxima (MAX)
- **Feedback sonoro** com buzzer:
  - Beep curto ao tentar entrar quando o sistema está cheio.
  - Beep duplo ao resetar o sistema.
- **Interface de usuário** via display OLED exibindo contagem atual de usuários e status.

---

## Componentes Utilizados

| Componente           | Descrição                         |
|---------------------|---------------------------------|
| Placa BitDogLab      | Microcontrolador RP2040          |
| FreeRTOS             | Sistema operacional de tempo real|
| Semáforo de contagem | Controle do número máximo de usuários simultâneos |
| Semáforo binário     | Reset do sistema acionado por interrupção |
| Mutex                | Proteção do acesso ao display OLED |
| Display OLED SSD1306 | Exibição de informações          |
| LEDs RGB             | Indicação visual do estado       |
| Buzzer PWM           | Sinalização sonora               |
| Botões               | Entrada, saída e reset           |

---

## Regras de Funcionamento

| Ação             | Dispositivo | Comportamento                                          |
|------------------|-------------|-------------------------------------------------------|
| Entrada de usuário| Botão A     | Incrementa contagem se limite não atingido, caso contrário emite aviso e beep curto |
| Saída de usuário  | Botão B     | Decrementa contagem, se houver usuários ativos       |
| Reset do sistema  | Botão joystick | Zera contagem e emite beep duplo                      |

---

## Estrutura do Código

- **Tarefas FreeRTOS:**
  - `vTaskEntrada`: Controla entrada de usuários, decrementando o semáforo de contagem se houver vagas.
  - `vTaskSaida`: Controla saída de usuários, liberando vaga no semáforo de contagem.
  - `vTaskReset`: Aguarda o semáforo binário de reset (ativado por interrupção) para zerar o sistema.
  - `vTaskSomInicial`: Emite som de inicialização ao ligar o sistema.
  - `vTaskInicializaDisplay`: Atualiza a tela principal do display OLED no início.
  
- **Semáforos e Mutex:**
  - `xUsuariosSem`: Semáforo de contagem para controlar o número de usuários simultâneos.
  - `xResetSem`: Semáforo binário para sinalizar o reset do sistema.
  - `xDisplayMutex`: Mutex para proteger o acesso ao display OLED.
  
- **Interrupção GPIO:**
  - Configurada para o botão de reset para liberar o semáforo binário de reset.
  
- **Controle de Hardware:**
  - LEDs RGB para indicação visual da ocupação.
  - Buzzer com PWM para emitir sons conforme eventos.
  - Display OLED para exibir mensagens e contagem.

---

## Definições Importantes

- **Capacidade Máxima (`MAX_USUARIOS`):** Número máximo de usuários simultâneos permitido. Definido como 10 no código.
- **Debounce:** Implementado via `vTaskDelay` para evitar múltiplas leituras errôneas dos botões.
- **Proteção contra condições de corrida:** Utiliza mutex para acesso ao display OLED.

---

## 📦 Requisitos para Compilação

Para compilar este projeto, é necessário baixar o núcleo do FreeRTOS manualmente. Siga os passos abaixo:

1. Clone o repositório do FreeRTOS Kernel:
   ```bash
   git clone https://github.com/FreeRTOS/FreeRTOS-Kernel.git
   ```

2. No arquivo `CMakeLists.txt` do seu projeto, ajuste o caminho da variável `FREERTOS_KERNEL_PATH` de acordo com o local onde você salvou a pasta clonada. Exemplo:

   ```cmake
   set(FREERTOS_KERNEL_PATH "Z:/FreeRTOS-Kernel") 
   include(${FREERTOS_KERNEL_PATH}/portable/ThirdParty/GCC/RP2040/FreeRTOS_Kernel_import.cmake)
   ```

3. Certifique-se de que o arquivo `FreeRTOSConfig.h` está localizado dentro da pasta `include/` no seu projeto.

Esses ajustes garantem que o FreeRTOS seja corretamente integrado ao ambiente de compilação para o RP2040.

---
