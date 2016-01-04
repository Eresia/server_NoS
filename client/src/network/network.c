#include "network.h"
#include "../launch/launch_client.h"

client_network init_client_network(char* serv_addr, int serv_port){
	client_network c;
	printf("port : %d\n", serv_port);

	c = malloc(sizeof(struct client_network_struct));
	c->write_buffer_busy =  malloc(sizeof(sem_t));
	c->write_buffer_empty =  malloc(sizeof(sem_t));
	c->write_buffer_full =  malloc(sizeof(sem_t));
	c->read_buffer_busy=  malloc(sizeof(sem_t));
	c->read_buffer_empty =  malloc(sizeof(sem_t));
	c->read_buffer_full =  malloc(sizeof(sem_t));

	if ((c->local_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1) {
		fprintf(stderr, "[reseau]Impossible d'ouvrir le socket local \n");
		pthread_exit(NULL);
	}
	#ifdef DEBUG
	printf("[reseau]connexion au port %i\n", c->local_socket);
	#endif

	c->serv_struct = malloc(sizeof(SOCKADDR_IN));
	c->serv_struct->sin_family = AF_INET;
	c->serv_struct->sin_port = htons(serv_port);
	IN_ADDR tmp_addr;
	if (inet_aton(serv_addr, &tmp_addr) == 0) {
		fprintf(stderr, "[reseau]Parsing de l'addresse serveur incorecte \n");
		exit(1);
	}else{
		strcpy(c->serv_ip_addr , serv_addr);
		c->serv_struct->sin_addr = tmp_addr;
		c->serv_struct->sin_family = AF_INET;
		c->serv_struct->sin_port = htons(serv_port);
		c->serv_port = serv_port;
	}
	// on ecrase ses buffers et on initialise les buffers
	sem_init(c->read_buffer_empty ,1,1);
	sem_init(c->write_buffer_full ,0,1);
	sem_init(c->write_buffer_busy ,0,1);
	sem_init(c->read_buffer_empty ,1,1);
	sem_init(c->read_buffer_full ,0,1);
	sem_init(c->read_buffer_busy ,0,1);

	c->BUFF_IN = calloc(SIZE_MESSAGE_MAX+1, sizeof(char));
	c->BUFF_OUT = calloc(SIZE_MESSAGE_MAX+1, sizeof(char));
	// on signale le client comme actif
	return c;

}
void* client_beat(void* client_struct){
	client_network cn = (client_network)  client_struct;
	#ifdef DEBUG
	printf("[reseau]demarrage du thread beat vers %s \n ",cn->serv_ip_addr);
	#endif
	while(*cn->isClosed == false){
		#ifdef DEBUG
		printf("[reseau]beat client vers %s  \n",cn->serv_ip_addr);
		#endif
		send_to_server(cn,"hello",strlen("hello"));
		sleep(1);
	}
	pthread_exit(NULL);
}

void* bufferize_from_server(void* client_struct){

	client_network cn = (client_network)  client_struct;
	printf("Point : %p, %d\n", cn, *cn->isClosed);
	#ifdef DEBUG
	printf("[reseau]demarrage du thread ecoute de %s \n ",cn->serv_ip_addr);
	#endif
	int n;
	while(*cn->isClosed == false){
		sem_wait(cn->read_buffer_full);
		sem_wait(cn->read_buffer_busy);
		if((n = recv(cn->local_socket, cn->BUFF_IN, SIZE_MESSAGE_MAX, 0)) < 0)
		{
			fprintf(stderr, "[reseau]erreur rcv from serveur \n");
			*cn->isClosed = true;
			pthread_exit(NULL);
		}
		#ifdef DEBUG
		printf("[reseau]reception de %i octet de %s \n" , (int) strlen(cn->BUFF_IN) , cn->serv_ip_addr);
		#endif
		sem_post(cn->read_buffer_busy);
		sem_post(cn->read_buffer_full);
		usleep(25000);
	}
	pthread_exit(NULL);
}

void* bufferize_to_server(void* client_struct){
	client_network cn = (client_network)  client_struct;
	printf("Point : %p, %d\n", cn, *cn->isClosed);
	#ifdef DEBUG
	printf("[reseau]demarrage du thread envoie de %s \n ",cn->serv_ip_addr);
	#endif
	while(*cn->isClosed == false){
		sem_wait(cn->write_buffer_full);
		sem_wait(cn->write_buffer_busy);
		if (sendto(cn->local_socket, cn->BUFF_OUT, strlen(cn->BUFF_OUT) , 0 ,  (SOCKADDR*)cn->serv_struct, sizeof(cn->serv_struct))==-1)
		{
			perror("erreur de send");
			fprintf(stderr, "erreur send from serveur %s \n", cn->BUFF_OUT);
			*cn->isClosed = true;
			pthread_exit(NULL);
		}
		#ifdef DEBUG
		printf("[reseau]envoie de  %i octet du buffer vers %s \n" , (int) strlen(cn->BUFF_OUT)  , cn->serv_ip_addr);
		#endif

		sem_post(cn->write_buffer_busy);
		sem_post(cn->write_buffer_empty);
		usleep(25000);
	}
	pthread_exit(NULL);
}

int send_to_server(client_network client_struct, void* data , int size){
	char* data_to_send = (char*) data;

	client_network cn = (client_network)  client_struct;

	printf("a\n");
	sem_wait(cn->write_buffer_empty);
	sem_wait(cn->write_buffer_busy);
	printf("b\n");
	strncpy(cn->BUFF_OUT,data_to_send,size);
	#ifdef DEBUG
	printf("[reseau]mise en buffer d'envoie de %i octets vers %s \n" , size , cn->serv_ip_addr);
	#endif
	sem_post(cn->write_buffer_busy);
	sem_post(cn->write_buffer_full);
	return size;
}
int receive_from_server(client_network client_struct, void* dest){

	char* data_to_send = (char*) dest;

	client_network cn = (client_network)  client_struct;
	int size = strlen(cn->BUFF_IN);
	sem_wait(cn->write_buffer_empty);
	sem_wait(cn->write_buffer_busy);
	strncpy(data_to_send,cn->BUFF_OUT,size);
	sem_post(cn->write_buffer_busy);
	sem_post(cn->write_buffer_full);
	#ifdef DEBUG
	printf("[reseau]recuperation en buffer %i octet de %s \n" , size , cn->serv_ip_addr);
	#endif
	return size;
}

void* processingReadData(void* client_struct){
	Client* c = (Client*) client_struct;
	client_network cn = c->cn;
	#ifdef DEBUG
	printf("[reseau]demarrage du thread traitement de donnes de %s \n ",cn->serv_ip_addr);
	#endif
	char* buff = calloc(SIZE_MESSAGE_MAX + 1, sizeof(char));
	while(*cn->isClosed == false){
		if(receive_from_server(cn, buff) != 0){
			parse_Protocole(c->process, buff);
		}
	}

	pthread_exit(NULL);
}

void* launch_network(void* client_struct){
	Client* c = (Client*) client_struct;
	client_network cn = c->cn;
	printf("Point : %p, %d\n", cn, *cn->isClosed);
	char* buff = calloc(4+strlen(c->process->player->name)+1, sizeof(char));
	sprintf(buff, "new,%s", c->process->player->name);
	pthread_t beatThread, writerThread, receverThread, readThread;
	//pthread_create(&beatThread, NULL , client_beat, cn);
	pthread_create(&writerThread, NULL , bufferize_to_server, cn);
	pthread_create(&receverThread, NULL , bufferize_from_server, cn);
	pthread_create(&readThread, NULL , processingReadData, c);
	#ifdef DEBUG
	printf("[reseau]lancement des threads\n");
	#endif

	//send_to_server(cn, buff, strlen(buff));

	//pthread_join(beatThread, NULL);
	pthread_join(writerThread, NULL);
	pthread_join(receverThread, NULL);
	pthread_join(readThread, NULL);
	free(cn);
	pthread_exit(NULL);
}
