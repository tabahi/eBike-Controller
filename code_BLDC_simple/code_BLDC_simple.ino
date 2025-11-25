/*
 * eBike Controller - Simple BLDC Motor Test Code
 * https://github.com/tabahi/eBike-Controller
 *
 * Purpose: Basic sanity checking and getting started code for BLDC motor control
 *
 * This code performs the following:
 * - Runs the BLDC motor at a slow speed (10/255 PWM) if everything works correctly
 * - Reports problems detected with Hall sensors or MOSFETs
 * - Automatically shuts down all FETs if any are blown or malfunctioning (prevents shoot-through)
 * - Tests at 12-40V input voltage range
 *
 * Configuration:
 * - Adjust `CRITITAL_LOW_VOLTS` and `VOLTS_ADC_FACTOR` for your voltage divider resistors
 *
 * Important Notes:
 * - Running without a motor connected will trigger multiple errors (this is expected)
 * - This is a diagnostic/testing tool, not the final firmware
 *
 * Target Hardware: ATmega328P (Arduino Nano SuperMini Type-C with CH340)
 */

// ============================================================================
// CONFIGURATION
// ============================================================================

// Current sensing limit (ADC value) - motor will be killed if exceeded
#define Isense_kill_limit 200

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
#define pin_Isense A5    // Current sensing (not used in this simple version)

#define pin_THOT_sense A3  // Throttle input (not used in this simple version)
#define pin_PAS_sense A4   // Pedal Assist Sensor (not used in this simple version)
#define pin_BUZZ 13        // Buzzer/LED output (not used in this simple version)
#define pin_HALT_IN 12     // Emergency halt signal input (not used in this simple version)

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
volatile uint8_t last_hall_state = 0;  // Previous hall state (1-6)
uint8_t thot = 0;                      // Throttle/speed value (0-255 PWM duty cycle)
uint16_t I_now = 0;                    // Current sensor reading (not used in simple version)

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

  // Test 1: Hall sensor check
  hall_test();

  // Test 2: MOSFET functionality check
  if (check_fets() == false) {
    kill_motor("FETs invalid");
    while (1) delay(1000);  // Halt execution if FETs are damaged
  }

  // Test 3: Verify valid hall state
  if (get_hall_state() == 0) {
    kill_motor("Hall invalid");
  }

  last_hall_state = get_hall_state();
}


// ============================================================================
// MAIN LOOP
// ============================================================================
volatile uint16_t loops_per_hall = 0;  // Performance metric (not actively used)

void loop() {
  // Set motor speed (PWM duty cycle)
  // Range: 0 (stopped) to 255 (full speed)
  thot = 10;  // Running at slow speed for testing

  motor_loop();  // Continuously update motor commutation
}














// ============================================================================
// HALL SENSOR READING AND DECODING
// ============================================================================
volatile uint8_t hall_state = 0;      // Current decoded hall state (1-6, or 0 if invalid)
volatile uint16_t count_zeros = 0;    // Counter for invalid hall states

/**
 * Read and decode hall sensor states into commutation step (1-6)
 *
 * Hall sensors provide 6 valid states as the motor rotates. Each state
 * corresponds to a specific rotor position and determines which motor
 * phases should be energized.
 *
 * Returns:
 *   1-6: Valid hall state corresponding to motor position
 *   0:   Invalid state (error condition or transition)
 */
uint8_t get_hall_state() {
  // Read all three hall sensors
  for (uint8_t i = 0; i < 3; i++) {
    HALL_states[i] = digitalRead(HALL_pins[i]);
  }

  // Decode the 3-bit hall sensor pattern into commutation step
  // Pattern: U V W -> State
  if ((HALL_states[U]) && (!HALL_states[V]) && (!HALL_states[W])) return 1;      // 1 0 0
  else if ((HALL_states[U]) && (HALL_states[V]) && (!HALL_states[W])) return 2;  // 1 1 0
  else if ((!HALL_states[U]) && (HALL_states[V]) && (!HALL_states[W])) return 3; // 0 1 0
  else if ((!HALL_states[U]) && (HALL_states[V]) && (HALL_states[W])) return 4;  // 0 1 1
  else if ((!HALL_states[U]) && (!HALL_states[V]) && (HALL_states[W])) return 5; // 0 0 1
  else if ((HALL_states[U]) && (!HALL_states[V]) && (HALL_states[W])) return 6;  // 1 0 1

  return 0;  // Invalid state
}





// ============================================================================
// MOTOR COMMUTATION CONTROL
// ============================================================================

/**
 * Main motor control loop - implements 6-step trapezoidal commutation
 *
 * This function reads hall sensors and energizes the appropriate motor phases
 * based on rotor position. The commutation follows this pattern:
 *
 * State | Hall(UVW) | High-side PWM | Low-side ON | Current Path
 * ------|-----------|---------------|-------------|-------------
 *   1   |    100    |      U        |      V      | U -> V
 *   2   |    110    |      U        |      W      | U -> W
 *   3   |    010    |      V        |      W      | V -> W
 *   4   |    011    |      V        |      U      | V -> U
 *   5   |    001    |      W        |      U      | W -> U
 *   6   |    101    |      W        |      V      | W -> V
 */
void motor_loop(void) {
  hall_state = get_hall_state();

  // Reset motor on hall state transition to prevent glitches
  if (last_hall_state != hall_state) {
    motor_off_state();
    count_zeros = 0;
  }

  // Commutation logic based on hall sensor state
  switch (hall_state) {
    case 1:  // State 1: U high (PWM), V low
      L_states[V] = 1;
      H_states[W] = 0;
      H_states[U] = 1;
      break;

    case 2:  // State 2: U high (PWM), W low
      H_states[U] = 1;
      L_states[V] = 0;
      L_states[W] = 1;
      break;

    case 3:  // State 3: V high (PWM), W low
      L_states[W] = 1;
      H_states[U] = 0;
      H_states[V] = 1;
      break;

    case 4:  // State 4: V high (PWM), U low
      H_states[V] = 1;
      L_states[W] = 0;
      L_states[U] = 1;
      break;

    case 5:  // State 5: W high (PWM), U low
      L_states[U] = 1;
      H_states[V] = 0;
      H_states[W] = 1;
      break;

    case 6:  // State 6: W high (PWM), V low
      H_states[W] = 1;
      L_states[U] = 0;
      L_states[V] = 1;
      break;

    default:  // Invalid hall state
      count_zeros++;
      if (count_zeros > 100)
        kill_motor(2);  // Kill motor if stuck in invalid state
      else
        motor_off_state();
      break;
  }

  drive_io_update();         // Apply the state changes to hardware
  last_hall_state = hall_state;
}




/**
 * Update physical outputs based on desired MOSFET states
 *
 * This function applies the H_states and L_states arrays to the actual
 * hardware pins. It includes shoot-through protection by ensuring the
 * low-side MOSFET is off before turning on the high-side MOSFET.
 */
void drive_io_update() {
  for (uint8_t i = 0; i < 3; i++) {
    if (H_states[i] == 1) {
      // High-side is active: apply PWM speed control
      digitalWrite(LOW_SIDE_pins[i], LOW);  // Ensure low-side is OFF first (shoot-through protection)
      analogWrite(HIGH_SIDE_pins[i], thot); // PWM control on high-side
    }
    else {
      // High-side is inactive: control low-side
      digitalWrite(HIGH_SIDE_pins[i], LOW);
      digitalWrite(LOW_SIDE_pins[i], L_states[i]);
    }
  }
}

/**
 * Turn off all MOSFETs and reset state variables
 *
 * This is a safe state that prevents shoot-through and is used during
 * transitions or error conditions.
 */
void motor_off_state() {
  for (uint8_t i = 0; i < 3; i++) {
    digitalWrite(HIGH_SIDE_pins[i], LOW);
    digitalWrite(LOW_SIDE_pins[i], LOW);
    H_states[i] = 0;
    L_states[i] = 0;
  }
}

/**
 * Emergency motor shutdown with reason code
 *
 * Reason codes:
 *   0 = Failed startup checks (FETs or hall sensors)
 *   1 = Overcurrent protection triggered
 *   2 = Hall sensor invalid or disconnected
 *   3 = External halt signal
 *  10 = Normal operation (OK)
 */
void kill_motor(uint8_t reason) {
  motor_off_state();
  Serial.print("Motor killed: ");
  Serial.println(reason);
}