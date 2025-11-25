/*
 * dummy.ino - Test and Debug Functions
 *
 * This file contains debug/testing functions for:
 * - IC validation (testing individual MOSFET switching)
 * - Manual motor control for testing
 */

// ============================================================================
// DEBUG/TEST FUNCTIONS
// ============================================================================

/**
 * Run motor at full speed for testing
 * Sets throttle to max and enables motor
 */
void dummy_run()
{
  thot = 255;
  motor_enabled = true;
  motor_loop();
}

/**
 * Test individual MOSFET switching for IC validation
 *
 * This function cycles through MOSFETs one at a time to verify:
 * - Gate driver functionality
 * - MOSFET switching behavior
 * - Shoot-through protection
 *
 * Currently only tests phase U for safety
 */
void dummy_loop_for_ic_test()
{
  motor_off_state();
  for (uint8_t i=0;i<3;i++)
  {
    if (i==U)
    {
      Serial.println(i);
      // trun on HIGH side for 1ms
      digitalWrite(LOW_SIDE_pins[i], 0);
      delay(1);
      digitalWrite(HIGH_SIDE_pins[i], 1);
      delay(1);
      digitalWrite(HIGH_SIDE_pins[i], 0);


      delay(10);  // wait 10 ms


      // trun on LOW side for 1ms
      digitalWrite(HIGH_SIDE_pins[i], 0);
      delay(1);
      digitalWrite(LOW_SIDE_pins[i], 1);
      delay(1);
      digitalWrite(LOW_SIDE_pins[i], 0);
      motor_off_state();
    }
  }
  motor_off_state();
  
}