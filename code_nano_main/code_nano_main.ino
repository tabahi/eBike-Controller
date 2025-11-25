/*
 * eBike Controller - Main Firmware
 * https://github.com/tabahi/eBike-Controller
 *
 * Full-featured BLDC motor controller for electric bikes with:
 * - Hall sensor-based 6-step trapezoidal commutation
 * - Throttle input processing with smooth acceleration
 * - Current sensing and overcurrent protection
 * - Voltage monitoring with battery level warnings
 * - MOSFET health checks and diagnostics
 * - Pedal Assist Sensor (PAS) support
 * - Audio feedback via buzzer
 * - Emergency halt functionality
 * - Idle power management
 *
 * Target Hardware: ATmega328P (Arduino Nano SuperMini Type-C with CH340)
 */

// Board: ATmega328P, Nano SuperMini Type-C development board with CH340

// ============================================================================
// CONFIGURATION
// ============================================================================

// Current sensing limit (ADC value) - motor will be killed if exceeded
#define Isense_kill_limit 200

// Idle timeout configuration
#define idle_itr_sleep 100  // Number of idle iterations before entering sleep mode

// ============================================================================
// PIN DEFINITIONS
// ============================================================================
// Note: Pin configuration may be reversed depending on your hardware layout

// MOSFET Gate Driver Pins (3-phase bridge)
#define pin_W_L 11       // W phase - Low side switch
#define pin_W_H 6        // W phase - High side switch (PWM capable)
#define pin_V_L 10       // V phase - Low side switch
#define pin_V_H 5        // V phase - High side switch (PWM capable)
#define pin_U_L 9        // U phase - Low side switch
#define pin_U_H 3        // U phase - High side switch (PWM capable)

// Hall Effect Sensor Input Pins
#define pin_W_HALL 8     // W phase hall sensor
#define pin_V_HALL 7     // V phase hall sensor
#define pin_U_HALL 4     // U phase hall sensor

// Analog Input Pins
#define pin_WOx A1       // W phase voltage sensing (power-side)
#define pin_VOx A2       // V phase voltage sensing (power-side)
#define pin_UOx A0       // U phase voltage sensing (power-side)
#define pin_Isense A5    // Current sensing

#define pin_THOT_sense A3  // Throttle input (analog potentiometer)
#define pin_PAS_sense A4   // Pedal Assist Sensor input
#define pin_BUZZ 13        // Buzzer/LED output
#define pin_HALT_IN 12     // Emergency halt signal input

// ============================================================================
// PHASE INDICES
// ============================================================================
#define U 0  // Phase U index
#define V 1  // Phase V index
#define W 2  // Phase W index

// ============================================================================
// GLOBAL ARRAYS AND STATE VARIABLES
// ============================================================================

// Pin arrays for easier iteration through all three phases
const uint8_t HALL_pins[3] = { pin_U_HALL, pin_V_HALL, pin_W_HALL };
const uint8_t HIGH_SIDE_pins[3] = { pin_U_H, pin_V_H, pin_W_H };
const uint8_t LOW_SIDE_pins[3] = { pin_U_L, pin_V_L, pin_W_L };
const uint8_t Ox_pins[3] = { pin_UOx, pin_VOx, pin_WOx };

// State tracking arrays
volatile uint8_t H_states[3] = { 0, 0, 0 };      // High-side MOSFET states (0=OFF, 1=ON)
volatile uint8_t L_states[3] = { 0, 0, 0 };      // Low-side MOSFET states (0=OFF, 1=ON)
volatile uint8_t HALL_states[3] = { 0, 0, 0 };   // Current hall sensor readings

// Motor control variables
bool startup_disable = true;              // Security feature: motor disabled until ignition sequence
volatile uint8_t last_hall_state = 0;     // Previous hall state (1-6)
uint8_t thot = 0;                         // Current throttle/speed value (0-255 PWM duty cycle)
uint16_t I_now = 0;                       // Current sensor reading (ADC value)
bool motor_enabled = false;               // Master motor enable flag
uint8_t kill_switch_state = 0;            // Current motor kill state (reason code)
volatile uint8_t sine_thot = 0;           // Sine wave throttle (reserved for future use)

// ============================================================================
// SETUP FUNCTION
// ============================================================================
void setup() {
  // Configure analog input pins
  pinMode(pin_Isense, INPUT);
  pinMode(pin_THOT_sense, INPUT);
  pinMode(pin_PAS_sense, INPUT);
  pinMode(pin_BUZZ, OUTPUT);
  pinMode(pin_HALT_IN, INPUT_PULLUP);
  digitalWrite(pin_BUZZ, LOW);

  // Configure pins for all three motor phases
  for (uint8_t i = 0; i < 3; i++) {
    pinMode(HALL_pins[i], INPUT_PULLUP);    // Hall sensors with pull-up
    pinMode(HIGH_SIDE_pins[i], OUTPUT);     // High-side MOSFET gates
    pinMode(LOW_SIDE_pins[i], OUTPUT);      // Low-side MOSFET gates
    pinMode(Ox_pins[i], INPUT);             // Voltage sensing inputs
  }
  motor_off_state();  // Ensure all MOSFETs are off at startup

  // -------------------------------------------------------------------------
  // Configure PWM frequencies for motor control
  // -------------------------------------------------------------------------
  // Pins D5 and D6 (Timer 0) - 7812.50 Hz PWM
  // Options tested:
  //   - 62500.00 Hz: Causes audible motor noise
  //   - 7812.50 Hz: Good balance (selected)
  //   - 980.39 Hz: Causes jitter
  TCCR0B = TCCR0B & B11111000 | B00000010;

  // Pin D3 (Timer 2) - 3921.16 Hz PWM
  // Options tested:
  //   - 31372.55 Hz: Too high
  //   - 3921.16 Hz: Good performance (selected)
  //   - 976.56 Hz (default): Causes jitter
  TCCR2B = TCCR2B & B11111000 | B00000010;

  // -------------------------------------------------------------------------
  // Initialize serial communication and run diagnostics
  // -------------------------------------------------------------------------
  Serial.begin(115200);
  Serial.println("Start");
  Serial.println("yo");

  motor_enabled = false;
  startup_disable = true;  // Security feature currently disabled for debugging

  // Run comprehensive system checks
  if (run_checks(true)) {
    /* Optional: Secret ignition sequence for security
     * This code disables the motor unless the throttle is pressed
     * within 0.5 seconds of the startup beep.
     *
     * Uncomment to enable:
     *
     * if (analogAvg(pin_THOT_sense) < 200) {
     *   beep(0);
     *   delay(500);
     *   if (analogAvg(pin_THOT_sense) > 800) {
     *     startup_disable = false;
     *     motor_enabled = true;
     *     beep(0);
     *     delay(1000);
     *   }
     * }
     * else beep(0);
     */
  }

  last_hall_state = get_hall_state();
}

// ============================================================================
// MAIN LOOP - TIMING AND STATE VARIABLES
// ============================================================================
unsigned long timer_offgap = 0;   // Timer for hall state transitions (not actively used)
unsigned long timer_thot = 0;     // Timer for throttle processing intervals
unsigned long timer_idle = 0;     // Timer for idle state management
uint16_t idle_itr = 0;            // Idle iteration counter

volatile uint16_t loops_per_hall = 0;  // Performance metric (not actively used)

// ============================================================================
// MAIN LOOP
// ============================================================================
void loop() {
  motor_enabled = true;  // Enable motor (bypasses startup security for testing)

  // Debug/testing functions (currently disabled)
  // dummy_loop_for_ic_test();
  // delay(1000);
  // dummy_run();
  // return;

  // -------------------------------------------------------------------------
  // Motor Disabled State - Run diagnostics and wait for enable
  // -------------------------------------------------------------------------
  if (!motor_enabled) {
    delay(100);

    if (run_checks(false)) {
      if (!startup_disable) {
        if (!motor_enabled) beep(0);  // Signal motor ready
        motor_enabled = true;
      }
      else {
        delay(300000);  // Long delay if startup security active (~5 minutes)
      }
      idle_itr = 0;
    }
    else {
      // Checks failed
      motor_off_state();
      motor_enabled = false;
      idle_itr++;

      if (idle_itr > idle_itr_sleep) {
        Serial.println("!Sleep");
        delay(200000);  // Deep sleep delay (~20 seconds)
      }
      delay(10000);
    }
  }
  // -------------------------------------------------------------------------
  // Motor Enabled State - Process throttle and run motor
  // -------------------------------------------------------------------------
  else {
    // Process throttle input every 100ms
    if (millis() - timer_thot > 100) {
      process_thot();
      timer_thot = millis();

      // Throttle is zero (idle state)
      if (thot == 0) {
        // Check if idle timeout reached (~1 second)
        if (timer_thot - timer_idle > 100000) {
          motor_off_state();
          timer_idle = timer_thot;
          idle_itr++;

          // Periodic health checks during idle
          if ((idle_itr % 5 == 0) && (idle_itr < idle_itr_sleep)) {
            if (!run_checks(true)) {
              kill_motor(0);  // Kill if checks fail
            }
          }
          // Enter deep sleep after prolonged idle
          else if (idle_itr >= idle_itr_sleep) {
            Serial.println("Sleep");
            delay(20000);
          }
          else {
            Serial.println("Idle");
          }
        }
      }
      // Throttle is active
      else {
        timer_idle = timer_thot;
        idle_itr = 0;
        // Optional: Debug output
        // Serial.print(thot); Serial.print('\t'); Serial.println(I_now);
      }
    }

    motor_loop();  // Continuously update motor commutation
  }
}

// ============================================================================
// ERROR REPORTING AND TELEMETRY
// ============================================================================

/**
 * Send error code to serial monitor
 */
void send_error(int error_code) {
  Serial.print("ERROR:");
  Serial.print(error_code);
  Serial.println(';');
}

/**
 * Send telemetry report (reserved for future implementation)
 */
void send_report() {
  // TODO: Implement telemetry reporting
}