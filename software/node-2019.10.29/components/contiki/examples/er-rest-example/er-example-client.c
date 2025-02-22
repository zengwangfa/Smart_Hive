/*
 * Copyright (c) 2013, Institute for Pervasive Computing, ETH Zurich
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * This file is part of the Contiki operating system.
 */

/**
 * \file
 *      Erbium (Er) CoAP client example.
 * \author
 *      Matthias Kovatsch <kovatsch@inf.ethz.ch>
 */
#define LOG_TAG    "coap_client"

#include <elog.h>
#include "rtthread.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "contiki.h"
#include "contiki-net.h"
#include "er-coap-engine.h"

#include "net/rpl/rpl.h"
#include "net/ip/uip.h"
#include "string.h"

#define DEBUG 0
#if DEBUG
#include <stdio.h>
#define PRINTF(...) log_i(__VA_ARGS__)
#define PRINT6ADDR(addr) PRINTF("[%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x]", ((uint8_t *)addr)[0], ((uint8_t *)addr)[1], ((uint8_t *)addr)[2], ((uint8_t *)addr)[3], ((uint8_t *)addr)[4], ((uint8_t *)addr)[5], ((uint8_t *)addr)[6], ((uint8_t *)addr)[7], ((uint8_t *)addr)[8], ((uint8_t *)addr)[9], ((uint8_t *)addr)[10], ((uint8_t *)addr)[11], ((uint8_t *)addr)[12], ((uint8_t *)addr)[13], ((uint8_t *)addr)[14], ((uint8_t *)addr)[15])
#define PRINTLLADDR(lladdr) PRINTF("[%02x:%02x:%02x:%02x:%02x:%02x]", (lladdr)->addr[0], (lladdr)->addr[1], (lladdr)->addr[2], (lladdr)->addr[3], (lladdr)->addr[4], (lladdr)->addr[5])
#else
#define PRINTF(...)
#define PRINT6ADDR(addr)
#define PRINTLLADDR(addr)
#endif

/* FIXME: This server address is hard-coded for Cooja and link-local for unconnected border router. */
//#define SERVER_NODE(ipaddr)   uip_ip6addr(ipaddr, 0xbbbb, 0, 0, 0, 0, 0, 0, 0x100)      /* cooja2 */
//#define SERVER_NODE(ipaddr)   uip_ip6addr(ipaddr, 0xbbbb, 0, 0, 0, 0xe581, 0xc232, 0x64b3, 0x6d5c)
#define SERVER_NODE(ipaddr)   uip_ip6addr(ipaddr, 0xbbbb, 0, 0, 0, 0x000a, 0x0bff, 0xfe0c, 0x0d0e)
#define LOCAL_PORT      UIP_HTONS(COAP_DEFAULT_PORT + 1)
#define REMOTE_PORT     UIP_HTONS(COAP_DEFAULT_PORT)

#define TOGGLE_INTERVAL 150

PROCESS(er_example_client, "Erbium Example Client");
//AUTOSTART_PROCESSES(&er_example_client);

uip_ipaddr_t server_ipaddr;
static struct etimer et;
static int random_status = 0;

/* Example URIs that can be queried.  */
#define NUMBER_OF_URLS 5
/* leading and ending slashes only for demo purposes, get cropped automatically when setting the Uri-Path */
char *service_urls[NUMBER_OF_URLS] =
{ ".well-known/core", "online", "keepalive", "battery/", "error/in//path" };

/* This function is will be passed to COAP_BLOCKING_REQUEST() to handle responses. */
void client_chunk_handler(void *response) {
    const uint8_t *chunk;
    int len = coap_get_payload(response, &chunk);

}

PROCESS_THREAD(er_example_client, ev, data) {

    PROCESS_BEGIN();
    static char msg[24];
    static int url_index;
    static coap_packet_t request[1]; /* This way the packet can be treated as pointer as usual. */

#ifdef auto_find_server
    /* ���� */
    static uip_ip6addr_t *globaladdr = NULL;
    static rpl_dag_t *dag;
    uip_ds6_addr_t *addr_desc = uip_ds6_get_global(ADDR_PREFERRED);
    if(addr_desc != NULL) {
      globaladdr = &addr_desc->ipaddr;
      dag = rpl_get_any_dag();
      if(dag) {
          uip_ipaddr_copy(&server_ipaddr, globaladdr);
          memcpy(&server_ipaddr.u8[8], &dag->dag_id.u8[8], sizeof(uip_ipaddr_t) / 2);
      } else {
          SERVER_NODE(&server_ipaddr);
      }
    }
#else
    SERVER_NODE(&server_ipaddr);
#endif
    
    sprintf(msg, "%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x", linkaddr_node_addr.u8[0], linkaddr_node_addr.u8[1],
      linkaddr_node_addr.u8[2], linkaddr_node_addr.u8[3], linkaddr_node_addr.u8[4],
      linkaddr_node_addr.u8[5], linkaddr_node_addr.u8[6], linkaddr_node_addr.u8[7]);
    
    /* receives all CoAP messages */
    coap_init_engine();
    random_status = random_rand()%20;
    etimer_set(&et, random_status * CLOCK_SECOND);
    url_index = 1;

    while (1) {
        PROCESS_YIELD();
        if (etimer_expired(&et)) {
            PRINTF("--Toggle timer--");
            /* prepare request, TID is set by COAP_BLOCKING_REQUEST() */
            coap_init_message(request, COAP_TYPE_CON, COAP_POST, 0);
            coap_set_header_uri_path(request, service_urls[url_index]);
            coap_set_payload(request, (uint8_t *) msg, sizeof(msg) - 1);
            PRINT6ADDR(&server_ipaddr);
            PRINTF(" : %u", UIP_HTONS(REMOTE_PORT));
            COAP_BLOCKING_REQUEST(&server_ipaddr, REMOTE_PORT, request, client_chunk_handler);
            PRINTF("--Done--");
            //etimer_reset(&et);
            random_status = random_rand()%TOGGLE_INTERVAL;
            etimer_reset_with_new_interval(&et, (TOGGLE_INTERVAL+random_status)*CLOCK_SECOND);
            url_index = 2;
        }
    }
    PROCESS_END();
}
