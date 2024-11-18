/**
  ******************************************************************************
  * @file    main.c
  * @author  Vladimir Leonidov
  * @version V1.0.0
  * @date    09.10.2021
  * @brief   Лабораторная работа №2 - "Введение в прерывания"
  *			 Отладочная плата: STM32F103 Nucleo-64
  *
  *			 - Реализован обработчик прерывания от базвого таймера TIM2.
  *			   Прерывание срабатывает раз в секунду.
  *			 - Реализован обработчик прерывания по спаду импульса на выводе PC13.
  *			   К данному выводу подключена кнопка.
  *
  ******************************************************************************
  */

#include "main.h"
#include "stdint.h"

/**
  * @brief  Подпрограмма обработчика прерывания
  *			по переполнению тайемера TIM2
  * @param  None
  * @retval None
  */

#define LED_BRIGHTNESS_LOW (5)
#define LED_BRIGHTNESS_MEDIUM (30)
#define LED_BRIGHTNESS_HIGH (90)
#define LED_COUNTER_MAX (100)
static uint32_t LED_counter = 0;
static uint32_t LED_brightness = LED_BRIGHTNESS_MEDIUM;

char RxBuffer[RX_BUFF_SIZE];					//Буфер приёма USART
char TxBuffer[TX_BUFF_SIZE];					//Буфер передачи USART
bool ComReceived;								//Флаг приёма строки данных

/**
  * @brief  Обработчик прерывания от USART2
  * @param  None
  * @retval None
  */
void USART2_IRQHandler(void)
{
	if ((USART2->SR & USART_SR_RXNE)!=0)		//Прерывание по приёму данных?
	{
		uint8_t pos = strlen(RxBuffer);			//Вычисляем позицию свободной ячейки

		RxBuffer[pos] = USART2->DR;				//Считываем содержимое регистра данных

		if (RxBuffer[pos]== 0x0D)							//Если это символ конца строки
		{
			ComReceived = true;					//- выставляем флаг приёма строки
			return;								//- и выходим
		}
	}
}
void TIM2_IRQHandler(void)
{
	TIM2->SR &= ~TIM_SR_UIF;			//Сброс флага переполнения
	LED_counter++;
	if (LED_counter < LED_brightness) {
		LED_ON();
	} else if (LED_counter == LED_COUNTER_MAX) {
		LED_counter = 0;
		LED_OFF();
	} else {
		LED_OFF();
	}
}
void TIM3_IRQHandler(void)
{
	TIM3->SR &= ~TIM_SR_UIF;			//Сброс флага переполнения
	TIM2->CR1 ^= TIM_CR1_CEN;		//Инвертируем состояние таймера
	LED_OFF();
}

/**
  * @brief  Подпрограмма обработчика прерывания
  *			по внешнему импульсу на выводах (EXTI15-EXTI10)
  *			Нас интересует EXTI13, там висит кнопка.
  * @param  None
  * @retval None
  */
void EXTI15_10_IRQHandler(void)
{
	// Т.к. этот обработчик вызывается, если произшло одно из прерываний EXTI15-EXI10,
	// нужно проверить, кто из них его вызвал.
	if (EXTI->PR & EXTI_PR_PR13) 		// нас интересует EXTI13
	{
		EXTI->PR |= EXTI_PR_PR13;
		delay(10000);					//Задержка для защиты от дребезга контактов
		switch(LED_brightness)	{
		case LED_BRIGHTNESS_LOW:
			LED_brightness = LED_BRIGHTNESS_MEDIUM;
			break;
		case LED_BRIGHTNESS_MEDIUM:
			LED_brightness = LED_BRIGHTNESS_HIGH;
			break;
		case LED_BRIGHTNESS_HIGH:
			LED_brightness = LED_BRIGHTNESS_LOW;
			break;
		default:
			LED_brightness = LED_BRIGHTNESS_LOW;
			break;
		}
	}
}

/**
  * @brief  Инициализация портов ввода-вывода
  * @param  None
  * @retval None
  */
void initPorts(void)
{
	RCC->APB2ENR |= RCC_APB2ENR_IOPAEN;	//включить тактирование GPIOA
	//очистка полей
	GPIOA->CRL &= ~(GPIO_CRL_CNF5 | GPIO_CRL_MODE5);
	//и конфигурация
	GPIOA->CRL |= GPIO_CRL_MODE5_1;		//PA5, выход 2МГц
}

/**
  * @brief  Инициализация USART2
  * @param  None
  * @retval None
  */
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
	Скорость передачи данных - 256000
	Частота шины APB1 - 32МГц

	1. USARTDIV = 32'000'000/(16*256000) = 7,8125
	2. 7 = 0x07
	3. 16*0.8125 = 13
	4. Итого 0x07D
	*****************************************/
	USART2->BRR = 0x07D;

	USART2->CR1 |= USART_CR1_RE | USART_CR1_TE | USART_CR1_UE;
	USART2->CR1 |= USART_CR1_RXNEIE;						//разрешить прерывание по приему байта данных

	NVIC_EnableIRQ(USART2_IRQn);
}
/**
  * @brief  Инициализация прерывания от кнопки (PC13)
  * @param  None
  * @retval None
  */
void initButton(void)
{
	//Включить тактирование порта GPIOC и альтернативных функций
	RCC->APB2ENR |= RCC_APB2ENR_IOPCEN | RCC_APB2ENR_AFIOEN;

	/* Настраиваем PC13 на вход, альтернативная функция */
	// Сбрасываем биты конфигурации порта...
	GPIOC->CRH &= ~(GPIO_CRH_MODE13 | GPIO_CRH_CNF13);
	//...и выставляем так, как нам нужно
	GPIOC->CRH |= GPIO_CRH_CNF13_1;			//Вход с подтяжкой
	GPIOC->BSRR = GPIO_BSRR_BS13;			//Подтяжка к Vdd

	/* Настройка самого прерывания */

	// Настройка альтернативных фукнций портов.
	// Настройки портов с 12 по 15 хранятся в регистре AFIO_EXTICR4.
	// Регистры объединены в массив AFIO->EXTICR, нумерация массива начинается с нулевого элемента.
	// Поэтому настройки AFIO_EXTICR4 хранятся в AFIO->EXTICR[3]
	AFIO->EXTICR[3] |= AFIO_EXTICR4_EXTI13_PC;

	EXTI->FTSR |= EXTI_FTSR_TR13;			//Прерывание по спаду импульса (при нажатии на кнопку)
	EXTI->IMR |= EXTI_IMR_MR13;				//Выставляем маску - EXTI13

	NVIC_EnableIRQ(EXTI15_10_IRQn);			//Разрешаем прерывание
	NVIC_SetPriority(EXTI15_10_IRQn, 0);	//Выставляем приоритет
}

/**
  * @brief  Инициализация таймера TIM2
  * @param  None
  * @retval None
  */
void initTIM2(void)
{
	RCC->APB1ENR |= RCC_APB1ENR_TIM2EN;		//Включить тактирование TIM2
	//Частота APB1 для таймеров = APB1Clk * 2 = 32МГц * 2 = 64МГц
	TIM2->PSC = 64-1;					//Предделитель частоты (64МГц/6400 = 10кГц)
	TIM2->ARR = 3-1;						//Модуль счёта таймера (1кГц/1000 = 1с)
	TIM2->DIER |= TIM_DIER_UIE;				//Разрешить прерывание по переполнению таймера
	TIM2->CR1 |= TIM_CR1_CEN;				//Включить таймер

	NVIC_EnableIRQ(TIM2_IRQn);				//Рарзрешить прерывание от TIM2
	NVIC_SetPriority(TIM2_IRQn, 1);			//Выставляем приоритет
}

/**
  * @brief  Инициализация таймера TIM3
  * @param  None
  * @retval None
  */
void initTIM3(void)
{
	RCC->APB1ENR |= RCC_APB1ENR_TIM3EN;		//Включить тактирование TIM6

	//Частота APB1 для таймеров = APB1Clk * 2 = 32МГц * 2 = 64МГц
	TIM3->PSC = 64000-1;					//Предделитель частоты (64МГц/64000 = 1кГц)
	TIM3->ARR = 1000-1;						//Модуль счёта таймера (1кГц/1 = 1мс)
	TIM3->DIER |= TIM_DIER_UIE;				//Разрешить прерывание по переполнению таймера
	TIM3->CR1 |= TIM_CR1_CEN;				//Включить таймер

	NVIC_EnableIRQ(TIM3_IRQn);				//Рарзрешить прерывание от TIM2
	NVIC_SetPriority(TIM3_IRQn, 1);			//Выставляем приоритет
}


/**
  * @brief  Инициализация систем тактирования:
  * 		Источник тактирования: HSI
  * 		Частота: 64МГц
  * @param  None
  * @retval None
  */
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

/**
  * @brief  Передача строки по USART2 без DMA
  * @param  *str - указатель на строку
  * @param  crlf - если true, перед отправкой добавить строке символы конца строки
  * @retval None
  */
void txStr(char *str, bool crlf)
{
	uint16_t i;

	if (crlf)												//если просят,
		strcat(str,"\r");									//добавляем символ конца строки

	for (i = 0; i < strlen(str); i++)
	{
		USART2->DR = str[i];								//передаём байт данных
		while ((USART2->SR & USART_SR_TC)==0) {};			//ждём окончания передачи
	}
}

/**
  * @brief  Передача строки по USART2 с помощью DMA
  * @param  str - Указатель на строку
  *  @param  crlf - если true, перед отправкой добавить строке символы конца строки
  * @retval None
  */
void txStrWithDMA(char *str, bool crlf)
{
	if (crlf)												//если просят,
		strcat(str,"\r\n");									//добавляем символ конца строки

	DMA1_Channel7->CCR &= ~DMA_CCR_EN;						//выключаем DMA
	DMA1_Channel7->CMAR = (uint32_t)str;					//указатель на строку, которую нужно передать
	DMA1_Channel7->CNDTR = strlen(str);						//длина строки
	DMA1->IFCR |= DMA_IFCR_CTCIF7;							//сброс флага окончания обмена
	DMA1_Channel7->CCR |= DMA_CCR_EN;    					//включить DMA
}

/**
  * @brief  Обработчик команд
  * @param  None
  * @retval None
  */
void ExecuteCommand(void)
{
	//txStr(RxBuffer, false);
	//txStr("\n", false);
	memset(TxBuffer,0,sizeof(TxBuffer));					//Очистка буфера передачи

	/* Обработчик команд */
	if (strncmp(RxBuffer,"*IDN?",5) == 0)					//Это команда "*IDN?"
	{
		//Она самая, возвращаем строку идентификации
		strcpy(TxBuffer,"Kruglov V.S., Bogatkov A.D., ICS4-73B");
	}
	else if (strncmp(RxBuffer,"REGIME",6) == 0)				//Команда запуска таймера?
	{
		uint16_t regime_value;
		sscanf(RxBuffer,"%*s %hu", &regime_value);				//преобразуем строку в целое число

		if ((1 <= regime_value) && (regime_value <= 3))		//параметр должен быть в заданных пределах!
		{
			switch(regime_value)	{
			case 1:
				LED_brightness = LED_BRIGHTNESS_LOW;
				break;
			case 2:
				LED_brightness = LED_BRIGHTNESS_MEDIUM;
				break;
			case 3:
				LED_brightness = LED_BRIGHTNESS_HIGH;
				break;
			}
			strcpy(TxBuffer, "OK");
		}
		else
			strcpy(TxBuffer, "Parameter is out of range");	//ругаемся
	}
	else if (strncmp(RxBuffer,"BRIGHTNESS",10) == 0)				//Команда остановки таймера?
	{
		uint16_t regime_value;
		sscanf(RxBuffer,"%*s %hu", &regime_value);				//преобразуем строку в целое число

		if ((0 <= regime_value) && (regime_value <= 100))		//параметр должен быть в заданных пределах!
		{
			LED_brightness = regime_value;
			strcpy(TxBuffer, "OK");
		}
		else
			strcpy(TxBuffer, "Parameter is out of range");	//ругаемся
	}
	else if (strncmp(RxBuffer,"PERIOD",6) == 0)				//Команда изменения периода таймера?
	{
		uint16_t tim_value;
		sscanf(RxBuffer,"%*s %hu", &tim_value);				//преобразуем строку в целое число

		if ((100 <= tim_value) && (tim_value <= 3000))		//параметр должен быть в заданных пределах!
		{
			TIM3->ARR = tim_value;
			TIM3->CNT = 0;

			strcpy(TxBuffer, "OK");
		}
		else
			strcpy(TxBuffer, "Parameter is out of range");	//ругаемся
	}
	else
		strcpy(TxBuffer,"Invalid Command");					//Если мы не знаем, чего от нас хотят, ругаемся в ответ

	// Передача принятой строки обратно одним из двух способов
	#ifdef USE_DMA
		txStrWithDMA(TxBuffer, true);
	#else
		txStr(TxBuffer, true);
	#endif

	memset(RxBuffer,0,RX_BUFF_SIZE);						//Очистка буфера приёма
	ComReceived = false;									//Сбрасываем флаг приёма строки
}
/**
  * @brief  Инициализация DMA для передачи данных по USART2
  * @param  None
  * @retval None
  */
void initDMA(void)
{
	RCC->AHBENR |= RCC_AHBENR_DMA1EN;				//разрешить такт. DMA

	DMA1_Channel7->CPAR = (uint32_t)&USART2->DR;	//указатель на регистр данных USART2

	DMA1_Channel7->CCR = 0;
	DMA1_Channel7->CCR |= DMA_CCR_DIR;				//направление - из памяти в устройство
	DMA1_Channel7->CCR |= DMA_CCR_MINC;			//инкремент указателя в памяти
	USART2->CR3 |= USART_CR3_DMAT;					//настроить USART2 на работу с DMA
}
/**
  * @brief  Основная программа
  * @param  None
  * @retval None
  */
int main(void)
{
	/*Инициализации всякие*/
	initClk();
	initPorts();
	initButton();
	initTIM2();
	initTIM3();
	initUSART2();
	initDMA();
	/*Основной цикл*/
	while(true){
		if (ComReceived){				//Ждём приема строки
			ExecuteCommand();
		}
	};
}

/**
  * @brief  Подпрограмма задержки
  * @param  takts - Кол-во тактов
  * @retval None
  */
void delay(uint32_t takts)
{
	for (uint32_t i = 0; i < takts; i++) {};
}
