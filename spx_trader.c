#include "spx_trader.h"

int ex_fifo;
int trader_fifo;
int owned_order;

int command_buy(char* line, int line_len){
    char* cpy = malloc(sizeof(char) * line_len);
	strcpy(cpy, line);
	strtok(cpy, " ");
    strtok(NULL, " ");
	char *product = strtok(NULL, " ");
	int qty = atoi(strtok(NULL, " "));
	int price = atoi(strtok(NULL, " "));
    printf("msg?......\n");
    if(qty >= LIMIT){
        free(cpy);
        return 0;
    }
    
    printf("msg???????\n");
    
    char msg[50];
    sprintf(msg, BUY_MSG, owned_order, product, qty, price);
    printf("msg!!!!!!!!:  %s\n", msg);
    write(trader_fifo, msg, strlen(msg));
    kill(getppid(), SIGUSR1);
    free(cpy);
    return 1;
}

void sig_recieved(int signo, siginfo_t *si, void *data){
    char *temp = malloc(sizeof(char));
    int temp_num = 1; // there should be one more, because we need to restore the readed content
    while(read(ex_fifo, &(temp[temp_num - 1]), 1) == 1){
        if(temp[temp_num - 1] == ';' || temp[temp_num - 1] == '\0'){
            temp[temp_num - 1] = '\0';
            if(strncasecmp(temp, "MARKET SELL ", 12) == 0){
                if(command_buy(temp, temp_num) == 0){
                    free(temp);
                    exit(0);
                }
                owned_order++;
                break;
            }
            break;
        }
        temp_num++;
        temp = realloc(temp, sizeof(char) * temp_num);
    }
    free(temp);
}

int main(int argc, char ** argv) {
    if (argc < 2) {
        printf("Not enough arguments\n");
        return 1;
    }
    // register signal handler
    int trader_id = atoi(argv[1]);
    owned_order = 0;
    // connect to named pipes
    char exchange_file[20];
	sprintf(exchange_file, FIFO_EXCHANGE, trader_id);

    char trader_file[20];
	sprintf(trader_file, FIFO_TRADER, trader_id);

    ex_fifo = open(exchange_file, O_RDONLY);
    trader_fifo = open(trader_file, O_WRONLY);

    struct sigaction got_sig;
	got_sig.sa_sigaction = sig_recieved;
	got_sig.sa_flags |= SA_SIGINFO;

    sigaction(SIGUSR1, &got_sig, NULL);
    // event loop:
    while(1){
    }
    // wait for exchange update (MARKET message)
    // send order
    // wait for exchange confirmation (ACCEPTED message)
}