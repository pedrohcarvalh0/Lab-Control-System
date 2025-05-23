#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "hardware/gpio.h"
#include "hardware/pwm.h"
#include "lib/ssd1306.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "pico/bootrom.h"
#include "stdio.h"

// Definições de pinos
#define I2C_PORT i2c1
#define I2C_SDA 14
#define I2C_SCL 15
#define ENDERECO 0x3C

#define BOTAO_A 5        // Botão de entrada
#define BOTAO_B 6        // Botão de saída
#define BOTAO_RESET 22   // Botão de reset (joystick)

#define LED_G 11         // LED RGB - Verde
#define LED_B 12         // LED RGB - Azul
#define LED_R 13         // LED RGB - Vermelho

#define BUZZER 21        // Pino do buzzer

// Definição da capacidade máxima
#define MAX_USUARIOS 10

// Variáveis globais
ssd1306_t ssd;
SemaphoreHandle_t xUsuariosSem;     // Semáforo de contagem para usuários
SemaphoreHandle_t xResetSem;        // Semáforo binário para reset
SemaphoreHandle_t xDisplayMutex;    // Mutex para acesso ao display
volatile uint8_t usuariosAtivos = 0;         // Contador de usuários ativos

// Protótipos de funções
void atualizarLED(uint8_t usuarios);
void buzzer_init(void);
void play_sound(int frequency, int dxUsuariosSemuration_ms);
void emitirBeep(bool duplo);
void atualizarDisplay();

// ISR para o botão de reset (joystick)
void gpio_reset_callback(uint gpio, uint32_t events)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    
    if (gpio == BOTAO_RESET) {
        xSemaphoreGiveFromISR(xResetSem, &xHigherPriorityTaskWoken);
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
}


// vTaskEntrada - agora usa xSemaphoreTake no semáforo de contagem para controlar limite
void vTaskEntrada(void *params)
{
    while (true)
    {
        if (gpio_get(BOTAO_A) == 0)
        {
            // Tenta obter o semáforo de contagem (vaga disponível)
            if (xSemaphoreTake(xUsuariosSem, 0) == pdTRUE)
            {
                // Se conseguiu vaga, incrementa o contador protegido por mutex
                if (xSemaphoreTake(xDisplayMutex, portMAX_DELAY) == pdTRUE)
                {
                    usuariosAtivos++;
                    atualizarDisplay();
                    xSemaphoreGive(xDisplayMutex);
                }
                atualizarLED(usuariosAtivos);
            }
            else
            {
                // Sistema cheio, beep curto e mensagem
                emitirBeep(false);

                if (xSemaphoreTake(xDisplayMutex, portMAX_DELAY) == pdTRUE)
                {
                    ssd1306_fill(&ssd, 0);
                    ssd1306_draw_string(&ssd, "SISTEMA CHEIO!", 10, 25);
                    ssd1306_draw_string(&ssd, "Aguarde saida", 10, 35);
                    ssd1306_send_data(&ssd);
                    xSemaphoreGive(xDisplayMutex);

                    vTaskDelay(pdMS_TO_TICKS(1500));

                    // Atualiza display para tela normal
                    if (xSemaphoreTake(xDisplayMutex, portMAX_DELAY) == pdTRUE)
                    {
                        atualizarDisplay();
                        xSemaphoreGive(xDisplayMutex);
                    }
                }
            }

            vTaskDelay(pdMS_TO_TICKS(300)); // debounce
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

// vTaskSaida - libera vaga no semáforo de contagem e decrementa contador
void vTaskSaida(void *params)
{
    while (true)
    {
        if (gpio_get(BOTAO_B) == 0)
        {
            // Só pode sair se usuáriosAtivos > 0
            if (usuariosAtivos > 0)
            {
                // Libera vaga no semáforo (incrementa)
                xSemaphoreGive(xUsuariosSem);

                // Decrementa contador protegido por mutex
                if (xSemaphoreTake(xDisplayMutex, portMAX_DELAY) == pdTRUE)
                {
                    usuariosAtivos--;
                    atualizarDisplay();
                    xSemaphoreGive(xDisplayMutex);
                }
                atualizarLED(usuariosAtivos);
            }

            vTaskDelay(pdMS_TO_TICKS(300)); // debounce
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

// vTaskReset - zera contador e reseta semáforo
void vTaskReset(void *params)
{
    while (true)
    {
        if (xSemaphoreTake(xResetSem, portMAX_DELAY) == pdTRUE)
        {
            // Resetar contador e semáforo de contagem
            if (xSemaphoreTake(xDisplayMutex, portMAX_DELAY) == pdTRUE)
            {
                // Libera todas as vagas no semáforo para garantir semáforo zerado
                // Primeiro calcula quantos usuários ativos temos
                uint8_t atual = usuariosAtivos;

                for (uint8_t i = 0; i < atual; i++)
                {
                    xSemaphoreGive(xUsuariosSem);
                }

                usuariosAtivos = 0;

                ssd1306_fill(&ssd, 0);
                ssd1306_draw_string(&ssd, "SISTEMA", 38, 15);
                ssd1306_draw_string(&ssd, "RESETADO", 35, 25);
                ssd1306_draw_string(&ssd, "Lab Liberado", 19, 45);
                ssd1306_send_data(&ssd);
                xSemaphoreGive(xDisplayMutex);
            }

            emitirBeep(true);
            atualizarLED(usuariosAtivos);

            vTaskDelay(pdMS_TO_TICKS(1500));

            if (xSemaphoreTake(xDisplayMutex, portMAX_DELAY) == pdTRUE)
            {
                atualizarDisplay();
                xSemaphoreGive(xDisplayMutex);
            }
        }
    }
}


// Função para atualizar o LED RGB com base no número de usuários
void atualizarLED(uint8_t usuarios)
{
    // Desliga todos os LEDs primeiro
    gpio_put(LED_R, 0);
    gpio_put(LED_G, 0);
    gpio_put(LED_B, 0);
    
    // Configura o LED de acordo com o número de usuários
    if (usuarios == 0) {
        // Azul - Nenhum usuário
        gpio_put(LED_B, 1);
    } else if (usuarios < MAX_USUARIOS - 1) {
        // Verde - Usuários ativos (0 a MAX-2)
        gpio_put(LED_G, 1);
    } else if (usuarios == MAX_USUARIOS - 1) {
        // Amarelo - Apenas 1 vaga restante (MAX-1)
        gpio_put(LED_R, 1);
        gpio_put(LED_G, 1);
    } else {
        // Vermelho - Capacidade máxima (MAX)
        gpio_put(LED_R, 1);
    }
}

// Inicializa PWM para o buzzer
void buzzer_init() {
    gpio_set_function(BUZZER, GPIO_FUNC_PWM);
    uint slice_num = pwm_gpio_to_slice_num(BUZZER);
    pwm_config config = pwm_get_default_config();
    
    // Configuração segura para o PWM
    pwm_config_set_clkdiv(&config, 100);
    pwm_config_set_wrap(&config, 1000);
    
    pwm_init(slice_num, &config, true);
    pwm_set_gpio_level(BUZZER, 0);  // Garante que o buzzer está desligado
}

// Tarefa dedicada para tocar sons no buzzer
void vTaskBuzzer(void *params) {
    int *sound_params = (int *)params;
    int frequency = sound_params[0];
    int duration_ms = sound_params[1];
    bool is_double = sound_params[2];
    
    uint slice_num = pwm_gpio_to_slice_num(BUZZER);
    
    // Configuração segura para evitar valores inválidos
    if (frequency < 100) frequency = 100;
    if (frequency > 10000) frequency = 10000;
    
    // Cálculo seguro do divisor e wrap
    float divider = 125.0f;  // Valor mais seguro
    pwm_set_clkdiv(slice_num, divider);
    
    // Fórmula ajustada para evitar valores extremos
    uint16_t wrap = (uint16_t)((125000000.0f / (frequency * divider)) - 1);
    if (wrap < 100) wrap = 100;
    if (wrap > 65000) wrap = 65000;
    
    pwm_set_wrap(slice_num, wrap);
    pwm_set_gpio_level(BUZZER, wrap / 2);  // 50% duty cycle
    
    vTaskDelay(pdMS_TO_TICKS(duration_ms));
    pwm_set_gpio_level(BUZZER, 0);  // Desliga o buzzer
    
    if (is_double) {
        vTaskDelay(pdMS_TO_TICKS(100));
        pwm_set_gpio_level(BUZZER, wrap / 2);
        vTaskDelay(pdMS_TO_TICKS(duration_ms));
        pwm_set_gpio_level(BUZZER, 0);
    }
    
    // Libera a memória alocada para os parâmetros
    vPortFree(sound_params);
    
    // Exclui a tarefa após a conclusão
    vTaskDelete(NULL);
}

// Função para tocar som no buzzer usando uma tarefa separada
void play_sound(int frequency, int duration_ms) {
    // Aloca memória para os parâmetros
    int *params = (int *)pvPortMalloc(3 * sizeof(int));
    if (params == NULL) return;  // Falha na alocação
    
    params[0] = frequency;
    params[1] = duration_ms;
    params[2] = 0;  // Não é duplo
    
    // Cria uma tarefa temporária para tocar o som
    TaskHandle_t buzzer_task_handle;
    xTaskCreate(vTaskBuzzer, "BuzzerTask", configMINIMAL_STACK_SIZE + 128, 
                params, tskIDLE_PRIORITY + 1, &buzzer_task_handle);
}

// Função para emitir beep no buzzer
void emitirBeep(bool duplo) {
    // Aloca memória para os parâmetros
    int *params = (int *)pvPortMalloc(3 * sizeof(int));
    if (params == NULL) return;  // Falha na alocação
    
    if (duplo) {
        params[0] = 1200;  // Frequência para beep duplo
        params[1] = 100;   // Duração
        params[2] = 1;     // É duplo
    } else {
        params[0] = 800;   // Frequência para beep único
        params[1] = 200;   // Duração
        params[2] = 0;     // Não é duplo
    }
    
    // Cria uma tarefa temporária para tocar o som
    TaskHandle_t buzzer_task_handle;
    xTaskCreate(vTaskBuzzer, "BuzzerTask", configMINIMAL_STACK_SIZE + 128, 
                params, tskIDLE_PRIORITY + 1, &buzzer_task_handle);
}

// Função para atualizar o display OLED
void atualizarDisplay() {
    char buffer[32];
    
    ssd1306_fill(&ssd, 0);
    ssd1306_draw_string(&ssd, "CONTROLE DO LAB", 5, 5);
    
    sprintf(buffer, "Usuarios: %d/%d", usuariosAtivos, MAX_USUARIOS);
    ssd1306_draw_string(&ssd, buffer, 5, 25);
    
    if (usuariosAtivos == 0) {
        ssd1306_draw_string(&ssd, "Status: Vazio", 5, 45);
    } else if (usuariosAtivos < MAX_USUARIOS) {
        sprintf(buffer, "Vagas: %d", MAX_USUARIOS - usuariosAtivos);
        ssd1306_draw_string(&ssd, buffer, 5, 45);
    } else {
        ssd1306_draw_string(&ssd, "Status: CHEIO", 5, 45);
    }
    
    ssd1306_send_data(&ssd);
}

// Tarefa para tocar o som de inicialização
void vTaskSomInicial(void *params) {
    // Toca um som de inicialização
    int *params1 = (int *)pvPortMalloc(3 * sizeof(int));
    if (params1 != NULL) {
        params1[0] = 1000;
        params1[1] = 200;
        params1[2] = 0;
        vTaskBuzzer(params1);
    }
    
    vTaskDelay(pdMS_TO_TICKS(300));
    
    int *params2 = (int *)pvPortMalloc(3 * sizeof(int));
    if (params2 != NULL) {
        params2[0] = 1500;
        params2[1] = 200;
        params2[2] = 0;
        vTaskBuzzer(params2);
    }
    
    vTaskDelete(NULL);
}

// Tarefa para inicializar o display com a tela principal
void vTaskInicializaDisplay(void *params) {
    // Pequeno atraso para garantir que o sistema esteja pronto
    vTaskDelay(pdMS_TO_TICKS(100));
    
    // Atualiza o display com a tela principal
    if (xSemaphoreTake(xDisplayMutex, portMAX_DELAY) == pdTRUE) {
        atualizarDisplay();
        xSemaphoreGive(xDisplayMutex);
    }
    
    vTaskDelete(NULL);
}

void setup() {
    // Inicialização dos botões
    gpio_init(BOTAO_A);
    gpio_set_dir(BOTAO_A, GPIO_IN);
    gpio_pull_up(BOTAO_A);
    
    gpio_init(BOTAO_B);
    gpio_set_dir(BOTAO_B, GPIO_IN);
    gpio_pull_up(BOTAO_B);
    
    gpio_init(BOTAO_RESET);
    gpio_set_dir(BOTAO_RESET, GPIO_IN);
    gpio_pull_up(BOTAO_RESET);

    // Configura interrupção para o botão de reset
    gpio_set_irq_enabled_with_callback(BOTAO_RESET, GPIO_IRQ_EDGE_FALL, true, &gpio_reset_callback);

    // Inicialização do LED RGB
    gpio_init(LED_R);
    gpio_init(LED_G);
    gpio_init(LED_B);
    gpio_set_dir(LED_R, GPIO_OUT);
    gpio_set_dir(LED_G, GPIO_OUT);
    gpio_set_dir(LED_B, GPIO_OUT);

    // Inicialização do display
    i2c_init(I2C_PORT, 400 * 1000);
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);
    ssd1306_init(&ssd, WIDTH, HEIGHT, false, ENDERECO, I2C_PORT);
    ssd1306_config(&ssd);
    
    // Inicialização do buzzer com PWM
    buzzer_init();
}

int main() {
    stdio_init_all();
    setup();

    // Cria semáforo de contagem para usuários
    xUsuariosSem = xSemaphoreCreateCounting(MAX_USUARIOS, MAX_USUARIOS);

    // Cria semáforo binário para reset
    xResetSem = xSemaphoreCreateBinary();
    
    // Cria mutex para acesso ao display
    xDisplayMutex = xSemaphoreCreateMutex();
    
    // Configura o LED inicial (azul - nenhum usuário)
    atualizarLED(0);
    
    // Cria as tarefas principais
    xTaskCreate(vTaskEntrada, "EntradaTask", configMINIMAL_STACK_SIZE + 256, NULL, 1, NULL);
    xTaskCreate(vTaskSaida, "SaidaTask", configMINIMAL_STACK_SIZE + 256, NULL, 1, NULL);
    xTaskCreate(vTaskReset, "ResetTask", configMINIMAL_STACK_SIZE + 256, NULL, 1, NULL);
    
    // Cria uma tarefa para tocar o som de inicialização após o FreeRTOS iniciar
    xTaskCreate(vTaskSomInicial, "SomInicialTask", configMINIMAL_STACK_SIZE + 128, NULL, 1, NULL);
    
    // Cria uma tarefa para inicializar o display com a tela principal
    xTaskCreate(vTaskInicializaDisplay, "InicializaDisplayTask", configMINIMAL_STACK_SIZE + 128, NULL, 1, NULL);
    
    // Inicia o agendador
    vTaskStartScheduler();
    
    // Nunca deve chegar aqui
    panic_unsupported();
}