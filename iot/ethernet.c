// Ethernet Example
// Jason Losh
//Name: Surabhi Chythanya Kumar
//-----------------------------------------------------------------------------
// Hardware Target
//-----------------------------------------------------------------------------

// Target Platform: EK-TM4C123GXL w/ ENC28J60
// Target uC:       TM4C123GH6PM
// System Clock:    40 MHz

// Hardware configuration:
// ENC28J60 Ethernet controller on SPI0
//   MOSI (SSI0Tx) on PA5
//   MISO (SSI0Rx) on PA4
//   SCLK (SSI0Clk) on PA2
//   ~CS (SW controlled) on PA3
//   WOL on PB3
//   INT on PC6

// Pinning for IoT projects with wireless modules:
// N24L01+ RF transceiver
//   MOSI (SSI0Tx) on PA5
//   MISO (SSI0Rx) on PA4
//   SCLK (SSI0Clk) on PA2
//   ~CS on PE0
//   INT on PB2
// Xbee module
//   DIN (UART1TX) on PC5
//   DOUT (UART1RX) on PC4

//-----------------------------------------------------------------------------
// Device includes, defines, and assembler directives
//-----------------------------------------------------------------------------

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "tm4c123gh6pm.h"
#include "eth0.h"
#include "gpio.h"
#include "spi0.h"
#include "uart0.h"
#include "wait.h"
#include "timer.h"

// Pins
#define RED_LED PORTF,1
#define BLUE_LED PORTF,2
#define GREEN_LED PORTF,3
#define PUSH_BUTTON PORTF,4

#define MAX_CHARS 80
#define MAX_FIELDS 6
#define delay4Cycles() __asm(" NOP\n NOP\n NOP\n NOP")

#define DHCPINIT 0
#define DHCPDISCOVER 1
#define DHCPOFFER 2
#define DHCPREQUEST 3
#define DHCPDECLINE 4
#define DHCPACK 5
#define DHCPNACK 6
#define DHCPRELEASE 7
//#define DHCPINFORM 8

//tcp states
#define CLOSED 0
#define LISTEN 1
#define SYN_SENT 2
#define SYN_RECEIVED 3
#define ESTABLISHED 4

#define FINWAIT_1 5
#define FINWAIT_2 6
//#define TIME_WAIT 7
//#define CLOSE_WAIT 8
//#define LAST_ACK 9

struct _shell1
{
    char str[MAX_CHARS+1];
    uint8_t position[MAX_FIELDS];
    uint8_t argCount;
    uint8_t argNo;
    uint8_t min_args;
    char strout[MAX_CHARS];
    uint8_t ip1,ip2,ip3,ip4;
    uint8_t gw1,gw2,gw3,gw4;
    uint8_t dns1,dns2,dns3,dns4;
    uint8_t sn1,sn2,sn3,sn4;

} shell1;



uint8_t state;
uint8_t tcpstate=CLOSED;

//-----------------------------------------------------------------------------
// Subroutines                
//-----------------------------------------------------------------------------
void sendDHCPmessage(uint8_t []);
void collectDHCPoffer(uint8_t []);
bool isDHCPOffer(uint8_t []);
void sendDHCPReq(uint8_t []);
bool isDHCPAck(uint8_t []);
void collectDHCPack(uint8_t []);
bool isDHCPNack(uint8_t []);
void sendDHCPRefresh(uint8_t []);
bool etherIsArpResponse(uint8_t []);
bool isTcpListen(uint8_t []);
void sendTCPSynAck(uint8_t []);
bool isTCPAck(uint8_t []);
bool isTCPTelnet(uint8_t []);
void  sendTCPAck(uint8_t []);
void  sendTelnetMsg(uint8_t []);
void sendTCPFin(uint8_t []);
void  sendTCPLastAck(uint8_t []);

extern uint8_t ipAddress[IP_ADD_LENGTH];
extern uint8_t ipSubnetMask[IP_ADD_LENGTH];
extern uint8_t ipGwAddress[IP_ADD_LENGTH];
bool tcp=false;
// Initialize Hardware
void initHw()
{
	// Configure HW to work with 16 MHz XTAL, PLL enabled, system clock of 40 MHz
    SYSCTL_RCC_R = SYSCTL_RCC_XTAL_16MHZ | SYSCTL_RCC_OSCSRC_MAIN | SYSCTL_RCC_USESYSDIV | (4 << SYSCTL_RCC_SYSDIV_S);

    // Enable clocks
    enablePort(PORTF);
    _delay_cycles(3);

    // Configure LED and pushbutton pins
    selectPinPushPullOutput(RED_LED);
    selectPinPushPullOutput(GREEN_LED);
    selectPinPushPullOutput(BLUE_LED);
    selectPinDigitalInput(PUSH_BUTTON);
}

void displayConnectionInfo()
{
    uint8_t i;
    char str[10];
    uint8_t mac[6];
    uint8_t ip[4];
    etherGetMacAddress(mac);
    putsUart0("HW: ");
    for (i = 0; i < 6; i++)
    {
        sprintf(str, "%02x", mac[i]);
        putsUart0(str);
        if (i < 6-1)
            putcUart0(':');
    }
    putcUart0('\n');
    etherGetIpAddress(ip);
    putsUart0("IP: ");
    for (i = 0; i < 4; i++)
    {
        sprintf(str, "%u", ip[i]);
        putsUart0(str);
        if (i < 4-1)
            putcUart0('.');
    }
    if (etherIsDhcpEnabled())
        putsUart0(" (dhcp)");
    else
        putsUart0(" (static)");
    putcUart0('\n');
    etherGetIpSubnetMask(ip);
    putsUart0("SN: ");
    for (i = 0; i < 4; i++)
    {
        sprintf(str, "%u", ip[i]);
        putsUart0(str);
        if (i < 4-1)
            putcUart0('.');
    }
    putcUart0('\n');
    etherGetIpGatewayAddress(ip);
    putsUart0("GW: ");
    for (i = 0; i < 4; i++)
    {
        sprintf(str, "%u", ip[i]);
        putsUart0(str);
        if (i < 4-1)
            putcUart0('.');
    }
    putcUart0('\n');
    if (etherIsLinkUp())
        putsUart0("Link is up\n");
    else
        putsUart0("Link is down\n");
}




char* getString(char str[],uint8_t MAX)
{

   int cnt;
   char c;
   for (cnt=0;cnt<MAX;cnt++)

   {
       if(cnt<MAX)
       {
       c=getcUart0();
       if(c==8||c==127)
       {
           if(cnt>0)
               cnt--;
           cnt--;
           continue;
       }
       if(c==10 ||c==13)
       {
           str[cnt]=0;
           break;
       }
       if(c>=' ')
       {
           str[cnt]=c;
       }
       }
       else
       {
           str[cnt]=0;
       break;
       }


 }
   return str;
}

char* parseString(char str[],uint8_t pos[],uint8_t maxField,uint8_t *argCnt)
{
     *argCnt=0;
    int i=0;
    int j=0;
    int len=0;
    for (i = 0; str[i] != '\0'; i++)
           {
               len++;
           }


    for ( i=0;i<=len;i++)
       {
           if((str[i]>=48 && str[i]<=57)||(str[i]>=65 && str[i]<=90)||( str[i]>=97 && str[i]<=122)/*||( str[i]==46)*/||(str[i]==45))
           {
               if(!((str[i-1]>=48 && str[i-1]<=57)||(str[i-1]>=65 && str[i-1]<=90)||( str[i-1]>=97 && str[i-1]<=122)||/*( str[i-1]==46)||*/(str[i-1]==45)))
               {

                   pos[j]=i;
                   j++;
                   (*argCnt)++;
                   if (*argCnt==maxField)
                   {
                       break;
                   }
                   if(str[i-1]==46)
                   {
                       str[i-1]=0;

                   }
               }
           }
            else if(!((str[i]>=48 && str[i]<=57)||(str[i]>=65 && str[i]<=90)||( str[i]>=97 && str[i]<=122)||/*( str[i]==46)||*/(str[i]==45)))
            {

                      str[i]=0;

            }


}

    shell1.min_args=shell1.argCount-1;
return str;
}

char* getArgString(char str[],uint8_t posi[],uint8_t argNum)
{

        return &str[posi[argNum]];

}

bool cmp(char str1[], char str2[])
{

    uint8_t i=0;
    while(str1[i]!='\0' || str2[i]!='\0')
    {
        if(str1[i]==str2[i] || str1[i]-str2[i]==32)
        {
            i++;
            continue;
        }
        else

            return false;

    }
    return true;


}


bool isCommand(char strCmd[],uint8_t minArgs)
{

           if(cmp(strCmd,getArgString(shell1.str,shell1.position,0)))
           {
               if(minArgs<shell1.argCount)
               {
                   return true;
               }
           }
   return false;
}

//reffered from geeksforgeeks
void reverse(char* s, uint8_t l)
{
    uint8_t i=0,j=l-1;
    char temp;
    for(i=0;i<j;i++)
    {
       temp=s[i];
       s[i]=s[j];
       s[j]=temp;
       j--;
    }
}
//reffered from geeksforgeeks
uint8_t itoa(uint16_t n, char s1[], uint8_t d)
{
    uint8_t i=0;
    while(n)
    {
        s1[i++]=n%10+'0';
        n=n/10;
    }
    while(i<d)
    {
        s1[i++]='0';
    }
    reverse(s1,i);
    s1[i]='\0';
    return i;
}


//referred from geeksforgeeks
int atoi(char* str)
{
    uint32_t res = 0; // Initialize result
    uint16_t i;
    // Iterate through all characters of input string and update result
    for (i = 0; str[i] != '\0'; ++i)
        res = res * 10 + str[i] - '0';

    // return result.
    return res;
}

void initEeprom()
  {
      SYSCTL_RCGCEEPROM_R = 1;
      _delay_cycles(3);
      while (EEPROM_EEDONE_R & EEPROM_EEDONE_WORKING);
  }

  void writeEeprom(uint16_t add, uint32_t data)
  {
      EEPROM_EEBLOCK_R = add >> 4;
      EEPROM_EEOFFSET_R = add & 0xF;
      EEPROM_EERDWR_R = data;
      while (EEPROM_EEDONE_R & EEPROM_EEDONE_WORKING);
  }

  uint32_t readEeprom(uint16_t add)
  {
      EEPROM_EEBLOCK_R = add >> 4;
      EEPROM_EEOFFSET_R = add & 0xF;
      return EEPROM_EERDWR_R;
  }
//-----------------------------------------------------------------------------
// Main
//-----------------------------------------------------------------------------

// Max packet is calculated as:
// Ether frame header (18) + Max MTU (1500) + CRC (4)
#define MAX_PACKET_SIZE 1522

void cb_discover()
{
    state=DHCPDISCOVER;
}
void cb_request()
{
    state=DHCPREQUEST;
}
void cb_renew()
{
    state=DHCPREQUEST;
    startPeriodicTimer(cb_request, 1);
}
void cb_rebind()
{
    stopTimer(cb_request);
    state=DHCPINIT;
    startPeriodicTimer(cb_discover, 15);

}
void cb_init()
{
    state=DHCPINIT;
}
int main(void)
{
    uint8_t* udpData;
    uint8_t data[MAX_PACKET_SIZE];


    // Init controller
    initHw();

    // Setup UART0
    initUart0();
    setUart0BaudRate(115200, 40e6);

    initEeprom();

    // Init ethernet interface (eth0)
    putsUart0("\nStarting eth0\n");
//    etherSetIpAddress(192, 168, 1, 199);
    etherSetMacAddress(2, 3, 4, 5, 6, 7);

    etherInit(ETHER_UNICAST | ETHER_BROADCAST | ETHER_HALFDUPLEX);
    
    etherDisableDhcpMode();
    
    etherSetIpSubnetMask(255, 255, 255, 0);
    etherSetIpGatewayAddress(192, 168, 1, 1);
    waitMicrosecond(100000);
    displayConnectionInfo();

    // Flash LED
    setPinValue(GREEN_LED, 1);
    waitMicrosecond(100000);
    setPinValue(GREEN_LED, 0);
    waitMicrosecond(100000);



    // Main Loop
    // RTOS and interrupts would greatly improve this code,
    // but the goal here is simplicity
    while (true)
    {
        shell1.argCount=0;
               shell1.argNo=0;
               shell1.min_args=0;

        // Put terminal processing here
        if (kbhitUart0())
        {
//            putsUart0("\r \n");
//            putsUart0("Enter any string\r\n");
//            putsUart0("\r \n");

            getString(shell1.str,MAX_CHARS);
            parseString(shell1.str,shell1.position,MAX_FIELDS, &shell1.argCount);

            if(isCommand("dhcp",1))
             {
                if( cmp("on",getArgString(shell1.str,shell1.position,1))) //Command: DHCP on
                  {
//                    putsUart0("dhcp oned");
                    etherEnableDhcpMode();

                    state=DHCPINIT;

                    putsUart0("\r \n");

                  }
                else if( cmp("off",getArgString(shell1.str,shell1.position,1))) //Command: DHCP off
                {
//                      putsUart0("dhcp offed");
                    etherDisableDhcpMode();
                      putsUart0("\r \n");

                }
                else if( cmp("renew",getArgString(shell1.str,shell1.position,1))) //Command: DHCP renew
                {
                    state=DHCPREQUEST;
                    putsUart0("\r \n");
                }
                else if( cmp("rebind",getArgString(shell1.str,shell1.position,1))) //Command: DHCP rebind
                {
                    state=DHCPINIT;
                    startPeriodicTimer(cb_discover, 15);
                    putsUart0("\r \n");
                }
                else if( cmp("refresh",getArgString(shell1.str,shell1.position,1))) //Command: DHCP refresh
                {
                    if(etherIsDhcpEnabled())
                    {
                        state=DHCPRELEASE;
                        sendDHCPRefresh(data);
                        putsUart0("\r \n");
                    }
                }

             }

            /*if(isCommand("dhcp",2))
            {
                if( cmp("dhcp",getArgString(shell1.str,shell1.position,1)))
                {

                if( cmp("refresh",getArgString(shell1.str,shell1.position,2))) //Command: DHCP on
                    {
                    if(etherIsDhcpEnabled())
                    {
                        state=DHCPRELEASE;
                        sendDHCPRefresh(data);
                        putsUart0("\r \n");
                    }

                    }
//               else if( cmp("RELEASE",getArgString(shell1.str,shell1.position,2))) //Command: DHCP off
//                 {
//                    putsUart0("dhcp released");
//                    putsUart0("\r \n");
//
//                 }
                }

            }*/

            if(isCommand("set",5))
            {
                if( cmp("IP",getArgString(shell1.str,shell1.position,1)))
                {
               /*shell1.ip1= atoi(getArgString(shell1.str,shell1.position,2));
               shell1.ip2= atoi(getArgString(shell1.str,shell1.position,3));
               shell1.ip3= atoi(getArgString(shell1.str,shell1.position,4));
               shell1.ip4= atoi(getArgString(shell1.str,shell1.position,5));*/

                    if(etherIsDhcpEnabled()==false)
                    {
                    ipAddress[0]=atoi(getArgString(shell1.str,shell1.position,2));
                    ipAddress[1]=atoi(getArgString(shell1.str,shell1.position,3));
                    ipAddress[2]=atoi(getArgString(shell1.str,shell1.position,4));
                    ipAddress[3]=atoi(getArgString(shell1.str,shell1.position,5));
                    putsUart0("\n \r");
                    etherSetIpAddress(ipAddress[0],ipAddress[1],ipAddress[2],ipAddress[3]);
                    tcp=true;
                    }
                    else
                    {
                        putsUart0("\n \r Cannot set IP as DHCP is on \n");
                    }


               /*itoa(shell1.ip1,shell1.strout,1);
               putsUart0(shell1.strout);
               putsUart0("\n \r");
               itoa(shell1.ip2,shell1.strout,1);
               putsUart0(shell1.strout);
               putsUart0("\n \r");
               itoa(shell1.ip3,shell1.strout,1);
               putsUart0(shell1.strout);
               putsUart0("\n \r");
               itoa(shell1.ip4,shell1.strout,1);
               putsUart0(shell1.strout);
               putsUart0("\n \r");*/
                }



                else  if( cmp("GW",getArgString(shell1.str,shell1.position,1)))
            {
                /*shell1.gw1= atoi(getArgString(shell1.str,shell1.position,2));
                shell1.gw2= atoi(getArgString(shell1.str,shell1.position,3));
                shell1.gw3= atoi(getArgString(shell1.str,shell1.position,4));
                shell1.gw4= atoi(getArgString(shell1.str,shell1.position,5));

                itoa(shell1.gw1,shell1.strout,1);
                putsUart0(shell1.strout);
                putsUart0("\n \r");
                itoa(shell1.gw2,shell1.strout,1);
                putsUart0(shell1.strout);
                putsUart0("\n \r");
                itoa(shell1.gw3,shell1.strout,1);
                putsUart0(shell1.strout);
                putsUart0("\n \r");
                itoa(shell1.gw4,shell1.strout,1);
                putsUart0(shell1.strout);
                putsUart0("\n \r");*/
                    if(etherIsDhcpEnabled()==false)
                    {
                        ipGwAddress[0]=atoi(getArgString(shell1.str,shell1.position,2));
                        ipGwAddress[1]=atoi(getArgString(shell1.str,shell1.position,3));
                        ipGwAddress[2]=atoi(getArgString(shell1.str,shell1.position,4));
                        ipGwAddress[3]=atoi(getArgString(shell1.str,shell1.position,5));
                        putsUart0("\n \r");
                    }
                    else
                    {
                        putsUart0("\n \r Cannot set GW as DHCP is on \n");
                    }
            }

                else  if( cmp("DNS",getArgString(shell1.str,shell1.position,1)))
             {
                shell1.dns1= atoi(getArgString(shell1.str,shell1.position,2));
                shell1.dns2= atoi(getArgString(shell1.str,shell1.position,3));
                shell1.dns3= atoi(getArgString(shell1.str,shell1.position,4));
                shell1.dns4= atoi(getArgString(shell1.str,shell1.position,5));
                putsUart0("\n \r");
                itoa(shell1.dns1,shell1.strout,1);
                putsUart0(shell1.strout);
                putsUart0("\n \r");
                itoa(shell1.dns2,shell1.strout,1);
                putsUart0(shell1.strout);
                putsUart0("\n \r");
                itoa(shell1.dns3,shell1.strout,1);
                putsUart0(shell1.strout);
                putsUart0("\n \r");
                itoa(shell1.dns4,shell1.strout,1);
                putsUart0(shell1.strout);
                putsUart0("\n \r");
             }

                else  if( cmp("SN",getArgString(shell1.str,shell1.position,1)))
              {
                /*shell1.sn1= atoi(getArgString(shell1.str,shell1.position,2));
                shell1.sn2= atoi(getArgString(shell1.str,shell1.position,3));
                shell1.sn3= atoi(getArgString(shell1.str,shell1.position,4));
                shell1.sn4= atoi(getArgString(shell1.str,shell1.position,5));

                itoa(shell1.sn1,shell1.strout,1);
                putsUart0(shell1.strout);
                putsUart0("\n \r");
                itoa(shell1.sn2,shell1.strout,1);
                putsUart0(shell1.strout);
                putsUart0("\n \r");
                itoa(shell1.sn3,shell1.strout,1);
                putsUart0(shell1.strout);
                putsUart0("\n \r");
                itoa(shell1.sn4,shell1.strout,1);
                putsUart0(shell1.strout);
                putsUart0("\n \r");*/
                    if(etherIsDhcpEnabled()==false)
                    {
                        ipSubnetMask[0]=atoi(getArgString(shell1.str,shell1.position,2));
                        ipSubnetMask[1]=atoi(getArgString(shell1.str,shell1.position,3));
                        ipSubnetMask[2]=atoi(getArgString(shell1.str,shell1.position,4));
                        ipSubnetMask[3]=atoi(getArgString(shell1.str,shell1.position,5));
                        putsUart0("\n \r");
                    }
                    else
                    {
                        putsUart0("\n \r Cannot set SN as DHCP is on \n");
                    }
              }
            }

        }

        if(etherIsDhcpEnabled())
        {

                if(state==DHCPINIT)
                {
                    initTimer();
                    startPeriodicTimer(cb_discover,15);
//                    state=DHCPDISCOVER;

                    sendDHCPmessage(data);
                    state=DHCPOFFER;
                }
                if(state==DHCPREQUEST)
                {
                    sendDHCPReq(data);
                    state=DHCPACK;
                }

        }
        // Packet processing
        if (etherIsDataAvailable())
        {
            if (etherIsOverflow())
            {
                setPinValue(RED_LED, 1);
                waitMicrosecond(100000);
                setPinValue(RED_LED, 0);
            }

            // Get packet
            etherGetPacket(data, MAX_PACKET_SIZE);

            // Handle ARP request
            if (etherIsArpRequest(data))
            {
                etherSendArpResponse(data);
            }

            // Handle IP datagram
            if (etherIsIp(data))
            {
            	if (etherIsIpUnicast(data))
            	{
            		// handle icmp ping request
					if (etherIsPingRequest(data))
					{
					  etherSendPingResponse(data);
					}

					// Process UDP datagram
					// test this with a udp send utility like sendip
					//   if sender IP (-is) is 192.168.1.198, this will attempt to
					//   send the udp datagram (-d) to 192.168.1.199, port 1024 (-ud)
					// sudo sendip -p ipv4 -is 192.168.1.198 -p udp -ud 1024 -d "on" 192.168.1.199
                    // sudo sendip -p ipv4 -is 192.168.1.198 -p udp -ud 1024 -d "off" 192.168.1.199
					if (etherIsUdp(data))
					{
						udpData = etherGetUdpData(data);
						if (strcmp((char*)udpData, "on") == 0)
			                setPinValue(GREEN_LED, 1);
                        if (strcmp((char*)udpData, "off") == 0)
			                setPinValue(GREEN_LED, 0);
						etherSendUdpResponse(data, (uint8_t*)"Received", 9);
					}
                }
            }
            if(etherIsDhcpEnabled())
            {
                if(isDHCPOffer(data))
                {

                    if(state==DHCPOFFER)
                    {
                        collectDHCPoffer(data);
                        state=DHCPREQUEST;
                        stopTimer(cb_discover);
                    }
                }
                 if(state==DHCPREQUEST)
                {
                    sendDHCPReq(data);
                    state=DHCPACK;
                }
                 if(isDHCPAck(data))
                {

                    if(state==DHCPACK)
                    {
                        collectDHCPack(data);

                        uint32_t T1=(dhcpoffer.DhcpOffLeaseTime)/2;
                        uint32_t T2=(7*(dhcpoffer.DhcpOffLeaseTime))/8;
                        etherSendArpRequest(data,dhcpack.DHCPipAdd);
                        _delay_cycles(5);
                        stopTimer(cb_request);



                        startOneshotTimer(cb_renew, T1);
                        startOneshotTimer(cb_rebind,T2);
                        uint8_t i;
                        if(etherIsArpResponse(data))
                        {
                            state=DHCPDECLINE;
                            for(i=0;i<4;i++)
                            {
                                dhcpack.DHCPipAdd[i]=0;

                            }
                            startOneshotTimer(cb_init,15);

                        }
                        else
                        {
                            etherSetIpAddress(ipAddress[0],ipAddress[1],ipAddress[2],ipAddress[3]);
                            tcp=true;
                        }

                    }
                }
                 if(isDHCPNack(data))
                {
                    state=DHCPDISCOVER;

                }


            }

       //start tcp
            else  if(etherIsDhcpEnabled()==false && tcp==false)
        {
            etherSetIpAddress(192, 168, 1, 199);
            tcp=true;
        }

            if(tcp)
            {
      if(tcpstate==CLOSED)

      {
          state=LISTEN;
          if(isTcpListen(data))
          {
            sendTCPSynAck(data);
            tcpstate=SYN_RECEIVED;
            _delay_cycles(6);

          }

      }

      else if(tcpstate==SYN_RECEIVED)
          {

                  if(isTCPAck(data))
                  {

                      tcpstate=ESTABLISHED;
                      _delay_cycles(6);

                  }

          }

      else if(tcpstate==ESTABLISHED)
          {
              if(isTCPTelnet(data))
              {

                  sendTCPAck(data);
                  _delay_cycles(6);
                  sendTelnetMsg(data);
                  tcpstate=FINWAIT_1;


                  _delay_cycles(6);
              }

          }

                   else if(tcpstate==FINWAIT_1)
          {

              if(isTCPAck(data))

              {

                  waitMicrosecond(1000000);
                  sendTCPFin(data);
                  tcpstate=FINWAIT_2;

              }

          }
                   else if(tcpstate==FINWAIT_2)
                   {
                       sendTCPLastAck(data);
                       tcpstate=CLOSED;
                   }

        }

        }
    }

}
