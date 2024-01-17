#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include <stdlib.h>
#include "nokia5110.h"

/*

Alunos: Miguel Warken e Gustavo Willian da Silva

OBS.: Na parte do led vermelho ficar piscando, fizemos para que a luz verde só ligue quando uma moeda for depositada, a fim de evitar confusões.

*/

// Definição dos pinos utilizados para o display e os botões
#define Ta;
#define DISPLAY_CE PB1
#define DISPLAY_DC PB2
#define DISPLAY_DIN PB3
#define DISPLAY_CLK PB4
#define BUTTON_UP PD0
#define BUTTON_DOWN PD1
#define BUTTON_FIRE PD2

// Definição do tamanho da matriz do jogo
#define GAME_WIDTH 14
#define GAME_HEIGHT 8

// Definição dos elementos do jogo
#define PLAYER 1
#define INVADER 2
#define MISSILE 3
#define EMPTY 0

// Tabela de caracteres para o display PCD8544
const unsigned char PROGMEM font[][5] = {
    { 0x00, 0x00, 0x00, 0x00, 0x00 }, // Espaço vazio
    { 0x1F, 0x1F, 0x1F, 0x1F, 0x1F }, // Jogador
    { 0x00, 0x1F, 0x1F, 0x1F, 0x00 }, // Invasor
    { 0x1F, 0x1F, 0x00, 0x1F, 0x1F }  // Míssil
};

// Variáveis globais
unsigned char game[GAME_HEIGHT][GAME_WIDTH]; // Matriz do jogo
int playerX; // Posição horizontal do jogador
int playerY; // Posição vertical do jogador
int invaderX; // Posição horizontal do invasor
int invaderY; // Posição vertical do invasor
int missileX; // Posição horizontal do míssil
int missileY; // Posição vertical do míssil

// Função para inicializar o display
void initDisplay() {
    // Configuração dos pinos do display como saída
    DDRB |= (1 << DISPLAY_RST) | (1 << DISPLAY_CE) | (1 << DISPLAY_DC) | (1 << DISPLAY_DIN) | (1 << DISPLAY_CLK);
    
    // Inicializa o display
    PORTB |= (1 << DISPLAY_RST);
    PORTB &= ~(1 << DISPLAY_CE);
    
    // Envia os comandos de inicialização para o display
    spiCommand(0x21); // Ativa o modo estendido
    spiCommand(0xC0); // Configura a tensão de polarização
    spiCommand(0x07); // Ajusta o contraste
    spiCommand(0x13); // Define o modo de exibição normal
    spiCommand(0x20); // Ativa o modo básico
    spiCommand(0x0C); // Ativa o display
}

// Função para enviar um comando para o display via SPI
void spiCommand(unsigned char cmd) {
    PORTB &= ~(1 << DISPLAY_DC);
    spiTransfer(cmd);
}

// Função para enviar um dado para o display via SPI
void spiData(unsigned char data) {
    PORTB |= (1 << DISPLAY_DC);
    spiTransfer(data);
}

// Função para transferir dados via SPI
void spiTransfer(unsigned char data) {
    for (unsigned char i = 0; i < 8; i++) {
        PORTB &= ~(1 << DISPLAY_CLK);
        if (data & 0x80) {
            PORTB |= (1 << DISPLAY_DIN);
        } else {
            PORTB &= ~(1 << DISPLAY_DIN);
        }
        PORTB |= (1 << DISPLAY_CLK);
        data <<= 1;
    }
}

// Função para limpar o display
void clearDisplay() {
    for (unsigned char y = 0; y < 6; y++) {
        spiCommand(0x40 | y);
        spiCommand(0x80);
        for (unsigned char x = 0; x < 84; x++) {
            spiData(0x00);
        }
    }
}

// Função para inicializar o jogo
void initGame() {
    playerX = GAME_WIDTH / 2;
    playerY = GAME_HEIGHT - 1;
    invaderX = GAME_WIDTH / 2;
    invaderY = 0;
    missileX = -1;
    missileY = -1;
    
    // Preenche a matriz do jogo com espaços vazios
    for (int i = 0; i < GAME_HEIGHT; i++) {
        for (int j = 0; j < GAME_WIDTH; j++) {
            game[i][j] = EMPTY;
        }
    }
    
    // Define a posição inicial do jogador e do invasor na matriz do jogo
    game[playerY][playerX] = PLAYER;
    game[invaderY][invaderX] = INVADER;
}

// Função para atualizar a posição dos elementos do jogo
void updateGame() {
    // Atualiza a posição do invasor
    invaderY++;
    if (invaderY >= GAME_HEIGHT) {
        invaderY = 0;
        invaderX = playerX; // Reinicia o invasor na posição atual do jogador
    }
    
    // Atualiza a posição do míssil, se estiver ativo
    if (missileY >= 0) {
        missileY--;
        if (missileY < 0) {
            missileX = -1; // Desativa o míssil quando atinge o topo da tela
        }
    }
}

// Função para renderizar o jogo no display
void renderGame() {
    clearDisplay();
    
    for (unsigned char y = 0; y < GAME_HEIGHT; y++) {
        for (unsigned char x = 0; x < GAME_WIDTH; x++) {
            unsigned char element = game[y][x];
            if (element != EMPTY) {
                spiCommand(0x40 | y);
                spiCommand(0x80 | x);
                for (unsigned char i = 0; i < 5; i++) {
                    unsigned char data = pgm_read_byte(&(font[element][i]));
                    spiData(data);
                }
            }
        }
    }
}

// Função para ler os botões e atualizar a posição do jogador ou disparar o míssil
void handleInput() {
    // Verifica se o botão de movimento para cima está pressionado
    if (!(PORTC & (1 << BUTTON_UP))) {
        if (playerX > 0) {
            playerX--;
        }
    }
    
    // Verifica se o botão de movimento para baixo está pressionado
    if (!(PORTC & (1 << BUTTON_DOWN))) {
        if (playerX < GAME_WIDTH - 1) {
            playerX++;
        }
    }
    
    // Verifica se o botão de disparo está pressionado
    if (!(PORTC & (1 << BUTTON_FIRE))) {
        if (missileY < 0) {
            missileX = playerX;
            missileY = playerY - 1;
        }
    }
}

int main() {
    // Configuração dos pinos dos botões como entrada com pull-up
    DDRD &= ~((1 << BUTTON_UP) | (1 << BUTTON_DOWN) | (1 << BUTTON_FIRE));
    PORTC |= (1 << BUTTON_UP) | (1 << BUTTON_DOWN) | (1 << BUTTON_FIRE);
    
    // Inicialização do display
    initDisplay();
    
    // Inicialização do jogo
    initGame();
    
    while (1) {
        // Atualiza a posição dos elementos do jogo
        updateGame();
        
        // Lê os botões e atualiza a posição do jogador ou dispara o míssil
        handleInput();
        
        // Renderiza o jogo no display
        renderGame();
        
        _delay_ms(100); // Atraso entre cada frame
    }
    
    return 0;
}
