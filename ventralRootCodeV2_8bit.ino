
/* Samples analog input on two ADC channels and 
   sends the data to a desktop computer via USB serial. */

// some global vars
volatile uint8_t  s0 = 0, s1 = 0, s = 0;
uint8_t result[3]; // final result to send over serial (8 bits)

// ADC interrupt routine - this happens when the ADC is finished taking a sample 
ISR (ADC_vect) { 
    s = ADCH;
    switch (ADMUX) {
        case 0x60: // sample from ADC pin 0
            s0 = s;
            ADMUX = 0x65; // switch it to pin 5
            break;
        case 0x65: // sample from ADC pin 5
            s1 = s;
            ADMUX = 0x60; // switch it to pin 0
            break;
        default:
            break;
    }
    ADCSRA |= 1 << ADSC; // start next conversion
}

int main(void) {
    // begin serial communication
    Serial.begin(8*115200); 
    
    // ADC set-up; ADC will read multiclamp output
    sei(); // enable global interrupts
    ADCSRA = 0x8A;
    ADMUX = 0x60;
    
    // first conversion
    ADCSRA |= 1 << ADSC; 
    
    // main loop
    while (1) {
        // pack data into a byte array, two leading flags
        result[0] = 0xFF; //b
        result[1] = s0; //b+1
        result[2] = s1; //b+2
        
        // send it
        Serial.write(result, 3);
    }

    return 0;
}

