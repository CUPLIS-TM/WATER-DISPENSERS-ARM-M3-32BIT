/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : state_machine.c
  * @brief          : State machine implementation for Water Dispenser Control
  * @author         : Cuplis Kei Darma
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "state_machine.h"
#include "sensors.h"

/* Private typedef -----------------------------------------------------------*/

/* Private define ------------------------------------------------------------*/

/* Private macro -------------------------------------------------------------*/

/* Private variables ---------------------------------------------------------*/
static StateMachine_t sm;  // State machine context

/* Private function prototypes -----------------------------------------------*/
static void EnterState(SystemState_t newState);
static uint8_t CheckSafetyConditions(void);
static void HandleIdleState(void);
static void HandleDoorOpenState(void);
static void HandleWaitSettleState(void);
static void HandleFillingState(void);
static void HandleFullState(void);
static void HandleErrorState(void);
static void HandleCooldownState(void);

/* Exported functions --------------------------------------------------------*/

/**
  * @brief  Initialize state machine
  * @param  None
  * @retval None
  */
void StateMachine_Init(void)
{
  // Initialize all variables
  sm.currentState = STATE_IDLE;
  sm.previousState = STATE_IDLE;
  sm.stateChangeTime = 0;
  sm.pumpStartTime = 0;
  sm.pumpStopTime = 0;
  sm.lastBlinkTime = 0;
  sm.ledBlinkState = 0;
  sm.errorCode = ERROR_NONE;

  // Initialize statistics
  sm.stats.totalPumpRunTime = 0;
  sm.stats.pumpCycleCount = 0;
  sm.stats.lastFillDuration = 0;
  sm.stats.errorCount = 0;
  sm.stats.lastErrorCode = ERROR_NONE;

  // Ensure pump is off
  PUMP_OFF();

  // Determine initial state based on inputs
  if(!Sensors_IsDoorClosed()) {
    EnterState(STATE_DOOR_OPEN);
  } else if(Sensors_IsTankFull()) {
    EnterState(STATE_FULL);
  } else {
    EnterState(STATE_IDLE);
  }
}

/**
  * @brief  Process state machine (call in main loop)
  * @param  None
  * @retval None
  */
void StateMachine_Process(void)
{
  switch(sm.currentState)
  {
    case STATE_IDLE:
      HandleIdleState();
      break;

    case STATE_DOOR_OPEN:
      HandleDoorOpenState();
      break;

    case STATE_WAIT_SETTLE:
      HandleWaitSettleState();
      break;

    case STATE_FILLING:
      HandleFillingState();
      break;

    case STATE_FULL:
      HandleFullState();
      break;

    case STATE_ERROR:
      HandleErrorState();
      break;

    case STATE_COOLDOWN:
      HandleCooldownState();
      break;

    default:
      EnterState(STATE_IDLE);
      break;
  }
}

/**
  * @brief  Update LED indicators based on current state
  * @param  None
  * @retval None
  */
void StateMachine_UpdateLEDs(void)
{
  uint32_t currentTime = HAL_GetTick();

  switch(sm.currentState)
  {
    case STATE_IDLE:
      // Program LED: ON solid, Water LED: OFF
      PROGRAM_LED_ON();
      STATUS_LED_OFF();
      break;

    case STATE_DOOR_OPEN:
      // Program LED: Fast blink (warning), Water LED: OFF
      if((currentTime - sm.lastBlinkTime) > LED_BLINK_FAST) {
        PROGRAM_LED_TOGGLE();
        sm.lastBlinkTime = currentTime;
      }
      STATUS_LED_OFF();
      break;

    case STATE_WAIT_SETTLE:
      // Program LED: ON solid, Water LED: Slow blink (preparing)
      PROGRAM_LED_ON();
      if((currentTime - sm.lastBlinkTime) > LED_BLINK_SLOW) {
        STATUS_LED_TOGGLE();
        sm.lastBlinkTime = currentTime;
      }
      break;

    case STATE_FILLING:
      // Program LED: ON solid, Water LED: Fast blink (actively filling)
      PROGRAM_LED_ON();
      if((currentTime - sm.lastBlinkTime) > LED_BLINK_FAST) {
        STATUS_LED_TOGGLE();
        sm.lastBlinkTime = currentTime;
      }
      break;

    case STATE_FULL:
      // Both LEDs: ON solid (tank full and ready)
      PROGRAM_LED_ON();
      STATUS_LED_ON();
      break;

    case STATE_COOLDOWN:
      // Program LED: ON solid, Water LED: Slow blink (cooldown)
      PROGRAM_LED_ON();
      if((currentTime - sm.lastBlinkTime) > LED_BLINK_SLOW) {
        STATUS_LED_TOGGLE();
        sm.lastBlinkTime = currentTime;
      }
      break;

    case STATE_ERROR:
      // Both LEDs: Fast blink alternating (error indication)
      if((currentTime - sm.lastBlinkTime) > LED_BLINK_ERROR) {
        PROGRAM_LED_TOGGLE();
        STATUS_LED_TOGGLE();
        sm.lastBlinkTime = currentTime;
      }
      break;

    default:
      break;
  }
}

/**
  * @brief  Get current system state
  * @param  None
  * @retval SystemState_t Current state
  */
SystemState_t StateMachine_GetState(void)
{
  return sm.currentState;
}

/**
  * @brief  Get current error code
  * @param  None
  * @retval uint8_t Error code (0 = no error)
  */
uint8_t StateMachine_GetErrorCode(void)
{
  return sm.errorCode;
}

/**
  * @brief  Get system statistics
  * @param  None
  * @retval SystemStats_t* Pointer to statistics structure
  */
SystemStats_t* StateMachine_GetStats(void)
{
  return &sm.stats;
}

/**
  * @brief  Force reset error state
  * @param  None
  * @retval None
  */
void StateMachine_ResetError(void)
{
  sm.errorCode = ERROR_NONE;
  sm.stats.pumpCycleCount = 0;
  sm.stats.totalPumpRunTime = 0;
  EnterState(STATE_IDLE);
}

/**
  * @brief  Get state name string (for debugging)
  * @param  state State to get name for
  * @retval const char* State name string
  */
const char* StateMachine_GetStateName(SystemState_t state)
{
  switch(state)
  {
    case STATE_IDLE:        return "IDLE";
    case STATE_DOOR_OPEN:   return "DOOR_OPEN";
    case STATE_WAIT_SETTLE: return "WAIT_SETTLE";
    case STATE_FILLING:     return "FILLING";
    case STATE_FULL:        return "FULL";
    case STATE_ERROR:       return "ERROR";
    case STATE_COOLDOWN:    return "COOLDOWN";
    default:                return "UNKNOWN";
  }
}

/* Private functions ---------------------------------------------------------*/

/**
  * @brief  Enter new state with timestamp
  * @param  newState Target state
  * @retval None
  */
static void EnterState(SystemState_t newState)
{
  sm.previousState = sm.currentState;
  sm.currentState = newState;
  sm.stateChangeTime = HAL_GetTick();
  sm.ledBlinkState = 0;
  sm.lastBlinkTime = sm.stateChangeTime;
}

/**
  * @brief  Check safety conditions before pump operation
  * @retval 1 if safe, 0 if unsafe
  */
static uint8_t CheckSafetyConditions(void)
{
  uint32_t currentTime = HAL_GetTick();

  // Check if door is closed
  if(!Sensors_IsDoorClosed()) {
    return 0;
  }

  // Check minimum interval between pump cycles
  #if ENABLE_COOLDOWN_PERIOD
  if(sm.pumpStopTime > 0) {
    if((currentTime - sm.pumpStopTime) < MIN_PUMP_INTERVAL) {
      return 0;
    }
  }
  #endif

  // Check for rapid cycling (potential sensor fault)
  #if ENABLE_RAPID_CYCLE_CHECK
  if(sm.stats.pumpCycleCount > MAX_RAPID_CYCLES) {
    uint32_t avgCycleTime = sm.stats.totalPumpRunTime / sm.stats.pumpCycleCount;
    if(avgCycleTime < MIN_AVG_CYCLE_TIME) {
      sm.errorCode = ERROR_RAPID_CYCLING;
      return 0;
    }
  }
  #endif

  return 1;
}

/**
  * @brief  Handle IDLE state
  * @retval None
  */
static void HandleIdleState(void)
{
  PUMP_OFF();

  // Check if door opened
  if(!Sensors_IsDoorClosed()) {
    EnterState(STATE_DOOR_OPEN);
    return;
  }

  // Check if tank needs refilling
  if(Sensors_IsTankEmpty()) {
    if(CheckSafetyConditions()) {
      #if ENABLE_STARTUP_DELAY
      EnterState(STATE_WAIT_SETTLE);
      #else
      EnterState(STATE_FILLING);
      PUMP_ON();
      sm.pumpStartTime = HAL_GetTick();
      sm.stats.pumpCycleCount++;
      #endif
    }
  } else if(Sensors_IsTankFull()) {
    EnterState(STATE_FULL);
  }
}

/**
  * @brief  Handle DOOR_OPEN state
  * @retval None
  */
static void HandleDoorOpenState(void)
{
  PUMP_OFF();

  // Wait for door to close
  if(Sensors_IsDoorClosed()) {
    #if ENABLE_STARTUP_DELAY
    EnterState(STATE_WAIT_SETTLE);
    #else
    if(Sensors_IsTankFull()) {
      EnterState(STATE_FULL);
    } else {
      EnterState(STATE_IDLE);
    }
    #endif
  }
}

/**
  * @brief  Handle WAIT_SETTLE state
  * @retval None
  */
static void HandleWaitSettleState(void)
{
  uint32_t currentTime = HAL_GetTick();
  uint32_t timeInState = currentTime - sm.stateChangeTime;

  PUMP_OFF();

  // Door opened again
  if(!Sensors_IsDoorClosed()) {
    EnterState(STATE_DOOR_OPEN);
    return;
  }

  // Wait for settling time
  if(timeInState >= PUMP_STARTUP_DELAY) {
    if(Sensors_IsTankFull()) {
      EnterState(STATE_FULL);
    } else if(Sensors_IsTankEmpty()) {
      if(CheckSafetyConditions()) {
        EnterState(STATE_FILLING);
        PUMP_ON();
        sm.pumpStartTime = currentTime;
        sm.stats.pumpCycleCount++;
      } else {
        EnterState(STATE_ERROR);
      }
    } else {
      EnterState(STATE_IDLE);
    }
  }
}

/**
  * @brief  Handle FILLING state
  * @retval None
  */
static void HandleFillingState(void)
{
  uint32_t currentTime = HAL_GetTick();
  uint32_t pumpRunTime = currentTime - sm.pumpStartTime;

  // Safety check: overflow detected
  if(Sensors_IsOverflow()) {
    PUMP_OFF();
    sm.pumpStopTime = currentTime;
    sm.stats.totalPumpRunTime += pumpRunTime;
    sm.stats.lastFillDuration = pumpRunTime;
    sm.errorCode = ERROR_OVERFLOW;
    sm.stats.errorCount++;
    sm.stats.lastErrorCode = ERROR_OVERFLOW;
    EnterState(STATE_ERROR);
    return;
  }

  // Safety check: door opened during filling
  if(!Sensors_IsDoorClosed()) {
    PUMP_OFF();
    sm.pumpStopTime = currentTime;
    sm.stats.totalPumpRunTime += pumpRunTime;
    sm.stats.lastFillDuration = pumpRunTime;
    EnterState(STATE_DOOR_OPEN);
    return;
  }

  // Tank full - normal completion
  if(Sensors_IsTankFull()) {
    PUMP_OFF();
    sm.pumpStopTime = currentTime;
    sm.stats.totalPumpRunTime += pumpRunTime;
    sm.stats.lastFillDuration = pumpRunTime;
    EnterState(STATE_FULL);
    return;
  }

  #if ENABLE_TIMEOUT_SAFETY
  // Safety timeout - maximum run time exceeded
  if(pumpRunTime > PUMP_MAX_RUN_TIME) {
    PUMP_OFF();
    sm.pumpStopTime = currentTime;
    sm.stats.totalPumpRunTime += pumpRunTime;
    sm.stats.lastFillDuration = pumpRunTime;
    sm.errorCode = ERROR_PUMP_TIMEOUT;
    sm.stats.errorCount++;
    sm.stats.lastErrorCode = ERROR_PUMP_TIMEOUT;
    EnterState(STATE_ERROR);
    return;
  }

  // Auto-stop at normal fill time if sensor doesn't trigger
  // (backup safety in case sensor fails high)
  if(pumpRunTime > PUMP_NORMAL_FILL_TIME && !Sensors_IsTankFull()) {
    PUMP_OFF();
    sm.pumpStopTime = currentTime;
    sm.stats.totalPumpRunTime += pumpRunTime;
    sm.stats.lastFillDuration = pumpRunTime;
    sm.errorCode = ERROR_GALLON_EMPTY;
    sm.stats.errorCount++;
    sm.stats.lastErrorCode = ERROR_GALLON_EMPTY;
    EnterState(STATE_ERROR);
    return;
  }
  #endif
}

/**
  * @brief  Handle FULL state
  * @retval None
  */
static void HandleFullState(void)
{
  PUMP_OFF();

  // Door opened
  if(!Sensors_IsDoorClosed()) {
    EnterState(STATE_DOOR_OPEN);
    return;
  }

  // Tank no longer full (water consumed)
  if(Sensors_IsTankEmpty()) {
    #if ENABLE_COOLDOWN_PERIOD
    EnterState(STATE_COOLDOWN);
    #else
    EnterState(STATE_WAIT_SETTLE);
    #endif
  }
}

/**
  * @brief  Handle ERROR state
  * @retval None
  */
static void HandleErrorState(void)
{
  uint32_t currentTime = HAL_GetTick();
  uint32_t timeInState = currentTime - sm.stateChangeTime;

  PUMP_OFF();

  // Error can be cleared by opening door for specified time
  if(!Sensors_IsDoorClosed()) {
    if(timeInState > ERROR_RESET_DOOR_TIME) {
      StateMachine_ResetError();
      EnterState(STATE_DOOR_OPEN);
    }
  } else {
    // Reset timer when door closes
    sm.stateChangeTime = currentTime;
  }
}

/**
  * @brief  Handle COOLDOWN state
  * @retval None
  */
static void HandleCooldownState(void)
{
  uint32_t currentTime = HAL_GetTick();
  uint32_t timeInState = currentTime - sm.stateChangeTime;

  PUMP_OFF();

  // Door opened
  if(!Sensors_IsDoorClosed()) {
    EnterState(STATE_DOOR_OPEN);
    return;
  }

  // Wait minimum interval before allowing refill
  if(timeInState >= MIN_PUMP_INTERVAL) {
    if(Sensors_IsTankEmpty()) {
      EnterState(STATE_WAIT_SETTLE);
    } else if(Sensors_IsTankFull()) {
      EnterState(STATE_FULL);
    } else {
      EnterState(STATE_IDLE);
    }
  }
}
