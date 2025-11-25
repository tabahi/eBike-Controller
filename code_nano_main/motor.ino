/*
 * motor.ino - Motor Control and Commutation
 *
 * This file contains:
 * - 6-step trapezoidal commutation based on hall sensors
 * - MOSFET drive logic with shoot-through protection
 * - Motor state management (enable/disable/kill)
 */

// ============================================================================
// MOTOR CONTROL STATE VARIABLES
// ============================================================================
volatile uint8_t hall_state = 0;      // Current decoded hall state (1-6, or 0 if invalid)
volatile uint16_t count_zeros = 0;    // Counter for consecutive invalid hall states

// ============================================================================
// MOTOR COMMUTATION LOOP
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
 *
 * Note: Commented code includes optional current sensing feedback
 */
void motor_loop(void) {
  
  hall_state = get_hall_state();

  if (last_hall_state != hall_state) 
  {
    motor_off_state();
    count_zeros = 0;
    //offgap_routine();
    //if (last_hall_state<1) last_hall_state = 6;
  }
  //uint8_t feedback_i = last_hall_state-1;

  switch (hall_state) {
    case 1:  //U pwm, V neg
      //Ox_buff[feedback_i] = analogRead(Ox_pins[W]);
      //Isense_buff[feedback_i] = analogRead(pin_Isense);
      L_states[V] = 1;
      
      H_states[W] = 0;
      H_states[U] = 1;
      break;
    case 2:  //U pwm, W neg
      //Ox_buff[feedback_i] = analogRead(Ox_pins[U]);
      //Isense_buff[feedback_i] = analogRead(pin_Isense);
      H_states[U] = 1;
      
      L_states[V] = 0;
      L_states[W] = 1;
      break;
    case 3:  //V pwm, W neg
      //Ox_buff[feedback_i] = analogRead(Ox_pins[U]);
      //Isense_buff[feedback_i] = analogRead(pin_Isense);
      L_states[W] = 1;

      H_states[U] = 0;
      H_states[V] = 1;
      break;
    case 4:  //V pwm, U neg
      //Ox_buff[feedback_i] = analogRead(Ox_pins[V]);
      //Isense_buff[feedback_i] = analogRead(pin_Isense);
      H_states[V] = 1;

      L_states[W] = 0;
      L_states[U] = 1;
      break;
    case 5:  //W pwm, U neg
      //Ox_buff[feedback_i] = analogRead(Ox_pins[V]);
      //Isense_buff[feedback_i] = analogRead(pin_Isense);
      L_states[U] = 1;

      H_states[V] = 0;
      H_states[W] = 1;
      break;
    case 6:  //W pwm, V neg
      //Ox_buff[feedback_i] = analogRead(Ox_pins[W]);
      //Isense_buff[feedback_i] = analogRead(pin_Isense);
      H_states[W] = 1;

      L_states[U] = 0;
      L_states[V] = 1;
      break;
    default:
      count_zeros++;
      if (count_zeros > 100)
        kill_motor(2);
      else motor_off_state();
      break;
  }
  
  drive_io_update();
  /*
  if (Isense_buff[feedback_i] > Isense_max)
  {
    Isense_max = Isense_buff[feedback_i];
    if (Isense_max > Isense_kill_limit)
    {
      kill_motor(1);
      Serial.print("!I"); Serial.print(Isense_max); Serial.println(';');
      beep(1);
      beep(1);
      beep(1);
    }
    
  }*/
  last_hall_state = hall_state;
   
}




// ============================================================================
// MOSFET DRIVE FUNCTIONS
// ============================================================================

/**
 * Update physical outputs based on desired MOSFET states
 *
 * This function applies the H_states and L_states arrays to the actual
 * hardware pins. It includes shoot-through protection by ensuring the
 * low-side MOSFET is off before turning on the high-side MOSFET.
 */
void drive_io_update()
{
  for (uint8_t i = 0; i < 3; i++)
  {
    if (H_states[i] == 1)
    {
      // High-side is active: apply PWM speed control
      digitalWrite(LOW_SIDE_pins[i], LOW);  // Ensure low-side is OFF first (shoot-through protection)
      analogWrite(HIGH_SIDE_pins[i], thot); // PWM control on high-side
    }
    else
    {
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
  for (uint8_t i = 0; i < 3; i++)
  {
    digitalWrite(HIGH_SIDE_pins[i], LOW);
    digitalWrite(LOW_SIDE_pins[i], LOW);
    H_states[i] = 0;
    L_states[i] = 0;
  }
}

// ============================================================================
// MOTOR STATE MANAGEMENT
// ============================================================================

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
  motor_enabled = false;

  // Update kill switch state (preserve first error that occurred)
  if ((kill_switch_state == 10) || (kill_switch_state == 0))
  {
    kill_switch_state = reason;
  }
}

/**
 * Enable motor and reset kill switch state
 */
void enable_motor()
{
  motor_off_state();
  motor_enabled = true;
  kill_switch_state = 10;  // Set to OK state
}

