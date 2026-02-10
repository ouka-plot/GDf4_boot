#include "delay.h"
#include "core_cm4.h"
//³õÊ¼»¯Ê±ÖÓ
void delay_init(void){

  systick_clksource_set(SYSTICK_CLKSOURCE_HCLK);


}



//us
void delay_us(uint32_t num){



  SysTick->LOAD  = num * 240;                                  /* set reload register */
 
  SysTick->VAL   = 0;                                          /* Load the SysTick Counter Value */
  SysTick->CTRL  |= SysTick_CTRL_ENABLE_Msk;

  while(!(SysTick->CTRL & SysTick_CTRL_COUNTFLAG_Msk));
  SysTick->CTRL  &= ~ SysTick_CTRL_ENABLE_Msk;
}


//ms
void delay_ms(uint32_t num){

   while(num--)
   {
        delay_us(1000);
   
 
   }





}
