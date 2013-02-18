/*
 *
 *  Definitions:
 *  - button:            
 *      physical buttons that can set a PIN to HIGH or LOW. 
 *      Buttons can control 0 or more light channels and/or 0 or more switch channels.
 *
 *  - switch (channel):  
 *      digital output PIN, primarily used to control relais.
 *      Switches can be controller by attaching them to a button or directly using 
 *  - light (channel):   analog output PIN, primarily used to control dimmers
 */

// ----------------------------------------------------------------- //

#define STEP_TIME               20      // minimal ms per step, lower is faster level change
#define PULSE_TIME              250     // ms to consider button state change to be a pulse

// ----------------------------------------------------------------- //

#define NR_LIGHT_CHANNELS       12      // number of PWM output channels used for dimmers
#define NR_SWITCH_CHANNELS      10      // nr of digital output channels used for relais
#define NR_BUTTONS              10      // nr of digital input buttons

#define NR_CHANNELS_PER_BUTTON  8       // maximum number of channels that a single button can control
                                        // this is light- and switch channels combined

#define DIR_UP                  1       // Direction of a fade
#define DIR_DOWN                0            

#define MAX_LIGHT_VALUE         255     // the maximum value a PWM output can have
#define MAX_ANALOG_IN_VALUE     1023    // the maximum value of a analogue input

// ------------------------------------------------------------------------- //
// PIN CONFIGURATION
//
int lightPins[NR_LIGHT_CHANNELS] = {    // MEGA pins used for PWM output to control dimmers
  2,3,4,5,6,7,8,9,10,11,12,13
};

int switchPins[NR_SWITCH_CHANNELS] = {  // MEGA pins used for digital output to control relais
  30,31,32,33,34,35,36,37,38,39
};

int buttonPins[NR_BUTTONS] = {          // MEGA pins used for digital input
  40,41,42,43,44,45,46,47,48,49
};

// ------------------------------------------------------------------------- //
// Forward declerations
//
struct Button;
struct LightChannel;
struct SwitchChannel;

void handleInput( int id );
void processLightTarget( int id );
void processSwitchTarget( int id );
void queueSwitch( SwitchChannel *c );
void processSwitchQueue();
void processSwitchUp( SwitchChannel *c );
void processSwitchDown( SwitchChannel *c );

// ------------------------------------------------------------------------- //
// Data structures
//
struct LightChannel
{
  int pin;
  int light_value;
  int last_light_value;
  int idle_light_value;
  int dir;
  int target_light_value;
  int speed_factor;
  unsigned long last_value_change;
  unsigned long last_target_change; 
  Button *button;
  boolean has_button;
};

enum SWITCH_TYPE { 
  SWITCH_TYPE_PULSE,                    // switch on when button pulses is on and off when the button is off
  SWITCH_TYPE_TOGGLE,                   // switch on when button pulses and hold, switch off when button pulsed again
  SWITCH_TYPE_DELAYED_START,            // switch on after (start_delay) and remain on
  SWITCH_TYPE_DELAYED_STOP,             // switch on and hold and keep on for a certain time (duration)
  SWITCH_TYPE_DELAYED_START_STOP        // switch on after (start_delay) and hold and keep on for a certain time (duration)
};

struct SwitchChannel 
{
  int pin;
  int state;
  int target_state;
  enum SWITCH_TYPE switch_type;
  unsigned long last_state_change;
  unsigned long last_target_change;     // used to control queue timings
  int duration;
  int start_delay;
  Button *button;
  boolean has_button;
  boolean always_on;                    // when true, this switch is turned on whenever any lightchannel has a non-0 value
};

struct Button
{
  int pin;
  int prev_state;                      // used for debounce detection
  int last_state;                      // used for state detection
  unsigned long last_change;
  unsigned long start_time;
  unsigned long stop_time;
  LightChannel *l_channels[NR_CHANNELS_PER_BUTTON];
  int nr_l_channels;
  SwitchChannel *sw_channels[NR_CHANNELS_PER_BUTTON];
  int nr_sw_channels;
  boolean fading;  
};

// Maintain 1:n button => light channel relation 
//
int buttonLights[   NR_BUTTONS ][ NR_CHANNELS_PER_BUTTON ];

// Maintain 1:n button => switch channel relation 
//
int buttonSwitches[ NR_BUTTONS ][ NR_CHANNELS_PER_BUTTON ];

LightChannel   l_channels[  NR_LIGHT_CHANNELS  ];
SwitchChannel  sw_channels[ NR_SWITCH_CHANNELS ];
Button         buttons[     NR_BUTTONS         ];

// Queued SwitchChannels of the switch_type SWITCH_TYPE_DELAYED_STOP and 
// SWITCH_TYPE_DELAYED_START_STOP are placed in this array. The content 
// is processed and checked for any expired switches which should be turned off
//
// All queue'ing is done in the HIGH flank of a button
//
//
int queued_sw_channels_length = 0;
SwitchChannel *queued_sw_channels[NR_SWITCH_CHANNELS];

// -------------------------------------------------------- //

void setLightTargetValue( int channel, int value, int speedFactor )
{
  LightChannel *c = &l_channels[channel];
  
  if ( c->light_value != value )
  {
    if ( !( speedFactor >= 0 && speedFactor <= 10 ) )
    {
      speedFactor = 5;
    }
   
    c->last_light_value = c->target_light_value;
    c->target_light_value = value;
    c->speed_factor = speedFactor;
  }
}

// -------------------------------------------------------- //

void setLightIdleValue( int channel )
{
  LightChannel *c = &l_channels[channel];

  setLightTargetValue( channel, c->idle_light_value, 2 );  
}

// -------------------------------------------------------- //

void setSwitchTargetState( int channel, int state )
{
  SwitchChannel *c = &sw_channels[channel];
  
  if ( c->state != state )
  {   
    c->target_state = state;
  }
}

// -------------------------------------------------------- //

void setSwitchState( int channel, int state, int start_delay, int duration )
{
  SwitchChannel *c = &sw_channels[channel];
  
  if ( 0 != start_delay && 0 != duration )
  {
    c->switch_type = SWITCH_TYPE_DELAYED_START_STOP;
    c->start_delay = start_delay;
    c->duration    = duration;
  }
  else if ( 0 != duration )
  {
    c->switch_type = SWITCH_TYPE_DELAYED_STOP;    
    c->start_delay = 0;
    c->duration    = duration;    
  }
  else
  {
    c->switch_type = SWITCH_TYPE_PULSE;
    c->start_delay = 0;
    c->duration    = 0;
  }
  
  if ( state )
  {
    processSwitchUp( &sw_channels[ channel ] );
  }
  else
  {
    processSwitchDown( &sw_channels[ channel ] );    
  }  
}

// -------------------------------------------------------- //

int getSwitchTargetState( int channel ) 
{
  return sw_channels[channel].target_state;
}

// -------------------------------------------------------- //

int getLightTargetValue( int channel ) 
{
  return l_channels[channel].target_light_value;
}

// -------------------------------------------------------- //

int getSpeedFactor( int channel ) 
{
  return l_channels[channel].speed_factor;
}

// -------------------------------------------------------- //

void setupDimmer() 
{
  // Button mappings
  //
  // Index 0 specifies the number of channels for that button
  // Index 1 and next is the specific channel
  //
  // Button => light channels mapping
  // 
  buttonLights[ 0][0]  =  1;    // BTN - Gang
  buttonLights[ 0][1]  =  5;    // LC  - Plafond gang
   
  buttonLights[ 1][0]  =  1;    // BTN - Slaapkamer 1  
  buttonLights[ 1][1]  =  4; // badkamer    // LC  - Plafond slaapkamer 
  
  buttonLights[ 2][0]  =  1;    // BTN - Slaapkamer 2  
  buttonLights[ 2][1]  =  8;   // slaapkamer plafond // LC  - Slaapkamer bed

  buttonLights[ 3][0]  =  1;    // BTN - Badkamer 1
  buttonLights[ 3][1]  = 10;    // LC  - Plafond badkamer
  
  buttonLights[ 4][0]  =  0;    // BTN - Badkamer 2  
  
  buttonLights[ 5][0]  =  1;    // BTN - Toilet
  buttonLights[ 5][1]  = 11;    // LC  - Plafond toilet  
  
  buttonLights[ 6][0]  =  0;    // BTN - Bed 1
  buttonLights[ 6][1]  =  5;    // LC  - Plafond slaapkamer
  
  buttonLights[ 7][0]  =  0;    // BTN - Bed 2
  buttonLights[ 7][1]  =  9;    // LC  - Plafond slaapkamer  
  
  buttonLights[ 8][0]  =  0;    // BTN - Woonkamer 1
  buttonLights[ 8][1]  =  1;    // LC  - links raam
  buttonLights[ 8][2]  =  2;    // LC  - a/v
  buttonLights[ 8][3]  =  3;    // LC  - links a/v  
  buttonLights[ 8][4]  =  4;    // LC  - midden raam
  
  buttonLights[ 9][0]  =  0;    // BTN - Woonkamer 2 (keuken)
  buttonLights[ 9][1]  =  7;    // LC  - keuken 1
  buttonLights[ 9][2]  =  8;    // LC  - keuken 2
 
  // Button => switch channel mapping 
  //  
  buttonSwitches[ 0][0] = 0;    // BTN - Gang
  buttonSwitches[ 0][1] = 5;
  
  buttonSwitches[ 1][0] = 0;    // BTN - Slaapkamer 1
  
  buttonSwitches[ 2][0] = 0;    // BTN - Slaapkamer 2

  buttonSwitches[ 3][0] = 0;    // BTN - Badkamer 1
  
  buttonSwitches[ 4][0] = 0;    // BTN - Badkamer 2
  
  buttonSwitches[ 5][0] = 0;    // BTN - Toilet
  
  buttonSwitches[ 6][0] = 0;    // BTN - Bed 1
  
  buttonSwitches[ 7][0] = 0;    // BTN - Bed 2

  buttonSwitches[ 8][0] = 0;    // BTN - Woonkamer 1
  
  buttonSwitches[ 9][0] = 0;    // BTN - Woonkamer 2
  
  
  l_channels[ 5 ].idle_light_value = 60;

  // Switch channel config
  //
  sw_channels[ 0 ].switch_type = SWITCH_TYPE_DELAYED_STOP; // MV - 2
  sw_channels[ 0 ].duration    = 10;
  
  sw_channels[ 1 ].switch_type = SWITCH_TYPE_DELAYED_STOP; // MV - 3
  sw_channels[ 1 ].duration    = 60;
  
  sw_channels[ 2 ].switch_type = SWITCH_TYPE_TOGGLE;       // Unassigned  
  
  // 3..4 not present
  
  sw_channels[ 5 ].switch_type = SWITCH_TYPE_TOGGLE;       // Floor LED
  
  sw_channels[ 6 ].switch_type = SWITCH_TYPE_TOGGLE;       // Unassigned
  
  // 7..9 not present

    
  // LIGHT - Initialize the per-channel datastructures 
  //
  for ( int i = 0; i < NR_LIGHT_CHANNELS; i++ )
  {    
    LightChannel *c = &l_channels[i];

    c->pin = lightPins[i];    
    c->light_value = 0;
    c->target_light_value = 0;
    c->last_light_value = 0;    
    c->dir = DIR_UP;
    c->speed_factor = 2;
    
    c->last_value_change  = now;
    c->last_target_change = now;
   
    c->has_button = false;
   
    pinMode( c->pin, OUTPUT );
  }
  
  // SWITCH - Initialize the per-channel datastructures
  //
  for ( int i = 0; i < NR_SWITCH_CHANNELS; i++ )
  {    
    SwitchChannel *s = &sw_channels[i];

    s->pin = switchPins[i];    
    
    s->state        = 0;
    s->target_state = 0;
    
    s->last_state_change  = now;
    s->last_target_change = now;
   
    s->has_button = false;
    s->always_on = false;
   
    pinMode( s->pin, OUTPUT );
  }
  
  // Turn on floor led, and flip to always_on to make sure it will follow lights
  //
  sw_channels[ 5 ].target_state = 1;
  sw_channels[ 5 ].always_on = true;
  
  // BUTTON - Initialize buttons
  //
  for ( int i = 0; i < NR_BUTTONS; i++ )
  {
    Button *b = &buttons[i];
    
    b->pin = buttonPins[i];
    b->start_time = now;
    b->stop_time  = now;
    b->last_change = now;
    b->last_state = LOW;
    b->fading = false;

    // Attach the light channels    
    b->nr_l_channels = buttonLights[i][0];
    
    for ( int j = 0; j < b->nr_l_channels; j++ )
    {
      b->l_channels[j] = &l_channels[ buttonLights[i][j+1] ];
      b->l_channels[j]->button = b;
      b->l_channels[j]->has_button = true;
    }
    
    // Attach the switch channels
    b->nr_sw_channels = buttonSwitches[i][0];
    
    for ( int j = 0; j < b->nr_sw_channels; j++ )
    {
      b->sw_channels[j] = &sw_channels[ buttonSwitches[i][j+1] ];
      b->sw_channels[j]->button = b;
      b->sw_channels[j]->has_button = true;
    }
    
    pinMode( b->pin, INPUT );    
  }
 
  if ( DIMMER_SERIAL_DEBUGGING ) 
    Serial << "Dimmer setup done\n";  
}

// -------------------------------------------------------- //

void loopDimmer() 
{   
  // Process the possible changes per group
  //
  for ( int i = 0; i < NR_BUTTONS; i++ )
  {
    handleInput( i );    
  }
  
  // Check if there is any lightchannel on
  //
  boolean any_on = false;
  
  for ( int i = 0; i < NR_LIGHT_CHANNELS; i++ )
  {    
    LightChannel *c = &l_channels[i];
    
    if ( c->light_value > 0 )
    {
      any_on = true;
      break;
    }
  }
  
  for ( int i = 0; i < NR_SWITCH_CHANNELS; i++ )
  {
    SwitchChannel *s = &sw_channels[ i ];
      
    if ( s->always_on )
    {
      s->target_state = any_on ? 1 : 0;
    }
  }

  // Process the queue of switches
  //
  processSwitchQueue();
  
  // Process all set targets
  //
  for ( int i = 0; i < NR_LIGHT_CHANNELS; i++ )
  { 
    processLightTarget( i );
  }
  
  for ( int i = 0; i < NR_SWITCH_CHANNELS; i++ )
  { 
    processSwitchTarget( i );
  }  
}

// -------------------------------------------------------- //


void handleInput( int id ) 
{
  Button *b = &buttons[id];
  
  // Stop if there are no light or switchchannels attached to this button
  //
  if ( 0 == b->nr_l_channels && 0 == b->nr_sw_channels ) { return; }

  int btnState = digitalRead( b->pin );  
  
  // Only process buttonstates that are the same for two loops, debouncing
  //
  if ( btnState != b->prev_state )
  {
    b->prev_state = btnState;
    return;
  }

  unsigned long interval = now - b->last_change;

  // Only process input every STEP_TIME
  //  
  if ( STEP_TIME > interval ) { return; } 

  // Save current light channel targets for change detection
  //
  int targets[b->nr_l_channels];
  for ( int i = 0; i < b->nr_l_channels; i++ )
  {
    targets[i] = b->l_channels[i]->target_light_value;
  }
  
  // Is the button pressed now while it wasn't the last time I checked? (same for released)
  //
  if ( b->last_state != btnState )
  {
     b->last_state = btnState;
    
    // It is a button state change, but was it short enough to be a pulse?
    //  
    if ( HIGH == btnState )
    {
      // Set the time the HIGH state was started for pulse detection
      //
      b->start_time = now;
      
      boolean pulse = ( PULSE_TIME > ( now - b->stop_time ));
      
      // Stop fading when not pulsed
      //
      if ( b->fading && !pulse )
      {
        b->fading = false;
      }      

      if ( DIMMER_SERIAL_DEBUGGING ) 
        Serial << "Pulse: " << ( pulse ? "true" : "false" ) << " stop_time: " << b->stop_time << "\n";      
      
      for ( int i = 0; i < b->nr_l_channels; i++ )
      {
        LightChannel *c = b->l_channels[i];       
        
        // Always return to previous value on UP flank
        //
  
        // change fade direction when in fade and pulsed
        //      
        if ( b->fading )
        {
          c->dir = pulse ? !c->dir : DIR_UP;          

          if ( DIMMER_SERIAL_DEBUGGING ) 
            Serial << "Continue fading into oposite direction\n";
        }
        else if ( 0 == c->light_value || MAX_LIGHT_VALUE == c->light_value )
        {
          if ( DIMMER_SERIAL_DEBUGGING ) 
            Serial << "Returning to last value, current value: [" << c->light_value << "] last light value: [" << c->last_light_value << "]\n";
          
          targets[i] = c->last_light_value;
        }      
        else
        {
          // Off on UP flank when on and not MAX
          //
          c->last_light_value = c->light_value;
          targets[i] = 0;
        }
      }      
      
      for ( int i = 0; i < b->nr_sw_channels; i++ )
      {
        processSwitchUp( b->sw_channels[i] );
      }      
    }   
    else // it is a change to btnState LOW
    {
      unsigned long prevStopTime = b->stop_time;
      
      // Set the time the LOW state was started for pulse detection
      //
      b->stop_time = now;
      
      boolean pulse       =       PULSE_TIME   > ( b->stop_time - b->start_time );      
      boolean doublePulse = ( 2 * PULSE_TIME ) > ( b->stop_time - prevStopTime  );
      
      if ( DIMMER_SERIAL_DEBUGGING ) 
        Serial << "Button to LOW, pulse: [" << ( pulse ? "true" : "false" ) << "] doublePulse: [" << ( doublePulse ? "true" : "false" ) << "]\n";
      
      // When double pulsed go to max value, and when already at max, go to 0
      //
      if ( doublePulse )
      {      
          for ( int i = 0; i < b->nr_l_channels; i++ )
          {
            int prevTarget =  targets[i];
            
            targets[i] = ( targets[i] == MAX_LIGHT_VALUE ) ? 0 : MAX_LIGHT_VALUE;
            
            if ( DIMMER_SERIAL_DEBUGGING ) 
              Serial << "DoublePulse stop_time: [" << b->stop_time << "] prevStopTime: [" << prevStopTime  <<"], going from: [" << prevTarget << "] to: [" << targets[i] << "]\n";            
          }
      }      
      
      // Set switches of type 'pulse' to off
      //
      for ( int i = 0; i < b->nr_sw_channels; i++ )
      {
        processSwitchDown( b->sw_channels[i] );
      }            
    }
  }
  // Enter or continue fading
  //
  else if ( HIGH == btnState && ( b->fading || PULSE_TIME > ( now - b->stop_time )))  
  { 
    // Start fading UP
    
    if ( !b->fading )
    {
      int dir = DIR_UP;  
      b->fading = true;
    }
    
    for ( int i = 0; i < b->nr_l_channels; i++ )
    {        
      LightChannel *c = b->l_channels[i];
    
      // Continue in the same direction as we already where going
      //
      if ( c->last_target_change < ( now - (2*STEP_TIME) ) )
      {
        targets[i] = c->dir ? targets[i] + 1 : targets[i] - 1;      

        // Flip direction when border reached
        //        
        if ( targets[ i ] < 0 )
        {
          c->dir = DIR_UP;
        }
        else if ( targets[ i ] > MAX_LIGHT_VALUE )
        {
          c->dir = DIR_DOWN;
        }
        
        if ( DIMMER_SERIAL_DEBUGGING > 1 ) 
          Serial << "Fading channel [" << i << "] into direction: [" << c->dir << "] new target: [" << targets[i] << "]\n";        
      }
    }
  } 
 
  // Process all changes made above to light value targets
  // 
  for ( int i = 0; i < b->nr_l_channels; i++ )
  {        
    LightChannel *c = b->l_channels[i];  

    targets[i] = constrain( targets[i], 0, MAX_LIGHT_VALUE );
    
    if ( c->target_light_value != targets[i] )
    {
      c->target_light_value = targets[i];
      c->speed_factor = 2;
      c->last_target_change = now;
    }    
  }
  
  b->prev_state = btnState;
}

// -------------------------------------------------------- //

void processSwitchUp( SwitchChannel *c ) 
{ 
  // Switch of type TOGGLE will change it's state when the button is pressed
  // while pulse switches just on (and will go off in the button release part)
  //
  switch ( c->switch_type )
  {        
    // Toggle state of the switch on every button press
    //
    case ( SWITCH_TYPE_TOGGLE ):
      c->target_state = !c->target_state;
      break;
      
    // Go to high on every button press
    //
    case ( SWITCH_TYPE_PULSE ):
      c->target_state = HIGH;
      break;
      
    // Go to high on every button press, but queue the switch to go off after 'duration' has passed
    //
    case ( SWITCH_TYPE_DELAYED_STOP ):
      c->target_state = HIGH;
      queueSwitch( c );
      break;
      
    // Queue switch for HIGH after start_delay has passed
    //
    case ( SWITCH_TYPE_DELAYED_START ):
      queueSwitch( c );
      break;      
      
    // Queue switch to go HIGH after start_delay has passed and to LOW after start_delay + duration has passed
    //
    case ( SWITCH_TYPE_DELAYED_START_STOP ):
      queueSwitch( c );
      break;
  }
}

// -------------------------------------------------------- //

void processSwitchDown( SwitchChannel *c )
{ 
  if ( SWITCH_TYPE_PULSE == c->switch_type )
  {
    c->target_state = LOW;
  }  
}

// -------------------------------------------------------- //

void queueSwitch( SwitchChannel *c )
{  
  boolean already_queued = false;
  
  if ( DIMMER_SERIAL_DEBUGGING )   
    Serial << "Queueing switch\n";
  
  // Check if already in the queue
  //
  for ( int i = 0; i < queued_sw_channels_length; i++ )  
  {
    if ( queued_sw_channels[i] == c )
    {
      already_queued = true;
      break;
    } 
  }
  
  // Add the switch channel to the queue if not queued already
  //
  if ( !already_queued )
  {
    queued_sw_channels[queued_sw_channels_length++] = c;
  }
  
  // Set (or reset in case of already queued) the last_target_change value to now
  // this is used when processing the queue.
  //
  c->last_target_change = now;  
  
  if ( DIMMER_SERIAL_DEBUGGING ) 
    Serial << "Queue length [" << queued_sw_channels_length << "]\n";  
}

// -------------------------------------------------------- //

void processSwitchQueue()
{ 
  // List of indexes of the queue to be removed
  //
  int remove_length = 0;
  int remove[queued_sw_channels_length];
  
  for ( int i = 0; i < queued_sw_channels_length; i++ )  
  {
    SwitchChannel *c = queued_sw_channels[i];       
    
    switch ( c->switch_type )
    {
      case ( SWITCH_TYPE_DELAYED_STOP ):
        if ( ( now - c->last_target_change ) > ( c->duration * 1000 ) )
        {
          c->target_state = LOW;
          
          if ( DIMMER_SERIAL_DEBUGGING ) 
            Serial << "Removing from queue\n";
          
          // remove from queue
          //
          remove[ remove_length++ ] = i;
        }
        break;      
        
      case ( SWITCH_TYPE_DELAYED_START ):
        if ( now - c->last_target_change > ( c->start_delay * 1000 ) )
        {
          c->target_state = HIGH;

          if ( DIMMER_SERIAL_DEBUGGING ) 
            Serial << "Start_delay passed, switching to HIGH\n";          
        }      
        break;        
        
      case ( SWITCH_TYPE_DELAYED_START_STOP ):
        if ( now - c->last_target_change > ( ( c->duration + c->start_delay ) * 1000  ))
        {
          c->target_state = LOW;
          
          if ( DIMMER_SERIAL_DEBUGGING ) 
            Serial << "Duration + start_delay passed, switching to LOW\n";

          // remove from queue
          //
          remove[ remove_length++ ] = i;          
        }      
        else if ( now - c->last_target_change > ( c->start_delay * 1000 ) )
        {
          c->target_state = HIGH;

          if ( DIMMER_SERIAL_DEBUGGING ) 
            Serial << "Start_delay passed, switching to HIGH\n";          
        }      
        break;
    }  
  }
  
  // Re-arrange queue if there are elements to be removed
  //
  if ( 0 < remove_length )
  {
    SwitchChannel *tmp[NR_SWITCH_CHANNELS];   
    
    if ( DIMMER_SERIAL_DEBUGGING ) 
      Serial << "Removing [" << remove_length << "] elements from queue\n";    
    
    int j = 0; // index of element in queue array
  
    for ( int i = 0; i < remove_length; i++ )
    {
      while ( remove[i] > j )
      {
        tmp[j - i] = queued_sw_channels[j];
        j++;
      }
    }
    
    queued_sw_channels_length -= remove_length;

    // copy over the tmp array to queue
    //
    for ( int i = 0; i < queued_sw_channels_length; i++ )
    {
      queued_sw_channels[i] = tmp[i];
    }
    
    if ( DIMMER_SERIAL_DEBUGGING ) 
      Serial << "Queue length [" << queued_sw_channels_length << "]\n";
  }
}

// -------------------------------------------------------- //

void processSwitchTarget( int id )
{
  SwitchChannel *c = &sw_channels[id];
   
  if ( c->state == c->target_state ) { return; }
  
  digitalWrite( c->pin, c->target_state );
  
  c->state = c->target_state;
  c->last_state_change = now;  
}

// -------------------------------------------------------- //

// Try to step towards the target value in case the current value 
// is different
//
void processLightTarget( int id )
{
  LightChannel *c = &l_channels[id];
  
  // ---------------------------------------------- //
  
//  unsigned long interval     = now - c->last_value_change;
//  unsigned long stopInterval = ( c->has_button ) ? now - c->button->stop_time : 0;
  
  if ( c->light_value == c->target_light_value  ) { return; }
    
  /*
  if ( c->light_value > c->target_light_value )
  {
    c->light_value--;
  } 
  else if ( c->light_value < c->target_light_value)
  {
    c->light_value++;
  }
  */

  // Immediately go to target value, the used dimmers are responding that 'slow' that there is no need to 'smooth' the fade in code
  //  
  c->light_value = c->target_light_value;

  // Should be taken care of when setting the target, but hey, ppl make mistakes
  //
  c->light_value = constrain( c->light_value, 0, MAX_LIGHT_VALUE );
  
  //Serial << "P " << c->pin << " V " << c->light_value << "\n";
  
  analogWrite( c->pin, c->light_value );
  
  // ---------------------------------------------- //

  c->last_value_change = now;
}

