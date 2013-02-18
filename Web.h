#define PREFIX ""

boolean webSetup = false;

WebServer webserver(PREFIX, 80);

P(crossdomain) = 
    "<?xml version='1.0'?>"
    "<!DOCTYPE cross-domain-policy SYSTEM 'http://www.macromedia.com/xml/dtds/cross-domain-policy.dtd'>"
    "<cross-domain-policy>"
    "<allow-access-from domain='*' />"
    "</cross-domain-policy>";

P(index) = "DoDuino Web API";

void getAllLightsCmd(WebServer &server, WebServer::ConnectionType type, char *url_tail, bool tail_complete)
{
  server.httpSuccess( "text/xml" );
  
  server << 
  "<?xml version='1.0'?>"
  "<Channels>";
  
  for ( int i = 0; i <= NR_LIGHT_CHANNELS; ++i)
  {
    server << 
    "<Channel nr='" << i << "'>" <<  
    "<Value>" << getLightTargetValue( i ) << "</Value>" <<
    "<SpeedFactor>" << getSpeedFactor( i ) << "</SpeedFactor>" <<
    "</Channel>\n";
  }
  
  server << 
  "</Channels>";  
}

void getAllSwitchesCmd(WebServer &server, WebServer::ConnectionType type, char *url_tail, bool tail_complete)
{
  server.httpSuccess( "text/xml" );
  
  server << 
  "<?xml version='1.0'?>"
  "<Channels>";
  
  for ( int i = 0; i <= NR_SWITCH_CHANNELS; ++i)
  {
    server << 
    "<Channel nr='" << i << "'>" <<  
    "<State>" << getSwitchTargetState( i ) << "</State>" <<
    "</Channel>\n";
  }
  
  server << 
  "</Channels>";  
}

void setLightCmd( WebServer &server, WebServer::ConnectionType type, char *url_tail, bool tail_complete )
{
  if ( type != WebServer::GET )
  {
    server.httpFail();
  }  
  else
  {
    int part = 0;
    int part_len = 0;
    
    char *sl_loc;
    char *qm_loc;
    
    int channel = -1;
    int value = -1;
    int speedFactor = 0;
    
    char buf[32];
    
    boolean done = false;
    
    do
    {
      sl_loc = strchr( url_tail, '/' );
      
      if ( NULL == sl_loc )
      {
        qm_loc = strchr( url_tail, '?' );
        part_len = ( NULL == qm_loc ) ? strlen( url_tail ) : qm_loc - url_tail;
        done = true;
      }
      else
      {
        part_len = ( NULL == sl_loc ) ? strlen( url_tail ) : sl_loc - url_tail;
      }
      
      // make room for 0 char
      //
      if ( part_len > 32 ) 
      {
        part_len = 31;
      }

      strncpy( buf, url_tail, part_len ); 
      buf[part_len] = '\0'; 

      switch ( part ) 
      {
        case 0: channel     = atoi( buf ); break;
        case 1: value       = atoi( buf ); break;
        case 2: speedFactor = atoi( buf ); break;
      }
      
      url_tail = url_tail + part_len + 1;
      part++;
    } while ( false == done && 0 != strlen( url_tail ));
    
//    Serial << "C: " << channel << " V: " << value << " S: " << speedFactor << "\n";
    
    if ( 0 <= channel && 12  >= channel &&
         0 <= value   && 255 >= value )
    {
      setLightTargetValue( channel, value, speedFactor );
    }
  }
}

void setSwitchCmd( WebServer &server, WebServer::ConnectionType type, char *url_tail, bool tail_complete )
{
  if ( type != WebServer::GET )
  {
    server.httpFail();
  }  
  else
  {
    int part = 0;
    int part_len = 0;
    
    char *sl_loc;
    char *qm_loc;
    
    int channel     = -1;
    int state       = -1;
    int start_delay = -1;
    int duration    = -1;
    
    char buf[32];
    
    boolean done = false;
    
    do
    {
      sl_loc = strchr( url_tail, '/' );
      
      if ( NULL == sl_loc )
      {
        qm_loc = strchr( url_tail, '?' );
        part_len = ( NULL == qm_loc ) ? strlen( url_tail ) : qm_loc - url_tail;
        done = true;
      }
      else
      {
        part_len = ( NULL == sl_loc ) ? strlen( url_tail ) : sl_loc - url_tail;
      }
      
      // make room for 0 char
      //
      if ( part_len > 32 ) 
      {
        part_len = 31;
      }

      strncpy( buf, url_tail, part_len ); 
      buf[part_len] = '\0'; 

      switch ( part ) 
      {
        case 0: channel     = atoi( buf ); break;
        case 1: state       = atoi( buf ); break;
        case 2: start_delay = atoi( buf ); break;
        case 3: duration    = atoi( buf ); break;        
      }
      
      url_tail = url_tail + part_len + 1;
      part++;
    } while ( false == done && 0 != strlen( url_tail ));
    
    Serial << "C: " << channel << " S: " << state << "\n";
    
    if ( 0 <= channel && 9  >= channel &&
         0 <= state  && 1 >= state &&
         0 <= start_delay && 999 >= start_delay &&
         0 <= duration && 999 >= duration
    ) {
      setSwitchState( channel, state, start_delay, duration );
    }
  }
}


void defaultCmd(WebServer &server, WebServer::ConnectionType type, char *url_tail, bool tail_complete)
{   
    server.httpSuccess( "text/html", false );    
    server.printP(index);
    
    server << "\n";  
}


void crossdomainCmd(WebServer &server, WebServer::ConnectionType type, char *url_tail, bool tail_complete)
{      
    server.httpSuccess( "text/xml", false );    
    server.printP(crossdomain);
    
    server << "\n";
}

void setupWeb()
{
  if ( true == webSetup )
  {
    return;
  }
  
  webserver.begin();
  
  webserver.setDefaultCommand(&defaultCmd);

  webserver.addCommand("getLightChannels", &getAllLightsCmd);
  webserver.addCommand("getSwitchChannels", &getAllSwitchesCmd);
  
  webserver.addCommand("setLightChannel", &setLightCmd);
  webserver.addCommand("setSwitchChannel", &setSwitchCmd);
  
  webserver.addCommand( "crossdomain.xml", &crossdomainCmd );
  
  webSetup = true;
  
  if ( WEBDUINO_SERIAL_DEBUGGING ) Serial << "Web setup done\n";
}

void loopWeb()
{
  webserver.processConnection();
}


