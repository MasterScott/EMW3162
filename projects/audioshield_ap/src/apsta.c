#include "wiced.h"
#include "http_server.h"
#include "resources.h"
#include "dns_redirect.h"
//#include "wifi_config_dct.h"
//#include "AudioCodec.h"
#include "audioboard/audioboard.h"

static const wiced_ip_setting_t ap_ip_settings =
{
    INITIALISER_IPV4_ADDRESS( .ip_address, MAKE_IPV4_ADDRESS( 192,168,  0,  1 ) ),
    INITIALISER_IPV4_ADDRESS( .netmask,    MAKE_IPV4_ADDRESS( 255,255,255,  0 ) ),
    INITIALISER_IPV4_ADDRESS( .gateway,    MAKE_IPV4_ADDRESS( 192,168,  0,  1 ) ),
};

static wiced_http_server_t ap_http_server;
static int  process_data_handler( const char* url, wiced_tcp_stream_t* stream, void* arg );

START_OF_HTTP_PAGE_DATABASE(ap_web_pages)
    ROOT_HTTP_PAGE_REDIRECT("/index.html"),
    { "/index.html", "text/html", WICED_RESOURCE_URL_CONTENT,   .url_content.resource_data  = &build_resources_index_html_c,    },
    { "/test.png",   "image/png", WICED_RESOURCE_URL_CONTENT,   .url_content.resource_data  = &build_resources_test_png_c,      },
    { "/data",   "text/html", WICED_DYNAMIC_URL_CONTENT, .url_content.dynamic_data = {process_data_handler, 0}, },
END_OF_HTTP_PAGE_DATABASE();

static dns_redirector_t    dns_redirector;

/******************************************************
 *               Function Definitions
 ******************************************************/

static int process_data_handler( const char* url, wiced_tcp_stream_t* stream, void* arg )
{
	char * res = "test";
	wiced_tcp_stream_write( stream, res, strlen(res) );
	 return 0;
}

#define UDP_TARGET_PORT             50007
#define UDP_TARGET_IP MAKE_IPV4_ADDRESS(192,168,0,2)
#define headerlength  12
#define UDP_MAX_DATA_LENGTH         (2000)
wiced_udp_socket_t  udp_socket;
static wiced_timed_event_t udp_tx_event;
static wiced_timed_event_t process_udp_rx_event;

char senddata[UDP_MAX_DATA_LENGTH];
char sendbusy = 0;
uint32_t timer = 0;
int seqnr = 0;
int tosendlengthinchar = 0;

wiced_result_t tx_udp_packet()
{
	 	 wiced_packet_t*          packet;
	    uint16_t                 available_data_length;
	    const wiced_ip_address_t INITIALISER_IPV4_ADDRESS( target_ip_addr, UDP_TARGET_IP );
	    char * udp_data;
	    int sizeinchar = tosendlengthinchar *sizeof(char);
	    if(sendbusy == 0) {
	    	// No new packages yet
	    	return WICED_SUCCESS;
	    }
	    /* Create the UDP packet */
	    if ( wiced_packet_create_udp( &udp_socket, sizeinchar+1, &packet, (uint8_t**) &udp_data, &available_data_length ) != WICED_SUCCESS )
	    {
	        WPRINT_APP_INFO( ("UDP tx packet creation failed\n") );
	        return WICED_SUCCESS;
	    }

	    /* Write packet number into the UDP packet data */
	    memcpy(udp_data, senddata, sizeinchar);
	    sendbusy = 0;
	    udp_data[tosendlengthinchar+1] = 0;
	    /* Set the end of the data portion */
	    wiced_packet_set_data_end( packet, (uint8_t*) udp_data + sizeinchar);

	    /* Send the UDP packet */
	    if ( wiced_udp_send( &udp_socket, &target_ip_addr, UDP_TARGET_PORT, packet ) != WICED_SUCCESS )
	    {
	        WPRINT_APP_INFO( ("UDP packet send failed\n") );
	        wiced_packet_delete( packet ); /* Delete packet, since the send failed */
	        return WICED_SUCCESS;
	    }
#undef RTPDEBUG
#ifdef RTPDEBUG
	    int i;
	    printf("\r\n\r\n");
	    for(i = 0; i < 20; i++) {
	    		char tmp[2] = { 0 };	// else printf will keep continuing if ff till it finds a 0
	    		tmp[0] = udp_data[i];
	    		printf("%02x ",tmp[0]);
	    	if(i % 10 == 0)
	    		printf("\r\n");
	    }
#endif

	    return WICED_SUCCESS;
}


void SendUdpData(uint16_t* data, int length) {
	if(sendbusy == 0) {
		 	  	  	  	  // RTP  payloadt	seqnr1	seqnr2	time1	time2   time3,    time4, unique ids					0x84, 0xdf, 0x13, 0x5a
		//char header[] = { 	0x80, 0x00, 	((seqnr & 0xFF00) >> 8), 	seqnr, 	((timer & 0xFF000000) >> 24),   ((timer & 0xFF0000) >> 16),   ((timer & 0xFF00) >> 8), timer, ((seqnr & 0xFF00) >> 24), 	((seqnr & 0xFF00) >> 16), ((seqnr & 0xFF00) >> 8), 	seqnr};
		//int headlength16 = headerlength / 2;
		//memcpy(senddata, header, headerlength * sizeof(char) );
		//memcpy(&senddata[headlength16], data, (UDP_MAX_DATA_LENGTH-(headerlength)) * sizeof(uint16_t));
		tosendlengthinchar = length;
		memcpy(senddata, data, (UDP_MAX_DATA_LENGTH) * sizeof(char));
		//free((void*) data);
		//timer += 441;
		//seqnr++;
		sendbusy = 1;
	}
}

wiced_result_t process_received_udp_packet()
{
    wiced_packet_t*           packet;
    char*                     rx_data;
    static uint16_t           rx_data_length;
    uint16_t                  available_data_length;



    /* Wait for UDP packet */
    wiced_result_t result = WICED_SUCCESS;
	while(result == WICED_SUCCESS) {
		result = wiced_udp_receive( &udp_socket, &packet, 10 );
		if ( ( result == WICED_ERROR ) || ( result == WICED_TIMEOUT ) )
		{
			return result;
		}
		
		/* Extract the received data from the UDP packet */
		wiced_packet_get_data( packet, 0, (uint8_t**) &rx_data, &rx_data_length, &available_data_length );

		/* Null terminate the received data, just in case the sender didn't do this */
		rx_data[ rx_data_length ] = '\x0';
		Audioboard_Receive_audio(rx_data, rx_data_length);
	
		//SendUdpData((uint16_t *) rx_data, rx_data_length);
		/* Delete the received packet, it is no longer needed */
		wiced_packet_delete( packet );
    }
    

    return WICED_SUCCESS;
}

void application_start(void)
{
    /* Initialise the device */
    wiced_init();
    WPRINT_PLATFORM_INFO( ("WICED initialized.\n") );

    /* Bring up the softAP interface ------------------------------------------------------------- */
    wiced_network_up(WICED_AP_INTERFACE, WICED_USE_INTERNAL_DHCP_SERVER, &ap_ip_settings);
    WPRINT_PLATFORM_INFO( ("Soft AP Up.\n") );
    /* Start a DNS redirect server to redirect wiced.com to the AP webserver database*/
    wiced_dns_redirector_start( &dns_redirector, WICED_AP_INTERFACE );
    WPRINT_PLATFORM_INFO( ("DNS Redirector initialised\n") );

    /* Start a web server on the AP interface */
    wiced_http_server_start( &ap_http_server, 80, ap_web_pages, WICED_AP_INTERFACE );
    WPRINT_PLATFORM_INFO( ("HTTP Daemon started\n") );

    WPRINT_PLATFORM_INFO( ("Going to initialize Audio shield.\n") );
    Audioboard_Init();
    WPRINT_PLATFORM_INFO( ("Audio shield initialized.\n") );

    if ( wiced_udp_create_socket( &udp_socket, UDP_TARGET_PORT, WICED_AP_INTERFACE ) != WICED_SUCCESS )
   {
	   WPRINT_APP_INFO( ("UDP socket creation failed\n") );


   } else {
	   wiced_rtos_register_timed_event( &udp_tx_event, WICED_NETWORKING_WORKER_THREAD, &tx_udp_packet, 1, senddata );
	    wiced_rtos_register_timed_event( &process_udp_rx_event, WICED_NETWORKING_WORKER_THREAD, &process_received_udp_packet, 1, 0 );

   }


	//wiced_result_t result = wm8533_init(&ac);
}


