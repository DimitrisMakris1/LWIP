/*
 * Licensed to the OpenAirInterface (OAI) Software Alliance under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The OpenAirInterface Software Alliance licenses this file to You under
 * the OAI Public License, Version 1.0  (the "License"); you may not use this file
 * except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.openairinterface.org/?page_id=698
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *-------------------------------------------------------------------------------
 * For more information about the OpenAirInterface (OAI) Software Alliance:
 *      contact@openairinterface.org
 */

/*! \file pdcp.c
 * \brief pdcp interface with RLC
 * \author Navid Nikaein and Lionel GAUTHIER
 * \date 2009-2012
 * \email navid.nikaein@eurecom.fr
 * \version 1.0
 */

#define PDCP_C
//#define DEBUG_PDCP_FIFO_FLUSH_SDU

#ifndef USER_MODE
#include <rtai_fifos.h>
#endif
#include "assertions.h"
#include "hashtable.h"
#include "pdcp.h"
#include "pdcp_util.h"
#include "pdcp_sequence_manager.h"
#include "LAYER2/RLC/rlc.h"
#include "LAYER2/MAC/extern.h"
#include "RRC/LITE/proto.h"
#include "pdcp_primitives.h"
#include "OCG.h"
#include "OCG_extern.h"
#include "otg_rx.h"
#include "UTIL/LOG/log.h"
#include <inttypes.h>
#include "platform_constants.h"
#include "UTIL/LOG/vcd_signal_dumper.h"
#include "msc.h"
#include <linux/if_packet.h>
#include<netinet/ip_icmp.h>   //Provides declarations for icmp header
#include<netinet/udp.h>   //Provides declarations for udp header
#include<netinet/tcp.h>   //Provides declarations for tcp header
#include<netinet/ip.h>    //Provides declarations for ip header
#include<sys/socket.h>
#include<arpa/inet.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <net/if.h>
#include <netinet/ether.h>
#include <sys/time.h>
#include <pthread.h>
#include <unistd.h>
#include <stdio.h>
#include <netinet/udp.h>
//#include <linux/icmp.h>




//#define MY_DEST_MAC0	0x48
//#define MY_DEST_MAC1	0xd2
//#define MY_DEST_MAC2	0x24
//#define MY_DEST_MAC3	0x28
//#define MY_DEST_MAC4	0xc1
//#define MY_DEST_MAC5	0x3e

//#define MY_DEST_MAC0	0xa0
//#define MY_DEST_MAC1	0x39
//#define MY_DEST_MAC2	0xf7
//#define MY_DEST_MAC3	0x45
//#define MY_DEST_MAC4	0x66
//#define MY_DEST_MAC5	0x66

// OAIUE
#define MY_DEST_MAC0	0xec
#define MY_DEST_MAC1	0x08
#define MY_DEST_MAC2	0x6b
#define MY_DEST_MAC3	0x0d
#define MY_DEST_MAC4	0x8e
#define MY_DEST_MAC5	0xbe

//Mobile Phone
/*#define MY_DEST_MAC0	0xa0
 #define MY_DEST_MAC1	0x39
 #define MY_DEST_MAC2	0xf7
 #define MY_DEST_MAC3	0x45
 #define MY_DEST_MAC4	0x66
 #define MY_DEST_MAC5	0x66
 */

#if defined(ENABLE_SECURITY)
# include "UTIL/OSA/osa_defs.h"
#endif

#if defined(ENABLE_ITTI)
# include "intertask_interface.h"
#endif

#if defined(LINK_ENB_PDCP_TO_GTPV1U)
#  include "gtpv1u_eNB_task.h"
#  include "gtpv1u.h"
#endif

#ifndef OAI_EMU
extern int otg_enabled;
#endif

//for steering
protocol_ctxt_t* copy_ctxt_pP;
srb_flag_t copy_srb_flagP;
//	srb_flag_t copy_srb_flagP;
rb_id_t copy_rb_idP;
mui_t copy_muiP;
confirm_t copy_confirmP;
sdu_size_t copy_sdu_buffer_sizeP;
unsigned char * copy_sdu_buffer_pP;
//	pdcp_transmission_mode_t copy_modeP;


int dup_ack_count = 0; // Initialize DUP-ACK count to 0
int index_split = 0;
int arr_dup_ack[5] = { 0, 0, 0, 0, 0 }; // Array which stores the DUPACK count for 2:1, 1:1, 1:2, 1:3, 1:4 respectively
float select_ratio[5] = { 1.1, 1.2, 1.3, 1.4, 1.5 };
int flag[5] = { 0, 0, 0, 0, 0 };
int position = 0;
unsigned short csum(unsigned short *buf, int nwords) {
	unsigned long sum;
	for (sum = 0; nwords > 0; nwords--) {
		sum += *buf++;
		//  printf("%x\n",buf);
	}
	sum = (sum >> 16) + (sum & 0xffff);
	sum += (sum >> 16);
	return (unsigned short) (~sum);
}

uint16_t tcp_checksum(const void *buff, uint16_t len, in_addr_t src_addr,
		in_addr_t dest_addr) {
	//printf("function called Buffer length %d , Src %u ,Dst %u\n",len,src_addr,dest_addr);
	const uint16_t *buf1 = buff;
	uint16_t *ip_src = (void *) &src_addr;
	uint16_t *ip_dst = (void *) &dest_addr;
	uint32_t sum;
	uint16_t length = len;
	// Calculate the sum                                            //
	sum = 0;
	while (len > 1) {
		//printf("%2x \t",*buf1);
		sum += *buf1++;
		if (sum & 0x80000000)
			sum = (sum & 0xFFFF) + (sum >> 16);
		len -= 2;
	}
	if (len & 1)
		// Add the padding if the packet length is odd          //
		sum += *((uint8_t *) buf1);
	//printf(" \n");
	// Add the pseudo-header                                        //
	sum += *(ip_src++);
	sum += *ip_src;
	sum += *(ip_dst++);
	sum += *ip_dst;
	sum += htons(IPPROTO_TCP);
	sum += htons(length);
	// Add the carries                                              //
	while (sum >> 16)
		sum = (sum & 0xFFFF) + (sum >> 16);
	//printf("%x\n",(~sum));
	// Return the one's complement of sum                           //
	return ((uint16_t) (~sum));
}
uint16_t udp_checksum(const void *buff, uint16_t len, in_addr_t src_addr,
		in_addr_t dest_addr) {
	const uint16_t *buf = buff;
	uint16_t *ip_src = (void *) &src_addr, *ip_dst = (void *) &dest_addr;
	uint32_t sum;
	uint16_t length = len;
	// Calculate the sum
	sum = 0;
	while (len > 1) {
		//printf("%2x \t",*buf);
		sum += *buf++;
		if (sum & 0x80000000)
			sum = (sum & 0xFFFF) + (sum >> 16);
		len -= 2;
	}
	if (len & 1)
		// Add the padding if the packet lengselect_ratio[0] = 1.1;ht is odd          //
		sum += *((uint8_t *) buf);
	// Add the pseudo-header                                        //
	sum += *(ip_src++);
	sum += *ip_src;
	sum += *(ip_dst++);
	sum += *ip_dst;
	sum += htons(IPPROTO_UDP);
	sum += htons(length);
	// Add the carries                                              //
	while (sum >> 16)
		sum = (sum & 0xFFFF) + (sum >> 16);
	// Return the one's complement of sum                           //
	return ((uint16_t) (~sum));
}



float current_timestamp() {
	struct timeval te;
	gettimeofday(&te, NULL); // get current time
	//long long milliseconds = te.tv_sec*1000LL + te.tv_usec/1000; // calculate milliseconds
	//long sec= te.tv_sec;
	float milliseconds = te.tv_usec / 1000;  // calculate milliseconds 1000

	return milliseconds;
}

float current_timestamp_3_sec() {
	struct timeval te;
	gettimeofday(&te, NULL); // get current time
	//long long milliseconds = te.tv_sec*1000LL + te.tv_usec/1000; // caculate milliseconds
	float sec = te.tv_sec * 1000LL + te.tv_usec / 1000; // calculate milliseconds 1000

	return sec;
}


// Thread Monitors the network performance by sending the packets
/*
 * ICP packets are sent periodically the listener thread verifies the packet is received or
 * lost over the period. Link trip time is calculated using the time stamps in the ICMP
 */

int send_probing_packets()
{

	struct iphdr *ip;
	struct icmphdr* icmp;
	struct sockaddr_in connection;
	uint16_t wifi_id_count=0, lte_id_count=1;
	while(1)
	{
		usleep (1000000);
		// send probe packet over Wi-Fi
		char *dst_addr="192.168.140.50"; //(Destination Wi-Fi interface IP)
		char *src_addr="192.168.140.40"; //Sender Wi-Fi interface IP address
		int sockfd, optval;

		memset(packet,0, sizeof(struct iphdr)+sizeof(struct icmphdr));
		ip = (struct iphdr*) packet;
		icmp = (struct icmphdr*) (packet + sizeof(struct iphdr));

		icmp->type      = ICMP_ECHO;
		wifi_id_count=wifi_id_count+2;
		icmp->un.echo.sequence=wifi_id_count;

		//icmp->checksum = in_cksum((unsigned short *)icmp, sizeof(struct icmphdr));
		icmp->checksum = csum((unsigned short *)icmp, sizeof(struct icmphdr)/2);
		ip->ihl         = 5;
		ip->version     = 4;
		ip->tot_len     = htons(sizeof(struct iphdr) + sizeof(struct icmphdr));
		ip->protocol    = IPPROTO_ICMP;
		ip->saddr       = inet_addr(src_addr);
		ip->daddr       = inet_addr(dst_addr);
		//ip->check 		= in_cksum((unsigned short *)ip, sizeof(struct iphdr));
		ip->ttl			= 64;
		ip->check		= csum((unsigned short *)ip, sizeof(struct iphdr)/2);

		//printf("length of ip %d", ip->tot_len);
		char sendbuf[sizeof(struct ether_header) +  sizeof(struct iphdr) + sizeof(struct icmphdr)];
		int tx_len = 0;
		struct ether_header *eh = (struct ether_header *) sendbuf;

		char *data = (char *) (sendbuf + sizeof(struct ether_header));
		memcpy(data, packet, (sizeof(struct iphdr) + sizeof(struct icmphdr)));
		struct sockaddr_ll socket_address;

		/* Ethernet header */
		eh->ether_shost[0] = ((uint8_t *) &if_mac.ifr_hwaddr.sa_data)[0];
		eh->ether_shost[1] = ((uint8_t *) &if_mac.ifr_hwaddr.sa_data)[1];
		eh->ether_shost[2] = ((uint8_t *) &if_mac.ifr_hwaddr.sa_data)[2];
		eh->ether_shost[3] = ((uint8_t *) &if_mac.ifr_hwaddr.sa_data)[3];
		eh->ether_shost[4] = ((uint8_t *) &if_mac.ifr_hwaddr.sa_data)[4];
		eh->ether_shost[5] = ((uint8_t *) &if_mac.ifr_hwaddr.sa_data)[5];
		eh->ether_dhost[0] = MY_DEST_MAC0;
		eh->ether_dhost[1] = MY_DEST_MAC1;
		eh->ether_dhost[2] = MY_DEST_MAC2;
		eh->ether_dhost[3] = MY_DEST_MAC3;
		eh->ether_dhost[4] = MY_DEST_MAC4;
		eh->ether_dhost[5] = MY_DEST_MAC5;
		/* Ethertype field */
		eh->ether_type = htons(ETH_P_IP);
		tx_len = sizeof(struct ether_header) + (sizeof(struct iphdr) + sizeof(struct icmphdr));

		/* Index of the network device */
		socket_address.sll_ifindex = if_idx.ifr_ifindex;
		/* Address length*/
		socket_address.sll_halen = ETH_ALEN;
		/* Destination MAC */
		socket_address.sll_addr[0] = MY_DEST_MAC0;
		socket_address.sll_addr[1] = MY_DEST_MAC1;
		socket_address.sll_addr[2] = MY_DEST_MAC2;
		socket_address.sll_addr[3] = MY_DEST_MAC3;
		socket_address.sll_addr[4] = MY_DEST_MAC4;
		socket_address.sll_addr[5] = MY_DEST_MAC5;
		wifi_pkt_tx_time=current_timestamp();
		wifi_transmitted_pkt_seq_number=wifi_id_count;
			if (sendto(so, sendbuf, tx_len, 0,(struct sockaddr *) &socket_address, sizeof(struct sockaddr_ll)) < 0) {
			char *src_addr="10.0.1.1"; //Sender Wi-Fi interface IP address		perror("send to failed");
				}

		//Send probe packets over LTE

		if (lte_enabled==1){
			struct iphdr *lte_ip;
			struct icmphdr* lte_icmp;
			char *src1_addr="10.0.1.1"; //Sender LTE interface IP address
			char *dst1_addr="10.0.1.9"; //Destination LTE interface IP address
			memset(lte_probe_packet,0, sizeof(struct iphdr)+sizeof(struct icmphdr));
			lte_ip = (struct iphdr*) lte_probe_packet;
			lte_icmp = (struct icmphdr*) (lte_probe_packet + sizeof(struct iphdr));
			lte_icmp->type      = ICMP_ECHO;
			lte_id_count=lte_id_count+2;
			lte_icmp->un.echo.sequence=lte_id_count;
			lte_icmp->checksum = csum((unsigned short *)lte_icmp, sizeof(struct icmphdr)/2);
			lte_ip->ihl         = 5;
			lte_ip->version     = 4;
			lte_ip->tot_len     = htons(sizeof(struct iphdr) + sizeof(struct icmphdr));
			lte_ip->protocol    = 1;
			lte_ip->saddr       = inet_addr(src1_addr);
			lte_ip->daddr       = inet_addr(dst1_addr);
			lte_ip->ttl			= 64;
			lte_ip->check		= csum((unsigned short *)lte_ip, sizeof(struct iphdr)/2);
			copy_sdu_buffer_pP=lte_probe_packet;
			copy_sdu_buffer_sizeP=sizeof(struct iphdr) + sizeof(struct icmphdr);
			pdcp_data_req(copy_ctxt_pP,copy_srb_flagP,copy_rb_idP,copy_muiP, copy_confirmP,	copy_sdu_buffer_sizeP,copy_sdu_buffer_pP, 2);
		//	printf("RB id =%d , muiP = %d , copy_confirmP= %d, 2  ******sent******\n", copy_rb_idP,copy_muiP,copy_confirmP);
			struct sockaddr_in tsaddr;
			struct sockaddr_in todaddr;
			tsaddr.sin_addr.s_addr = lte_ip->saddr;
			todaddr.sin_addr.s_addr = lte_ip->daddr;
			lte_pkt_tx_time=current_timestamp();
			lte_transmitted_pkt_seq_number=lte_id_count;
		//	printf("@@ sender Source ip = %s,  ||@@ destination ip = %s\n ",inet_ntoa(tsaddr.sin_addr), inet_ntoa(todaddr.sin_addr));
		//	printf("length of ip = %d,  sent packet ID= %d", ip->tot_len,lte_id_count);
			//char sendbuf[sizeof(struct ether_header) +  sizeof(struct iphdr) + sizeof(struct icmphdr)];
		}
	}
	return 1;
}


int receive_probing_packets()
{
	struct iphdr *ip_reply;
	struct icmphdr* icmp_hdr;
	struct sockaddr_in connection;
	int  sockfd1, addrlen, numBytes, optval;

	/* open ICMP socket */
	if ((sockfd1 = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP)) == -1) {
		perror("socket");
		exit(EXIT_FAILURE);
	}
	setsockopt(sockfd1, IPPROTO_IP, IP_HDRINCL, &optval, sizeof(int));
	//char *dst_addr="192.168.140.50";
	connection.sin_family       = AF_INET;
	connection.sin_addr.s_addr  = htonl(INADDR_ANY);
	addrlen= sizeof(connection);
	while(1){
		if (recvfrom(sockfd1, probe_buffer, sizeof(struct iphdr) + sizeof(struct icmphdr), 0, (struct sockaddr *)&connection, &addrlen) == -1)
		{
			perror("recv");
		}
		ip_reply = (struct iphdr*) probe_buffer;
		icmp_hdr = (struct icmphdr*)(probe_buffer+sizeof(struct iphdr));
		//		//cp = (char *)&ip_reply->saddr;
		//		//printf("Received %d byte reply from %u.%u.%u.%u:\n", ntohs(ip_reply->tot_len), cp[0]&0xff,cp[1]&0xff,cp[2]&0xff,cp[3]&0xff);
		if(icmp_hdr->un.echo.sequence%2==0)
		{
			if (icmp_hdr->un.echo.sequence==wifi_transmitted_pkt_seq_number){
			RTT_wifi=(0.4*RTT_wifi)+(0.6*(current_timestamp()-wifi_pkt_tx_time));
			printf(" Wifi packet sequence= %d RTT wifi= %f \n",icmp_hdr->un.echo.sequence, RTT_wifi);
			}
		}
		else{
			if (icmp_hdr->un.echo.sequence==lte_transmitted_pkt_seq_number){
			RTT_lte=(0.4*RTT_lte)+(0.6*(current_timestamp()-lte_pkt_tx_time));
			printf(" lte packet sequence= %d\n RTT lte= %f",icmp_hdr->un.echo.sequence , RTT_lte);
			}
		}
		//printf("ID: %d un chnaged ID: %d  Sequence number = % d\n ", ntohs(ip_reply->id), ip_reply->id, icmp_hdr->un.echo.sequence);
		//printf("TTL: %d\n", ip_reply->ttl);

	}
	return 1;
}




//-----------------------------------------------------------------------------
/*
 * If PDCP_UNIT_TEST is set here then data flow between PDCP and RLC is broken
 * and PDCP has no longer anything to do with RLC. In this case, after it's handed
 * an SDU it appends PDCP header and returns (by filling in incoming pointer parameters)
 * this mem_block_t to be dissected for testing purposes. For further details see test
 * code at targets/TEST/PDCP/test_pdcp.c:test_pdcp_data_req()
 */
boolean_t pdcp_data_req(protocol_ctxt_t* ctxt_pP, const srb_flag_t srb_flagP,
		const rb_id_t rb_idP, const mui_t muiP, const confirm_t confirmP,
		const sdu_size_t sdu_buffer_sizeP, unsigned char * const sdu_buffer_pP,
		const pdcp_transmission_mode_t modeP)
//-----------------------------------------------------------------------------
{
	pdcp_t *pdcp_p = NULL;
	uint8_t i = 0;
	uint8_t pdcp_header_len = 0;
	uint8_t pdcp_tailer_len = 0;
	uint16_t pdcp_pdu_size = 0;
	uint16_t current_sn = 0;
	mem_block_t *pdcp_pdu_p = NULL;
	rlc_op_status_t rlc_status;
	boolean_t ret = TRUE;

	hash_key_t key = HASHTABLE_NOT_A_KEY_VALUE;
	hashtable_rc_t h_rc;
	VCD_SIGNAL_DUMPER_DUMP_FUNCTION_BY_NAME(VCD_SIGNAL_DUMPER_FUNCTIONS_PDCP_DATA_REQ,VCD_FUNCTION_IN); CHECK_CTXT_ARGS(ctxt_pP);

#if T_TRACER
	if (ctxt_pP->enb_flag != ENB_FLAG_NO)
		T(T_ENB_PDCP_DL, T_INT(ctxt_pP->module_id), T_INT(ctxt_pP->rnti), T_INT(rb_idP), T_INT(sdu_buffer_sizeP));
#endif

	if (sdu_buffer_sizeP == 0) {
		LOG_W(PDCP, "Handed SDU is of size 0! Ignoring...\n");
		return FALSE ;
	}

	/*
	 * XXX MAX_IP_PACKET_SIZE is 4096, shouldn't this be MAX SDU size, which is 8188 bytes?
	 */

	if (sdu_buffer_sizeP > MAX_IP_PACKET_SIZE) {
		LOG_E(PDCP,
				"Requested SDU size (%d) is bigger than that can be handled by PDCP (%u)!\n",
				sdu_buffer_sizeP, MAX_IP_PACKET_SIZE);
		// XXX What does following call do?
		mac_xface->macphy_exit("PDCP sdu buffer size > MAX_IP_PACKET_SIZE");
	}
	if (lte_enabled==0 && ctxt_pP->rnti>0){

				memset(copy_ctxt_pP, 0, sizeof(protocol_ctxt_t));
				copy_ctxt_pP->configured=ctxt_pP->configured;
				copy_ctxt_pP->eNB_index=ctxt_pP->eNB_index;
				copy_ctxt_pP->enb_flag=ctxt_pP->enb_flag;
				copy_ctxt_pP->frame=ctxt_pP->frame;
				copy_ctxt_pP->instance=ctxt_pP->instance;
				copy_ctxt_pP->module_id=ctxt_pP->module_id;
				copy_ctxt_pP->rnti=ctxt_pP->rnti;
				copy_ctxt_pP->subframe=ctxt_pP->subframe;
				copy_srb_flagP=false;//srb_flagP;
				copy_rb_idP=rb_idP;
				copy_muiP=muiP;
				copy_confirmP=confirmP;
				lte_enabled=1;
				//copy_ctxt_pP=ctxt_pP;
				copy_rb_idP=rb_idP;
				//	copy_sdu_buffer_sizeP=sdu_buffer_sizeP;
				//	copy_sdu_buffer_pP=sdu_buffer_pP;
				//	copy_modeP=modeP;
			//	printf("Original RB id =%d , muiP = %d , copy_confirmP= %d, mode= %d \n", copy_rb_idP,copy_muiP,copy_confirmP,modeP);
			}

	if (modeP == PDCP_TRANSMISSION_MODE_DATA) {


		//printf("Original Context inst= %d module = %d rnti=%d configured=%d  srbid=%d\n ", ctxt_pP->instance, ctxt_pP->module_id, ctxt_pP->rnti, ctxt_pP->configured, srb_flagP);
		//printf("Copied Context inst= %d module = %d rnti=%d configured=%d \n ", copy_ctxt_pP->instance, copy_ctxt_pP->module_id, copy_ctxt_pP->rnti, copy_ctxt_pP->configured);

		//printf("*");
		struct iphdr *iph = (struct iphdr*) sdu_buffer_pP;
		//char source_ip[32] ;

		char datagram[sdu_buffer_sizeP];
		//	char datagram=malloc(sdu_buffer_sizeP);
		memcpy(datagram, sdu_buffer_pP, sdu_buffer_sizeP);
		struct iphdr *iph1 = (struct iphdr*) datagram;


		if (iph->protocol==1){
			//printf("Main function RB id =%d , muiP = %d , copy_confirmP= %d, mode= %d \n", copy_rb_idP,copy_muiP,copy_confirmP,modeP);
			struct sockaddr_in tsaddr;
			struct sockaddr_in tdaddr;
			tsaddr.sin_addr.s_addr = iph1->saddr;
			tdaddr.sin_addr.s_addr = iph1->daddr;
			//printf("##Source ip = %s,  destination ip = %s\n ",inet_ntoa(tsaddr.sin_addr), inet_ntoa(tdaddr.sin_addr));
		/*	printf("size of Buffer = %d",sdu_buffer_sizeP);
			for (i=0; i<sdu_buffer_sizeP; i++)
			{
				printf("%.2X \t ",*(sdu_buffer_pP+i));
			}
			printf("\n");*/
		}
		//int so = 0;

		/*if(ntohl(tcph1->ack_seq)==seq){
		 dup_ack_count+=1;
		 }
		 arr_dup_ack[] = dup_ack_count;
		 */

		//int so = socket(AF_PACKET, SOCK_RAW, IPPROTO_RAW);
		/*struct sockaddr_in temp_source,temp_dest;
		 temp_source.sin_addr.s_addr = iph1->saddr;
		 temp_dest.sin_addr.s_addr = iph1->daddr;
		 printf("   |-Source IP        : %s\n",inet_ntoa(temp_source.sin_addr));
		 printf("   |-Destination IP   : %s\n",inet_ntoa(temp_dest.sin_addr));*/
		/*printf("   |-ACK Number       : %ld\n",ntohl(tcph1->ack));
		 printf("   |-ACK SequenceNo   : %ld\n",ntohl(tcph1->ack_seq));
		 printf("   |-ACK SequenceNo   : %ld\n",seq);
		 printf("   |-Modified Source IP      : %s\n",inet_ntoa(source.sin_addr));
		 printf("   |-Modified Destination IP : %s\n",inet_ntoa(dest.sin_addr));
		 */
		//Change 1
		iph1->daddr = inet_addr("192.168.140.50"); //130.119
		iph1->check = htons(0);
		iph1->check = csum((unsigned short *) datagram,
				sizeof(struct iphdr) / 2);
		struct tcphdr *tcph1;
		if (iph->protocol == 6) {
			tcph1 = (struct tcphdr*) (datagram + 20);
			tcph1->check = 0;
			printf(" Tcp seq nu= %lu ack number= %lu synflag=%d ack = %lu\n", tcph1->seq, tcph1->ack, tcph1->syn,tcph1->ack);
			tcph1->check = tcp_checksum(
					(void *) (datagram + sizeof(struct iphdr)),
					(sdu_buffer_sizeP - sizeof(struct iphdr)), iph1->saddr,
					iph1->daddr);
		} else if (iph->protocol == 17) {
			struct udphdr *udph1 = (struct udphdr*) (datagram + 20);
			udph1->check = 0;
			udph1->check = udp_checksum(
					(void *) (datagram + sizeof(struct iphdr)),
					ntohs(udph1->len), iph1->saddr, iph1->daddr);

		}


		char sendbuf[sizeof(struct ether_header) + sdu_buffer_sizeP];
		//char sendbuf=malloc(sizeof(struct ether_header)+sdu_buffer_sizeP);
		int tx_len = 0;
		struct ether_header *eh = (struct ether_header *) sendbuf;

		char *data = (char *) (sendbuf + sizeof(struct ether_header));
		memcpy(data, datagram, sdu_buffer_sizeP);
		struct sockaddr_ll socket_address;

		/* Ethernet header */
		eh->ether_shost[0] = ((uint8_t *) &if_mac.ifr_hwaddr.sa_data)[0];
		eh->ether_shost[1] = ((uint8_t *) &if_mac.ifr_hwaddr.sa_data)[1];
		eh->ether_shost[2] = ((uint8_t *) &if_mac.ifr_hwaddr.sa_data)[2];
		eh->ether_shost[3] = ((uint8_t *) &if_mac.ifr_hwaddr.sa_data)[3];
		eh->ether_shost[4] = ((uint8_t *) &if_mac.ifr_hwaddr.sa_data)[4];
		eh->ether_shost[5] = ((uint8_t *) &if_mac.ifr_hwaddr.sa_data)[5];
		eh->ether_dhost[0] = MY_DEST_MAC0;
		eh->ether_dhost[1] = MY_DEST_MAC1;
		eh->ether_dhost[2] = MY_DEST_MAC2;
		eh->ether_dhost[3] = MY_DEST_MAC3;
		eh->ether_dhost[4] = MY_DEST_MAC4;
		eh->ether_dhost[5] = MY_DEST_MAC5;
		/* Ethertype field */
		eh->ether_type = htons(ETH_P_IP);
		tx_len = sizeof(struct ether_header) + sdu_buffer_sizeP;

		/* Index of the network device */
		socket_address.sll_ifindex = if_idx.ifr_ifindex;
		/* Address length*/
		socket_address.sll_halen = ETH_ALEN;
		/* Destination MAC */
		socket_address.sll_addr[0] = MY_DEST_MAC0;
		socket_address.sll_addr[1] = MY_DEST_MAC1;
		socket_address.sll_addr[2] = MY_DEST_MAC2;
		socket_address.sll_addr[3] = MY_DEST_MAC3;
		socket_address.sll_addr[4] = MY_DEST_MAC4;
		socket_address.sll_addr[5] = MY_DEST_MAC5;

		//printf(" Wifi count= %d , LTE count =%d\n",wifi_count,lte_count);
		//if(iph1->saddr==inet_addr("192.168.130.132") && iph1->id%10!=2 && iph1->id%10!=4 && iph1->id%10!=6 && iph1->id%10!=8  && iph1->id%10!=0 && iph1->protocol==6)
		//if(iph1->saddr==inet_addr("192.168.130.132") && iph1->protocol!=3 && iph1->protocol!=4 && iph1->id%10!=5 && iph1->id%10!=6 && iph1->id%10!=7 && iph1->id%10!=8  && iph1->protocol!=9 && iph1->id%10!=0)

		int dynamic = 0; //1-dynamic , 0- static
		float lte_wifi_ratio;

		lte_wifi_ratio = 0; //Set static Value of split here, if dynamic it will be changed dynamically
		int current = 1;
		if (dynamic == 1) {
			/******************* Probing Section [probing time = 500 msec] ***************************/

			long rem = ((int)current_timestamp_3_sec()) % 3000;
			//printf("\nCurrent Timestamp: %d \n", rem);
			int min = 999;

			if (rem <= 100) {
				index_split = 0;
				if (flag[index_split] == 0) {
					dup_ack_count = 0;
					flag[index_split] = 1;
				}
				//select_ratio[0] = 2.1;
				position = 6;
				//printf("Dup ack in index 0 =%d",dup_ack_count);
			} else if (rem >= 101 && rem <= 200) {
				index_split = 1;
				if (flag[index_split] == 0) {
					arr_dup_ack[index_split - 1] = dup_ack_count;
					//	printf("Dup ack in index 0 =%d\n",dup_ack_count);
					dup_ack_count = 0;
					flag[index_split] = 1;
				}

				//select_ratio[1] = 1.1;

				position = 6;
				arr_dup_ack[index_split] = dup_ack_count;

			} else if (rem >= 201 && rem <= 300) {
				index_split = 2;
				if (flag[index_split] == 0) {
					arr_dup_ack[index_split - 1] = dup_ack_count;
					//	printf("Dup ack in index 1 =%d\n",dup_ack_count);
					dup_ack_count = 0;
					flag[index_split] = 1;
				}

				//select_ratio[2] = 1.2;

				position = 6;
				//arr_dup_ack[index_split]=dup_ack_count;

			} else if (rem >= 301 && rem <= 400) {
				index_split = 3;
				if (flag[index_split] == 0) {
					arr_dup_ack[index_split - 1] = dup_ack_count;
					//	printf("Dup ack in index 2 =%d\n",dup_ack_count);
					dup_ack_count = 0;
					flag[index_split] = 1;
				}

				//select_ratio[3] = 1.3;

				position = 6;
				//arr_dup_ack[index_split]=dup_ack_count;

			} else if (rem >= 401 && rem <= 500) {
				index_split = 4;
				if (flag[index_split] == 0) {
					arr_dup_ack[index_split - 1] = dup_ack_count;
					//printf("Dup ack in index 3 =%d\n",dup_ack_count);
					dup_ack_count = 0;
					flag[index_split] = 1;
				}

				//select_ratio[4] = 1.4;

				position = 6;
				arr_dup_ack[index_split] = dup_ack_count;
				//printf("Dup ack in index 4 =%d\n",dup_ack_count);
			}

			if (position == 6 && rem >= 500) {

				for (i = 0; i < 5; i++) {
					flag[i] = 0;
					if (min >= arr_dup_ack[i]) {
						//printf("\nNumber of dupacks for split %f = %d\n",select_ratio[i],arr_dup_ack[i]);
						min = arr_dup_ack[i];
						position = i;
					}
					printf("\nDupAck Count of %f split = %d", select_ratio[i],
							arr_dup_ack[i]);
				}
				index_split = position;
				printf("\nThe split %f is being used !!!\n index = %d",
						select_ratio[index_split], index_split);
			}

			lte_wifi_ratio = select_ratio[index_split];
		}
		// Lowest RTT first Steering algorithm
		window_size=ceil(RTT_lte)+ ceil(RTT_wifi);
		local_tcp_counter++;
		if (iph1->protocol==6 && tcph1->syn==1){
			local_tcp_counter=0;
		}
		//find a window size where steering will based on RTT
	if (wifi_count< window_size-ceil(RTT_wifi) && iph1->protocol==6 && tcph1->syn==0 && local_tcp_counter > 100) // LTE : Wi-Fi ::
		{
			printf("Packet sent through Wi-Fi, TCP Sequence number is %lu\n and tcp ack number %lu \n", tcph1->seq, tcph1->ack);
				if (sendto(so, sendbuf, tx_len, 0,
						(struct sockaddr *) &socket_address,
						sizeof(struct sockaddr_ll)) < 0) {
					perror("send to failed");
				}
				wifi_count++;
				return ret;

		}
		else if (lte_count< window_size-ceil(RTT_lte) && iph1->protocol!=1){
			lte_count++;
		}
		else{
			wifi_count=0;
			lte_count=0;
			//lte_count++;
		}

/*
 	 	 		if (lte_wifi_ratio == 8.2) // LTE : Wi-Fi :: 80% : 20%
		{
			if (iph1->saddr == inet_addr("192.168.0.82") && iph1->id % 10 != 0
					&& iph1->id % 10 != 4 && iph1->id % 10 != 5
					&& iph1->id % 10 != 6 && iph1->id % 10 != 7
					&& iph1->id % 10 != 3 && iph1->id % 10 != 2
					&& iph1->id % 10 != 1) {
				wifi_count++;
				if (sendto(so, sendbuf, tx_len, 0,
						(struct sockaddr *) &socket_address,
						sizeof(struct sockaddr_ll)) < 0) {
					perror("send to failed");
				}
				//close(so);
				return ret;
			} else {
				lte_count++;
			}
		}



		if (lte_wifi_ratio == 1.11) // LTE : Wi-Fi :: 1 : 11
		{
			if (iph1->saddr == inet_addr("192.168.0.82")
					&& iph1->id % 12 != 0) {
				wifi_count++;
				//printf("To wi-Fi");
				//	printf("   |-Identification    : %u\n",iph->id);
				if (sendto(so, sendbuf, tx_len, 0,
						(struct sockaddr *) &socket_address,
						sizeof(struct sockaddr_ll)) < 0) {
					perror("send to failed");
				}
				//close(so);
				return ret;
			} else {
				lte_count++;
			}
		}

		if (lte_wifi_ratio == 1.9) // LTE : Wi-Fi :: 10% : 90%
		{
			if (iph1->saddr == inet_addr("192.168.0.82")
					&& iph1->id % 10 != 0) {
				wifi_count++;
				//printf("To wi-Fi");
				//	printf("   |-Identification    : %u\n",iph->id);
				if (sendto(so, sendbuf, tx_len, 0,
						(struct sockaddr *) &socket_address,
						sizeof(struct sockaddr_ll)) < 0) {
					perror("send to failed");
				}
				//close(so);
				return ret;
			} else {
				lte_count++;
			}
		}

		if (lte_wifi_ratio == 2.8 || current == 1) // LTE : Wi-Fi :: 20% : 80%
		{
			printf("@");
			//if (iph1->saddr == inet_addr("172.16.0.1") && iph1->id % 10 != 1) {
			if (iph1->id % 10 != 1 ) {
				printf("->");
				wifi_count++;
				//printf("To wi-Fi");
				//	printf("   |-Identification    : %u\n",iph->id);
				if (sendto(so, sendbuf, tx_len, 0,
						(struct sockaddr *) &socket_address,
						sizeof(struct sockaddr_ll)) < 0) {
					perror("send to failed");
				}
				//close(so);
				return ret;
			} else {
				lte_count++;
			}
		}
		if (lte_wifi_ratio == 2.1) // LTE : Wi-Fi :: 66% : 33%
		{
			if (iph1->saddr == inet_addr("192.168.0.82") && iph1->id % 12 == 9
					&& iph1->id % 12 == 10 && iph1->id % 12 == 11) {
				wifi_count++;
				if (sendto(so, sendbuf, tx_len, 0,
						(struct sockaddr *) &socket_address,
						sizeof(struct sockaddr_ll)) < 0) {
					perror("send to failed");
				}
				return ret;
			} else {
				lte_count++;
			}
		}

		if (lte_wifi_ratio == 5.5) // LTE : Wi-Fi :: 50% : 50%
		{
			if (iph1->saddr == inet_addr("192.168.188.36") && iph1->id % 10 != 0
					&& iph1->id % 10 != 1 && iph1->id % 10 != 2
					&& iph1->id % 10 != 3 && iph1->id % 10 != 4) {
				printf("Routing");
				wifi_count++;
				if (sendto(so, sendbuf, tx_len, 0,
						(struct sockaddr *) &socket_address,
						sizeof(struct sockaddr_ll)) < 0) {
					perror("send to failed");
				}
				return ret;
			} else {
				lte_count++;
			}
		}
		if (lte_wifi_ratio == 1.2) // LTE : Wi-Fi :: 33% : 66%
		{
			if (iph1->saddr == inet_addr("192.168.0.82") && iph1->id % 10 != 0
					&& iph1->id % 10 != 1 && iph1->id % 10 != 2) {
				wifi_count++;
				if (sendto(so, sendbuf, tx_len, 0,
						(struct sockaddr *) &socket_address,
						sizeof(struct sockaddr_ll)) < 0) {
					perror("send to failed");
				}
				return ret;
			} else {
				lte_count++;
			}
		}
		if (lte_wifi_ratio == 1.3) // LTE : Wi-Fi :: 25% : 75%
		{
			if (iph1->saddr == inet_addr("192.168.0.82") && iph1->id % 12 != 0
					&& iph1->id % 12 != 1 && iph1->id % 12 != 2) {
				wifi_count++;
				if (sendto(so, sendbuf, tx_len, 0,
						(struct sockaddr *) &socket_address,
						sizeof(struct sockaddr_ll)) < 0) {
					perror("send to failed");
				}
				return ret;
			} else {
				lte_count++;
			}
		}

		if (lte_wifi_ratio == 1.4) // LTE : Wi-Fi :: 20% : 80%
		{
			if (iph1->saddr == inet_addr("192.168.0.82") && iph1->id % 10 != 0
					&& iph1->id % 10 != 1) {
				wifi_count++;
				if (sendto(so, sendbuf, tx_len, 0,
						(struct sockaddr *) &socket_address,
						sizeof(struct sockaddr_ll)) < 0) {
					perror("send to failed");
				}
				return ret;
			} else {
				lte_count++;
			}
		}
		if (lte_wifi_ratio == 1.5) // LTE : Wi-Fi :: 10% : 50%
		{
			if (iph1->saddr == inet_addr("192.168.0.82") && iph1->id % 12 != 0
					&& iph1->id % 12 != 1 && iph1->id % 12 != 2) {
				wifi_count++;
				//printf("To wi-Fi");
				//	printf("   |-Identification    : %u\n",iph->id);
				if (sendto(so, sendbuf, tx_len, 0,
						(struct sockaddr *) &socket_address,
						sizeof(struct sockaddr_ll)) < 0) {
					perror("send to failed");
				}
				//close(so);
				return ret;
			} else {
				lte_count++;
			}
		}

*/
	}

	if (modeP == PDCP_TRANSMISSION_MODE_TRANSPARENT) {
		AssertError(rb_idP < NB_RB_MBMS_MAX, return FALSE,
				"RB id is too high (%u/%d) %u %u!\n", rb_idP, NB_RB_MBMS_MAX,
				ctxt_pP->module_id, ctxt_pP->rnti);
	} else {
		if (srb_flagP) {
			AssertError(rb_idP < 3, return FALSE,
					"RB id is too high (%u/%d) %u %u!\n", rb_idP, 3,
					ctxt_pP->module_id, ctxt_pP->rnti);
		} else {
			AssertError(rb_idP < maxDRB, return FALSE,
					"RB id is too high (%u/%d) %u %u!\n", rb_idP, maxDRB,
					ctxt_pP->module_id, ctxt_pP->rnti);
		}
	}

	key = PDCP_COLL_KEY_VALUE(ctxt_pP->module_id, ctxt_pP->rnti,
			ctxt_pP->enb_flag, rb_idP, srb_flagP);
	h_rc = hashtable_get(pdcp_coll_p, key, (void**) &pdcp_p);

	if (h_rc != HASH_TABLE_OK) {
		if (modeP != PDCP_TRANSMISSION_MODE_TRANSPARENT) {
			LOG_W(PDCP,
					PROTOCOL_CTXT_FMT" Instance is not configured for rb_id %d Ignoring SDU...\n",
					PROTOCOL_CTXT_ARGS(ctxt_pP), rb_idP);
			ctxt_pP->configured = FALSE;
			return FALSE ;
		}
	} else {
		// instance for a given RB is configured
		ctxt_pP->configured = TRUE;
	}

	if (ctxt_pP->enb_flag == ENB_FLAG_NO) {
		start_meas(&eNB_pdcp_stats[ctxt_pP->module_id].data_req);
	} else {
		start_meas(&UE_pdcp_stats[ctxt_pP->module_id].data_req);
	}

	// PDCP transparent mode for MBMS traffic

	if (modeP == PDCP_TRANSMISSION_MODE_TRANSPARENT) {
		LOG_D(PDCP, " [TM] Asking for a new mem_block of size %d\n",
				sdu_buffer_sizeP);
		pdcp_pdu_p = get_free_mem_block(sdu_buffer_sizeP, __func__);

		if (pdcp_pdu_p != NULL) {
			memcpy(&pdcp_pdu_p->data[0], sdu_buffer_pP, sdu_buffer_sizeP);
#if defined(DEBUG_PDCP_PAYLOAD)
			rlc_util_print_hex_octets(PDCP,
					(unsigned char*)&pdcp_pdu_p->data[0],
					sdu_buffer_sizeP);
#endif
			rlc_status = rlc_data_req(ctxt_pP, srb_flagP, MBMS_FLAG_YES, rb_idP,
					muiP, confirmP, sdu_buffer_sizeP, pdcp_pdu_p);
		} else {
			rlc_status = RLC_OP_STATUS_OUT_OF_RESSOURCES;
			LOG_W(PDCP,
					PROTOCOL_CTXT_FMT" PDCP_DATA_REQ SDU DROPPED, OUT OF MEMORY \n",
					PROTOCOL_CTXT_ARGS(ctxt_pP));
#if defined(STOP_ON_IP_TRAFFIC_OVERLOAD)
			AssertFatal(0, PROTOCOL_CTXT_FMT"[RB %u] PDCP_DATA_REQ SDU DROPPED, OUT OF MEMORY \n",
					PROTOCOL_CTXT_ARGS(ctxt_pP),
					rb_idP);
#endif
		}
	} else {
		// calculate the pdcp header and trailer size
		if (srb_flagP) {
			pdcp_header_len = PDCP_CONTROL_PLANE_DATA_PDU_SN_SIZE;
			pdcp_tailer_len = PDCP_CONTROL_PLANE_DATA_PDU_MAC_I_SIZE;
		} else {
			pdcp_header_len = PDCP_USER_PLANE_DATA_PDU_LONG_SN_HEADER_SIZE;
			pdcp_tailer_len = 0;
		}

		pdcp_pdu_size = sdu_buffer_sizeP + pdcp_header_len + pdcp_tailer_len;

		LOG_D(PDCP,
				PROTOCOL_PDCP_CTXT_FMT"Data request notification  pdu size %d (header%d, trailer%d)\n",
				PROTOCOL_PDCP_CTXT_ARGS(ctxt_pP,pdcp_p), pdcp_pdu_size,
				pdcp_header_len, pdcp_tailer_len);

		/*
		 * Allocate a new block for the new PDU (i.e. PDU header and SDU payload)
		 */
		pdcp_pdu_p = get_free_mem_block(pdcp_pdu_size, __func__);

		if (pdcp_pdu_p != NULL) {
			/*
			 * Create a Data PDU with header and append data
			 *
			 * Place User Plane PDCP Data PDU header first
			 */

			if (srb_flagP) { // this Control plane PDCP Data PDU
				pdcp_control_plane_data_pdu_header pdu_header;
				pdu_header.sn = pdcp_get_next_tx_seq_number(pdcp_p);
				current_sn = pdu_header.sn;
				memset(&pdu_header.mac_i[0], 0,
						PDCP_CONTROL_PLANE_DATA_PDU_MAC_I_SIZE);
				memset(&pdcp_pdu_p->data[sdu_buffer_sizeP + pdcp_header_len], 0,
						PDCP_CONTROL_PLANE_DATA_PDU_MAC_I_SIZE);

				if (pdcp_serialize_control_plane_data_pdu_with_SRB_sn_buffer(
						(unsigned char*) pdcp_pdu_p->data, &pdu_header) == FALSE) {
					LOG_E(PDCP,
							PROTOCOL_PDCP_CTXT_FMT" Cannot fill PDU buffer with relevant header fields!\n",
							PROTOCOL_PDCP_CTXT_ARGS(ctxt_pP,pdcp_p));

					if (ctxt_pP->enb_flag == ENB_FLAG_NO) {
						stop_meas(&eNB_pdcp_stats[ctxt_pP->module_id].data_req);
					} else {
						stop_meas(&UE_pdcp_stats[ctxt_pP->module_id].data_req);
					}

					VCD_SIGNAL_DUMPER_DUMP_FUNCTION_BY_NAME(VCD_SIGNAL_DUMPER_FUNCTIONS_PDCP_DATA_REQ,VCD_FUNCTION_OUT);
					return FALSE ;
				}
			} else {
				pdcp_user_plane_data_pdu_header_with_long_sn pdu_header;
				pdu_header.dc =
						(modeP == PDCP_TRANSMISSION_MODE_DATA) ?
								PDCP_DATA_PDU_BIT_SET :
								PDCP_CONTROL_PDU_BIT_SET;
				pdu_header.sn = pdcp_get_next_tx_seq_number(pdcp_p);
				current_sn = pdu_header.sn;

				if (pdcp_serialize_user_plane_data_pdu_with_long_sn_buffer(
						(unsigned char*) pdcp_pdu_p->data, &pdu_header) == FALSE) {
					LOG_E(PDCP,
							PROTOCOL_PDCP_CTXT_FMT" Cannot fill PDU buffer with relevant header fields!\n",
							PROTOCOL_PDCP_CTXT_ARGS(ctxt_pP,pdcp_p));

					if (ctxt_pP->enb_flag == ENB_FLAG_NO) {
						stop_meas(&eNB_pdcp_stats[ctxt_pP->module_id].data_req);
					} else {
						stop_meas(&UE_pdcp_stats[ctxt_pP->module_id].data_req);
					}

					VCD_SIGNAL_DUMPER_DUMP_FUNCTION_BY_NAME(VCD_SIGNAL_DUMPER_FUNCTIONS_PDCP_DATA_REQ,VCD_FUNCTION_OUT);
					return FALSE ;
				}
			}

			/*
			 * Validate incoming sequence number, there might be a problem with PDCP initialization
			 */
			if (current_sn
					> pdcp_calculate_max_seq_num_for_given_size(
							pdcp_p->seq_num_size)) {
				LOG_E(PDCP,
						PROTOCOL_PDCP_CTXT_FMT" Generated sequence number (%"PRIu16") is greater than a sequence number could ever be!\n" "There must be a problem with PDCP initialization, ignoring this PDU...\n",
						PROTOCOL_PDCP_CTXT_ARGS(ctxt_pP,pdcp_p), current_sn);

				free_mem_block(pdcp_pdu_p, __func__);

				if (ctxt_pP->enb_flag == ENB_FLAG_NO) {
					stop_meas(&eNB_pdcp_stats[ctxt_pP->module_id].data_req);
				} else {
					stop_meas(&UE_pdcp_stats[ctxt_pP->module_id].data_req);
				}

				VCD_SIGNAL_DUMPER_DUMP_FUNCTION_BY_NAME(VCD_SIGNAL_DUMPER_FUNCTIONS_PDCP_DATA_REQ,VCD_FUNCTION_OUT);
				return FALSE ;
			}

			LOG_D(PDCP, "Sequence number %d is assigned to current PDU\n",
					current_sn);

			/* Then append data... */
			memcpy(&pdcp_pdu_p->data[pdcp_header_len], sdu_buffer_pP,
					sdu_buffer_sizeP);

			//For control plane data that are not integrity protected,
			// the MAC-I field is still present and should be padded with padding bits set to 0.
			// NOTE: user-plane data are never integrity protected
			for (i = 0; i < pdcp_tailer_len; i++) {
				pdcp_pdu_p->data[pdcp_header_len + sdu_buffer_sizeP + i] = 0x00; // pdu_header.mac_i[i];
			}

#if defined(ENABLE_SECURITY)

			if ((pdcp_p->security_activated != 0) &&
					(((pdcp_p->cipheringAlgorithm) != 0) ||
							((pdcp_p->integrityProtAlgorithm) != 0))) {

				if (ctxt_pP->enb_flag == ENB_FLAG_NO) {
					start_meas(&eNB_pdcp_stats[ctxt_pP->module_id].apply_security);
				} else {
					start_meas(&UE_pdcp_stats[ctxt_pP->module_id].apply_security);
				}

				pdcp_apply_security(ctxt_pP,
						pdcp_p,
						srb_flagP,
						rb_idP % maxDRB,
						pdcp_header_len,
						current_sn,
						pdcp_pdu_p->data,
						sdu_buffer_sizeP);

				if (ctxt_pP->enb_flag == ENB_FLAG_NO) {
					stop_meas(&eNB_pdcp_stats[ctxt_pP->module_id].apply_security);
				} else {
					stop_meas(&UE_pdcp_stats[ctxt_pP->module_id].apply_security);
				}
			}

#endif

			/* Print octets of outgoing data in hexadecimal form */
			LOG_D(PDCP,
					"Following content with size %d will be sent over RLC (PDCP PDU header is the first two bytes)\n",
					pdcp_pdu_size);
			//util_print_hex_octets(PDCP, (unsigned char*)pdcp_pdu_p->data, pdcp_pdu_size);
			//util_flush_hex_octets(PDCP, (unsigned char*)pdcp_pdu->data, pdcp_pdu_size);
		} else {
			LOG_E(PDCP, "Cannot create a mem_block for a PDU!\n");

			if (ctxt_pP->enb_flag == ENB_FLAG_NO) {
				stop_meas(&eNB_pdcp_stats[ctxt_pP->module_id].data_req);
			} else {
				stop_meas(&UE_pdcp_stats[ctxt_pP->module_id].data_req);
			}

#if defined(STOP_ON_IP_TRAFFIC_OVERLOAD)
			AssertFatal(0, "[FRAME %5u][%s][PDCP][MOD %u/%u][RB %u] PDCP_DATA_REQ SDU DROPPED, OUT OF MEMORY \n",
					ctxt_pP->frame,
					(ctxt_pP->enb_flag) ? "eNB" : "UE",
							ctxt_pP->enb_module_id,
							ctxt_pP->ue_module_id,
							rb_idP);
#endif
			VCD_SIGNAL_DUMPER_DUMP_FUNCTION_BY_NAME(VCD_SIGNAL_DUMPER_FUNCTIONS_PDCP_DATA_REQ,VCD_FUNCTION_OUT);
			return FALSE ;
		}

		/*
		 * Ask sublayer to transmit data and check return value
		 * to see if RLC succeeded
		 */
#ifdef PDCP_MSG_PRINT
		int i=0;
		LOG_F(PDCP,"[MSG] PDCP DL %s PDU on rb_id %d\n", (srb_flagP)? "CONTROL" : "DATA", rb_idP);

		for (i = 0; i < pdcp_pdu_size; i++) {
			LOG_F(PDCP,"%02x ", ((uint8_t*)pdcp_pdu_p->data)[i]);
		}

		LOG_F(PDCP,"\n");
#endif
		rlc_status = rlc_data_req(ctxt_pP, srb_flagP, MBMS_FLAG_NO, rb_idP,
				muiP, confirmP, pdcp_pdu_size, pdcp_pdu_p);

	}

	switch (rlc_status) {
	case RLC_OP_STATUS_OK:
		LOG_D(PDCP, "Data sending request over RLC succeeded!\n");
		ret = TRUE;
		break;

	case RLC_OP_STATUS_BAD_PARAMETER:
		LOG_W(PDCP,
				"Data sending request over RLC failed with 'Bad Parameter' reason!\n");
		ret = FALSE;
		break;

	case RLC_OP_STATUS_INTERNAL_ERROR:
		LOG_W(PDCP,
				"Data sending request over RLC failed with 'Internal Error' reason!\n");
		ret = FALSE;
		break;

	case RLC_OP_STATUS_OUT_OF_RESSOURCES:
		LOG_W(PDCP,
				"Data sending request over RLC failed with 'Out of Resources' reason!\n");
		ret = FALSE;
		break;

	default:
		LOG_W(PDCP,
				"RLC returned an unknown status code after PDCP placed the order to send some data (Status Code:%d)\n",
				rlc_status);
		ret = FALSE;
		break;
	}

	if (ctxt_pP->enb_flag == ENB_FLAG_NO) {
		stop_meas(&eNB_pdcp_stats[ctxt_pP->module_id].data_req);
	} else {
		stop_meas(&UE_pdcp_stats[ctxt_pP->module_id].data_req);
	}

	/*
	 * Control arrives here only if rlc_data_req() returns RLC_OP_STATUS_OK
	 * so we return TRUE afterwards
	 */
	/*
	 if (rb_id>=DTCH) {
	 if (ctxt_pP->enb_flag == 1) {
	 Pdcp_stats_tx[module_id][(rb_id & RAB_OFFSET2 )>> RAB_SHIFT2][(rb_id & RAB_OFFSET)-DTCH]++;
	 Pdcp_stats_tx_bytes[module_id][(rb_id & RAB_OFFSET2 )>> RAB_SHIFT2][(rb_id & RAB_OFFSET)-DTCH] += sdu_buffer_size;
	 } else {
	 Pdcp_stats_tx[module_id][(rb_id & RAB_OFFSET2 )>> RAB_SHIFT2][(rb_id & RAB_OFFSET)-DTCH]++;
	 Pdcp_stats_tx_bytes[module_id][(rb_id & RAB_OFFSET2 )>> RAB_SHIFT2][(rb_id & RAB_OFFSET)-DTCH] += sdu_buffer_size;
	 }
	 }*/
	VCD_SIGNAL_DUMPER_DUMP_FUNCTION_BY_NAME(VCD_SIGNAL_DUMPER_FUNCTIONS_PDCP_DATA_REQ,VCD_FUNCTION_OUT);
	return ret;

}

//-----------------------------------------------------------------------------
boolean_t pdcp_data_ind(const protocol_ctxt_t* const ctxt_pP,
		const srb_flag_t srb_flagP, const MBMS_flag_t MBMS_flagP,
		const rb_id_t rb_idP, const sdu_size_t sdu_buffer_sizeP,
		mem_block_t* const sdu_buffer_pP)
//-----------------------------------------------------------------------------
{
	pdcp_t *pdcp_p = NULL;
	list_t *sdu_list_p = NULL;
	mem_block_t *new_sdu_p = NULL;
	uint8_t pdcp_header_len = 0;
	uint8_t pdcp_tailer_len = 0;
	pdcp_sn_t sequence_number = 0;
	volatile sdu_size_t payload_offset = 0;
	rb_id_t rb_id = rb_idP;
	boolean_t packet_forwarded = FALSE;
	hash_key_t key = HASHTABLE_NOT_A_KEY_VALUE;
	hashtable_rc_t h_rc;
#if defined(LINK_ENB_PDCP_TO_GTPV1U)
	MessageDef *message_p = NULL;
	uint8_t *gtpu_buffer_p = NULL;
#endif

	VCD_SIGNAL_DUMPER_DUMP_FUNCTION_BY_NAME(VCD_SIGNAL_DUMPER_FUNCTIONS_PDCP_DATA_IND,VCD_FUNCTION_IN);

#ifdef OAI_EMU

	CHECK_CTXT_ARGS(ctxt_pP);

#endif
#ifdef PDCP_MSG_PRINT
	int i=0;
	LOG_F(PDCP,"[MSG] PDCP UL %s PDU on rb_id %d\n", (srb_flagP)? "CONTROL" : "DATA", rb_idP);

	for (i = 0; i < sdu_buffer_sizeP; i++) {
		LOG_F(PDCP,"%02x ", ((uint8_t*)sdu_buffer_pP->data)[i]);
	}

	LOG_F(PDCP,"\n");
#endif

#if T_TRACER
	if (ctxt_pP->enb_flag != ENB_FLAG_NO)
		T(T_ENB_PDCP_UL, T_INT(ctxt_pP->module_id), T_INT(ctxt_pP->rnti), T_INT(rb_idP), T_INT(sdu_buffer_sizeP));
#endif

	if (MBMS_flagP) {
		AssertError(rb_idP < NB_RB_MBMS_MAX, return FALSE,
				"RB id is too high (%u/%d) %u rnti %x!\n", rb_idP,
				NB_RB_MBMS_MAX, ctxt_pP->module_id, ctxt_pP->rnti);

		if (ctxt_pP->enb_flag == ENB_FLAG_NO) {
			LOG_D(PDCP,
					"e-MBMS Data indication notification for PDCP entity from eNB %u to UE %x "
					"and radio bearer ID %d rlc sdu size %d ctxt_pP->enb_flag %d\n",
					ctxt_pP->module_id, ctxt_pP->rnti, rb_idP, sdu_buffer_sizeP,
					ctxt_pP->enb_flag);

		} else {
			LOG_D(PDCP,
					"Data indication notification for PDCP entity from UE %x to eNB %u "
					"and radio bearer ID %d rlc sdu size %d ctxt_pP->enb_flag %d\n",
					ctxt_pP->rnti, ctxt_pP->module_id, rb_idP, sdu_buffer_sizeP,
					ctxt_pP->enb_flag);
		}

	} else {
		rb_id = rb_idP % maxDRB;
		AssertError(rb_id < maxDRB, return FALSE,
				"RB id is too high (%u/%d) %u UE %x!\n", rb_id, maxDRB,
				ctxt_pP->module_id, ctxt_pP->rnti);
		AssertError(rb_id > 0, return FALSE,
				"RB id is too low (%u/%d) %u UE %x!\n", rb_id, maxDRB,
				ctxt_pP->module_id, ctxt_pP->rnti);
		key = PDCP_COLL_KEY_VALUE(ctxt_pP->module_id, ctxt_pP->rnti,
				ctxt_pP->enb_flag, rb_id, srb_flagP);
		h_rc = hashtable_get(pdcp_coll_p, key, (void**) &pdcp_p);

		if (h_rc != HASH_TABLE_OK) {
			LOG_W(PDCP,
					PROTOCOL_CTXT_FMT"Could not get PDCP instance key 0x%"PRIx64"\n",
					PROTOCOL_CTXT_ARGS(ctxt_pP), key);
			free_mem_block(sdu_buffer_pP, __func__);
			VCD_SIGNAL_DUMPER_DUMP_FUNCTION_BY_NAME(VCD_SIGNAL_DUMPER_FUNCTIONS_PDCP_DATA_IND,VCD_FUNCTION_OUT);
			return FALSE ;
		}
	}

	sdu_list_p = &pdcp_sdu_list;

	if (sdu_buffer_sizeP == 0) {
		LOG_W(PDCP, "SDU buffer size is zero! Ignoring this chunk!\n");
		return FALSE ;
	}

	if (ctxt_pP->enb_flag) {
		start_meas(&eNB_pdcp_stats[ctxt_pP->module_id].data_ind);
	} else {
		start_meas(&UE_pdcp_stats[ctxt_pP->module_id].data_ind);
	}

	/*
	 * Parse the PDU placed at the beginning of SDU to check
	 * if incoming SN is in line with RX window
	 */

	if (MBMS_flagP == 0) {
		if (srb_flagP) { //SRB1/2
			pdcp_header_len = PDCP_CONTROL_PLANE_DATA_PDU_SN_SIZE;
			pdcp_tailer_len = PDCP_CONTROL_PLANE_DATA_PDU_MAC_I_SIZE;
			sequence_number = pdcp_get_sequence_number_of_pdu_with_SRB_sn(
					(unsigned char*) sdu_buffer_pP->data);
		} else { // DRB
			pdcp_tailer_len = 0;

			if (pdcp_p->seq_num_size == PDCP_SN_7BIT) {
				pdcp_header_len = PDCP_USER_PLANE_DATA_PDU_SHORT_SN_HEADER_SIZE;
				sequence_number = pdcp_get_sequence_number_of_pdu_with_short_sn(
						(unsigned char*) sdu_buffer_pP->data);
			} else if (pdcp_p->seq_num_size == PDCP_SN_12BIT) {
				pdcp_header_len = PDCP_USER_PLANE_DATA_PDU_LONG_SN_HEADER_SIZE;
				sequence_number = pdcp_get_sequence_number_of_pdu_with_long_sn(
						(unsigned char*) sdu_buffer_pP->data);
			} else {
				//sequence_number = 4095;
				LOG_E(PDCP,
						PROTOCOL_PDCP_CTXT_FMT"wrong sequence number  (%d) for this pdcp entity \n",
						PROTOCOL_PDCP_CTXT_ARGS(ctxt_pP, pdcp_p),
						pdcp_p->seq_num_size);
			}

			//uint8_t dc = pdcp_get_dc_filed((unsigned char*)sdu_buffer_pP->data);
		}

		/*
		 * Check if incoming SDU is long enough to carry a PDU header
		 */
		if (sdu_buffer_sizeP < pdcp_header_len + pdcp_tailer_len) {
			LOG_W(PDCP,
					PROTOCOL_PDCP_CTXT_FMT"Incoming (from RLC) SDU is short of size (size:%d)! Ignoring...\n",
					PROTOCOL_PDCP_CTXT_ARGS(ctxt_pP, pdcp_p), sdu_buffer_sizeP);
			free_mem_block(sdu_buffer_pP, __func__);

			if (ctxt_pP->enb_flag) {
				stop_meas(&eNB_pdcp_stats[ctxt_pP->module_id].data_ind);
			} else {
				stop_meas(&UE_pdcp_stats[ctxt_pP->module_id].data_ind);
			}

			VCD_SIGNAL_DUMPER_DUMP_FUNCTION_BY_NAME(VCD_SIGNAL_DUMPER_FUNCTIONS_PDCP_DATA_IND,VCD_FUNCTION_OUT);
			return FALSE ;
		}

		if (pdcp_is_rx_seq_number_valid(sequence_number, pdcp_p, srb_flagP)
				== TRUE) {
#if 0
			LOG_T(PDCP, "Incoming PDU has a sequence number (%d) in accordance with RX window\n", sequence_number);
#endif
			/* if (dc == PDCP_DATA_PDU )
			 LOG_D(PDCP, "Passing piggybacked SDU to NAS driver...\n");
			 else
			 LOG_D(PDCP, "Passing piggybacked SDU to RRC ...\n");*/
		} else {
			LOG_W(PDCP,
					PROTOCOL_PDCP_CTXT_FMT"Incoming PDU has an unexpected sequence number (%d), RX window synchronisation have probably been lost!\n",
					PROTOCOL_PDCP_CTXT_ARGS(ctxt_pP, pdcp_p), sequence_number);
			/*
			 * XXX Till we implement in-sequence delivery and duplicate discarding
			 * mechanism all out-of-order packets will be delivered to RRC/IP
			 */
#if 0
			LOG_D(PDCP, "Ignoring PDU...\n");
			free_mem_block(sdu_buffer, __func__);
			return FALSE;
#else
			//LOG_W(PDCP, "Delivering out-of-order SDU to upper layer...\n");
#endif
		}

		// SRB1/2: control-plane data
		if (srb_flagP) {
#if defined(ENABLE_SECURITY)

			if (pdcp_p->security_activated == 1) {
				if (ctxt_pP->enb_flag == ENB_FLAG_NO) {
					start_meas(&eNB_pdcp_stats[ctxt_pP->module_id].validate_security);
				} else {
					start_meas(&UE_pdcp_stats[ctxt_pP->module_id].validate_security);
				}

				pdcp_validate_security(ctxt_pP,
						pdcp_p,
						srb_flagP,
						rb_idP,
						pdcp_header_len,
						sequence_number,
						sdu_buffer_pP->data,
						sdu_buffer_sizeP - pdcp_tailer_len);

				if (ctxt_pP->enb_flag == ENB_FLAG_NO) {
					stop_meas(&eNB_pdcp_stats[ctxt_pP->module_id].validate_security);
				} else {
					stop_meas(&UE_pdcp_stats[ctxt_pP->module_id].validate_security);
				}
			}

#endif
			//rrc_lite_data_ind(module_id, //Modified MW - L2 Interface
			MSC_LOG_TX_MESSAGE(
					(ctxt_pP->enb_flag == ENB_FLAG_NO)? MSC_PDCP_UE:MSC_PDCP_ENB,
							(ctxt_pP->enb_flag == ENB_FLAG_NO)? MSC_RRC_UE:MSC_RRC_ENB,
									NULL,0,
									PROTOCOL_PDCP_CTXT_FMT" DATA-IND len %u",
									PROTOCOL_PDCP_CTXT_ARGS(ctxt_pP, pdcp_p),
									sdu_buffer_sizeP - pdcp_header_len - pdcp_tailer_len);
			rrc_data_ind(ctxt_pP, rb_id,
					sdu_buffer_sizeP - pdcp_header_len - pdcp_tailer_len,
					(uint8_t*) &sdu_buffer_pP->data[pdcp_header_len]);
			free_mem_block(sdu_buffer_pP, __func__);

			// free_mem_block(new_sdu, __func__);
			if (ctxt_pP->enb_flag) {
				stop_meas(&eNB_pdcp_stats[ctxt_pP->module_id].data_ind);
			} else {
				stop_meas(&UE_pdcp_stats[ctxt_pP->module_id].data_ind);
			}

			VCD_SIGNAL_DUMPER_DUMP_FUNCTION_BY_NAME(VCD_SIGNAL_DUMPER_FUNCTIONS_PDCP_DATA_IND,VCD_FUNCTION_OUT);
			return TRUE;
		}

		/*
		 * DRBs
		 */
		payload_offset = pdcp_header_len; // PDCP_USER_PLANE_DATA_PDU_LONG_SN_HEADER_SIZE;
#if defined(ENABLE_SECURITY)

		if (pdcp_p->security_activated == 1) {
			if (ctxt_pP->enb_flag == ENB_FLAG_NO) {
				start_meas(&eNB_pdcp_stats[ctxt_pP->module_id].validate_security);
			} else {
				start_meas(&UE_pdcp_stats[ctxt_pP->module_id].validate_security);
			}

			pdcp_validate_security(
					ctxt_pP,
					pdcp_p,
					srb_flagP,
					rb_idP,
					pdcp_header_len,
					sequence_number,
					sdu_buffer_pP->data,
					sdu_buffer_sizeP - pdcp_tailer_len);

			if (ctxt_pP->enb_flag == ENB_FLAG_NO) {
				stop_meas(&eNB_pdcp_stats[ctxt_pP->module_id].validate_security);
			} else {
				stop_meas(&UE_pdcp_stats[ctxt_pP->module_id].validate_security);
			}

		}

#endif
	} else {
		payload_offset = 0;
	}

#if defined(USER_MODE) && defined(OAI_EMU)

	if (oai_emulation.info.otg_enabled == 1) {
		//unsigned int dst_instance;
		int ctime;

		if ((pdcp_p->rlc_mode == RLC_MODE_AM)&&(MBMS_flagP==0) ) {
			pdcp_p->last_submitted_pdcp_rx_sn = sequence_number;
		}

#if defined(DEBUG_PDCP_PAYLOAD)
		rlc_util_print_hex_octets(PDCP,
				(unsigned char*)&sdu_buffer_pP->data[payload_offset],
				sdu_buffer_sizeP - payload_offset);
#endif

		ctime = oai_emulation.info.time_ms; // avg current simulation time in ms : we may get the exact time through OCG?
		if (MBMS_flagP == 0) {
			LOG_D(PDCP,
					PROTOCOL_PDCP_CTXT_FMT"Check received buffer :  (dst %d)\n",
					PROTOCOL_PDCP_CTXT_ARGS(ctxt_pP, pdcp_p),
					ctxt_pP->instance);
		}
		if (otg_rx_pkt(
				ctxt_pP->instance,
				ctime,
				(const char*)(&sdu_buffer_pP->data[payload_offset]),
				sdu_buffer_sizeP - payload_offset ) == 0 ) {
			free_mem_block(sdu_buffer_pP, __func__);

			if (ctxt_pP->enb_flag) {
				stop_meas(&eNB_pdcp_stats[ctxt_pP->module_id].data_ind);
			} else {
				stop_meas(&UE_pdcp_stats[ctxt_pP->module_id].data_ind);
			}

			VCD_SIGNAL_DUMPER_DUMP_FUNCTION_BY_NAME(VCD_SIGNAL_DUMPER_FUNCTIONS_PDCP_DATA_IND,VCD_FUNCTION_OUT);
			return TRUE;
		}
	}

#else

	if (otg_enabled == 1) {
		LOG_D(OTG, "Discarding received packed\n");
		free_mem_block(sdu_buffer_pP, __func__);

		if (ctxt_pP->enb_flag) {
			stop_meas(&eNB_pdcp_stats[ctxt_pP->module_id].data_ind);
		} else {
			stop_meas(&UE_pdcp_stats[ctxt_pP->module_id].data_ind);
		}

		VCD_SIGNAL_DUMPER_DUMP_FUNCTION_BY_NAME(VCD_SIGNAL_DUMPER_FUNCTIONS_PDCP_DATA_IND,VCD_FUNCTION_OUT);
		return TRUE;
	}

#endif

	// XXX Decompression would be done at this point

	/*
	 * After checking incoming sequence number PDCP header
	 * has to be stripped off so here we copy SDU buffer starting
	 * from its second byte (skipping 0th and 1st octets, i.e.
	 * PDCP header)
	 */
#if defined(LINK_ENB_PDCP_TO_GTPV1U)

	if ((TRUE == ctxt_pP->enb_flag) && (FALSE == srb_flagP)) {
		MSC_LOG_TX_MESSAGE(
				MSC_PDCP_ENB,
				MSC_GTPU_ENB,
				NULL,0,
				"0 GTPV1U_ENB_TUNNEL_DATA_REQ  ue %x rab %u len %u",
				ctxt_pP->rnti,
				rb_id + 4,
				sdu_buffer_sizeP - payload_offset);
		//LOG_T(PDCP,"Sending to GTPV1U %d bytes\n", sdu_buffer_sizeP - payload_offset);
		gtpu_buffer_p = itti_malloc(TASK_PDCP_ENB, TASK_GTPV1_U,
				sdu_buffer_sizeP - payload_offset + GTPU_HEADER_OVERHEAD_MAX);
		AssertFatal(gtpu_buffer_p != NULL, "OUT OF MEMORY");
		memcpy(&gtpu_buffer_p[GTPU_HEADER_OVERHEAD_MAX], &sdu_buffer_pP->data[payload_offset], sdu_buffer_sizeP - payload_offset);
		message_p = itti_alloc_new_message(TASK_PDCP_ENB, GTPV1U_ENB_TUNNEL_DATA_REQ);
		AssertFatal(message_p != NULL, "OUT OF MEMORY");
		GTPV1U_ENB_TUNNEL_DATA_REQ(message_p).buffer = gtpu_buffer_p;
		GTPV1U_ENB_TUNNEL_DATA_REQ(message_p).length = sdu_buffer_sizeP - payload_offset;
		GTPV1U_ENB_TUNNEL_DATA_REQ(message_p).offset = GTPU_HEADER_OVERHEAD_MAX;
		GTPV1U_ENB_TUNNEL_DATA_REQ(message_p).rnti = ctxt_pP->rnti;
		GTPV1U_ENB_TUNNEL_DATA_REQ(message_p).rab_id = rb_id + 4;
		itti_send_msg_to_task(TASK_GTPV1_U, INSTANCE_DEFAULT, message_p);
		packet_forwarded = TRUE;
	}

#else
	packet_forwarded = FALSE;
#endif

	if (FALSE == packet_forwarded) {
		new_sdu_p = get_free_mem_block(
				sdu_buffer_sizeP - payload_offset
				+ sizeof(pdcp_data_ind_header_t), __func__);

		if (new_sdu_p) {
			if (pdcp_p->rlc_mode == RLC_MODE_AM) {
				pdcp_p->last_submitted_pdcp_rx_sn = sequence_number;
			}

			/*
			 * Prepend PDCP indication header which is going to be removed at pdcp_fifo_flush_sdus()
			 */
			memset(new_sdu_p->data, 0, sizeof(pdcp_data_ind_header_t));
			((pdcp_data_ind_header_t *) new_sdu_p->data)->data_size =
					sdu_buffer_sizeP - payload_offset;
			AssertFatal((sdu_buffer_sizeP - payload_offset >= 0),
					"invalid PDCP SDU size!");

			// Here there is no virtualization possible
			// set ((pdcp_data_ind_header_t *) new_sdu_p->data)->inst for IP layer here
			if (ctxt_pP->enb_flag == ENB_FLAG_NO) {
				((pdcp_data_ind_header_t *) new_sdu_p->data)->rb_id = rb_id;
#if defined(OAI_EMU)
				((pdcp_data_ind_header_t*) new_sdu_p->data)->inst = ctxt_pP->module_id + oai_emulation.info.nb_enb_local - oai_emulation.info.first_ue_local;
#else
#  if defined(ENABLE_USE_MME)
				/* for the UE compiled in S1 mode, we need 1 here
				 * for the UE compiled in noS1 mode, we need 0
				 * TODO: be sure of this
				 */
				((pdcp_data_ind_header_t*) new_sdu_p->data)->inst = 1;
#  endif
#endif
			} else {
				((pdcp_data_ind_header_t*) new_sdu_p->data)->rb_id = rb_id
						+ (ctxt_pP->module_id * maxDRB);
#if defined(OAI_EMU)
				((pdcp_data_ind_header_t*) new_sdu_p->data)->inst = ctxt_pP->module_id - oai_emulation.info.first_enb_local;
#endif
			}
#ifdef DEBUG_PDCP_FIFO_FLUSH_SDU
			static uint32_t pdcp_inst = 0;
			((pdcp_data_ind_header_t*) new_sdu_p->data)->inst = pdcp_inst++;
			LOG_D(PDCP, "inst=%d size=%d\n", ((pdcp_data_ind_header_t*) new_sdu_p->data)->inst, ((pdcp_data_ind_header_t *) new_sdu_p->data)->data_size);
#endif

			memcpy(&new_sdu_p->data[sizeof(pdcp_data_ind_header_t)],
					&sdu_buffer_pP->data[payload_offset],
					sdu_buffer_sizeP - payload_offset);
			list_add_tail_eurecom(new_sdu_p, sdu_list_p);

			/* Print octets of incoming data in hexadecimal form */
			LOG_D(PDCP,
					"Following content has been received from RLC (%d,%d)(PDCP header has already been removed):\n",
					sdu_buffer_sizeP - payload_offset
					+ (int )sizeof(pdcp_data_ind_header_t),
					sdu_buffer_sizeP - payload_offset);
			//util_print_hex_octets(PDCP, &new_sdu_p->data[sizeof (pdcp_data_ind_header_t)], sdu_buffer_sizeP - payload_offset);
			//util_flush_hex_octets(PDCP, &new_sdu_p->data[sizeof (pdcp_data_ind_header_t)], sdu_buffer_sizeP - payload_offset);

			/*
			 * Update PDCP statistics
			 * XXX Following two actions are identical, is there a merge error?
			 */

			/*if (ctxt_pP->enb_flag == 1) {
			 Pdcp_stats_rx[module_id][(rb_idP & RAB_OFFSET2) >> RAB_SHIFT2][(rb_idP & RAB_OFFSET) - DTCH]++;
			 Pdcp_stats_rx_bytes[module_id][(rb_idP & RAB_OFFSET2) >> RAB_SHIFT2][(rb_idP & RAB_OFFSET) - DTCH] += sdu_buffer_sizeP;
			 } else {
			 Pdcp_stats_rx[module_id][(rb_idP & RAB_OFFSET2) >> RAB_SHIFT2][(rb_idP & RAB_OFFSET) - DTCH]++;
			 Pdcp_stats_rx_bytes[module_id][(rb_idP & RAB_OFFSET2) >> RAB_SHIFT2][(rb_idP & RAB_OFFSET) - DTCH] += sdu_buffer_sizeP;
			 }*/
		}
	}

#if defined(STOP_ON_IP_TRAFFIC_OVERLOAD)
	else {
		AssertFatal(0, PROTOCOL_PDCP_CTXT_FMT" PDCP_DATA_IND SDU DROPPED, OUT OF MEMORY \n",
				PROTOCOL_PDCP_CTXT_ARGS(ctxt_pP, pdcp_p));
	}

#endif

	free_mem_block(sdu_buffer_pP, __func__);

	if (ctxt_pP->enb_flag) {
		stop_meas(&eNB_pdcp_stats[ctxt_pP->module_id].data_ind);
	} else {
		stop_meas(&UE_pdcp_stats[ctxt_pP->module_id].data_ind);
	}

	VCD_SIGNAL_DUMPER_DUMP_FUNCTION_BY_NAME(VCD_SIGNAL_DUMPER_FUNCTIONS_PDCP_DATA_IND,VCD_FUNCTION_OUT);
	return TRUE;
}

//-----------------------------------------------------------------------------
void pdcp_run(const protocol_ctxt_t* const ctxt_pP)
//-----------------------------------------------------------------------------
{
#if defined(ENABLE_ITTI)
	MessageDef *msg_p;
	const char *msg_name;
	instance_t instance;
	int result;
	protocol_ctxt_t ctxt;
#endif

	if (ctxt_pP->enb_flag) {
		start_meas(&eNB_pdcp_stats[ctxt_pP->module_id].pdcp_run);
	} else {
		start_meas(&UE_pdcp_stats[ctxt_pP->module_id].pdcp_run);
	}

	VCD_SIGNAL_DUMPER_DUMP_FUNCTION_BY_NAME(VCD_SIGNAL_DUMPER_FUNCTIONS_PDCP_RUN, VCD_FUNCTION_IN);

#if defined(ENABLE_ITTI)

	do {
		// Checks if a message has been sent to PDCP sub-task
		itti_poll_msg (ctxt_pP->enb_flag ? TASK_PDCP_ENB : TASK_PDCP_UE, &msg_p);

		if (msg_p != NULL) {
			msg_name = ITTI_MSG_NAME (msg_p);
			instance = ITTI_MSG_INSTANCE (msg_p);

			switch (ITTI_MSG_ID(msg_p)) {
			case RRC_DCCH_DATA_REQ:
				PROTOCOL_CTXT_SET_BY_MODULE_ID(
						&ctxt,
						RRC_DCCH_DATA_REQ (msg_p).module_id,
						RRC_DCCH_DATA_REQ (msg_p).enb_flag,
						RRC_DCCH_DATA_REQ (msg_p).rnti,
						RRC_DCCH_DATA_REQ (msg_p).frame,
						0,
						RRC_DCCH_DATA_REQ (msg_p).eNB_index);
				LOG_I(PDCP, PROTOCOL_CTXT_FMT"Received %s from %s: instance %d, rb_id %d, muiP %d, confirmP %d, mode %d\n",
						PROTOCOL_CTXT_ARGS(&ctxt),
						msg_name,
						ITTI_MSG_ORIGIN_NAME(msg_p),
						instance,
						RRC_DCCH_DATA_REQ (msg_p).rb_id,
						RRC_DCCH_DATA_REQ (msg_p).muip,
						RRC_DCCH_DATA_REQ (msg_p).confirmp,
						RRC_DCCH_DATA_REQ (msg_p).mode);

				result = pdcp_data_req (&ctxt,
						SRB_FLAG_YES,
						RRC_DCCH_DATA_REQ (msg_p).rb_id,
						RRC_DCCH_DATA_REQ (msg_p).muip,
						RRC_DCCH_DATA_REQ (msg_p).confirmp,
						RRC_DCCH_DATA_REQ (msg_p).sdu_size,
						RRC_DCCH_DATA_REQ (msg_p).sdu_p,
						RRC_DCCH_DATA_REQ (msg_p).mode);
				if (result != TRUE)
					LOG_E(PDCP, "PDCP data request failed!\n");

				// Message buffer has been processed, free it now.
				result = itti_free (ITTI_MSG_ORIGIN_ID(msg_p), RRC_DCCH_DATA_REQ (msg_p).sdu_p);
				AssertFatal (result == EXIT_SUCCESS, "Failed to free memory (%d)!\n", result);
				break;

			default:
				LOG_E(PDCP, "Received unexpected message %s\n", msg_name);
				break;
			}

			result = itti_free (ITTI_MSG_ORIGIN_ID(msg_p), msg_p);
			AssertFatal (result == EXIT_SUCCESS, "Failed to free memory (%d)!\n", result);
		}
	}while(msg_p != NULL);

# if 0
	{
		MessageDef *msg_resp_p;

		msg_resp_p = itti_alloc_new_message(TASK_PDCP_ENB, MESSAGE_TEST);

		itti_send_msg_to_task(TASK_RRC_ENB, 1, msg_resp_p);
	}
	{
		MessageDef *msg_resp_p;

		msg_resp_p = itti_alloc_new_message(TASK_PDCP_ENB, MESSAGE_TEST);

		itti_send_msg_to_task(TASK_ENB_APP, 2, msg_resp_p);
	}
	{
		MessageDef *msg_resp_p;

		msg_resp_p = itti_alloc_new_message(TASK_PDCP_ENB, MESSAGE_TEST);

		itti_send_msg_to_task(TASK_MAC_ENB, 3, msg_resp_p);
	}
# endif
#endif

#if defined(USER_MODE) && defined(OAI_EMU)
	pdcp_fifo_read_input_sdus_from_otg(ctxt_pP);

#endif

	// IP/NAS -> PDCP traffic : TX, read the pkt from the upper layer buffer
#if defined(LINK_ENB_PDCP_TO_GTPV1U)

	if (ctxt_pP->enb_flag == ENB_FLAG_NO)
#endif
	{
		pdcp_fifo_read_input_sdus(ctxt_pP);
	}

	// PDCP -> NAS/IP traffic: RX
	if (ctxt_pP->enb_flag) {
		start_meas(&eNB_pdcp_stats[ctxt_pP->module_id].pdcp_ip);
	}

	else {
		start_meas(&UE_pdcp_stats[ctxt_pP->module_id].pdcp_ip);
	}

	pdcp_fifo_flush_sdus(ctxt_pP);

	if (ctxt_pP->enb_flag) {
		stop_meas(&eNB_pdcp_stats[ctxt_pP->module_id].pdcp_ip);
	} else {
		stop_meas(&UE_pdcp_stats[ctxt_pP->module_id].pdcp_ip);
	}

	if (ctxt_pP->enb_flag) {
		stop_meas(&eNB_pdcp_stats[ctxt_pP->module_id].pdcp_run);
	} else {
		stop_meas(&UE_pdcp_stats[ctxt_pP->module_id].pdcp_run);
	} VCD_SIGNAL_DUMPER_DUMP_FUNCTION_BY_NAME(VCD_SIGNAL_DUMPER_FUNCTIONS_PDCP_RUN, VCD_FUNCTION_OUT);
}

//-----------------------------------------------------------------------------
boolean_t pdcp_remove_UE(const protocol_ctxt_t* const ctxt_pP)
//-----------------------------------------------------------------------------
{
	DRB_Identity_t srb_id = 0;
	DRB_Identity_t drb_id = 0;
	hash_key_t key = HASHTABLE_NOT_A_KEY_VALUE;
	hashtable_rc_t h_rc;

	// check and remove SRBs first

	for (srb_id = 0; srb_id < 2; srb_id++) {
		key = PDCP_COLL_KEY_VALUE(ctxt_pP->module_id, ctxt_pP->rnti,
				ctxt_pP->enb_flag, srb_id, SRB_FLAG_YES);
		h_rc = hashtable_remove(pdcp_coll_p, key);
	}

	for (drb_id = 0; drb_id < maxDRB; drb_id++) {
		key = PDCP_COLL_KEY_VALUE(ctxt_pP->module_id, ctxt_pP->rnti,
				ctxt_pP->enb_flag, drb_id, SRB_FLAG_NO);
		h_rc = hashtable_remove(pdcp_coll_p, key);

	}

	(void) h_rc; /* remove gcc warning "set but not used" */

	return 1;
}

//-----------------------------------------------------------------------------
boolean_t rrc_pdcp_config_asn1_req(const protocol_ctxt_t* const ctxt_pP,
		SRB_ToAddModList_t * const srb2add_list_pP,
		DRB_ToAddModList_t * const drb2add_list_pP,
		DRB_ToReleaseList_t * const drb2release_list_pP,
		const uint8_t security_modeP, uint8_t * const kRRCenc_pP,
		uint8_t * const kRRCint_pP, uint8_t * const kUPenc_pP
#if defined(Rel10) || defined(Rel14)
		,PMCH_InfoList_r9_t* const pmch_InfoList_r9_pP
#endif
		, rb_id_t * const defaultDRB)
//-----------------------------------------------------------------------------
{
	long int lc_id = 0;
	DRB_Identity_t srb_id = 0;
	long int mch_id = 0;
	rlc_mode_t rlc_type = RLC_MODE_NONE;
	DRB_Identity_t drb_id = 0;
	DRB_Identity_t *pdrb_id_p = NULL;
	uint8_t drb_sn = 12;
	uint8_t srb_sn = 5; // fixed sn for SRBs
	uint8_t drb_report = 0;
	long int cnt = 0;
	uint16_t header_compression_profile = 0;
	config_action_t action = CONFIG_ACTION_ADD;
	SRB_ToAddMod_t *srb_toaddmod_p = NULL;
	DRB_ToAddMod_t *drb_toaddmod_p = NULL;
	pdcp_t *pdcp_p = NULL;

	hash_key_t key = HASHTABLE_NOT_A_KEY_VALUE;
	hashtable_rc_t h_rc;
	hash_key_t key_defaultDRB = HASHTABLE_NOT_A_KEY_VALUE;
	hashtable_rc_t h_defaultDRB_rc;
#if defined(Rel10) || defined(Rel14)
	int i,j;
	MBMS_SessionInfoList_r9_t *mbms_SessionInfoList_r9_p = NULL;
	MBMS_SessionInfo_r9_t *MBMS_SessionInfo_p = NULL;
#endif

	LOG_T(PDCP, PROTOCOL_CTXT_FMT" %s() SRB2ADD %p DRB2ADD %p DRB2RELEASE %p\n",
			PROTOCOL_CTXT_ARGS(ctxt_pP), __FUNCTION__, srb2add_list_pP,
			drb2add_list_pP, drb2release_list_pP);

	// srb2add_list does not define pdcp config, we use rlc info to setup the pdcp dcch0 and dcch1 channels

	if (srb2add_list_pP != NULL) {
		for (cnt = 0; cnt < srb2add_list_pP->list.count; cnt++) {
			srb_id = srb2add_list_pP->list.array[cnt]->srb_Identity;
			srb_toaddmod_p = srb2add_list_pP->list.array[cnt];
			rlc_type = RLC_MODE_AM;
			lc_id = srb_id;
			key = PDCP_COLL_KEY_VALUE(ctxt_pP->module_id, ctxt_pP->rnti,
					ctxt_pP->enb_flag, srb_id, SRB_FLAG_YES);
			h_rc = hashtable_get(pdcp_coll_p, key, (void**) &pdcp_p);

			if (h_rc == HASH_TABLE_OK) {
				action = CONFIG_ACTION_MODIFY;
				LOG_D(PDCP,
						PROTOCOL_PDCP_CTXT_FMT" CONFIG_ACTION_MODIFY key 0x%"PRIx64"\n",
						PROTOCOL_PDCP_CTXT_ARGS(ctxt_pP, pdcp_p), key);
			} else {
				action = CONFIG_ACTION_ADD;
				pdcp_p = calloc(1, sizeof(pdcp_t));
				h_rc = hashtable_insert(pdcp_coll_p, key, pdcp_p);

				if (h_rc != HASH_TABLE_OK) {
					LOG_E(PDCP,
							PROTOCOL_PDCP_CTXT_FMT" CONFIG_ACTION_ADD key 0x%"PRIx64" FAILED\n",
							PROTOCOL_PDCP_CTXT_ARGS(ctxt_pP, pdcp_p), key);
					free(pdcp_p);
					return TRUE;

				} else {
					LOG_D(PDCP,
							PROTOCOL_PDCP_CTXT_FMT" CONFIG_ACTION_ADD key 0x%"PRIx64"\n",
							PROTOCOL_PDCP_CTXT_ARGS(ctxt_pP, pdcp_p), key);
				}
			}

			if (srb_toaddmod_p->rlc_Config) {
				switch (srb_toaddmod_p->rlc_Config->present) {
				case SRB_ToAddMod__rlc_Config_PR_NOTHING:
					break;

				case SRB_ToAddMod__rlc_Config_PR_explicitValue:
					switch (srb_toaddmod_p->rlc_Config->choice.explicitValue.present) {
					case RLC_Config_PR_NOTHING:
						break;

					default:
						pdcp_config_req_asn1(ctxt_pP, pdcp_p,
								SRB_FLAG_YES, rlc_type, action, lc_id, mch_id, srb_id,
								srb_sn,
								0, // drb_report
								0, // header compression
								security_modeP, kRRCenc_pP, kRRCint_pP,
								kUPenc_pP);
						break;
					}

					break;

					case SRB_ToAddMod__rlc_Config_PR_defaultValue:
						pdcp_config_req_asn1(ctxt_pP, pdcp_p,
								SRB_FLAG_YES, rlc_type, action, lc_id, mch_id, srb_id,
								srb_sn, 0, // drb_report
								0, // header compression
								security_modeP, kRRCenc_pP, kRRCint_pP, kUPenc_pP);
						// already the default values
						break;

					default:
						DevParam(srb_toaddmod_p->rlc_Config->present,
								ctxt_pP->module_id, ctxt_pP->rnti);
						break;
				}
			}
		}
	}

	// reset the action

	if (drb2add_list_pP != NULL) {
		for (cnt = 0; cnt < drb2add_list_pP->list.count; cnt++) {

			drb_toaddmod_p = drb2add_list_pP->list.array[cnt];

			drb_id = drb_toaddmod_p->drb_Identity; // + drb_id_offset;
			if (drb_toaddmod_p->logicalChannelIdentity) {
				lc_id = *(drb_toaddmod_p->logicalChannelIdentity);
			} else {
				LOG_E(PDCP,
						PROTOCOL_PDCP_CTXT_FMT" logicalChannelIdentity is missing in DRB-ToAddMod information element!\n",
						PROTOCOL_PDCP_CTXT_ARGS(ctxt_pP, pdcp_p));
				continue;
			}

			if (lc_id == 1 || lc_id == 2) {
				LOG_E(RLC,
						PROTOCOL_CTXT_FMT" logicalChannelIdentity = %ld is invalid in RRC message when adding DRB!\n",
						PROTOCOL_CTXT_ARGS(ctxt_pP), lc_id);
				continue;
			}

			DevCheck4(drb_id < maxDRB, drb_id, maxDRB, ctxt_pP->module_id,
					ctxt_pP->rnti);
			key = PDCP_COLL_KEY_VALUE(ctxt_pP->module_id, ctxt_pP->rnti,
					ctxt_pP->enb_flag, drb_id, SRB_FLAG_NO);
			h_rc = hashtable_get(pdcp_coll_p, key, (void**) &pdcp_p);

			if (h_rc == HASH_TABLE_OK) {
				action = CONFIG_ACTION_MODIFY;
				LOG_D(PDCP,
						PROTOCOL_PDCP_CTXT_FMT" CONFIG_ACTION_MODIFY key 0x%"PRIx64"\n",
						PROTOCOL_PDCP_CTXT_ARGS(ctxt_pP, pdcp_p), key);

			} else {
				action = CONFIG_ACTION_ADD;
				pdcp_p = calloc(1, sizeof(pdcp_t));
				h_rc = hashtable_insert(pdcp_coll_p, key, pdcp_p);

				// save the first configured DRB-ID as the default DRB-ID
				if ((defaultDRB != NULL) && (*defaultDRB == drb_id)) {
					key_defaultDRB = PDCP_COLL_KEY_DEFAULT_DRB_VALUE(
							ctxt_pP->module_id, ctxt_pP->rnti,
							ctxt_pP->enb_flag);
					h_defaultDRB_rc = hashtable_insert(pdcp_coll_p,
							key_defaultDRB, pdcp_p);
				} else {
					h_defaultDRB_rc = HASH_TABLE_OK; // do not trigger any error handling if this is not a default DRB
				}

				if (h_defaultDRB_rc != HASH_TABLE_OK) {
					LOG_E(PDCP,
							PROTOCOL_PDCP_CTXT_FMT" CONFIG_ACTION_ADD ADD default DRB key 0x%"PRIx64" FAILED\n",
							PROTOCOL_PDCP_CTXT_ARGS(ctxt_pP, pdcp_p),
							key_defaultDRB);
					free(pdcp_p);
					return TRUE;
				} else if (h_rc != HASH_TABLE_OK) {
					LOG_E(PDCP,
							PROTOCOL_PDCP_CTXT_FMT" CONFIG_ACTION_ADD ADD key 0x%"PRIx64" FAILED\n",
							PROTOCOL_PDCP_CTXT_ARGS(ctxt_pP, pdcp_p), key);
					free(pdcp_p);
					return TRUE;
				} else {
					LOG_D(PDCP,
							PROTOCOL_PDCP_CTXT_FMT" CONFIG_ACTION_ADD ADD key 0x%"PRIx64"\n",
							PROTOCOL_PDCP_CTXT_ARGS(ctxt_pP, pdcp_p), key);
				}
			}

			if (drb_toaddmod_p->pdcp_Config) {
				if (drb_toaddmod_p->pdcp_Config->discardTimer) {
					// set the value of the timer
				}

				if (drb_toaddmod_p->pdcp_Config->rlc_AM) {
					drb_report =
							drb_toaddmod_p->pdcp_Config->rlc_AM->statusReportRequired;
					drb_sn = PDCP_Config__rlc_UM__pdcp_SN_Size_len12bits; // default SN size
					rlc_type = RLC_MODE_AM;
				}

				if (drb_toaddmod_p->pdcp_Config->rlc_UM) {
					drb_sn = drb_toaddmod_p->pdcp_Config->rlc_UM->pdcp_SN_Size;
					rlc_type = RLC_MODE_UM;
				}

				switch (drb_toaddmod_p->pdcp_Config->headerCompression.present) {
				case PDCP_Config__headerCompression_PR_NOTHING:
				case PDCP_Config__headerCompression_PR_notUsed:
					header_compression_profile = 0x0;
					break;

				case PDCP_Config__headerCompression_PR_rohc:

					// parse the struc and get the rohc profile
					if (drb_toaddmod_p->pdcp_Config->headerCompression.choice.rohc.profiles.profile0x0001) {
						header_compression_profile = 0x0001;
					} else if (drb_toaddmod_p->pdcp_Config->headerCompression.choice.rohc.profiles.profile0x0002) {
						header_compression_profile = 0x0002;
					} else if (drb_toaddmod_p->pdcp_Config->headerCompression.choice.rohc.profiles.profile0x0003) {
						header_compression_profile = 0x0003;
					} else if (drb_toaddmod_p->pdcp_Config->headerCompression.choice.rohc.profiles.profile0x0004) {
						header_compression_profile = 0x0004;
					} else if (drb_toaddmod_p->pdcp_Config->headerCompression.choice.rohc.profiles.profile0x0006) {
						header_compression_profile = 0x0006;
					} else if (drb_toaddmod_p->pdcp_Config->headerCompression.choice.rohc.profiles.profile0x0101) {
						header_compression_profile = 0x0101;
					} else if (drb_toaddmod_p->pdcp_Config->headerCompression.choice.rohc.profiles.profile0x0102) {
						header_compression_profile = 0x0102;
					} else if (drb_toaddmod_p->pdcp_Config->headerCompression.choice.rohc.profiles.profile0x0103) {
						header_compression_profile = 0x0103;
					} else if (drb_toaddmod_p->pdcp_Config->headerCompression.choice.rohc.profiles.profile0x0104) {
						header_compression_profile = 0x0104;
					} else {
						header_compression_profile = 0x0;
						LOG_W(PDCP, "unknown header compresion profile\n");
					}

					// set the applicable profile
					break;

				default:
					LOG_W(PDCP,
							PROTOCOL_PDCP_CTXT_FMT"[RB %ld] unknown drb_toaddmod->PDCP_Config->headerCompression->present \n",
							PROTOCOL_PDCP_CTXT_ARGS(ctxt_pP,pdcp_p), drb_id);
					break;
				}

				pdcp_config_req_asn1(ctxt_pP, pdcp_p,
						SRB_FLAG_NO, rlc_type, action, lc_id, mch_id, drb_id, drb_sn,
						drb_report, header_compression_profile, security_modeP,
						kRRCenc_pP, kRRCint_pP, kUPenc_pP);
			}
		}
	}

	if (drb2release_list_pP != NULL) {
		for (cnt = 0; cnt < drb2release_list_pP->list.count; cnt++) {
			pdrb_id_p = drb2release_list_pP->list.array[cnt];
			drb_id = *pdrb_id_p;
			key = PDCP_COLL_KEY_VALUE(ctxt_pP->module_id, ctxt_pP->rnti,
					ctxt_pP->enb_flag, srb_id, SRB_FLAG_NO);
			h_rc = hashtable_get(pdcp_coll_p, key, (void**) &pdcp_p);

			if (h_rc != HASH_TABLE_OK) {
				LOG_E(PDCP, PROTOCOL_CTXT_FMT" PDCP REMOVE FAILED drb_id %ld\n",
						PROTOCOL_CTXT_ARGS(ctxt_pP), drb_id);
				continue;
			}
			lc_id = pdcp_p->lcid;

			action = CONFIG_ACTION_REMOVE;
			pdcp_config_req_asn1(ctxt_pP, pdcp_p,
					SRB_FLAG_NO, rlc_type, action, lc_id, mch_id, drb_id, 0, 0, 0,
					security_modeP, kRRCenc_pP, kRRCint_pP, kUPenc_pP);
			h_rc = hashtable_remove(pdcp_coll_p, key);

			if ((defaultDRB != NULL) && (*defaultDRB == drb_id)) {
				// default DRB being removed. nevertheless this shouldn't happen as removing default DRB is not allowed in standard
				key_defaultDRB = PDCP_COLL_KEY_DEFAULT_DRB_VALUE(
						ctxt_pP->module_id, ctxt_pP->rnti, ctxt_pP->enb_flag);
				h_defaultDRB_rc = hashtable_get(pdcp_coll_p, key_defaultDRB,
						(void**) &pdcp_p);

				if (h_defaultDRB_rc == HASH_TABLE_OK) {
					h_defaultDRB_rc = hashtable_remove(pdcp_coll_p,
							key_defaultDRB);
				} else {
					LOG_E(PDCP,
							PROTOCOL_CTXT_FMT" PDCP REMOVE FAILED default DRB\n",
							PROTOCOL_CTXT_ARGS(ctxt_pP));
				}
			} else {
				key_defaultDRB = HASH_TABLE_OK; // do not trigger any error handling if this is not a default DRB
			}
		}
	}

#if defined(Rel10) || defined(Rel14)

	if (pmch_InfoList_r9_pP != NULL) {
		for (i=0; i<pmch_InfoList_r9_pP->list.count; i++) {
			mbms_SessionInfoList_r9_p = &(pmch_InfoList_r9_pP->list.array[i]->mbms_SessionInfoList_r9);

			for (j=0; j<mbms_SessionInfoList_r9_p->list.count; j++) {
				MBMS_SessionInfo_p = mbms_SessionInfoList_r9_p->list.array[j];
				lc_id = MBMS_SessionInfo_p->sessionId_r9->buf[0];
				mch_id = MBMS_SessionInfo_p->tmgi_r9.serviceId_r9.buf[2]; //serviceId is 3-octet string

				// can set the mch_id = i
				if (ctxt_pP->enb_flag) {
					drb_id = (mch_id * maxSessionPerPMCH ) + lc_id; //+ (maxDRB + 3)*MAX_MOBILES_PER_ENB; // 1

					if (pdcp_mbms_array_eNB[ctxt_pP->module_id][mch_id][lc_id].instanciated_instance == TRUE) {
						action = CONFIG_ACTION_MBMS_MODIFY;
					} else {
						action = CONFIG_ACTION_MBMS_ADD;
					}
				} else {
					drb_id = (mch_id * maxSessionPerPMCH ) + lc_id; // + (maxDRB + 3); // 15

					if (pdcp_mbms_array_ue[ctxt_pP->module_id][mch_id][lc_id].instanciated_instance == TRUE) {
						action = CONFIG_ACTION_MBMS_MODIFY;
					} else {
						action = CONFIG_ACTION_MBMS_ADD;
					}
				}

				pdcp_config_req_asn1 (
						ctxt_pP,
						NULL,  // unused for MBMS
						SRB_FLAG_NO,
						RLC_MODE_NONE,
						action,
						lc_id,
						mch_id,
						drb_id,
						0,// unused for MBMS
						0,// unused for MBMS
						0,// unused for MBMS
						0,// unused for MBMS
						NULL,// unused for MBMS
						NULL,// unused for MBMS
						NULL);// unused for MBMS
			}
		}
	}

#endif
	return 0;
}

//-----------------------------------------------------------------------------
boolean_t pdcp_config_req_asn1(const protocol_ctxt_t* const ctxt_pP,
		pdcp_t * const pdcp_pP, const srb_flag_t srb_flagP,
		const rlc_mode_t rlc_modeP, const config_action_t actionP,
		const uint16_t lc_idP, const uint16_t mch_idP, const rb_id_t rb_idP,
		const uint8_t rb_snP, const uint8_t rb_reportP,
		const uint16_t header_compression_profileP,
		const uint8_t security_modeP, uint8_t * const kRRCenc_pP,
		uint8_t * const kRRCint_pP, uint8_t * const kUPenc_pP)
//-----------------------------------------------------------------------------
{

	switch (actionP) {
	case CONFIG_ACTION_ADD:
		DevAssert(pdcp_pP != NULL);
		if (ctxt_pP->enb_flag == ENB_FLAG_YES) {
			pdcp_pP->is_ue = FALSE;
			//pdcp_eNB_UE_instance_to_rnti[ctxtP->module_id] = ctxt_pP->rnti;
			pdcp_eNB_UE_instance_to_rnti[pdcp_eNB_UE_instance_to_rnti_index] =
					ctxt_pP->rnti;
			//pdcp_eNB_UE_instance_to_rnti_index = (pdcp_eNB_UE_instance_to_rnti_index + 1) % NUMBER_OF_UE_MAX;
		} else {
			pdcp_pP->is_ue = TRUE;
			pdcp_UE_UE_module_id_to_rnti[ctxt_pP->module_id] = ctxt_pP->rnti;
		}
		pdcp_pP->is_srb = (srb_flagP == SRB_FLAG_YES) ? TRUE : FALSE;
		pdcp_pP->lcid = lc_idP;
		pdcp_pP->rb_id = rb_idP;
		pdcp_pP->header_compression_profile = header_compression_profileP;
		pdcp_pP->status_report = rb_reportP;

		if (rb_snP == PDCP_Config__rlc_UM__pdcp_SN_Size_len12bits) {
			pdcp_pP->seq_num_size = PDCP_SN_12BIT;
		} else if (rb_snP == PDCP_Config__rlc_UM__pdcp_SN_Size_len7bits) {
			pdcp_pP->seq_num_size = PDCP_SN_7BIT;
		} else {
			pdcp_pP->seq_num_size = PDCP_SN_5BIT;
		}

		pdcp_pP->rlc_mode = rlc_modeP;
		pdcp_pP->next_pdcp_tx_sn = 0;
		pdcp_pP->next_pdcp_rx_sn = 0;
		pdcp_pP->next_pdcp_rx_sn_before_integrity = 0;
		pdcp_pP->tx_hfn = 0;
		pdcp_pP->rx_hfn = 0;
		pdcp_pP->last_submitted_pdcp_rx_sn = 4095;
		pdcp_pP->first_missing_pdu = -1;
		pdcp_pP->rx_hfn_offset = 0;

		LOG_N(PDCP,
				PROTOCOL_PDCP_CTXT_FMT" Action ADD  LCID %d (%s id %d) " "configured with SN size %d bits and RLC %s\n",
				PROTOCOL_PDCP_CTXT_ARGS(ctxt_pP,pdcp_pP), lc_idP,
				(srb_flagP == SRB_FLAG_YES) ? "SRB" : "DRB", rb_idP,
						pdcp_pP->seq_num_size,
						(rlc_modeP == RLC_MODE_AM) ? "AM" :
								(rlc_modeP == RLC_MODE_TM) ? "TM" : "UM");
		/* Setup security */
		if (security_modeP != 0xff) {
			pdcp_config_set_security(ctxt_pP, pdcp_pP, rb_idP, lc_idP,
					security_modeP, kRRCenc_pP, kRRCint_pP, kUPenc_pP);
		}
		break;

	case CONFIG_ACTION_MODIFY:
		DevAssert(pdcp_pP != NULL);
		pdcp_pP->header_compression_profile = header_compression_profileP;
		pdcp_pP->status_report = rb_reportP;
		pdcp_pP->rlc_mode = rlc_modeP;

		/* Setup security */
		if (security_modeP != 0xff) {
			pdcp_config_set_security(ctxt_pP, pdcp_pP, rb_idP, lc_idP,
					security_modeP, kRRCenc_pP, kRRCint_pP, kUPenc_pP);
		}

		if (rb_snP == PDCP_Config__rlc_UM__pdcp_SN_Size_len7bits) {
			pdcp_pP->seq_num_size = 7;
		} else if (rb_snP == PDCP_Config__rlc_UM__pdcp_SN_Size_len12bits) {
			pdcp_pP->seq_num_size = 12;
		} else {
			pdcp_pP->seq_num_size = 5;
		}

		LOG_N(PDCP,
				PROTOCOL_PDCP_CTXT_FMT" Action MODIFY LCID %d " "RB id %d reconfigured with SN size %d and RLC %s \n",
				PROTOCOL_PDCP_CTXT_ARGS(ctxt_pP,pdcp_pP), lc_idP, rb_idP,
				rb_snP,
				(rlc_modeP == RLC_MODE_AM) ? "AM" :
						(rlc_modeP == RLC_MODE_TM) ? "TM" : "UM");
		break;

	case CONFIG_ACTION_REMOVE:
		DevAssert(pdcp_pP != NULL);
		//#warning "TODO pdcp_module_id_to_rnti"
		//pdcp_module_id_to_rnti[ctxt_pP.module_id ][dst_id] = NOT_A_RNTI;
		LOG_D(PDCP,
				PROTOCOL_PDCP_CTXT_FMT" CONFIG_ACTION_REMOVE LCID %d RBID %d configured\n",
				PROTOCOL_PDCP_CTXT_ARGS(ctxt_pP,pdcp_pP), lc_idP, rb_idP);

		/* Security keys */
		if (pdcp_pP->kUPenc != NULL) {
			free(pdcp_pP->kUPenc);
		}

		if (pdcp_pP->kRRCint != NULL) {
			free(pdcp_pP->kRRCint);
		}

		if (pdcp_pP->kRRCenc != NULL) {
			free(pdcp_pP->kRRCenc);
		}

		memset(pdcp_pP, 0, sizeof(pdcp_t));
		break;
#if defined(Rel10) || defined(Rel14)

	case CONFIG_ACTION_MBMS_ADD:
	case CONFIG_ACTION_MBMS_MODIFY:
		LOG_D(PDCP," %s service_id/mch index %d, session_id/lcid %d, rbid %d configured\n",
				//PROTOCOL_PDCP_CTXT_ARGS(ctxt_pP,pdcp_pP),
				actionP == CONFIG_ACTION_MBMS_ADD ? "CONFIG_ACTION_MBMS_ADD" : "CONFIG_ACTION_MBMS_MODIFY",
						mch_idP,
						lc_idP,
						rb_idP);

		if (ctxt_pP->enb_flag == ENB_FLAG_YES) {
			pdcp_mbms_array_eNB[ctxt_pP->module_id][mch_idP][lc_idP].instanciated_instance = TRUE;
			pdcp_mbms_array_eNB[ctxt_pP->module_id][mch_idP][lc_idP].rb_id = rb_idP;
		} else {
			pdcp_mbms_array_ue[ctxt_pP->module_id][mch_idP][lc_idP].instanciated_instance = TRUE;
			pdcp_mbms_array_ue[ctxt_pP->module_id][mch_idP][lc_idP].rb_id = rb_idP;
		}

		break;
#endif

	case CONFIG_ACTION_SET_SECURITY_MODE:
		pdcp_config_set_security(ctxt_pP, pdcp_pP, rb_idP, lc_idP,
				security_modeP, kRRCenc_pP, kRRCint_pP, kUPenc_pP);
		break;

	default:
		DevParam(actionP, ctxt_pP->module_id, ctxt_pP->rnti);
		break;
	}

	return 0;
}

//-----------------------------------------------------------------------------
void pdcp_config_set_security(const protocol_ctxt_t* const ctxt_pP,
		pdcp_t * const pdcp_pP, const rb_id_t rb_idP, const uint16_t lc_idP,
		const uint8_t security_modeP, uint8_t * const kRRCenc,
		uint8_t * const kRRCint, uint8_t * const kUPenc)
//-----------------------------------------------------------------------------
{
	DevAssert(pdcp_pP != NULL);

	if ((security_modeP >= 0) && (security_modeP <= 0x77)) {
		pdcp_pP->cipheringAlgorithm = security_modeP & 0x0f;
		pdcp_pP->integrityProtAlgorithm = (security_modeP >> 4) & 0xf;

		LOG_D(PDCP,
				PROTOCOL_PDCP_CTXT_FMT" CONFIG_ACTION_SET_SECURITY_MODE: cipheringAlgorithm %d integrityProtAlgorithm %d\n",
				PROTOCOL_PDCP_CTXT_ARGS(ctxt_pP,pdcp_pP),
				pdcp_pP->cipheringAlgorithm, pdcp_pP->integrityProtAlgorithm);

		pdcp_pP->kRRCenc = kRRCenc;
		pdcp_pP->kRRCint = kRRCint;
		pdcp_pP->kUPenc = kUPenc;

		/* Activate security */
		pdcp_pP->security_activated = 1;
		MSC_LOG_EVENT(
				(ctxt_pP->enb_flag == ENB_FLAG_YES) ? MSC_PDCP_ENB:MSC_PDCP_UE,
						"0 Set security ciph %X integ %x UE %"PRIx16" ",
						pdcp_pP->cipheringAlgorithm,
						pdcp_pP->integrityProtAlgorithm,
						ctxt_pP->rnti);
	} else {
		MSC_LOG_EVENT(
				(ctxt_pP->enb_flag == ENB_FLAG_YES) ? MSC_PDCP_ENB:MSC_PDCP_UE,
						"0 Set security failed UE %"PRIx16" ",
						ctxt_pP->rnti);
		LOG_E(PDCP, PROTOCOL_PDCP_CTXT_FMT"  bad security mode %d",
				PROTOCOL_PDCP_CTXT_ARGS(ctxt_pP,pdcp_pP), security_modeP);
	}
}

//-----------------------------------------------------------------------------
void rrc_pdcp_config_req(const protocol_ctxt_t* const ctxt_pP,
		const srb_flag_t srb_flagP, const uint32_t actionP,
		const rb_id_t rb_idP, const uint8_t security_modeP)
//-----------------------------------------------------------------------------
{
	pdcp_t *pdcp_p = NULL;
	hash_key_t key = PDCP_COLL_KEY_VALUE(ctxt_pP->module_id, ctxt_pP->rnti,
			ctxt_pP->enb_flag, rb_idP, srb_flagP);
	hashtable_rc_t h_rc;
	h_rc = hashtable_get(pdcp_coll_p, key, (void**) &pdcp_p);

	if (h_rc == HASH_TABLE_OK) {

		/*
		 * Initialize sequence number state variables of relevant PDCP entity
		 */
		switch (actionP) {
		case CONFIG_ACTION_ADD:
			pdcp_p->is_srb = srb_flagP;
			pdcp_p->rb_id = rb_idP;

			if (ctxt_pP->enb_flag == ENB_FLAG_NO) {
				pdcp_p->is_ue = TRUE;
			} else {
				pdcp_p->is_ue = FALSE;
			}

			pdcp_p->next_pdcp_tx_sn = 0;
			pdcp_p->next_pdcp_rx_sn = 0;
			pdcp_p->tx_hfn = 0;
			pdcp_p->rx_hfn = 0;
			/* SN of the last PDCP SDU delivered to upper layers */
			pdcp_p->last_submitted_pdcp_rx_sn = 4095;

			if (rb_idP < DTCH) { // SRB
				pdcp_p->seq_num_size = 5;
			} else { // DRB
				pdcp_p->seq_num_size = 12;
			}

			pdcp_p->first_missing_pdu = -1;
			LOG_D(PDCP,
					PROTOCOL_PDCP_CTXT_FMT" Config request : Action ADD:  radio bearer id %d (already added) configured\n",
					PROTOCOL_PDCP_CTXT_ARGS(ctxt_pP,pdcp_p), rb_idP);
			break;

		case CONFIG_ACTION_MODIFY:
			break;

		case CONFIG_ACTION_REMOVE:
			LOG_D(PDCP,
					PROTOCOL_PDCP_CTXT_FMT" CONFIG_ACTION_REMOVE: radio bearer id %d configured\n",
					PROTOCOL_PDCP_CTXT_ARGS(ctxt_pP,pdcp_p), rb_idP);
			pdcp_p->next_pdcp_tx_sn = 0;
			pdcp_p->next_pdcp_rx_sn = 0;
			pdcp_p->tx_hfn = 0;
			pdcp_p->rx_hfn = 0;
			pdcp_p->last_submitted_pdcp_rx_sn = 4095;
			pdcp_p->seq_num_size = 0;
			pdcp_p->first_missing_pdu = -1;
			pdcp_p->security_activated = 0;
			h_rc = hashtable_remove(pdcp_coll_p, key);

			break;

		case CONFIG_ACTION_SET_SECURITY_MODE:
			if ((security_modeP >= 0) && (security_modeP <= 0x77)) {
				pdcp_p->cipheringAlgorithm = security_modeP & 0x0f;
				pdcp_p->integrityProtAlgorithm = (security_modeP >> 4) & 0xf;
				LOG_D(PDCP,
						PROTOCOL_PDCP_CTXT_FMT" CONFIG_ACTION_SET_SECURITY_MODE: cipheringAlgorithm %d integrityProtAlgorithm %d\n",
						PROTOCOL_PDCP_CTXT_ARGS(ctxt_pP,pdcp_p),
						pdcp_p->cipheringAlgorithm,
						pdcp_p->integrityProtAlgorithm);
			} else {
				LOG_W(PDCP, PROTOCOL_PDCP_CTXT_FMT" bad security mode %d",
						PROTOCOL_PDCP_CTXT_ARGS(ctxt_pP,pdcp_p),
						security_modeP);
			}

			break;

		default:
			DevParam(actionP, ctxt_pP->module_id, ctxt_pP->rnti);
			break;
		}
	} else {
		switch (actionP) {
		case CONFIG_ACTION_ADD:
			pdcp_p = calloc(1, sizeof(pdcp_t));
			h_rc = hashtable_insert(pdcp_coll_p, key, pdcp_p);

			if (h_rc != HASH_TABLE_OK) {
				LOG_E(PDCP, PROTOCOL_PDCP_CTXT_FMT" PDCP ADD FAILED\n",
						PROTOCOL_PDCP_CTXT_ARGS(ctxt_pP, pdcp_p));
				free(pdcp_p);

			} else {
				pdcp_p->is_srb = srb_flagP;
				pdcp_p->rb_id = rb_idP;

				if (ctxt_pP->enb_flag == ENB_FLAG_NO) {
					pdcp_p->is_ue = TRUE;

				} else {
					pdcp_p->is_ue = FALSE;
				}

				pdcp_p->next_pdcp_tx_sn = 0;
				pdcp_p->next_pdcp_rx_sn = 0;
				pdcp_p->tx_hfn = 0;
				pdcp_p->rx_hfn = 0;
				/* SN of the last PDCP SDU delivered to upper layers */
				pdcp_p->last_submitted_pdcp_rx_sn = 4095;

				if (rb_idP < DTCH) { // SRB
					pdcp_p->seq_num_size = 5;

				} else { // DRB
					pdcp_p->seq_num_size = 12;
				}

				pdcp_p->first_missing_pdu = -1;
				LOG_D(PDCP,
						PROTOCOL_PDCP_CTXT_FMT" Inserting PDCP instance in collection key 0x%"PRIx64"\n",
						PROTOCOL_PDCP_CTXT_ARGS(ctxt_pP,pdcp_p), key);
				LOG_D(PDCP,
						PROTOCOL_PDCP_CTXT_FMT" Config request : Action ADD:  radio bearer id %d configured\n",
						PROTOCOL_PDCP_CTXT_ARGS(ctxt_pP,pdcp_p), rb_idP);
			}

			break;

		case CONFIG_ACTION_REMOVE:
			LOG_D(PDCP,
					PROTOCOL_CTXT_FMT" CONFIG_REQ PDCP CONFIG_ACTION_REMOVE PDCP instance not found\n",
					PROTOCOL_CTXT_ARGS(ctxt_pP));
			break;

		default:
			LOG_E(PDCP, PROTOCOL_CTXT_FMT" CONFIG_REQ PDCP NOT FOUND\n",
					PROTOCOL_CTXT_ARGS(ctxt_pP));
		}
	}
}

//-----------------------------------------------------------------------------
// TODO PDCP module initialization code might be removed
int pdcp_module_init(void)
//-----------------------------------------------------------------------------
{
#ifdef PDCP_USE_RT_FIFO
	int ret;

	ret=rtf_create(PDCP2NW_DRIVER_FIFO,32768);

	if (ret < 0) {
		LOG_E(PDCP, "Cannot create PDCP2NW_DRIVER_FIFO fifo %d (ERROR %d)\n", PDCP2NW_DRIVER_FIFO, ret);
		return -1;
	} else {
		LOG_D(PDCP, "Created PDCP2NAS fifo %d\n", PDCP2NW_DRIVER_FIFO);
		rtf_reset(PDCP2NW_DRIVER_FIFO);
	}

	ret=rtf_create(NW_DRIVER2PDCP_FIFO,32768);

	if (ret < 0) {
		LOG_E(PDCP, "Cannot create NW_DRIVER2PDCP_FIFO fifo %d (ERROR %d)\n", NW_DRIVER2PDCP_FIFO, ret);

		return -1;
	} else {
		LOG_D(PDCP, "Created NW_DRIVER2PDCP_FIFO fifo %d\n", NW_DRIVER2PDCP_FIFO);
		rtf_reset(NW_DRIVER2PDCP_FIFO);
	}

	pdcp_2_nas_irq = 0;
	pdcp_input_sdu_remaining_size_to_read=0;
	pdcp_input_sdu_size_read=0;
#endif

	return 0;
}

//-----------------------------------------------------------------------------
void pdcp_free(void* pdcp_pP)
//-----------------------------------------------------------------------------
{
	pdcp_t* pdcp_p = (pdcp_t*) pdcp_pP;

	if (pdcp_p != NULL) {
		if (pdcp_p->kUPenc != NULL) {
			free(pdcp_p->kUPenc);
		}

		if (pdcp_p->kRRCint != NULL) {
			free(pdcp_p->kRRCint);
		}

		if (pdcp_p->kRRCenc != NULL) {
			free(pdcp_p->kRRCenc);
		}

		memset(pdcp_pP, 0, sizeof(pdcp_t));
		free(pdcp_pP);
	}
}

//-----------------------------------------------------------------------------
void pdcp_module_cleanup(void)
//-----------------------------------------------------------------------------
{
#ifdef PDCP_USE_RT_FIFO
	rtf_destroy(NW_DRIVER2PDCP_FIFO);
	rtf_destroy(PDCP2NW_DRIVER_FIFO);
#endif
}

//-----------------------------------------------------------------------------
void pdcp_layer_init(void)
//-----------------------------------------------------------------------------
{

	module_id_t instance;
#if defined(Rel10) || defined(Rel14)
	mbms_session_id_t session_id;
	mbms_service_id_t service_id;
#endif
	/*
	 * Initialize SDU list
	 */
	list_init(&pdcp_sdu_list, NULL);
	pdcp_coll_p = hashtable_create((maxDRB + 2) * 16, NULL, pdcp_free);
	AssertFatal(pdcp_coll_p != NULL,
			"UNRECOVERABLE error, PDCP hashtable_create failed");

	for (instance = 0; instance < NUMBER_OF_UE_MAX; instance++) {
#if defined(Rel10) || defined(Rel14)

		for (service_id = 0; service_id < maxServiceCount; service_id++) {
			for (session_id = 0; session_id < maxSessionPerPMCH; session_id++) {
				memset(&pdcp_mbms_array_ue[instance][service_id][session_id], 0, sizeof(pdcp_mbms_t));
			}
		}
#endif
		pdcp_eNB_UE_instance_to_rnti[instance] = NOT_A_RNTI;
	}
	pdcp_eNB_UE_instance_to_rnti_index = 0;

	for (instance = 0; instance < NUMBER_OF_eNB_MAX; instance++) {
#if defined(Rel10) || defined(Rel14)

		for (service_id = 0; service_id < maxServiceCount; service_id++) {
			for (session_id = 0; session_id < maxSessionPerPMCH; session_id++) {
				memset(&pdcp_mbms_array_eNB[instance][service_id][session_id], 0, sizeof(pdcp_mbms_t));
			}
		}

#endif
	}
	lte_enabled=0;
	LOG_I(PDCP, "PDCP layer has been initialized\n");
	//Modified Thomas
	so = socket (AF_PACKET, SOCK_RAW, IPPROTO_RAW);
	if(so == -1)
	{
		//socket creation failed, may be because of non-root privileges
		perror("Failed to create raw socket");
		exit(1);
	}

	packet = malloc(sizeof(struct iphdr) + sizeof(struct icmphdr));
	lte_probe_packet=malloc(sizeof(struct iphdr) + sizeof(struct icmphdr));
	probe_buffer = malloc(sizeof(struct iphdr) + sizeof(struct icmphdr));
	copy_ctxt_pP = malloc(sizeof(protocol_ctxt_t));

	char ifName[100];
	strcpy(ifName, "eth1");
	memset(&if_idx, 0, sizeof(struct ifreq));
	strncpy(if_idx.ifr_name, ifName, IFNAMSIZ-1);
	if (ioctl(so, SIOCGIFINDEX, &if_idx) < 0)
		perror("SIOCGIFINDEX");
	/* Get the MAC address of the interface to send on */
	memset(&if_mac, 0, sizeof(struct ifreq));
	strncpy(if_mac.ifr_name, ifName, IFNAMSIZ-1);
	if (ioctl(so, SIOCGIFHWADDR, &if_mac) < 0)
		perror("SIOCGIFHWADDR");

	pdcp_output_sdu_bytes_to_write = 0;
	pdcp_output_header_bytes_to_write = 0;
	pdcp_input_sdu_remaining_size_to_read = 0;
	RTT_lte=0;
	RTT_wifi=0;
	wifi_count=0;
	local_tcp_counter=0;
	lte_count=0;
	memset(Pdcp_stats_tx, 0, sizeof(Pdcp_stats_tx));
	memset(Pdcp_stats_tx_bytes, 0, sizeof(Pdcp_stats_tx_bytes));
	memset(Pdcp_stats_tx_bytes_last, 0, sizeof(Pdcp_stats_tx_bytes_last));
	memset(Pdcp_stats_tx_rate, 0, sizeof(Pdcp_stats_tx_rate));

	memset(Pdcp_stats_rx, 0, sizeof(Pdcp_stats_rx));
	memset(Pdcp_stats_rx_bytes, 0, sizeof(Pdcp_stats_rx_bytes));
	memset(Pdcp_stats_rx_bytes_last, 0, sizeof(Pdcp_stats_rx_bytes_last));
	memset(Pdcp_stats_rx_rate, 0, sizeof(Pdcp_stats_rx_rate));

	pthread_attr_t     attr;
	struct sched_param sched_param;
	sched_param.sched_priority = 11;
	pthread_attr_setschedpolicy(&attr, SCHED_RR);
	pthread_attr_setschedparam(&attr, &sched_param);

	//int c=;
	//printf("value of c= %d ",c);
	if ( pthread_create(&probe_send, 0, &send_probing_packets, NULL)!= 0) {
		LOG_E(PDCP, "[LWIP PDCP] Failed to create new thread for probe send (%d:%s)\n",
				errno, strerror(errno));
		printk("Failed to create thread");
		exit(EXIT_FAILURE);
	}
	pthread_setname_np( probe_send, "probe send");

	pthread_attr_t     attr1;
	struct sched_param sched_param1;
	sched_param1.sched_priority = 11;
	pthread_attr_setschedpolicy(&attr1, SCHED_RR);
	pthread_attr_setschedparam(&attr1, &sched_param1);
	printk("Init Probe Listener");
	if (pthread_create(&probe_listener, 0, &receive_probing_packets, NULL) != 0) {
		LOG_E(PDCP, "[LWIP PDCP] Failed to create new thread for probe listener (%d:%s)\n",
				errno, strerror(errno));
		printk("Failed to create thread");
		exit(EXIT_FAILURE);
	}
	pthread_setname_np( probe_listener, "probe listener");



}

//-----------------------------------------------------------------------------
void pdcp_layer_cleanup(void)
//-----------------------------------------------------------------------------
{
	list_free(&pdcp_sdu_list);
	hashtable_destroy(pdcp_coll_p);
	pthread_cancel(probe_listener);
	pthread_cancel(probe_send);
}

#ifdef PDCP_USE_RT_FIFO
EXPORT_SYMBOL(pdcp_2_nas_irq);
#endif //PDCP_USE_RT_FIFO
