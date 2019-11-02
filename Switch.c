#include <stdint.h>
#include "../inc/tm4c123gh6pm.h"
#include "Switch.h"
#include "Tracks.h"

void Timer2Arm(void){
	SYSCTL_RCGCTIMER_R |= 0x04;   // 0) activate timer2
  TIMER2_CTL_R = 0x00000000;    // 1) disable TIMER2A during setup
  TIMER2_CFG_R = 0x00000000;    // 2) configure for 32-bit mode
  TIMER2_TAMR_R = 0x0000001;    // 3) 1-SHOT mode
  TIMER2_TAILR_R = 160000;      // 4) 10ms reload value
  TIMER2_TAPR_R = 0;            // 5) bus clock resolution
  TIMER2_ICR_R = 0x00000001;    // 6) clear TIMER2A timeout flag
  TIMER2_IMR_R = 0x00000001;    // 7) arm timeout interrupt
  NVIC_PRI5_R = (NVIC_PRI5_R&0x00FFFFFF)|0x80000000; // 8) priority 4
// interrupts enabled in the main program after all devices initialized
// vector number 35, interrupt number 19
  NVIC_EN0_R = 1<<23;           // 9) enable IRQ 19 in NVIC
  TIMER2_CTL_R = 0x00000001;    // 10) enable TIMER0A
}

uint32_t RisingEdges;

void GPIOArm(){
	GPIO_PORTB_ICR_R |= GPIO_PORTB_DATA_R;
	GPIO_PORTB_IM_R |= GPIO_PORTB_DATA_R;
	NVIC_PRI7_R = (NVIC_PRI7_R&0xFF00FFFF)|0x00A00000;
	NVIC_EN0_R = 0x40000000;
}
	
void Switch_Init(void){                          
  SYSCTL_RCGCGPIO_R |= 0x00000002; // (a) activate clock for port F
  RisingEdges = 0;             // (b) initialize counter
  GPIO_PORTB_DIR_R &= ~0xCF;    // (c) make PB0-3 , 6, 7 in (built-in button)
  GPIO_PORTB_AFSEL_R &= ~0xCF;  //     disable alt funct on PB0-3 , 6, 7
  GPIO_PORTB_DEN_R |= 0xCF;     //     enable digital I/O on PB0-3 , 6, 7   
  GPIO_PORTB_PCTL_R &= ~0xFF00FFFF; // configure PB0-3 , 6, 7 as GPIO
  GPIO_PORTB_AMSEL_R = 0;       //     disable analog functionality on PF
  //GPIO_PORTF_PUR_R |= 0x10;     //     enable weak pull-up on PF4
  GPIO_PORTB_IS_R &= ~0xCF;     // (d) PB0-3 , 6, 7 is edge-sensitive
  GPIO_PORTB_IBE_R |= 0xCF;    //     PB0-3 , 6, 7 is both edges
  //GPIO_PORTB_IEV_R |= 0xCF;    //     PB0-3 , 6, 7 rising edge event
  //GPIO_PORTB_ICR_R = 0xCF;      // (e) clear flag0-3, 6, 7
  //GPIO_PORTB_IM_R |= 0xCF;      // (f) arm interrupt on PB0-3 , 6, 7 *** No IME bit as mentioned in Book ***

	
	//GPIOArm();
	
}
	
void GPIOPortB_Handler(void){
	if (GPIO_PORTB_RIS_R > 0){
		GPIO_PORTB_IM_R &= ~GPIO_PORTB_DATA_R;
    //GPIO_PORTB_ICR_R = GPIO_PORTB_DATA_R;      // acknowledge flag0-3, 6, 7
    RisingEdges = RisingEdges + 1;
	  Timer2Arm();
	}
}
	
uint8_t buttonVals[10];
uint8_t flag = 0;




//this will disable interrupts every 10ms, to debounce the switch
void Timer2A_Handler(void){
	TIMER2_IMR_R = 0x00000000;
	flag = 0;
	Buttons_Pressed();
	GPIOArm();
}


uint32_t bounce1[2];
uint32_t bounce2[2];
uint32_t bounce3[2];



uint32_t a;

void Buttons_Pressed(){
	
	uint32_t portb = GPIO_PORTB_DATA_R;
	
	if(GPIO_PORTB_DATA_R & 0x01) {	
		
		GPIO_PORTF_DATA_R = 0x04; // red
		
	}
	
	if(GPIO_PORTB_DATA_R & 0x02){
		
		GPIO_PORTF_DATA_R&= ~0x04; // blue
		
	}
	
	if(GPIO_PORTB_DATA_R & 0x04){

		GPIO_PORTF_DATA_R = 0x04; // red
	
	}
	
	if(GPIO_PORTB_DATA_R & 0x08){
		
		GPIO_PORTF_DATA_R&= ~0x04;

	}
	
	if(GPIO_PORTB_DATA_R & 0x40){

		GPIO_PORTF_DATA_R = 0x04; // red
	
	}
	
	if(GPIO_PORTB_DATA_R & 0x80){
		
		GPIO_PORTF_DATA_R&= ~0x04;

	}
	
}