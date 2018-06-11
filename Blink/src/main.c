#include <asf.h>
#include <avr/io.h>
#include <util/delay.h>
#include <stdio.h>
#include <main.h>

#define F_CPU 8000000


// various stored sequences
void initialize1sBlinkSequence(struct State *state);
void initializeHuracanPSSequence(struct State *state);


int main (void)
{
	struct State *state = initialize(0, 8000000);
	
	while(true)
	{
		run(state);
	}
}

// called from initialize
void initializeControlRegisters(void)
{
	// Configure the LED port PB
	DDRB |= (1<<DDB1);
	DDRB |= (1<<DDB2);
	DDRD |= (1<<DDD0); // touch 0
	DDRD |= (1<<DDD1); // touch 1
	DDRD |= (1<<DDD2); // touch 2
	//DDRD |= (1<<DDD3); // touch 3
	//DDRD &= ~(1<<DDD0); // set as input
	DDRB &= ~(1<<DDB0); // set as input

	// enable 16-bit timer, no prescaling
	TCCR1B = 1;

	//PORTD = 0;
	//PORTD |= 1 << PIND0;
	PORTB |= 1 << PINB0;
}
//called from initialize
void initializeTapSequences(struct State *state)
{
	initialize1sBlinkSequence(state);
	//initializeHuracanPSSequence(state);
}
// called from run
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
// called from run
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
// called from run
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


	// activate the output ports based on the IsActive_Touch state
	// touch 0
	if (state->IsActive_Touch[0])
	{
		PORTD |= (1<<PIND0);
	}
	else
	{
		PORTD &= ~(1<<PIND0);
	}

	// touch 1
	if (state->IsActive_Touch[1])
	{
		PORTD |= (1<<PIND1);
	}
	else
	{
		PORTD &= ~(1<<PIND1);
	}

	// touch 2
	if (state->IsActive_Touch[2])
	{
		PORTD |= (1<<PIND2);
	}
	else
	{
		PORTD &= ~(1<<PIND2);
	}
	 
	 // touch 3
	//if (state->IsActive_Touch[3])
	//{
		//PORTD |= (1<<PIND2);
	//}
	//else
	//{
		//PORTD &= ~(1<<PIND2);
	//}
}


// sequences

// tap Touch0 every 1 second for 10 seconds
void initialize1sBlinkSequence(struct State *state)
{
	step current, tmp;
	// sequence 0 is the start pedal, sequence 1 is the shift paddle and sequence 2 is the N02 button
	tmp = createTap(1000);
	current = tmp;// start immediately after the start button is pressed, remain pressed for 3.455 seconds
	state->TouchDefault[0] = current;

	tmp = createTap(2000);
	current->Next = tmp;
	current = current->Next;

	tmp = createTap(3000);
	current->Next = tmp;
	current = current->Next;

	tmp = createTap(4000);
	current->Next = tmp;
	current = current->Next;

	tmp = createTap(5000);
	current->Next = tmp;
	current = current->Next;

	tmp = createTap(6000);
	current->Next = tmp;
	current = current->Next;

	tmp = createTap(7000);
	current->Next = tmp;
	current = current->Next;

	tmp = createTap(8000);
	current->Next = tmp;
	current = current->Next;

	tmp = createTap(9000);
	current->Next = tmp;
	current = current->Next;

	tmp = createTap(10000);
	current->Next = tmp;
	current = current->Next;

}

// timing sequence for Lambo Huracan Performante Spyder
void initializeHuracanPSSequence(struct State *state)
{
	step current, tmp;
	int startOffset = 3455 + 300;
	// sequence 0 is the start pedal, sequence 1 is the shift paddle and sequence 2 is the N02 button
	tmp = createTouch(0, 3455);
	current = tmp;// start immediately after the start button is pressed, remain pressed for 3.455 seconds
	state->TouchDefault[0] = current;

	// now there is a gap of time to allow the needle to fall to start position

	// now there is a short delay before shifting to second gear, which is the first activation for sequence 1
	tmp = createTap(startOffset + 572); // shift to 2nd gear **NOTE** NO2 and Shift offsets are relative to start button as well
	current = tmp;
	state->TouchDefault[1] = current;
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
	state->TouchDefault[2] = current;
}


