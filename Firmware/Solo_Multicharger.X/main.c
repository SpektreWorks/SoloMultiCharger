/*****************************************************************************
    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*****************************************************************************/

#include <xc.h>

typedef unsigned char ubyte;
typedef unsigned short uword;
typedef unsigned long ulong;

// #pragma config statements should precede project file includes.
// Use project enums instead of #define for ON and OFF.

// PIC16F1773 Configuration Bit Settings
// 'C' source line config statements

// CONFIG1
#pragma config FOSC = INTOSC    // Oscillator Selection Bits (INTOSC oscillator: I/O function on CLKIN pin)
#pragma config WDTE = OFF       // Watchdog Timer Enable (WDT disabled)
#pragma config PWRTE = OFF      // Power-up Timer Enable (PWRT disabled)
#pragma config MCLRE = ON       // MCLR Pin Function Select (MCLR/VPP pin function is MCLR)
#pragma config CP = OFF         // Flash Program Memory Code Protection (Program memory code protection is disabled)
#pragma config BOREN = ON       // Brown-out Reset Enable (Brown-out Reset enabled)
#pragma config CLKOUTEN = OFF   // Clock Out Enable (CLKOUT function is disabled. I/O or oscillator function on the CLKOUT pin)
#pragma config IESO = ON        // Internal/External Switchover Mode (Internal/External Switchover Mode is enabled)
#pragma config FCMEN = ON       // Fail-Safe Clock Monitor Enable (Fail-Safe Clock Monitor is enabled)

// CONFIG2
#pragma config WRT = OFF        // Flash Memory Self-Write Protection (Write protection off)
#pragma config PPS1WAY = ON     // Peripheral Pin Select one-way control (The PPSLOCK bit cannot be cleared once it is set by software)
#pragma config ZCD = OFF        // Zero-cross detect disable (Zero-cross detect circuit is disabled at POR)
#pragma config PLLEN = OFF      // Phase Lock Loop enable (4x PLL is enabled when software sets the SPLLEN bit)
#pragma config STVREN = ON      // Stack Overflow/Underflow Reset Enable (Stack Overflow or Underflow will cause a Reset)
#pragma config BORV = LO        // Brown-out Reset Voltage Selection (Brown-out Reset Voltage (Vbor), low trip point selected.)
#pragma config LPBOR = OFF      // Low-Power Brown Out Reset (Low-Power BOR is disabled)
#pragma config LVP = OFF        // Low-Voltage Programming Enable (High-voltage on MCLR/VPP must be used for programming)

#define _XTAL_FREQ 16000000

#define TRUE 1
#define FALSE 0
#define BAY_ON 1
#define BAY_OFF 0

#define BATT1_CURR_SENSE_CHNL 0b001110 
#define BATT2_CURR_SENSE_CHNL 0b001111 
#define BATT3_CURR_SENSE_CHNL 0b000001 
#define BATT4_CURR_SENSE_CHNL 0b000000 
#define BATT1_V_SENSE_CHNL 0b001000
#define BATT2_V_SENSE_CHNL 0b001010
#define BATT3_V_SENSE_CHNL 0b001011
#define BATT4_V_SENSE_CHNL 0b001001
#define CONTROLLER_V_SENSE_CHNL 0b001100
#define CONTROLLER_CURR_SENSE_CHNL 0b010011

#define SRC1_BAY1 LATAbits.LATA7
#define SRC1_BAY2 LATAbits.LATA6
#define SRC1_BAY3 LATAbits.LATA4
#define SRC2_BAY2 LATCbits.LATC1
#define SRC2_BAY3 LATAbits.LATA3
#define SRC2_BAY4 LATBbits.LATB5

#define CONTROLLER_EN LATCbits.LATC4
#define USB1_EN LATCbits.LATC6
#define USB2_EN LATCbits.LATC5

/* Solo Batteries */
#define DAC_MIN_VOLT 0x3FF
#define ADC2VOLTS 0.01958
#define ADC2AMPS 0.0048876
#define VOLTS_THRESH 14.5
#define AMPS_THRESH 0.050
#define MAX_AMPS 3.3
#define MAX_VOLTS 16.8
#define DAC_15V 450

/* Hand Controller Battery */
#define ADC2VOLTS_HC 0.009775
#define ADC2AMPS_HC 0.0048876
#define MAX_AMPS_HC 1.2
#define MAX_VOLTS_HC 8.3
#define DAC_7V_HC 645

#define SRC1 0b00000001
#define SRC2 0b00000010
#define BATT1 0b00000001
#define BATT2 0b00000010
#define BATT3 0b00000100
#define BATT4 0b00001000

/* Globals */

const ubyte batt_v_chnl[] = {0,BATT1_V_SENSE_CHNL,BATT2_V_SENSE_CHNL,BATT3_V_SENSE_CHNL,BATT4_V_SENSE_CHNL};
const ubyte batt_curr_chnl[] = {0,BATT1_CURR_SENSE_CHNL,BATT2_CURR_SENSE_CHNL,BATT3_CURR_SENSE_CHNL,BATT4_CURR_SENSE_CHNL};

//Master global clock. Increments every overflow of Timer1 (50ms).
volatile ulong clock = 0;

/* All arrays have an extra unit of length so that
 * we can index starting from 1 instead of 0. This
 * way code matches schematic and PCB. */

//Voltage source structure
volatile struct
{
    ubyte in_use;    //whether voltage source is actively charging a battery (t/f)
    ubyte servicing; //which battery bay the voltage source is servicing (1 or 2)
    uword dac;       //the current DAC value controlling the voltage source
}vsrc[4];

//Battery bay structure
volatile struct
{
    ubyte occupied;       //whether a battery is occupying the bay (t/f)
    ubyte setToCharge;    //flag to indicate battery should be charging if it's not already (t/f)
    ubyte charging;       //flag to indicate battery is actively charging
    float measured_volts; //measured voltage at battery bay
    float measured_amps;  //measured current into battery
    ubyte fault;
}bay[5];

/* Functions */

void InitPorts(void)
{
    LATA = 0;
    LATB = 0;
    LATC = 0;

    TRISAbits.TRISA0 = 1; //BATT4_CURR_SENSE
    TRISAbits.TRISA1 = 1; //BATT3_CURR_SENSE
    TRISAbits.TRISA2 = 0; //V2_ADJ_DAC
    TRISAbits.TRISA3 = 0; //BATT3_SRC2
    TRISAbits.TRISA4 = 0; //BATT3_SRC1
    TRISAbits.TRISA5 = 0; //V1_ADJ_DAC
    TRISAbits.TRISA6 = 0; //BATT2_SRC1
    TRISAbits.TRISA7 = 0; //BATT1_SRC1

    TRISBbits.TRISB0 = 1; //CONTROLLER_V_SENSE
    TRISBbits.TRISB1 = 1; //BATT2_V_SENSE
    TRISBbits.TRISB2 = 1; //BATT1_V_SENSE
    TRISBbits.TRISB3 = 1; //BATT4_V_SENSE
    TRISBbits.TRISB4 = 1; //BATT3_V_SENSE
    TRISBbits.TRISB5 = 0; //BATT4_SRC2
    TRISBbits.TRISB6 = 1; //PGC
    TRISBbits.TRISB7 = 1; //PGD

    TRISCbits.TRISC0 = 0; //CONTROLLER_ADJ_DAC
    TRISCbits.TRISC1 = 0; //BATT2_SRC2
    TRISCbits.TRISC2 = 1; //BATT1_CURR_SENSE
    TRISCbits.TRISC3 = 1; //BATT2_CURR_SENSE
    TRISCbits.TRISC4 = 0; //CONTROLLER_EN
    TRISCbits.TRISC5 = 0; //USB2_EN
    TRISCbits.TRISC6 = 0; //USB1_EN
    TRISCbits.TRISC7 = 1; //CONTROLLER_CURR_SENSE

    ANSELA = 0;
    ANSELAbits.ANSA0 = 1; //BATT4_CURR_SENSE (AN0)
    ANSELAbits.ANSA1 = 1; //BATT3_CURR_SENSE (AN1)

    ANSELB = 0;
    ANSELBbits.ANSB0 = 1; //CONTROLLER_V_SENSE (AN12)
    ANSELBbits.ANSB1 = 1; //BATT2_V_SENSE (AN10)
    ANSELBbits.ANSB2 = 1; //BATT1_V_SENSE (AN8)
    ANSELBbits.ANSB3 = 1; //BATT4_V_SENSE (AN9)
    ANSELBbits.ANSB4 = 1; //BATT3_V_SENSE (AN11)

    ANSELC = 0;
    ANSELCbits.ANSC2 = 1; //BATT1_CURR_SENSE (AN14)
    ANSELCbits.ANSC3 = 1; //BATT2_CURR_SENSE (AN15)
    ANSELCbits.ANSC7 = 1; //CONTROLLER_CURR_SENSE (AN16)
}

void InitTimer1(void)
{
    //50ms tic timer, 16-bit
    T1CONbits.CS = 0b00; //FOSC/4
    T1CONbits.CKPS = 0b11; //1:8 prescaler
    T1CONbits.OSCEN = 0;
    PIR1bits.TMR1IF = 0;
    TMR1H = 0x9E;
    TMR1L = 0x57;
    PIE1bits.TMR1IE = 0;
    T1CONbits.ON = 1;
}

void InitADC(void)
{
    ADCON1bits.ADFM = 1; //Right justified
    ADCON1bits.ADCS = 0b100; //FOSC/4
    ADCON1bits.ADNREF = 0; //VREF- is VSS 
    ADCON1bits.ADPREF = 0b00; //VREF+ is VDD
    ADCON0bits.ADON = 1;
}

uword GetADC(ubyte chnl)
{
    //Return a single ADC read from the specified channel (chnl)
    uword result;

    ADCON0bits.CHS = chnl;

    __delay_us(10);
    ADCON0bits.GO = 1;
    while(ADCON0bits.GO);
    result = (((uword)ADRESH)<<8) + ((uword)ADRESL);
    return result; 
}

uword GetADC16(ubyte chnl)
{
    //Return the average of 16 subsequent ADC reads
    uword result;
    ADCON0bits.CHS = chnl;
    
    uword sum = 0;
    for(ubyte a=0;a<16;a++)
    {
        __delay_us(10);
        ADCON0bits.GO = 1;
        while(ADCON0bits.GO);
        sum += ((((uword)ADRESH)<<8) + ((uword)ADRESL));
    }
    result = sum >> 4;
    return result; 
}

void UpdateBayData(ubyte b)
{
    //Check voltage and current flow at bay b. Update whether bay b
    //is occupied with a battery or not. A newly detected battery
    //that is showing a voltage within 200mv of max is considered
    //fully charged and is ignored.

    bay[b].measured_volts = (float)GetADC16(batt_v_chnl[b])*ADC2VOLTS;
    bay[b].measured_amps = (float)GetADC16(batt_curr_chnl[b])*ADC2AMPS;

    if ( !bay[b].occupied )
    {
        //Clean up, just in case one of these is hanging
        bay[b].charging = FALSE;
        bay[b].setToCharge = FALSE;

        if ( bay[b].measured_volts > (MAX_VOLTS-0.2) )
        {
            //1. Fully charged battery (ignore it)
            return;
        }

        if ( bay[b].measured_volts > VOLTS_THRESH )
        {
            //2. Uncharged battery just plugged in
            bay[b].occupied = TRUE;
            return;
        }
    }
    else //bay[b].occupied == TRUE
    {
        if ( (bay[b].measured_volts > VOLTS_THRESH) && (bay[b].measured_amps > AMPS_THRESH) )
        {
            //3. Battery charging
            bay[b].charging = TRUE;
            return;
        }

        if ( (bay[b].measured_volts > VOLTS_THRESH) && (bay[b].measured_amps < AMPS_THRESH) )
        {
            //4. Battery done charging or battery removed while charging
            bay[b].charging = FALSE;
            bay[b].setToCharge = FALSE;
            bay[b].occupied = FALSE;
            return;
        }
    }
}

void SetDAC(ubyte s, uword val)
{
    //Voltage is changed on the voltage sources by making very
    //small adjustments to the center tap of the feedback resistor
    //networks on the switching regulators. The small adjustments
    //are accomplished using buffered DACs, which are controlled here.

    //Reject nonsense values
    if ( val > 0x3FF )return;

    switch(s)
    {
        case 1:
            DAC2REFH = (ubyte)(val>>8);
            DAC2REFL = (ubyte)val;
            DACLDbits.DAC2LD = 1;
            vsrc[1].dac = val;
            break;
        case 2:
            DAC1REFH = (ubyte)(val>>8);
            DAC1REFL = (ubyte)val;
            DACLDbits.DAC1LD = 1;
            vsrc[2].dac = val;
            break;
        case 3: 
            DAC5REFH = (ubyte)(val>>8);
            DAC5REFL = (ubyte)val;
            DACLDbits.DAC5LD = 1;
            vsrc[3].dac = val;
            break;
    }
}

void InitDACs(void)
{
    //Source 1
    DAC2CON0bits.FM = 0; //DAC2 reference selection right justified
    DAC2CON0bits.PSS = 0b00; //VDD is positive source
    DAC2CON0bits.NSS = 0b00; //GND is negative source
    SetDAC(1,0x3FF); //Initialize to highest DAC voltage, which corresponds to lowest regulator output  
    DACLDbits.DAC2LD = 1; //Load in new DAC value
    DAC2CON0bits.OE1 = 1; //DAC2OUT1 enabled
    DAC2CON0bits.EN = 1;
    
    //Source 2
    DAC1CON0bits.FM = 0;
    DAC1CON0bits.PSS = 0b00;
    DAC1CON0bits.NSS = 0b00;
    SetDAC(2,0x3FF);
    DACLDbits.DAC1LD = 1;
    DAC1CON0bits.OE1 = 1;
    DAC1CON0bits.EN = 1;
    
    //Controller source
    DAC5CON0bits.FM = 0;
    DAC5CON0bits.PSS = 0b00;
    DAC5CON0bits.NSS = 0b00;
    SetDAC(3,0x3FF);
    DACLDbits.DAC5LD = 1;
    DAC5CON0bits.OE1 = 1;
    DAC5CON0bits.EN = 1;
}

void InitStructs(void)
{
    //vsrc[0] and bay[0] are unused (see comment at top of code)
    //but initialize to defaults here anyway.
    for(ubyte a=0;a<=3;a++)
    {
        vsrc[a].in_use = FALSE;
        vsrc[a].servicing = 0;
    }

    for(ubyte a=0;a<=4;a++)
    {
        bay[a].occupied = FALSE;
        bay[a].setToCharge = FALSE;
        bay[a].charging = FALSE;
        bay[a].measured_volts = 0.0;
        bay[a].measured_amps = 0.0;
        bay[a].fault = 0;
    }
}

void Enable_HC_and_USB(void)
{
    //Turn on PFETs for the hand controller and both USB ports
    CONTROLLER_EN = TRUE;
    USB1_EN = TRUE;
    USB2_EN = TRUE;
}

void Deactivate(ubyte s)
{
    //Turn off selected source to all batteries at once. There should
    //never be a time that one source is servicing more than one battery.
    //This function provides a common way to automatically update the vsrc
    //structures with correct values.
    SetDAC(s,DAC_MIN_VOLT);
    vsrc[s].servicing = 0;
    vsrc[s].in_use = FALSE;
    switch(s)
    {
        case 1:
            SRC1_BAY1 = FALSE;
            SRC1_BAY2 = FALSE;
            SRC1_BAY3 = FALSE;
            break;
        case 2:
            SRC2_BAY2 = FALSE;
            SRC2_BAY3 = FALSE;
            SRC2_BAY4 = FALSE;
            break;
    }
}

void Activate(ubyte s, ubyte b)
{
    //Low level battery bay control. No error checking done.
    vsrc[s].servicing = b;
    vsrc[s].in_use = TRUE;
    switch(s)
    {
        case 1:
            switch(b)
            {
                case 1: SRC1_BAY1 = TRUE;break;
                case 2: SRC1_BAY2 = TRUE;break;
                case 3: SRC1_BAY3 = TRUE;break;
            }
            break;
        case 2:
            switch(b)
            {
                case 2: SRC2_BAY2 = TRUE;break;
                case 3: SRC2_BAY3 = TRUE;break;
                case 4: SRC2_BAY4 = TRUE;break;
            }
            break;
    }
}

void SwapVsrc(ubyte vsrc_from, ubyte vsrc_to, ubyte b)
{
    //Seemlessly swap which voltage source is servicing a specific battery bay.
    //For example, if voltage source 1 is currently servicing bay 2, the following
    //function call will deactivate voltage source 1 and activate voltage source 2
    //for bay 2 instead: SwapVsrc(1,2,2);

    if ( (b == 1) || (b == 4) )
    {
        //This should never happen because only source 1 can service
        //bay 1 and only source 2 can service bay 4, so these
        //batteries cannot have their sources swapped.
        return;
    }

    if ( (vsrc_from > 2) || (vsrc_to > 2) )
    {
        //Only two voltage sources available for swapping
        return;
    }

    switch(vsrc_from)
    {
        case 1:
            //Set source 2 voltage equal to source 1 voltage.
            SetDAC(2,vsrc[1].dac);

            //Turn on source 2 for specified battery
            switch(b)
            {
                case 2: Activate(2,2);break;
                case 3: Activate(2,3);break;
            }

            //PFET takes a few 10s of milliseconds to fully turn on
            __delay_ms(100);

            //Turn off source 1 from specified battery
            Deactivate(1);
            break;

        case 2:
            //Set source 1 voltage equal to source 2 voltage.
            SetDAC(1,vsrc[2].dac);

            //Turn on source 1 for specified battery
            switch(b)
            {
                case 2: Activate(1,2);break;
                case 3: Activate(1,3);break;
            }

            //PFET takes a few 10s of milliseconds to fully turn on
            __delay_ms(100);

            //Turn off source 2 from specified battery
            Deactivate(2);
            break;
    }
    //There was a voltage spike observed on the "other" port
    //(i.e., if bay 3 has a battery, then a spike was observed
    //on an unconnected bay 2). This function (SwapVsrc) is immediately
    //followed by an Activate() function call, which was apparently
    //turning on the other port before the previous port was fully
    //turned off. This delay appears to remove the spike.
    __delay_ms(20);
}

void BayControl(ubyte b, ubyte bay_onoff)
{
    //High level control of each battery bay with error checking.
    //b: battery bay number
    //bay_onoff: On or off control

    //To minimize hardware components, only two voltage sources serve all
    //four battery bays:
    //  Voltage Source 1 can serve bays 1, 2, and 3.
    //  Voltage Source 2 can serve bays 2, 3, and 4.
    //This introduces the need for active switching of the voltage source between
    //battery bays. If, for example, a battery is charging in bay 2 using voltage 
    //source 1 and then a second battery is placed in bay 1, then voltage source 1
    //must switch over to the new battery and voltage source 2 must take over charging
    //the first battery.

    //If a fault has already been detected on the requested battery bay,
    //just ignore request and exit.
    if ( bay[b].fault )
    {
        return;
    }

    if ( bay_onoff )
    {
        if ( vsrc[1].in_use && vsrc[2].in_use )
        {
            //Two batteries are already charging. Can't do anything more.
            return;
        }

        if ( (vsrc[1].servicing == b) || (vsrc[2].servicing == b) )
        {
            //Requested bay is already being serviced by one of the voltage sources.
            return;
        }

        if ( b == 1 )
        {
            if ( vsrc[1].in_use )
            {
                //Only source 1 can service battery bay 1, so have source 2 take
                //over whichever battery bay source 1 is currently servicing
                SwapVsrc(1,2,vsrc[1].servicing);
            }
            Activate(1,1);
        }
        else if ( (b==2) || (b==3) )
        {
            if ( vsrc[1].in_use )
            {
                Activate(2,b);
            }
            else if ( !vsrc[1].in_use )
            {
                Activate(1,b);
            }
        }
        else if ( b == 4 )
        {
            if ( vsrc[2].in_use )
            {
                //Only source 2 can service battery bay 4, so have source 1 take
                //over whichever battery bay source 2 is currently servicing
                SwapVsrc(2,1,vsrc[2].servicing);
            }
            Activate(2,4);
        }
    }
    else if ( !bay_onoff ) 
    {
        //Deactivating a battery bay results in deacticting the source
        //that was connected to it. So deactivate the whole source here.
        if ( vsrc[1].servicing == b )
        {
            Deactivate(1);
        }
        else if ( vsrc[2].servicing == b )
        {
            Deactivate(2);
        }
    }
}

ubyte NumBaysSetToCharge(void)
{
    return bay[1].setToCharge + bay[2].setToCharge + bay[3].setToCharge + bay[4].setToCharge;
}

ubyte NumBaysOccupied(void)
{
    return bay[1].occupied + bay[2].occupied + bay[3].occupied + bay[4].occupied;
}

ubyte NextToCharge(void)
{
    //Returns the index of the battery bay with the highest measured voltage.
    //A battery that is already charging will be skipped over.
    float highvolts = 0.0;
    ubyte highbay = 0;

    for(ubyte b=1;b<=4;b++)
    {
        if ( !bay[b].setToCharge )
        {
            if ( bay[b].occupied )
            {
                if ( bay[b].measured_volts > highvolts ) 
                {
                    highvolts = bay[b].measured_volts;
                    highbay= b;
                }
            }
        }
    }
    return highbay;
}

void SetToCharge(ubyte b)
{
    //The setToCharge flag is true as soon as a battery has been queued up to charge
    //and remains true for as long as the battery is actively charging.
    bay[b].setToCharge = TRUE;
}

void FaultCheck(void)
{
    float volts;
    float amps;
    static ubyte overcurrent[5] = {0,0,0,0,0};

    for(ubyte b=1;b<=4;b++)
    {
        //Dead short condition
        if ( (bay[b].measured_volts < VOLTS_THRESH) && (bay[b].measured_amps > AMPS_THRESH) )
        {
            bay[b].fault = 1;
        }

        //Another short condition
        if ( (bay[b].occupied == FALSE) && (bay[b].measured_amps > AMPS_THRESH) )
        {
            bay[b].fault = 2;
        }

        //Over-current condition
        //Allow up to 250ms of over-current before throwing fault
        if ( bay[b].measured_amps > 3.5 )
        {
            overcurrent[b]++;
            if ( overcurrent[b] >= 5 )
            {
                bay[b].fault = 3;
            }
        }
        else
        {
            overcurrent[b] = 0;
        }

        //Over-voltage condition
        if ( bay[b].measured_volts > 18.0 )
        {
            bay[b].fault = 4;
        }

        //If any fault condition is detected on a bay, disable
        //all voltage sources associated with that bay.
        if ( bay[b].fault )
        {
            switch(b)
            {
                case 1:
                    SRC1_BAY1 = FALSE;
                    break;
                case 2:
                    SRC1_BAY2 = FALSE;
                    SRC2_BAY2 = FALSE;
                    break;
                case 3:
                    SRC1_BAY3 = FALSE;
                    SRC2_BAY3 = FALSE;
                    break;
                case 4:
                    SRC2_BAY4 = FALSE;
                    break;
            }
            bay[b].occupied = FALSE;
            bay[b].setToCharge = FALSE;
            bay[b].charging = FALSE;

            BayControl(b,BAY_OFF);
        }
    }    
}

void IncrementVolts(ubyte b)
{
    //The smaller the DAC value, the higher the output of the voltage source
    if ( vsrc[1].servicing == b )
    {
        if ( vsrc[1].dac > 0 )
        {
            SetDAC(1,vsrc[1].dac-5);
        }
    }
    else if ( vsrc[2].servicing == b )
    {
        if ( vsrc[2].dac > 0 )
        {
            SetDAC(2,vsrc[2].dac-5);
        }
    }
}

void DecrementVolts(ubyte b)
{
    //The larger the DAC value, the lower the output of the voltage source
    if ( vsrc[1].servicing == b )
    {
        if ( vsrc[1].dac < DAC_MIN_VOLT )
        {
            SetDAC(1,vsrc[1].dac+5);
        }
    }
    else if ( vsrc[2].servicing == b )
    {
        if ( vsrc[2].dac < DAC_MIN_VOLT )
        {
            SetDAC(2,vsrc[2].dac+5);
        }
    }
}

void ScanForBatteries(void)
{
    //Main state machine that continously checks for the presence of new batteries
    //and queues them up to be charged.

    static ulong tic;
    float volts;
    ubyte new = 0;
    static ubyte old = 0xFF;

    static enum
    {
        START_SCAN,
        CHECK_BAY1,
        DELAY_BAY1,
        CHECK_BAY2,
        DELAY_BAY2,
        CHECK_BAY3,
        DELAY_BAY3,
        CHECK_BAY4,
        DELAY_BAY4,
        CHECK_IF_NEW,
        WAIT_5SEC_FOR_NEW
    }scan_state = START_SCAN;

    //Slowly cycle through each battery bay to detect presence of newly
    //inserted batteries. Solo batteries will "wake up" after a few seconds
    //if a small voltage is applied to its terminals. To detect a battery,
    //the PFET for that bay is turned on and a low voltage is applied. If
    //a higher voltage is observed within 5.5 seconds, a battery must be
    //present.
    //
    //Given the choice between more than one battery to charge, the charger
    //will choose the one(s) with the highest voltage. However, once it commits
    //to charging a battery, it will not stop charging even if a high-voltage
    //battery is later inserted.
    switch(scan_state)
    {
        case START_SCAN:
            if ( NumBaysSetToCharge() >= 2 )
            {
                //If two batteries are already charging, don't check
                //for more batteries yet.
                old = 0xFF;
                break;
            }
            scan_state = CHECK_BAY1;
            break;

        case CHECK_BAY1:
            /* Check Battery Bay 1 */
            if ( bay[1].setToCharge )
            {
                //Don't bother checking bay if it's already charging a battery
                scan_state = CHECK_BAY2;
                break;
            } 

            //Turn on PFET to Bay 1
            BayControl(1,BAY_ON);

            //5.5 seconds from now
            tic = clock + 110;
            scan_state = DELAY_BAY1;
            break;

        case DELAY_BAY1:
            UpdateBayData(1);
            if ( bay[1].occupied )
            {  
                //Turn off PFET to Bay 1
                BayControl(1,BAY_OFF);
                scan_state = CHECK_BAY2;
                break;
            }

            if ( clock >= tic )
            {
                //Give up and move on to bay 2
                BayControl(1,BAY_OFF);
                scan_state = CHECK_BAY2;
            }
            break;

        case CHECK_BAY2:
            /* Check Battery Bay 2 */
            if ( bay[2].setToCharge )
            {
                //Don't bother checking bay if it's already charging a battery
                scan_state = CHECK_BAY3;
                break;
            }

            //Turn on PFET to Bay 2
            BayControl(2,BAY_ON);

            //5.5 seconds from now
            tic = clock + 110;
            scan_state = DELAY_BAY2;
            break;

        case DELAY_BAY2:
            UpdateBayData(2);
            if ( bay[2].occupied )
            {
                //Turn off PFET to Bay 2
                BayControl(2,BAY_OFF);
                scan_state = CHECK_BAY3;
                break;
            }

            if ( clock >= tic )
            {
                //Give up and move on to bay 3
                BayControl(2,BAY_OFF);
                scan_state = CHECK_BAY3;
            }
            break;

        case CHECK_BAY3:
            /* Check Battery Bay 3 */
            if ( bay[3].setToCharge )
            {
                //Don't bother checking bay if it's already charging a battery
                scan_state = CHECK_BAY4;
                break;
            }

            //Turn on PFET to Bay 3
            BayControl(3,BAY_ON);

            //5.5 seconds from now
            tic = clock + 110;
            scan_state = DELAY_BAY3;
            break;

        case DELAY_BAY3:
            UpdateBayData(3);
            if ( bay[3].occupied )
            {
                //Turn off PFET to Bay 3
                BayControl(3,BAY_OFF);
                scan_state = CHECK_BAY4;
                break;
            }

            if ( clock >= tic )
            {
                //Give up and move on to bay 4
                BayControl(3,BAY_OFF);
                scan_state = CHECK_BAY4;
            }
            break;

        case CHECK_BAY4:
            /* Check Battery Bay 4 */
            if ( bay[4].setToCharge )
            {
                //Don't bother checking bay if it's already charging a battery
                scan_state = CHECK_IF_NEW;
                break;
            }

            //Turn on PFET to Bay 4
            BayControl(4,BAY_ON);

            //5.5 seconds from now
            tic = clock + 110;
            scan_state = DELAY_BAY4;
            break;

        case DELAY_BAY4:
            UpdateBayData(4);
            if ( bay[4].occupied )
            {
                //Turn off PFET to Bay 3
                BayControl(4,BAY_OFF);
                scan_state = CHECK_IF_NEW;
                break;
            }

            if ( clock >= tic )
            {
                //Give up and move on
                BayControl(4,BAY_OFF);
                scan_state = CHECK_IF_NEW;
            }
            break;

        case CHECK_IF_NEW:
            //If one battery has been charging and more show up, we want to start
            //charging the highest-voltage new battery first. So first check
            //if any new batteries have showed up.
            new = 0;
            for(ubyte b=1;b<=4;b++)
            {
                if ( !bay[b].setToCharge )
                {
                    if ( bay[b].occupied )
                    {
                        new |= (1<<b);
                    }
                }
            }

            if ( new == 0 )
            {
                //No batteries detected
                scan_state = START_SCAN;
                break;
            }

            if ( new != old )
            {
                //New batteries detected, which probably means user is actively
                //inserting new batteries. Wait 5 seconds and start over to give
                //user chance to finish placing all batteries in bays.
                old = new;
                tic = clock + 100;
                scan_state = WAIT_5SEC_FOR_NEW;
                break;
            }
            else
            {
                //User hasn't put any new batteries in for 5 seconds,
                //so decide which ones gets charge.
                SetToCharge(NextToCharge());

                //We can only charge two batteries at a time, so set up another
                //battery to charge only if exactly one other battery is already
                //set to charge.
                if ( (NumBaysSetToCharge() == 1) && (NumBaysOccupied() > 1) )
                {
                    SetToCharge(NextToCharge());
                }
                old = 0xFF;
                scan_state = START_SCAN;
                break;
            }

        case WAIT_5SEC_FOR_NEW:
            if ( clock >= tic )
            {
                scan_state = START_SCAN;
            }
            break;
    }
}

void ChargeControl(void)
{
    //Active charging control of the Solo batteries

    float amps;
    float volts;

    //Loop through the four charging bays
    for(ubyte b=1;b<=4;b++)
    {
        if ( bay[b].setToCharge )
        {
            //Activate the bay if it is not already activated
            if ( !bay[b].charging )
            {
                //Turn on PFET for bay b
                BayControl(b,BAY_ON);

                //Give the voltage a quick boost up to 15V 
                if ( vsrc[1].servicing == b )
                {
                    SetDAC(1,DAC_15V);
                }
                else if ( vsrc[2].servicing == b )
                {
                    SetDAC(2,DAC_15V);
                }

                //Keep incrementing voltage in loop until a small current is observed
                do
                {
                    IncrementVolts(b);
                    __delay_ms(100);
                    amps = (float)GetADC(batt_curr_chnl[b])*ADC2AMPS;
                    volts = (float)GetADC(batt_v_chnl[b])*ADC2VOLTS;
                }while( (amps < (AMPS_THRESH*2.0)) && (volts < MAX_VOLTS) );
                __delay_ms(200);
            }

            //Update the charging data of the battery bay
            UpdateBayData(b);

            if ( !bay[b].charging )
            {
                //If the battery has been removed or is fully charged (both conditions look the same)
                //then turn off PFET for bay b
                BayControl(b,BAY_OFF);
            }
            else
            {

                //Regulate charge
                if ( (bay[b].measured_volts < MAX_VOLTS) && (bay[b].measured_amps < MAX_AMPS) )
                {
                    IncrementVolts(b);
                }
                else if ( (bay[b].measured_volts > MAX_VOLTS) )
                {
                    DecrementVolts(b);
                }
                else if ( (bay[b].measured_amps > MAX_AMPS) )
                {
                    DecrementVolts(b);
                }
            }
        }
    }
}

void ChargeControlHC(void)
{
    //Active charging control of the hand controller. Since there is only one 
    //hand controller charging port with its own dedicated voltage source, all
    //of the charging code is encapsulated in this one function.

    float amps_hc = (float)GetADC(CONTROLLER_CURR_SENSE_CHNL)*ADC2AMPS_HC;
    float volts_hc = (float)GetADC(CONTROLLER_V_SENSE_CHNL)*ADC2VOLTS_HC;
    static ubyte reset_timer = 0;
    static ubyte overcurrent_count = 0;

    if ( reset_timer > 0 )
    {
        //If an overcurrent reset occured, keep output off for 10 seconds
        reset_timer--;

        if ( reset_timer == 0 )
        {
            //SetDAC(3,DAC_7V_HC);
            SetDAC(3,0x3FF);
            CONTROLLER_EN = TRUE;
        }
        return;
    }

    if ( volts_hc > (MAX_VOLTS_HC+1) )
    {
        //Over-voltage reset condition. Turn off output and set timer for 10 seconds
        CONTROLLER_EN = FALSE;
        reset_timer = 200;
        //SetDAC(3,DAC_7V_HC);
        SetDAC(3,0x3FF);
        return;
    }

    if ( volts_hc > (MAX_VOLTS_HC+1) )
    {
        //Over-current reset condition. Turn off output and set timer for 10 seconds
        CONTROLLER_EN = FALSE;
        reset_timer = 200;
        //SetDAC(3,DAC_7V_HC);
        SetDAC(3,0x3FF);
        return ;
    }

    if ( amps_hc > (MAX_AMPS_HC+0.4) )
    {
        //Keep track of how many times this has happened
        overcurrent_count++;

        if ( overcurrent_count >= 3)
        {
            //Over-current reset condition. Turn off output and set timer for 10 seconds
            CONTROLLER_EN = FALSE;
            //SetDAC(3,DAC_7V_HC);
            SetDAC(3,0x3FF);
            reset_timer = 200;
            overcurrent_count = 0;
        }
        return;
    }
    else
    {
        overcurrent_count = 0;
    }
 
    if ( volts_hc > (MAX_VOLTS_HC+0.5) )
    {
        //If charge voltage is 500mV over max voltage, drop voltage significantly
        SetDAC(3,0x3FF);
        return;
    }

    if ( (amps_hc > (MAX_AMPS_HC+0.01)) || (volts_hc > (MAX_VOLTS_HC+0.03)) )
    {
        //If above max amps or volts by a small amount, reduce voltage by a small amount
        if ( vsrc[3].dac < 0x3F9 )
        {
            SetDAC(3,vsrc[3].dac+3);
        }
    }
    else if ( (amps_hc < (MAX_AMPS_HC-0.01)) && (volts_hc < (MAX_VOLTS_HC-0.05)) )
    {
        //If below max amps or volts by a small amount, increase voltage by a small amount
        if ( vsrc[3].dac > 6 )
        {
            SetDAC(3,vsrc[3].dac-3);
        }
    }
}
    

void main(void)
{
    //FOSC: 16MHz INTSOC
    OSCCONbits.IRCF = 0b1111;

    //Initialize port pins
    InitPorts();

    //Tic timer
    InitTimer1();

    //Initialize ADC
    InitADC();

    //Initialize DACs 
    InitDACs();

    //Initialize source and bay structures
    InitStructs();

    //Enable hand controller and USB power supplies
    Enable_HC_and_USB();

    while(1)
    {
        ScanForBatteries();
        FaultCheck();
        ChargeControl();
        ChargeControlHC();

        while(!PIR1bits.TMR1IF);
        PIR1bits.TMR1IF = 0;
        clock++;
        //50ms tic timer
        TMR1H = 0x9E;
        TMR1L = 0x57;
    }
}
