#include "spx_trader.h"

int ex_fifo;
int trader_fifo;

int check_valid(int qty){
    if(qty >= LIMIT){
        return 0;
    }
    return 1;
}

int command_buy(char* line, int line_len, pid_t parent_pid){
    char* cpy = malloc(sizeof(char) * line_len);
	strcpy(cpy, line);
	strtok(cpy, " ");
    strtok(NULL, " ");
	char *product = strtok(NULL, " ");
	int qty = atoi(strtok(NULL, " "));
	int price = atoi(strtok(NULL, " "));

    if(!check_valid(qty)){
        free(cpy);
        return 0;
    }
    
    char msg[50];
    sprintf(msg, BUY_MSG, owned_order, product, qty, price);
    write(trader_fifo, msg, strlen(msg));
    kill(SIGUSR1, parent_pid);
    free(cpy);
    return 1;
}

void sig_recieved(int signo, siginfo_t *si, void *data){
    printf("got market open\n");
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
    // event loop:
    struct sigaction got_sig;
	got_sig.sa_sigaction = sig_recieved;
	got_sig.sa_flags |= SA_SIGINFO;

    sigaction(SIGUSR1, &got_sig, NULL);
    pause();
    char msg[] = "SELL 0 TSLA 50 987";
    write(trader_fifo, msg, strlen(msg));
    kill(SIGUSR1, getppid());
    sleep(1);
    msg = "SELL 0 TSLA 5000 1000";
    write(trader_fifo, msg, strlen(msg));
    kill(SIGUSR1, getppid());
    // wait for exchange update (MARKET message)
    // send order
    // wait for exchange confirmation (ACCEPTED message)
}
