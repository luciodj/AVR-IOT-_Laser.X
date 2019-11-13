/*
\file   main.c

\brief  Main source file.

(c) 2018 Microchip Technology Inc. and its subsidiaries.

Subject to your compliance with these terms, you may use Microchip software and any
derivatives exclusively with Microchip products. It is your responsibility to comply with third party
license terms applicable to your use of third party software (including open source software) that
may accompany Microchip software.

THIS SOFTWARE IS SUPPLIED BY MICROCHIP "AS IS". NO WARRANTIES, WHETHER
EXPRESS, IMPLIED OR STATUTORY, APPLY TO THIS SOFTWARE, INCLUDING ANY
IMPLIED WARRANTIES OF NON-INFRINGEMENT, MERCHANTABILITY, AND FITNESS
FOR A PARTICULAR PURPOSE.

IN NO EVENT WILL MICROCHIP BE LIABLE FOR ANY INDIRECT, SPECIAL, PUNITIVE,
INCIDENTAL OR CONSEQUENTIAL LOSS, DAMAGE, COST OR EXPENSE OF ANY KIND
WHATSOEVER RELATED TO THE SOFTWARE, HOWEVER CAUSED, EVEN IF MICROCHIP
HAS BEEN ADVISED OF THE POSSIBILITY OR THE DAMAGES ARE FORESEEABLE. TO
THE FULLEST EXTENT ALLOWED BY LAW, MICROCHIP'S TOTAL LIABILITY ON ALL
CLAIMS IN ANY WAY RELATED TO THIS SOFTWARE WILL NOT EXCEED THE AMOUNT
OF FEES, IF ANY, THAT YOU HAVE PAID DIRECTLY TO MICROCHIP FOR THIS
SOFTWARE.
*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "mcc_generated_files/config/clock_config.h"
#include <util/delay.h>
#include "mcc_generated_files/application_manager.h"
#include "mcc_generated_files/led.h"
#include "mcc_generated_files/sensors_handling.h"
#include "mcc_generated_files/cloud/cloud_service.h"
#include "mcc_generated_files/debug_print.h"
#include "mcc_generated_files/include/port.h"

#include "json.h"


void SERVO_initialize(void){
    TCA0.SINGLE.CTRLA = TCA_SINGLE_CLKSEL_DIV8_gc /* Clock Selection: System Clock / 64 */
                | 1 << TCA_SINGLE_ENABLE_bp; /* Module Enable: enabled */

    TCA0.SINGLE.CTRLB = 0 << TCA_SINGLE_ALUPD_bp /* Auto Lock Update: disabled */
                | 1 << TCA_SINGLE_CMP0EN_bp /* Compare 0 enabled */
                | 1 << TCA_SINGLE_CMP1EN_bp /* Compare 1 enabled */
                | 0 << TCA_SINGLE_CMP2EN_bp /* Compare 2 disabled */
                | TCA_SINGLE_WGMODE_SINGLESLOPE_gc; /* Waveform generation mode: single slope */
    TCA0.SINGLE.PER = 20833; /* 15ms Period @10MHz */

    // enable C0 and C1 output WO0, WO1
    PORTMUX.TCAROUTEA = 2;  // select PC0 PC1 PC2
    PORTC_set_pin_dir(0, PORT_DIR_OUT);
    PORTC_set_pin_dir(1, PORT_DIR_OUT);
}

void PWM0_SetDutyValue(uint16_t dc){
    /* Set Compare 0  */
    TCA0.SINGLE.CMP0 = dc;
}
void PWM1_SetDutyValue(uint16_t dc){
    /* Set Compare 0  */
    TCA0.SINGLE.CMP1 = dc;
}
void PWM2_SetDutyValue(uint16_t dc){
    /* Set Compare 0  */
    TCA0.SINGLE.CMP2 = dc;
}

void SERVO_set(uint8_t servo, int8_t pos) {
    /* pos : -100 <> +100 */
    int16_t center = TCA0.SINGLE.PER / 10;  // 1.5 ms = 15ms/10
    float gain = (center * 0.006);
    if (servo == 0)
        PWM0_SetDutyValue(center + pos * gain);
    else if (servo == 1)
        PWM1_SetDutyValue(center + pos * gain);
    else if (servo == 2)
        PWM2_SetDutyValue(center + pos * gain);
}

#define FONT_SIZE   10 // char + count + 8 vectors
static uint8_t steps = 0;
static uint8_t sequence = 0;
static char Msg[80];
static char *pMsg = NULL;

uint8_t font[] = {
// step encoding f yyy xxxx
//               7 654 3210 bit
//  f = fire, x,y = coordinates
//  steps    0     1     2     3     4     5     6
       'C', 5, 0x04, 0x80, 0x84, 0xC4,    0,    0,    0,   0,
       'M', 6, 0x40, 0x80, 0xA2, 0x84, 0xC4,    0,    0,   0,
       'O', 5, 0x84, 0xC4, 0xC0, 0x80,    0,    0,    0,   0,
       'U', 4, 0xC0, 0xC4, 0x84,    0,    0,    0,    0,   0,
       'S', 7, 0x04, 0x80, 0xA0, 0xA4, 0xC4, 0xC0,    0,   0,
       'E', 8, 0x04, 0x80, 0xA0, 0xA2, 0xA0, 0xC0, 0xC4,   0,
       'R', 8, 0x40, 0x80, 0x84, 0xA4, 0xA0, 0xA2, 0xC4,   0,
       '\0' // end
};

void fire(bool x)
{
    PORTD_set_pin_level(4, x);
}

void stroke(uint8_t c)
{
    uint8_t x = (c>>4) & 0x7;
    uint8_t y = c & 0xf;
    fire(c & 0x80);
    SERVO_set(0, (x<<3)-40); SERVO_set(1, -(y<<3));
}

void sequence_set(char c)
{
    uint8_t index = 0;
    while ((font[index] != c) && (font[index]!= '\0') ) {
        index += FONT_SIZE;
    }
    steps = font[index+1];
    sequence = index+2;
}

void sequence_step(void)
{
    if (steps) {
        stroke(font[sequence++]);
        steps--;
        if (steps == 0) { // advance to next char in Msg string
            uint8_t c = *pMsg++;
            if (c)
                sequence_set(c);
        }
    }
}

//This handles messages published from the MQTT server when subscribed
void receivedFromCloud(uint8_t *topic, uint8_t *payload)
{
	LED_flashRed();
//	debug_printer(SEVERITY_NONE, LEVEL_NORMAL, "topic: %s", topic);
	debug_printer(SEVERITY_NONE, LEVEL_NORMAL, "payload: %s", payload);
    int pan, tilt, fire;
    if (JSON_getInt(payload, "pan", &pan)) {
        SERVO_set(1, pan -7 );
    }
    if (JSON_getInt(payload, "tilt", &tilt)) {
        SERVO_set(0, tilt -19);
    }

    if (JSON_getInt(payload, "fire", &fire))
        PORTD_set_pin_level(4, (fire == 1));

    uint8_t *p = JSON_getValue(payload, "text1");
    if (*p++ == '"') { // must be a string
        char c, *sMsg = &Msg[0];
        while((c = *p++) != '"')    // copy until end of string (")
            *sMsg++ = c;
        pMsg = &Msg[0]; // reset the string pointer
        sequence_set(*pMsg++);
    }
}

// This will get called every CFG_SEND_INTERVAL second only while we have a valid Cloud connection
void sendToCloud(void)
{
    static char json[70];

    int rawTemperature = SENSORS_getTempValue();
    int light = SENSORS_getLightValue();
    int len = sprintf(json, "{\"Light\":%d,\"Temp\":%d}", light,rawTemperature/100);

    if (len >0) {
        CLOUD_publishData((uint8_t*)json, len);
    }

    LED_flashYellow();
}


int main(void)
{
    uint8_t count = 0;
    application_init();
    SERVO_initialize();
    // enable laser output control pin
    PORTD_set_pin_dir(4, PORT_DIR_OUT);


    while (1)
    {
        runScheduler();
        if (TCA0.SINGLE.INTFLAGS & 1) { // check TCA0 period OVF
            TCA0.SINGLE.INTFLAGS = 1;   // clear interrupt
            if ((count++ & 0xf) == 0)
                sequence_step();
       }
    }

    return 0;
}
