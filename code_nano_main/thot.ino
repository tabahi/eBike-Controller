/*
 * thot.ino - Throttle Input Processing
 *
 * This file contains:
 * - Throttle ADC reading and filtering
 * - Linear mapping from ADC to PWM (0-255)
 * - Smooth acceleration ramping
 * - Throttle input validation
 */

// ============================================================================
// THROTTLE CONFIGURATION
// ============================================================================
#define THOT_ADC_MIN 180  // Minimum ADC value for throttle (zero position)
#define THOT_ADC_MAX 850  // Maximum ADC value for throttle (full position)

// ============================================================================
// THROTTLE STATE VARIABLES
// ============================================================================
int thot_adc = 0;                    // Filtered throttle ADC value (0-1023)
int I_adc = 0;                       // Filtered current ADC value (0-1023)
int thot_buff[10] = {0,0,0,0,0,0,0,0,0,0};  // Throttle moving average buffer
int I_buff[10] = {0,0,0,0,0,0,0,0,0,0};     // Current moving average buffer
uint8_t tbuff_i = 0;                 // Buffer index for circular buffer
uint8_t thot_instant = 0;            // Instantaneous throttle value (0-255)

// ============================================================================
// THROTTLE PROCESSING
// ============================================================================

/**
 * Read and process throttle input with filtering and ramping
 *
 * This function:
 * 1. Reads throttle and current ADC values
 * 2. Applies 10-sample moving average filter
 * 3. Maps throttle ADC (THOT_ADC_MIN to THOT_ADC_MAX) to PWM (0-255)
 * 4. Implements smooth acceleration ramp (increment by 1 per call)
 *
 * Note: Currently overrides throttle to max (255) for testing
 */
void process_thot()
{
  thot_buff[tbuff_i] = analogRead(pin_THOT_sense);//dummy_thot();
  I_buff[tbuff_i] = analogRead(pin_Isense);
  tbuff_i++; if (tbuff_i>=10) tbuff_i = 0;

  thot_adc = 0;
  I_adc = 0;
  for (uint8_t j = 0; j<10; j++)
  {
    thot_adc += thot_buff[j];
    I_adc += I_buff[j];
  }
  thot_adc /= 10;
  I_adc /= 10;
  I_now = I_adc;
  if (thot_adc < THOT_ADC_MIN) thot_instant = 0;
  else if (thot_adc > THOT_ADC_MAX) thot_instant = 255;
  else
  {
    thot_instant = ((float)(thot_adc-THOT_ADC_MIN)*255)/(THOT_ADC_MAX-THOT_ADC_MIN);
  }
  if (thot < thot_instant) thot++;
  else thot = thot_instant;
  
  thot = 255;
}

// ============================================================================
// THROTTLE VALIDATION
// ============================================================================

/**
 * Validate throttle sensor is connected and functioning
 *
 * Takes three readings with delays to ensure stable, valid throttle signal.
 * A reading below 100 indicates disconnected or shorted sensor.
 *
 * Returns:
 *   true:  Throttle sensor is connected and valid
 *   false: Throttle sensor disconnected, shorted, or malfunctioning
 */
bool check_thot()
{
  int read_thot1 = analogAvg(pin_THOT_sense);
  delay(10);
  int read_thot2 = analogAvg(pin_THOT_sense);
  delay(10);
  int read_thot3 = analogAvg(pin_THOT_sense);

  if ((read_thot1 < 100) || (read_thot2 < 100) || (read_thot3 < 100)) {
    return false;
  }
  return true;
}
