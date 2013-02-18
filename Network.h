void setIp()
{
  Ethernet.begin( mac, ip, gateway, netmask );   
  
  if ( NETWORK_SERIAL_DEBUGGING ) 
  {
    Serial << "IP Set: ["; 
    printArray( &Serial, ".", ip, 4, 10);
    Serial << "]\n"; 
  }
}

void setupNetwork()
{  
  setIp();
}

