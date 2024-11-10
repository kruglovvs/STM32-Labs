Reset_Handler:
  ldr   r0, =_estack
  mov   sp, r0          /* set stack pointer */

 ldr		r0, =PERIPH_BB_BASE + \
				(RCC_APB2ENR-PERIPH_BASE)*32 + \
				2*4						@ вычисляем адрес для BitBanding 2-го бита регистра RCC_APB2ENR
	mov		r1, #1						@ включаем тактирование порта A (во 2-й бит RCC_APB2ENR пишем '1`)
	str 	r1, [r0]					@ загружаем это значение
	ldr		r0, =GPIOA_CRL				@ адрес порта
	mov		r1, #0x0003					@ 4-битная маска настроек для Output mode 50mHz, Push-Pull ("0011")
	ldr		r2, [r0]					@ считать порт
    bfi		r2, r1, #20, #4    			@ скопировать биты маски в позицию PIN5
    str		r2, [r0]					@ загрузить результат в регистр настройки порта
    ldr		r0, =GPIOA_BSRR				@ адрес порта выходных сигналов

    ldr		r3, =PERIPH_BB_BASE + \
				(RCC_APB2ENR-PERIPH_BASE)*32 + \
				4*4						@ вычисляем адрес для BitBanding 2-го бита регистра RCC_APB2ENR
	mov		r4, #1						@ включаем тактирование порта С
	str 	r4, [r3]					@ загружаем это значение
	ldr		r3, =GPIOC_CRH				@ адрес порта
	mov		r4, #0b1000					@ 4-битная маска настроек для Output mode 50mHz, Push-Pull ("0011")
	ldr		r5, [r3]					@ считать порт
    bfi		r5, r4, #20, #4    			@ скопировать биты маски в позицию PIN5
    str		r5, [r3]					@ загрузить результат в регистр настройки порта

	mov 	r9, #1						@ r9 означает текущее состояние яркости

loop:									@ Бесконечный цикл
	ldr 	r7, =GPIOC_IDR				@ считать входные значения
	ldr 	r6, [r7]					@ загружаем в порт
	and 	r6,	#0x2000
	cmp		r6,	#0
	beq		button_pressed
	bge		button_unpressed
button_pressed:
	ldr	r10, =0x00FF					@ псевдоинструкция Thumb (загрузить константу в регистр)
	bl		delay						@ задержка
	ldr 	r6, [r7]					@ загружаем в порт
	and 	r6,	#0x2000
	cmp		r6,	#0
	it eq
	addeq 	r9, #0x001
button_unpressed:
	cmp		r9,	#0x003
	it	eq
	moveq	r9, #0
	bl		led_light
	b 		loop

led_light:								@ функция отправки 1 ШИМ сигнала, сперва нужно записать в R9 уровень яркости
	push 	{lr}
	cmp 	r9, #0						@ если уровень яркости 0
    beq led_light_little
	cmp 	r9, #1						@ если уровень яркости 1
    beq led_light_medium
	cmp 	r9, #2						@ если уровень яркости 2
    beq led_light_large
led_light_little:
	ldr 	r1, =GPIO_BSRR_BS5			@ устанавливаем вывод в '1'
	str 	r1, [r0]					@ загружаем в порт
	ldr	r10, =0x00FF					@ псевдоинструкция Thumb (загрузить константу в регистр)
	bl		delay						@ задержка
	ldr		r1, =GPIO_BSRR_BR5			@ сбрасываем в '0'
	str 	r1, [r0]					@ загружаем в порт
	ldr	r10, =0x0F00					@ псевдоинструкция Thumb (загрузить константу в регистр)
	bl		delay						@ задержка
	b		led_light_end
led_light_medium:
	ldr 	r1, =GPIO_BSRR_BS5			@ устанавливаем вывод в '1'
	str 	r1, [r0]					@ загружаем в порт
	ldr	r10, =0x05FF					@ псевдоинструкция Thumb (загрузить константу в регистр)
	bl		delay						@ задержка
	ldr		r1, =GPIO_BSRR_BR5			@ сбрасываем в '0'
	str 	r1, [r0]					@ загружаем в порт
	ldr	r10, =0x0A00					@ псевдоинструкция Thumb (загрузить константу в регистр)
	bl		delay						@ задержка
	b		led_light_end
led_light_large:
	ldr 	r1, =GPIO_BSRR_BS5			@ устанавливаем вывод в '1'
	str 	r1, [r0]					@ загружаем в порт
	ldr	r10, =0x0FF0					@ псевдоинструкция Thumb (загрузить константу в регистр)
	bl		delay						@ задержка
	ldr		r1, =GPIO_BSRR_BR5			@ сбрасываем в '0'
	str 	r1, [r0]					@ загружаем в порт
	ldr	r10, =0x000F					@ псевдоинструкция Thumb (загрузить константу в регистр)
	bl		delay						@ задержка
	b		led_light_end
led_light_end:
	pop 	{lr}
	bx 		lr							@ возвращаемся к началу цикла

delay:									@ функция задержки, сперва нужно записать в R10 начение задержки
	subs	r10, #1						@ SUB с установкой флагов результата
	it 		NE
	bne		delay						@ переход, если Z==0 (результат вычитания не равен нулю)
	bx		lr							@ выход из подпрограммы (переход к адресу в регистре LR - вершина стека)
