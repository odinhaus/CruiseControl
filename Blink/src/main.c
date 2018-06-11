/**
 * \file
 *
 * \brief Empty user application template
 *
 */

/**
 * \mainpage User Application template doxygen documentation
 *
 * \par Empty user application template
 *
 * Bare minimum empty user application template
 *
 * \par Content
 *
 * -# Include the ASF header files (through asf.h)
 * -# "Insert system clock initialization code here" comment
 * -# Minimal main function that starts with a call to board_init()
 * -# "Insert application code here" comment
 *
 */

/*
 * Include header files for all drivers that have been imported from
 * Atmel Software Framework (ASF).
 */
/*
 * Support and FAQ: visit <a href="http://www.atmel.com/design-support/">Atmel Support</a>
 */
#include <asf.h>
#include <avr/io.h>
#include <util/delay.h>
#include <stdio.h>

#define F_CPU 8000000


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
	unsigned long BaseTime; // very first raw clock value after power on device (ticks)
	unsigned long StartTime; // clock count at the time the user pressed the start button (ticks)
	unsigned long Ticks; // sum total of all raw clock counts after power on device - raw system uptime value (ticks)
	unsigned long DeltaTime; // the amount of real time passed after pressing the start button (microseconds 10-6)
	unsigned long DeltaTimeMS; // the amount of real time passed after pressing the start button (milliseconds 10-3)
	unsigned int LastCount; // the last recorded raw clock value
	step Touch[3];
};

void getClockTime(struct State *state);
void getUserInput(struct State *state);
void setOutputs(struct State *state);
void setStartTime(struct State * state);
void execute(struct State *state);
void initializeControlRegisters(void);
void initializeTapSequences(struct State *state);
void initializeState(struct State *state);
void resetTouchSteps(struct State *state);
step createTap(int offset);
step createTouch(int offset, int duration);

step sequences[3];


int main (void)
{
	struct State *state = (struct State*)malloc(sizeof(struct State));
	
	initializeControlRegisters();
	initializeTapSequences(state);
	initializeState(state);
	
	while(true)
	{
		getClockTime(state);
		getUserInput(state);
		execute(state);
		setOutputs(state);
	}
}

void initializeControlRegisters(void)
{
	// Configure the LED port PB
	DDRB |= (1<<DDB1);
	DDRB |= (1<<DDB2);
	DDRB |= (1<<DDB3);
	DDRD |= (1<<DDD0);
	//DDRD &= ~(1<<DDD0); // set as input
	DDRB &= ~(1<<DDB0); // set as input

	// enable 16-bit timer, no prescaling
	TCCR1B = 1;

	//PORTD = 0;
	//PORTD |= 1 << PIND0;
	PORTB |= 1 << PINB0;
}


void initializeState(struct State *state)
{
	state->BaseTime = TCNT1;
	state->DeltaTime = 0;
	state->DeltaTimeMS = 0;
	state->IsRunning = false;
	state->StartTime = 0;
	state->Ticks = state->BaseTime;
	state->LastCount = state->BaseTime;

	resetTouchSteps(state);
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
		double deltaUS = ((double)startDelta / ((double)F_CPU)) * 1000000L;
		state->DeltaTime = (long)deltaUS;
		state->DeltaTimeMS = (long)(deltaUS / 1000L);
	}
}

void setStartTime(struct State *state)
{
	state->StartTime = state->Ticks;
	state->DeltaTime = 0;
	state->DeltaTimeMS = 0;
}

void execute(struct State *state)
{
	int s = 0;
	step step;

	if (!(state->IsRunning))
	{
		if (state->IsPressed_StartButton)
		{
			// we need to start the sequence
			state->IsRunning = true;
			// we need to reset our sequence steps to the beginning
			resetTouchSteps(state);
			// reset our start timer
			setStartTime(state);
		}
	}
	
	if (state->IsRunning)
	{
		// we need to run the sequence
		// we need to check if the current step for each touch sensor is:
		// a) not yet ready to execute, and should continue to wait
		// b) should be active at the current time, and have its output turned on
		// c) just ended execution, and should move to the next step in its sequence

		// now loop over the items in our Touch array

		for(s = 0; s < 3; s++)
		{
			step = state->Touch[s];

			// first check to see if the current time has moved beyond the current step
			while(!state->IsComplete_Touch[s] && state->DeltaTimeMS > step->Offset + step->Duration)
			{
				// assign the current step to the child step, if a child exists
				step = step->Next;
				if (step->Next == NULL)
				{
					(*state).IsComplete_Touch[s] = true;
				}
				
			}

			if (!state->IsComplete_Touch[s]) // the sequence isn't finished yet
			{
				// lets see if this step is ready to execute
				if (step->Offset <= state->DeltaTimeMS && step->Offset + step->Duration >= state->DeltaTimeMS)
				{
					// lets check that the duration is greater than zero.  Zero duration steps are simply placeholders
					if (step->Duration > 0)
					{
						// ok, we need to execute this step by activating the touch signal
						state->IsActive_Touch[s] = true;
					}
				}
				else
				{
					state->IsActive_Touch[s] = false;
				}

				if (step->Offset + step->Duration < state->DeltaTimeMS && step->Next == NULL)
				{
					// the sequence is complete
					state->IsComplete_Touch[s] = true;
				}
			}
		}

		// now check to see if all sequences are finished
		bool isComplete = false;
		for(s = 0; s < 3; s++)
		{
			if (!state->IsComplete_Touch[s])
			{
				isComplete = false;
				break;
			}
		}

		if (isComplete)
		{
			// all the sequences are finished, so take us out of run mode
			state->IsRunning = false;
		}
	}
}

void setOutputs(struct State *state)
{
	if (state->IsRunning)
	{
		// toggle the red and green LED pins to show green and hide red
		PORTB &= ~(1<<PINB2);
		PORTB |= (1<<PINB1);
	}
	else
	{
		// toggle the red and green LED pins to show red and hide green
		PORTB |= (1<<PINB2);
		PORTB &= ~(1<<PINB1);
	}
}

void getUserInput(struct State *state)
{
	if ((PINB & 1) == 0)
	{
		state->IsPressed_StartButton = true;
	}
	else
	{
		state->IsPressed_StartButton = false;
	}
}

void resetTouchSteps(struct State *state)
{
	state->Touch[0] = sequences[0];
	state->Touch[1] = sequences[1];
	state->Touch[2] = sequences[2];
}


void initializeTapSequences(struct State *state)
{
	step current, tmp;
	int startOffset = 3455 + 300; 
	// sequence 0 is the start pedal, sequence 1 is the shift paddle and sequence 2 is the N02 button
	tmp = createTouch(0, 3455);
	current = tmp;// start immediately after the start button is pressed, remain pressed for 3.455 seconds
	sequences[0] = current;

	// now there is a gap of time to allow the needle to fall to start position

	// now there is a short delay before shifting to second gear, which is the first activation for sequence 1
	tmp = createTap(startOffset + 572); // shift to 2nd gear **NOTE** NO2 and Shift offsets are relative to start button as well
	current = tmp;
	sequences[1] = current;
	// shift to 3rd gear
	tmp = createTap(startOffset + 1622);
	current->Next = tmp;
	current = current->Next;
	// shift to 4th gear
	tmp = createTap(startOffset + 1728);
	current->Next = tmp;
	current = current->Next;
	// shift to 5th gear
	tmp = createTap(startOffset + 2308);
	current->Next = tmp;
	current = current->Next;
	// shift to 6th gear
	tmp = createTap(startOffset + 3358);
	current->Next = tmp;
	current = current->Next;
	// shift to 7th gear
	tmp = createTap(startOffset + 3812);
	current->Next = tmp;
	current = current->Next;
	
	tmp = createTap(startOffset + 1728); // hit N02 at the same time we shift to 4th
	current = tmp;
	sequences[2] = current;
}

step createTap(int offset)
{
	step new;
	new = (step)malloc(sizeof(struct Step));
	new->Offset = offset;
	new->Duration = 50;
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


