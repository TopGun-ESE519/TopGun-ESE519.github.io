/**
 * Hunter Adams (vha3@cornell.edu)
 * 
 * Mandelbrot set calculation and visualization
 * Uses PIO-assembly VGA driver.
 * 
 * Core 1 draws the bottom half of the set using floating point.
 * Core 0 draws the top half of the set using fixed point.
 * This illustrates the speed improvement of fixed point over floating point.
 * 
 * https://vanhunteradams.com/FixedPoint/FixedPoint.html
 * https://vanhunteradams.com/Pico/VGA/VGA.html
 *
 * HARDWARE CONNECTIONS
 *  - GPIO 16 ---> VGA Hsync
 *  - GPIO 17 ---> VGA Vsync
 *  - GPIO 18 ---> 330 ohm resistor ---> VGA Red
 *  - GPIO 19 ---> 330 ohm resistor ---> VGA Green
 *  - GPIO 20 ---> 330 ohm resistor ---> VGA Blue
 *  - RP2040 GND ---> VGA GND
 *
 * RESOURCES USED
 *  - PIO state machines 0, 1, and 2 on PIO instance 0
 *  - DMA channels 0 and 1
 *  - 153.6 kBytes of RAM (for pixel color data)
 *
 */
#include "vga_graphics.h"
#include <stdio.h>
#include <stdlib.h>
#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "hardware/pio.h"
#include "hardware/dma.h"
#include "hardware/adc.h"
#include "registers.h"
#include "hardware/irq.h"  // interrupts
#include "pwm.h"  // pwm 
#include "hardware/sync.h" // wait for interrupt 
#include <time.h>

////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////// Stuff for Mandelbrot ///////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
// Fixed point data type
typedef signed int fix28 ;
#define multfix28(a,b) ((fix28)(((( signed long long)(a))*(( signed long long)(b)))>>28)) 
#define float2fix28(a) ((fix28)((a)*268435456.0f)) // 2^28
#define fix2float28(a) ((float)(a)/268435456.0f) 
#define int2fix28(a) ((a)<<28)
// the fixed point value 4
#define FOURfix28 0x40000000 
#define SIXTEENTHfix28 0x01000000
#define ONEfix28 0x10000000


// Maximum number of iterations
#define max_count 1000

#define LEFT_VERT 150
#define MID_VERT 290
#define RIGHT_VERT 430

#define LEFT_VERT_TILES 160
#define MID_VERT_TILES 300
#define RIGHT_VERT_TILES 440
// Audio PIN is to match some of the design guide shields. 
#define AUDIO_PIN 28  // you can change this to whatever you like
#define RESTART_PIN 4
#define sw 22
#define RESTART_PIN_REG ((volatile uint32_t *)(IO_BANK0_BASE + 0x010))
uint16_t x_previous;
uint16_t y_previous;

#include "sample.h"
int wav_position = 0;
bool flag=false;

void pwm_interrupt_handler() {
    pwm_clear_irq(pwm_gpio_to_slice_num(AUDIO_PIN));    
    if (wav_position < (WAV_DATA_LENGTH<<3) - 1) { 
        // set pwm level 
        // allow the pwm value to repeat for 8 cycles this is >>3 
        pwm_set_gpio_level(AUDIO_PIN, WAV_DATA[wav_position>>3]);  
        wav_position++;
    } else {
        // reset to start
        wav_position = 0;
    }
}
void draw_jet(uint16_t x0, uint16_t y0, uint16_t color){
    fillRect(x0, y0, 30,17, color);
    fillCircle(x0+30, y0+8, 8, color);     
    int y=y0-5;
    for (int i=1; i<=5;i++){
        drawHLine(x0,  y, i,  color);
        y++;
    }
    y=y0+16;
    for (int i=5; i>0;i--){
        drawHLine(x0,  y, i,  color);
        y++;
    }
    y=y0-10;
    for (int i=1; i<=10;i++){
        drawHLine(x0+10,  y, i,  color);
        y++;
    }
    y=y0+16;
    for (int i=10; i>0;i--){
        drawHLine(x0+10,  y, i,  color);
        y++;
    }           
}
void wings(uint16_t x0, uint16_t y0, uint16_t h, uint16_t color){
    int x=x0;
    int y=y0-1;

    for (int i=h; i>0;i--){
        drawHLine(x, y, i,  color);
        y--;
        x++;
        }

    x=x0;
    y=y0+17;

    for (int i=h; i>0;i--){
        drawHLine(x, y, i,  color);
        y++;
        x++;    
    }
}
void draw_missile(uint16_t x, uint16_t y, uint16_t color){
    fillRect(x,y, 30,17, color);
    fillCircle(x, y+8, 8, color);
    wings(x+25, y, 5 , color);
}

void jet_adc(uint16_t *x_pos, uint16_t *y_pos, uint16_t color1){
    
    const uint adc_max = (1 << 12) - 1;
    const uint bar_width = 40;
    int radius=0;

    adc_select_input(0);
    uint adc_x_raw = adc_read();
    adc_select_input(1);
    uint adc_y_raw = adc_read();

    uint bar_x_pos = adc_x_raw * bar_width / adc_max;
    uint bar_y_pos = adc_y_raw * bar_width / adc_max;
    printf("X: %d\n",bar_x_pos);
    printf("Y: %d\n",bar_y_pos);
    if (bar_x_pos<15){
        printf("X+\n");
        *x_pos-=10;}
    if (bar_x_pos>25){
        *x_pos+=10;
        printf("X-\n");
        }
    if (bar_y_pos<15){
        *y_pos+=10;
        printf("Y+\n");
        }
    if (bar_y_pos>25){
        *y_pos-=10;
        printf("Y-\n");
        }

    if(x_previous!=*x_pos|| y_previous!=*y_pos){
    draw_jet( x_previous,  y_previous,  BLACK);
    x_previous=*x_pos;
    y_previous=*y_pos;
    }
    draw_jet( (short)*x_pos, (short)(*y_pos),  YELLOW);
        
       sleep_ms(10);       
}



void draw_fill_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, char color, uint16_t inc_dec){
    fillRect(x,y,w,h,color);
    fillRect(x,y,w,inc_dec,0);
    fillRect(x,y+h,w,inc_dec,color);
    sleep_ms(10);
}

void update_score(uint score){
    fillRect(30,60,240,20,0);
    /* setCursor(30, 30); */
    /* setTextSize(3); */
    char str_score[3] = {'0', '0', '0'};
    str_score[2] = (score % 10) + '0';
    str_score[1] = ((score/10) % 10) + '0';
    str_score[0] = (((score/10)/10) % 10) + '0';
    drawChar(30, 60, str_score[0], WHITE, 0, 2);
    drawChar(45, 60, str_score[1], WHITE, 0, 2);
    drawChar(60, 60, str_score[2], WHITE, 0, 2);
}

void game_over(uint16_t color){
    drawChar(240, 180, 'G', color, 0, 5);
    drawChar(270, 180, 'A', color, 0, 5);
    drawChar(300, 180, 'M', color, 0, 5);
    drawChar(330, 180, 'E', color, 0, 5);
    drawChar(240, 280, 'O', color, 0, 5);
    drawChar(270, 280, 'V', color, 0, 5);
    drawChar(300, 280, 'E', color, 0, 5);
    drawChar(330, 280, 'R', color, 0, 5);
}

void new_game(uint16_t color){
    drawChar(180, 240, 'N', color, 0, 5);
    drawChar(210, 240, 'E', color, 0, 5);
    drawChar(240, 240, 'W', color, 0, 5);
    drawChar(270, 240, ' ', color, 0, 5);
    drawChar(300, 240, 'G', color, 0, 5);
    drawChar(330, 240, 'A', color, 0, 5);
    drawChar(360, 240, 'M', color, 0, 5);
    drawChar(390, 240, 'E', color, 0, 5);
    drawChar(420, 240, '!', color, 0, 5);
    drawChar(450, 240, '!', color, 0, 5);
}
void win_game(uint16_t color){
    drawChar(180, 240, 'Y', color, 0, 5);
    drawChar(210, 240, 'O', color, 0, 5);
    drawChar(240, 240, 'U', color, 0, 5);
    drawChar(270, 240, ' ', color, 0, 5);
    drawChar(300, 240, 'W', color, 0, 5);
    drawChar(330, 240, 'O', color, 0, 5);
    drawChar(360, 240, 'N', color, 0, 5);
    drawChar(390, 240, '!', color, 0, 5); 
    drawChar(420, 240, '!', color, 0, 5);
}

int * getRandom( ) {

   static int  r[10];
   int i;

   for ( i = 0; i < 10; ++i) {
        r[i] = rand()%100 + 10;
    //  printf( "r[%d] = %d\n", i, r[i]);
   }

   return r;
}

void game(){
    game_over(BLACK);
    win_game(BLACK);
    new_game(WHITE);
    sleep_ms(1000);
    new_game(BLACK);

    int x[7];
    for (int i=0; i<7;i++){
        x[i]=570;
    }
    
    uint16_t x_pos=100;
    uint16_t y_pos=240;
    while(true) {

          /* a pointer to an int */
        int *p;
        int i;
        int a=1000;
        int b=1000;
            int c=8;
        int d=8;
        int e=0;


        for (int i=0; i<7;i++){
            if (x[i]<0)
            x[i]=570;
        }
        int y_temp=420;
        for(int i=0; i<7;i++){
            jet_adc(&x_pos, &y_pos , YELLOW);
            draw_missile( x[i],  y_temp,  RED);
            y_temp-=50;
            
        }

        sleep_ms(1000);

        y_temp=420;
        for(int i=0; i<7;i++){
            jet_adc(&x_pos, &y_pos , YELLOW);
            draw_missile( x[i],  y_temp,  BLACK);
            y_temp-=50;
            
        }
        p = getRandom();


        int y_=y_pos-10;
        for(int y=y_;y<y_pos+28;y+=18) {
            if(y>414 && y<443){
                if(a==1000)
                    a=x[0]-8;
                else
                    b=x[0]-8;
                    // printf("1");
                if(c==1)
                    c = 1;
                else
                    d=1;
                    printf("1\n");
            }
            if(y>364 && y<393){
                if(a==1000)
                    a=x[1]-8;
                else
                    b=x[1]-8;
                    // printf("2");
                if(c==8)
                    c = 2;
                else
                    d=2;
                    printf("2\n");
            }
            if(y>314 && y<343){
                if(a==1000)
                    a=x[2]-8;
                else
                    b=x[2]-8;
                    // printf("3");
                if(c==8)
                    c = 3;
                else
                    d=3;
                    printf("3\n");
            }
            if(y>264 && y<293){
                if(a==1000)
                    a=x[3]-8;
                else
                    b=x[3]-8;
                    // printf("4");
                if(c==8)
                    c = 4;
                else
                    d=4;
                    printf("4\n");
            }
            if(y>214 && y<243){
                if(a==1000)
                    a=x[4]-8;
                else
                    b=x[4]-8;
                    // printf("5");
                if(c==8)
                    c = 5;
                else
                    d=4;
                    printf("5\n");
            }
            if(y>164 && y<193){
                if(a==1000)
                    a=x[5]-8;
                else
                    b=x[5]-8;
                    // printf("6");
                if(c==8)
                    c = 6;
                else
                    d=6;
                    printf("6\n");
            }
            if(y>114 && y<143){
                if(a==1000)
                    a=x[6]-8;
                else
                    b=x[6]-8;
                    // printf("7");
                if(c==8)
                    c = 7;
                else
                d=7;
                    printf("7\n");
            }
        }

      
        if(((a-x_pos-38)/50) && ((b-x_pos-38)/50)){
            fillRect(x_pos+38,y_pos+8, 30,3, GREEN);
            sleep_ms(50);
            fillRect(x_pos+38,y_pos+8, 30,3, BLACK);
        }

        else if((a-x_pos-38)/50){
            fillRect(x_pos+38,y_pos+8-30, 3,30, GREEN);
            sleep_ms(50);
            fillRect(x_pos+38,y_pos+8-30, 3,30, BLACK);  
        }

        else if((b-x_pos-38)/50){
            fillRect(x_pos+38,y_pos+8, 3,30, GREEN);
            sleep_ms(50);
            fillRect(x_pos+38,y_pos+8, 3,30, BLACK);  
        }
            
            
        // if ((((x_pos+70)>a)&&((x_pos-30)<a))||(((x_pos+70)>b)&&((x_pos-30)<a))||x_pos<30||y_pos<10||y_pos>450){

        if ((((x_pos+70)>a)&&((x_pos-30)<a))||(((x_pos+70)>b)&&((x_pos-30)<a))){
            game_over(RED);
            printf("Game Over. Noob.\n");
            printf("a:%d, b:%d\n",a,b);

            while(true){
                if(gpio_get(22))
                    game();
            }
            }

            if(x_pos+38>620){
                win_game(WHITE);
                while(true){
                if(gpio_get(22))
                    game();
                }        
            }
                for (int i=0; i<7;i++){
                    x[i]-=p[i];
                }
    }
}

int main() {
    stdio_init_all();

    gpio_init(22);
    gpio_set_dir(22, GPIO_IN);
 
    
    gpio_init(RESTART_PIN);

    gpio_set_dir(RESTART_PIN, GPIO_IN);

    // Initialize stdio
    
    // Initialize VGA
    initVGA() ;

    /* int pattern_array[6] = {20, 80, 20, 120, 60, 20} */
    
    adc_init();
    // Make sure GPIO is high-impedance, no pullups etc
    adc_gpio_init(26);
    adc_gpio_init(27);

    const uint adc_max = (1 << 12) - 1;
    const uint bar_width = 40;

    uint blue_indx = 20, green_indx = 40, cyan_indx = 60, joystick_pos = 0;
    uint curr_score = 0, buttons_status = 0;

    // set_sys_clock_khz(176000, true); 
    gpio_set_function(AUDIO_PIN, GPIO_FUNC_PWM);

    int audio_pin_slice = pwm_gpio_to_slice_num(AUDIO_PIN);

    // Setup PWM interrupt to fire when PWM cycle is complete
    pwm_clear_irq(audio_pin_slice);
    pwm_set_irq_enabled(audio_pin_slice, true);
    // set the handle function above
    irq_set_exclusive_handler(PWM_IRQ_WRAP, pwm_interrupt_handler); 
    irq_set_enabled(PWM_IRQ_WRAP, true);


    // Setup PWM for audio output
    pwm_config config = pwm_get_default_config();
    /* Base clock 176,000,000 Hz divide by wrap 250 then the clock divider further divides
     * to set the interrupt rate. 
     * 
     * 11 KHz is fine for speech. Phone lines generally sample at 8 KHz
     * 
     * 
     * So clkdiv should be as follows for given sample rate
     *  8.0f for 11 KHz
     *  4.0f for 22 KHz
     *  2.0f for 44 KHz etc
     */

    pwm_config_set_clkdiv(&config, 8.0f); 
    pwm_config_set_wrap(&config, 250); 
    pwm_init(audio_pin_slice, &config, true);

    pwm_set_gpio_level(AUDIO_PIN, 0);
    drawChar(210, 40, 'T', GREEN, 0, 5);
    drawChar(240, 40, 'o', GREEN, 0, 5);
    drawChar(270, 40, 'p', GREEN, 0, 5);
    drawChar(300, 40, ' ', GREEN, 0, 5);
    drawChar(330, 40, 'G', GREEN, 0, 5);
    drawChar(360, 40, 'u', GREEN, 0, 5);
    drawChar(390, 40, 'n', GREEN, 0, 5);
   

    game();
    
    }
