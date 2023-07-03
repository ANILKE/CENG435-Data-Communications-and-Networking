#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <poll.h>
#include <math.h>
#include <stdbool.h>

#define MAXLEN 1024
#define WINDOW_SIZE 16
#define MESDIV MAXLEN/16
#define TIMEOUT 440000


struct UserDatagramProtocol
{
    char app_data[17];
    int checksum;
    int sequence_no;
    struct timeval arr_time;
    bool ACK;
};


struct Window
{
    struct UserDatagramProtocol pakages[MESDIV];
    int send_base;
    int next_seq_no;
    int size_N;
    int slot;

};


int checksum(struct UserDatagramProtocol *pack)
{

    int result = pack->sequence_no;

    for (int i = 0; i < 16; i++)
        result += pack->app_data[i];

    return result;
}


void InitWindow(struct Window *w)
{
    memset(w, 0, sizeof(*w));
    w->send_base = 0;
    w->next_seq_no = 0;
    w->size_N = WINDOW_SIZE;
    w->slot = WINDOW_SIZE;
}


struct UserDatagramProtocol* init_pack(int seq_no, char *divided_message)
{
    int datagram_size = sizeof(struct UserDatagramProtocol);
    struct UserDatagramProtocol* pack;
    pack = (struct UserDatagramProtocol*) malloc(datagram_size);
    memset(pack, 0, datagram_size);

    pack->sequence_no = seq_no;
    pack->ACK = false;
    strcpy(pack->app_data, divided_message);
    pack->checksum = checksum(pack);
    gettimeofday(&(pack->arr_time), NULL);
    return pack;
}


int Initilazer(struct sockaddr_in* address, int PortNum)
{
    address -> sin_family = AF_INET;
    address -> sin_addr.s_addr = INADDR_ANY;
    address -> sin_port = htons(PortNum);
}

char **divide_message(char* message_buff)
{

    char **divided_message = (char **)calloc(MESDIV , sizeof(char*));

    for (int i = 0 ; i < MESDIV; i++)
        divided_message[i] = (char*)calloc(17, sizeof(char));

    int len = strlen(message_buff);

    int index_1 = 0;
    int index_2 = 0;

    for (int i = 0; i < len; i++)
    {
        if (i % 16 == 0 && i != 0)
        {
            divided_message[index_1][index_2] = '\0';
            index_1++;
            index_2 = 0;
        }

        divided_message[index_1][index_2++] = message_buff[i];
    }

    return divided_message;
}


void go_back_n(struct sockaddr_in* server_add, int socket_no, char* message_buff){
    struct Window w;
    InitWindow(&w);

    int num_events = 0;
    int waiting_messages = 0;

    struct pollfd fd[2];
    fd[0].fd = socket_no;
    fd[0].events = POLLIN;
    fd[1].fd = STDIN_FILENO;
    fd[1].events = POLLIN;

    char **simultaneous_msg_bfr = (char **)calloc(32, sizeof(char*));
    for (int i = 0 ; i < 32; i++){
        simultaneous_msg_bfr[i] = (char*)calloc(1024, sizeof(char));
    }

    int sended_packs = 0;
    int empty_new_lines = 0;
    int send_size = 0;
    int sended_index = 0;
    int msg_buff_index = 0;
    int msg_buff_size = 0;
    int pack_seq_no= 0;
    char** divided_message;
    char ** chunks;
    struct UserDatagramProtocol* old_received_pack;

    while(1){
        if(sended_index == send_size && msg_buff_index == msg_buff_size){
            InitWindow(&w);
            msg_buff_size = 0;
            msg_buff_index = 0;
            send_size = 0;
            sended_index = 0;
        }
        else if(sended_index == send_size && msg_buff_index < msg_buff_size){
            InitWindow(&w);
            message_buff = simultaneous_msg_bfr[msg_buff_index++];
            divided_message = divide_message(message_buff);
            if((strlen(message_buff)) % 16){
                send_size = strlen(message_buff) / 16;
            }
            else{
                send_size = ((strlen(message_buff)) / 16) + 1;
            }
            sended_index = 0;
        }
        if(sended_index < send_size){
            if(w.slot){
                sended_index++;
                struct UserDatagramProtocol* pack;
                pack = init_pack(w.next_seq_no, divided_message[pack_seq_no]);
                int pack_size = sizeof(*pack);
                sendto(socket_no, (const struct UserDatagramProtocol*) pack, pack_size, MSG_CONFIRM, (const struct sockaddr *)server_add, sizeof(server_add));
                if (strcmp(pack->app_data, "\n") == 0 )
                {
                    empty_new_lines++;
                }
                else{
                    empty_new_lines = 0;
                }
                if(empty_new_lines == 3){
                    break;
                }
                w.pakages[w.next_seq_no] = *pack;
                w.next_seq_no = (w.next_seq_no+1)%16;
                w.slot -=1;
                pack_seq_no +=1;
                sended_packs++;
            }

        }


        num_events = poll(fd, 2, 2500);
        if(!num_events) {
            continue;
        }
        if(fd[0].revents & POLLIN){
            struct UserDatagramProtocol *new_receive;
            new_receive = (struct UserDatagramProtocol*) malloc(sizeof(struct UserDatagramProtocol));
            recvfrom(socket_no, (struct UserDatagramProtocol *)new_receive, MAXLEN, MSG_WAITALL,(struct sockaddr *)server_add, sizeof(server_add));
            if (strcmp(pack->app_data, "\n") == 0 )
            {
                empty_new_lines++;
            }
            else{
                empty_new_lines = 0;
            }
            if(empty_new_lines == 3){
                break;
            }
            bool is_check_sum_ok = new_receive->sequence_no == checksum(new_receive);

            if(!is_check_sum_ok){   //If checksum is not same wait till timout to resend of data
                if(!new_receive->ACK){
                    if((new_receive->sequence_no)%16==w.next_seq_no){
                        new_receive->ACK = true;
                        sendto(socket_no, (const struct UserDatagramProtocol*)new_receive, sizeof(*new_receive), MSG_CONFIRM,(const struct sockaddr *)server_add, sizeof(server_add));
                        old_received_pack = new_receive;

                    }
                    else{
                        sendto(socket_no, (const struct UserDatagramProtocol*)old_received_pack, sizeof(*old_received_pack), MSG_CONFIRM,(const struct sockaddr *)server_add, sizeof(server_add));
                    }
                }
                else{
                    if((new_receive->sequence_no)%16==w.send_base) {
                        sended_packs--;
                        w.pakages[w.send_base] = *new_receive;
                        w.send_base = (w.send_base + 1) % 16;
                        w.slot++;
                    }
                }

            }

        }
        else if(fd[1].revents & POLLIN){
            if (w.slot>0)
            {
                if (send_size == 0)
                {
                    fgets(message_buff, MAXLEN, stdin);

                    divided_message = divide_message(message_buff);

                    if((strlen(message_buff)) % 16){
                        send_size = strlen(message_buff) / 8;
                    }
                    else{
                        send_size = ((strlen(message_buff)) / 16) + 1;
                    }
                    InitWindow(&w);
                    pack_seq_no = 0;

                    struct UserDatagramProtocol* pack;
                    pack = init_pack(w.next_seq_no, divided_message[pack_seq_no]);
                    w.pakages[w.next_seq_no] = *pack;
                    w.next_seq_no = (w.next_seq_no+1)%16;
                    w.slot -=1;
                    pack_seq_no +=1;
                }
            }
            else
            {
                fgets(message_buff, MAXLEN, stdin);
                simultaneous_msg_bfr[msg_buff_index++] = message_buff;
            }

        }

        if(w.slot!=16){
           struct timeval time_now;

            gettimeofday(&time_now, NULL);

            long time_now_microsecond = time_now.tv_sec * 1000000 + time_now.tv_usec;
            long time_interval = time_now_microsecond - (w.pakages[w.send_base].arr_time.tv_sec * 1000000 + w.pakages[w.send_base].arr_time.tv_usec);
            if(time_interval>TIMEOUT){
                int temp = w.send_base;
                for(int i=0;i<(16-w.slot);i++){
                    struct UserDatagramProtocol* resend_pack;
                    resend_pack = init_pack(w.pakages[temp].sequence_no, w.pakages[temp].app_data);
                    sendto(socket_no, (const struct UserDatagramProtocol*)resend_pack, sizeof(*resend_pack), MSG_CONFIRM,(const struct sockaddr *)server_add, sizeof(server_add));
                    temp = (temp+1)%16;
                }
            }
        }
    }
}


int main(int argc, char * argv[]){

    char* ip = argv[1];
    int serverPort = 49;
    int clientPort = 50;


    struct sockaddr_in client_add, server_add;
    memset(&client_add, 0, sizeof(client_add));
    memset(&server_add, 0, sizeof(server_add));
    inet_aton("172.0.0.20", &client_add.sin_addr.s_addr);
    inet_aton("172.0.0.10", &server_add.sin_addr.s_addr);
    Initilazer(&client_add, clientPort);
    Initilazer(&server_add, serverPort);

    int socketfd = socket(AF_INET, SOCK_DGRAM, 0);
    if(socketfd < 0)
        printf("Socket init error\n");
    exit(EXIT_FAILURE);

    int is_bind= bind(socketfd,(const struct sockaddr *) &client_add, sizeof(client_add));
    if (is_bind<0){
        printf("Socket binding error\n");
        exit(EXIT_FAILURE);
    }

    char message_buff[MAXLEN];

    go_back_n(&server_add, socketfd, message_buff);

    close(socketfd);
    return 0;
}