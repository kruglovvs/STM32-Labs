#include "stm32f1xx.h"
#include "stdbool.h"
#include "stdint.h"
#include "string.h"

void initTIM2(void);
void initADC(void);
void initUSART2(void);
void txStr(char *str, bool crlf);
void Delay(uint32_t takts);
void initClk(void);
uint16_t Read_ADC(void);

#define TX_BUFF_SIZE	(512)
#define ITOA_BUFF_SIZE	(8)

static char TxBuffer[TX_BUFF_SIZE] = {0};
static char hex_buffer[ITOA_BUFF_SIZE] = {0};
static uint8_t sample_counter = 0;

void initClk(void)
{
	// Enable HSI
	RCC->CR |= RCC_CR_HSION;
	while(!(RCC->CR & RCC_CR_HSIRDY)){};
	// Enable Prefetch Buffer
	FLASH->ACR |= FLASH_ACR_PRFTBE;
	// Flash 2 wait state
	FLASH->ACR &= ~FLASH_ACR_LATENCY;
	FLASH->ACR |= FLASH_ACR_LATENCY_2;
	// HCLK = SYSCLK
	RCC->CFGR |= RCC_CFGR_HPRE_DIV1;
	// PCLK2 = HCLK
	RCC->CFGR |= RCC_CFGR_PPRE2_DIV1;
	// PCLK1 = HCLK/2
	RCC->CFGR |= RCC_CFGR_PPRE1_DIV2;
	// PLL configuration: PLLCLK = HSI/2 * 16 = 64 MHz
	RCC->CFGR &= ~RCC_CFGR_PLLSRC;
	RCC->CFGR |= RCC_CFGR_PLLMULL16;
	// Enable PLL
	RCC->CR |= RCC_CR_PLLON;
	// Wait till PLL is ready
	while((RCC->CR & RCC_CR_PLLRDY) == 0) {};
	// Select PLL as system clock source
	RCC->CFGR &= ~RCC_CFGR_SW;
	RCC->CFGR |= RCC_CFGR_SW_PLL;
	// Wait till PLL is used as system clock source
	while ((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_PLL){};
}

void TIM2_IRQHandler(void)
{
	TIM2->SR &= ~TIM_SR_UIF;			//Сброс флага переполнения
	sample_counter++;
	uint16_t adc_value = Read_ADC();

    sprintf(hex_buffer, "%04X", adc_value);
	strcat(TxBuffer, hex_buffer);
	if (sample_counter < 64)
	{
		strcat(TxBuffer, ":");
	}
	else
	{
		sample_counter = 0;
		txStr(TxBuffer, true);
		TxBuffer[0] = '\0';
	}
}

void initADC(void)
{
	RCC->APB2ENR |= RCC_APB2ENR_IOPCEN;					//Включить тактирование порта GPIOC
	GPIOC->CRL &= ~(GPIO_CRL_MODE4 | GPIO_CRL_CNF4);	//PC4 на вход
	RCC->APB2ENR |= RCC_APB2ENR_ADC1EN;  				//Включить тактирование АЦП
	//Настройка времени преобразования каналов
	ADC1->SMPR1 |= ADC_SMPR1_SMP14_Msk;						//Канал 14 - 239.5 тактов
	ADC1->CR2 |= ADC_CR2_EXTSEL;       					//Выбрать в качестве источника запуска SWSTART
	ADC1->CR2 |= ADC_CR2_ADON;         					//Включить АЦП
	Delay(5);											//Задержка перед калибровкой
	ADC1->CR2 |= ADC_CR2_CAL;							//Запуск калибровки
	while (ADC1->CR2 & ADC_CR2_CAL){};	 				//Ожидание окончания калибровки								//Записываем номер канала в регистр SQR3
}

void initUSART2(void)
{
	RCC->APB2ENR |= RCC_APB2ENR_IOPAEN;
	RCC->APB2ENR |= RCC_APB2ENR_AFIOEN;						//включить тактирование альтернативных ф-ций портов
	RCC->APB1ENR |= RCC_APB1ENR_USART2EN;					//включить тактирование UART2
	GPIOA->CRL &= ~(GPIO_CRL_MODE2 | GPIO_CRL_CNF2);		//PA2 на выход
	GPIOA->CRL |= (GPIO_CRL_MODE2_1 | GPIO_CRL_CNF2_1);
	GPIOA->CRL &= ~(GPIO_CRL_MODE3 | GPIO_CRL_CNF3);		//PA3 - вход
	GPIOA->CRL |= GPIO_CRL_CNF3_0;
	/*****************************************
	Скорость передачи данных - 115200
	Частота шины APB1 - 64МГц
	1. USARTDIV = 32'000'000/(16*115200) = 17,36
	2. 17 = 0x11
	3. 16*0.36 = 5,76 = 0x6
	4. Итого 0x116
	*****************************************/
	USART2->BRR = 0x116;
	USART2->CR1 |= USART_CR1_RE | USART_CR1_TE | USART_CR1_UE;
	USART2->CR1 |= USART_CR1_RXNEIE;						//разрешить прерывание по приему байта данных
	NVIC_EnableIRQ(USART2_IRQn);
}

void initTIM2(void)
{
	RCC->APB1ENR |= RCC_APB1ENR_TIM2EN;		//Включить тактирование TIM2
	TIM2->PSC = 64000-1;					//Предделитель частоты (64МГц/64000 = 1кГц)
	TIM2->ARR = 10-1;						//Модуль счёта таймера (1кГц/10 = 100Гц)
	TIM2->DIER |= TIM_DIER_UIE;				//Разрешить прерывание по переполнению таймера
	TIM2->CR1 |= TIM_CR1_CEN;				//Включить таймер
	NVIC_EnableIRQ(TIM2_IRQn);				//Рарзрешить прерывание от TIM2
	NVIC_SetPriority(TIM2_IRQn, 1);			//Выставляем приоритет
}

uint16_t Read_ADC()
{
	ADC1->SQR3 = 14;
	//ADC1->CR2 |= ADC_CR2_SWSTART;
	ADC1->CR2 |= ADC_CR2_ADON;					//Запускаем преобразование в регулярном канале
	while(!(ADC1->SR & ADC_SR_EOC));				//Ждем окончания преобразования
	return ADC1->DR;								//Читаем результат
}

void txStr(char *str, bool crlf)
{
	if (crlf)												//если просят,
	{
		strcat(str,"\r");									//добавляем символ конца строки
	}
	for (uint16_t i = 0; i < strlen(str); i++)
	{
		USART2->DR = str[i];								//передаём байт данных
		while ((USART2->SR & USART_SR_TC)==0) {};			//ждём окончания передачи
	}
}

void Delay(uint32_t takts)
{
	volatile uint32_t i;

	for (i = 0; i < takts; i++) {};
}

int main(void)
{
	initClk();
	initADC();
	initUSART2();
	initTIM2();

	while(true){

	};
}

