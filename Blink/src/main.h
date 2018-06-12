/*
 * main.h
 *
 * Created: 6/11/2018 8:37:50 AM
 *  Author: odinh
 */ 

 struct Step
 {
	 int Offset;
	 int Duration;
	 struct Step *Next;
 };

 typedef struct Step *step; // define step as a pointer of data type struct Step

 struct State
 {
	 bool IsPressed_StartButton;
	 bool IsRunning;
	 bool IsActive_Touch[3];
	 bool IsComplete_Touch[3];
	 unsigned long ClockSpeed;
	 unsigned int ClockPrescaler;
	 unsigned long BaseTime; // very first raw clock value after power on device (ticks)
	 unsigned long StartTime; // clock count at the time the user pressed the start button (ticks)
	 unsigned long Ticks; // sum total of all raw clock counts after power on device - raw system uptime value (ticks)
	 unsigned long DeltaTime; // the amount of real time passed after pressing the start button (microseconds 10-6)
	 unsigned long DeltaTimeMS; // the amount of real time passed after pressing the start button (milliseconds 10-3)
	 unsigned int LastCount; // the last recorded raw clock value
	 step Touch[3];
	 step TouchDefault[3];
 };

 // prototypes
 void run(struct State *state);
 void getClockTime(struct State *state);
 void getUserInput(struct State *state);
 void setOutputs(struct State *state);
 void setStartTime(struct State * state);
 void execute(struct State *state);
 void initializeControlRegisters(void);
 void initializeTapSequences(struct State *state);
 void initializeState(struct State *state, unsigned int clockPrescaler, unsigned long clockSpeed);
 void resetTouchSteps(struct State *state);
 step createTap(int offset);
 step createTouch(int offset, int duration);
 struct State* initialize(unsigned int clockPrescaler, unsigned long clockSpeed);

 void run(struct State *state)
 {
	 getClockTime(state);
	 getUserInput(state);
	 execute(state);
	 setOutputs(state);
 }

 void initializeState(struct State *state, unsigned int clockPrescaler, unsigned long clockSpeed)
 {
	 state->BaseTime = TCNT1;
	 state->DeltaTime = 0;
	 state->DeltaTimeMS = 0;
	 state->IsRunning = false;
	 state->StartTime = 0;
	 state->Ticks = state->BaseTime;
	 state->LastCount = state->BaseTime;
	 state->ClockPrescaler = clockPrescaler;
	 state->ClockSpeed = clockSpeed;

	 resetTouchSteps(state);
 }

  struct State* initialize(unsigned int clockPrescaler, unsigned long clockSpeed)
  {
	  struct State *state = (struct State*)malloc(sizeof(struct State));
	  
	  initializeControlRegisters();
	  initializeTapSequences(state);
	  initializeState(state, clockPrescaler, clockSpeed);
	  return state;
  }

  void getClockTime(struct State *state)
  {
	  // TCNT1 is a 16-bit counter with a max value of 65,535 before rolling back to zero
	  // state.BaseTime tracks the very first value of TCNT1
	  // depending on clock prescaler and cpu speed, the value of TCNT1 needs to be converted into actual time
	  // and the roll-over from 65,536 back to 0 needs to be tracked manually
	  // state.LastCount tracks the last raw value of TCNT1 to be used to detect rollovers
	  // state.Ticks tracks the sum total of all TCNT1 counter events without rollovers

	  int count = TCNT1;
	  int delta = 0;

	  if (count < state->LastCount)
	  {
		  // the time rolled over, so we need to figure out how many ticks occured across the rollover
		  delta = 65535 - state->LastCount; // get the upper delta
		  delta += count;  // now add the lower delta after the wrap
	  }
	  else
	  {
		  // no rollover, just a simple subtraction
		  delta = count - state->LastCount;
	  }

	  // increment the Ticks by the delta - ticks is basically a measure of raw uptime
	  state->Ticks += delta;
	  state->LastCount = count;

	  if (state->IsRunning)
	  {
		  // now update DeltaTime based on delta, cpu speed and prescaling
		  // tick count / CPU speed => fractional time in seconds; multiply by 1,000,000 to convert to time in microseconds
		  // for 8 MHz processor, if 1000 ticks elapsed:  (1000 / 8000000) * 1,000,000 = 125 uS
		  double startDelta = state->Ticks - state->StartTime;
		  if (state->ClockPrescaler > 0)
		  {
			startDelta /= state->ClockPrescaler;
		  }
		  double deltaUS = ((double)startDelta / ((double)F_CPU)) * 1000000L;
		  state->DeltaTime = (long)deltaUS;
		  state->DeltaTimeMS = (long)(deltaUS / 1000L);
	  }
  }

 void resetTouchSteps(struct State *state)
 {
	 state->Touch[0] = state->TouchDefault[0];
	 state->Touch[1] = state->TouchDefault[1];
	 state->Touch[2] = state->TouchDefault[2];
	 state->IsActive_Touch[0] = false;
	 state->IsActive_Touch[1] = false;
	 state->IsActive_Touch[2] = false;
	 state->IsComplete_Touch[0] = false;
	 state->IsComplete_Touch[1] = false;
	 state->IsComplete_Touch[2] = false;
 }

 
 step createTap(int offset)
 {
	 step new;
	 new = (step)malloc(sizeof(struct Step));
	 new->Offset = offset;
	 new->Duration = 25;
	 new->Next = NULL;
	 return new;
 }

 step createTouch(int offset, int duration)
 {
	 step new;
	 new = (step)malloc(sizeof(struct Step));
	 new->Offset = offset;
	 new->Duration = duration;
	 new->Next = NULL;
	 return new;
 }

 void setStartTime(struct State *state)
 {
	 state->StartTime = state->Ticks;
	 state->DeltaTime = 0;
	 state->DeltaTimeMS = 0;
 }