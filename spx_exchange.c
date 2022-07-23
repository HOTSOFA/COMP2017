/**
 * comp2017 - assignment 3
 * <Eric Pan>
 * <500172254>
 */

#include "spx_exchange.h"

int trader_num;
Trader** trader_list;
Orderbook* orderbook;
long int fee;
int disconneted_num;

void free_Order_lv(Order_lv* order_lv){
	for(int i = 0; i < order_lv->order_num; i++){
		free(order_lv->orders[i]);
	}
	free(order_lv->orders);
	free(order_lv);
}

void free_product(Product* product){
	free(product->product_name);
	for(int i = 0; i < product->sell_num; i++){
		free_Order_lv(product->sell_orders[i]);
	}
	free(product->sell_orders);

	for(int j = 0; j < product->buy_num; j++){
		free_Order_lv(product->buy_orders[j]);
	}
	free(product->buy_orders);

	free(product);
}

void free_orderbook(){
	for(int i = 0; i < orderbook->product_num; i++){
		free_product(orderbook->products[i]);
	}
	free(orderbook->products);
	free(orderbook);
}

void free_properties(Properties** properties, int property_num){
	for(int i = 0; i < property_num; i++){
		free(properties[i]->product_name);
		free(properties[i]);
	}
	free(properties);
}

void free_traders(){
	for(int i = 0; i < trader_num; i++){
		free_properties(trader_list[i]->properties, trader_list[i]->property_num);
		free(trader_list[i]);
	}
	free(trader_list);
}

void initialize_traders(char* file_path, int trader_id){
	trader_list[trader_id] = malloc(sizeof(Trader));
	trader_list[trader_id]->disconnected = 0;
	trader_list[trader_id]->trader_id = trader_id;
	trader_list[trader_id]->owned_order = 0;
	trader_list[trader_id]->property_num = orderbook->product_num;
	trader_list[trader_id]->properties = malloc(sizeof(Properties*) * (orderbook->product_num));
	
	int i = 0;
	while(i < orderbook->product_num){
		trader_list[trader_id]->properties[i] = malloc(sizeof(Properties));
		trader_list[trader_id]->properties[i]->product_name = malloc(sizeof(char) * orderbook->products[i]->name_len);
		strcpy(trader_list[trader_id]->properties[i]->product_name,orderbook->products[i]->product_name);
		trader_list[trader_id]->properties[i]->quantity = 0;
		trader_list[trader_id]->properties[i]->money_owned = 0;
		i++;
	}

	char exchange_file[20];
	char trader_file[20];

	sprintf(exchange_file, FIFO_EXCHANGE, trader_id);
	mkfifo(exchange_file, 0777);
	printf("%s Created FIFO %s\n", LOG_PREFIX, exchange_file);

	sprintf(trader_file, FIFO_TRADER, trader_id);
	mkfifo(trader_file, 0777);
	printf("%s Created FIFO %s\n", LOG_PREFIX, trader_file);

	pid_t pid = fork();

	if(pid == 0){
		char temp[5];
		sprintf(temp,"%d", trader_id);
		execlp(file_path, file_path, temp, NULL);
	}else{
		printf("%s Starting trader %d (%s)\n", LOG_PREFIX, trader_id, file_path);
		trader_list[trader_id]->pid = pid;

		printf("%s Connected to %s\n", LOG_PREFIX, exchange_file);
		trader_list[trader_id]->exchange_file = open(exchange_file, O_WRONLY);

		printf("%s Connected to %s\n", LOG_PREFIX, trader_file);
		trader_list[trader_id]->trader_file = open(trader_file, O_RDONLY);
	}
}


void initialize_orderbook(char* path){
	FILE* f = fopen(path, "r");// open the product file

	char line[NAME];
	fgets(line, NAME, f);
	int j = 0;
	while(j < NAME){
		if (line[j] == '\n'){
			line[j] = '\0';//we do not want \n when we do atoi
			break;
		}
		j++;
	}

	orderbook->product_num = atoi(line);
	orderbook->products = malloc(sizeof(Product*) * orderbook->product_num);

	int i = 0;
	while(i < orderbook->product_num){
		fgets(line, NAME, f);
		int j = 0;
		int valid = 0;
		while(j < NAME){
			if(line[j] != ' ' && j > 0){
				valid = 1;
			}
			if (line[j] == '\n'){
				line[j] = '\0';// we do not want \n be stored, because it will be hard to compare with the others(strcmp)
				break;
			}
			j++;
		}

		if(valid){//put everything into the product
			orderbook->products[i] = malloc(sizeof(Product));//we cannot get the local variable out of the function, unless we alloc memory for it
			orderbook->products[i]->name_len = strlen(line) + 1;
			orderbook->products[i]->product_name = malloc(sizeof(char) * orderbook->products[i]->name_len);
			strcpy(orderbook->products[i]->product_name, line);
			orderbook->products[i]->sell_orders = NULL;
			orderbook->products[i]->sell_num = 0;
			orderbook->products[i]->buy_orders = NULL;
			orderbook->products[i]->buy_num = 0;

			i++;
		}
	}
	fclose(f);
}

int check_valid(Trader* trader, int order_id, int qty, int price){
	if(order_id != trader->owned_order || order_id < 0 || order_id > 999999){
		return 0;
	}

	if(qty < 1 || qty > 999999){
		return 0;
	}

	if(price < 1 || price > 999999){
		return 0;
	}
	return 1;
}

int check_enough_argument(int argument_len, char* line, int line_len){
	char* cpy = malloc(sizeof(char) * line_len);
	strcpy(cpy, line);
	strtok(cpy, " ");
	int i = 1;
	while(strtok(NULL, " ") != NULL){
		i++;
	}
	free(cpy);
	if(i != argument_len){
		return 0;
	}
	return 1;
}

void send_signal_to_all(char* info, int info_len, int not_included_trader_id){
	for(int i = 0; i < trader_num; i++){
		if(trader_list[i]->trader_id != not_included_trader_id && trader_list[i]->disconnected == 0){
			write(trader_list[i]->exchange_file, info, info_len);
			kill(trader_list[i]->pid, SIGUSR1);
		}
	}
}

int command_buy(Trader* trader, char* line, int line_len){
	if(!check_enough_argument(5, line, line_len)){
		return 0;
	}
	char* cpy = malloc(sizeof(char) * line_len);
	strcpy(cpy, line);
	strtok(cpy, " ");
	int order_id = atoi(strtok(NULL, " "));
	char *product = strtok(NULL, " ");
	long int qty = atoi(strtok(NULL, " "));
	long int price = atoi(strtok(NULL, " "));

	if(!check_valid(trader, order_id, qty, price)){
		free(cpy);
		return 0;
	}

	Product* p;
	int found = 0;
	for(int i = 0; i < orderbook->product_num; i++){
		if(strcmp(product, orderbook->products[i]->product_name) == 0){
			p = orderbook->products[i];
			found = 1;
			break;
		}
	}

	if (found){
		trader->owned_order++;

		char temperate[15];
		sprintf(temperate, "ACCEPTED %d;", order_id);
		write(trader->exchange_file, temperate, strlen(temperate));
		kill(trader->pid, SIGUSR1);

		char tmp[50];
		sprintf(tmp, "MARKET BUY %s %ld %ld;", product, qty, price);
		send_signal_to_all(tmp, strlen(tmp), trader->trader_id);

		// all of these long code could be optimized, just like what i did in command_amend, but I do not have time to do it :(
		for(int i = 0; i < p->sell_num; i++){//find is there any sell order could be matched
			if(p->sell_orders[i]->price <= price){
				int clear_lv = 0;
				for(int j = 0;j < p->sell_orders[i]->order_num; j++){//loop for all the order that have the same price
					if(p->sell_orders[i]->orders[j]->quantity > qty){
						long int f = (long int)(qty * p->sell_orders[i]->price * 0.01 + 0.5);
						printf("%s Match: Order %d [T%d], New Order %d [T%d], value: $%ld, fee: $%ld.\n", LOG_PREFIX, p->sell_orders[i]->orders[j]->order_id, 
						p->sell_orders[i]->orders[j]->trader->trader_id, order_id, trader->trader_id, qty * p->sell_orders[i]->price, f);

						fee += f;
						
						for(int k = 0; k < p->sell_orders[i]->orders[j]->trader->property_num; k++){
							if (strcmp(p->sell_orders[i]->orders[j]->trader->properties[k]->product_name, product) == 0){
								p->sell_orders[i]->orders[j]->trader->properties[k]->quantity -= qty;
								p->sell_orders[i]->orders[j]->trader->properties[k]->money_owned += (qty * p->sell_orders[i]->price);
							}
						}

						char t[30];

						if(p->sell_orders[i]->orders[j]->trader->disconnected == 0){
							sprintf(t, "FILL %d %ld;", p->sell_orders[i]->orders[j]->order_id, qty);
							write(p->sell_orders[i]->orders[j]->trader->exchange_file, t, strlen(t));
							kill(p->sell_orders[i]->orders[j]->trader->pid, SIGUSR1);
						}

						for(int k = 0; k < trader->property_num; k++){
							if (strcmp(trader->properties[k]->product_name, product) == 0){
								trader->properties[k]->quantity += qty;
								trader->properties[k]->money_owned -= (qty * p->sell_orders[i]->price + f);
							}
						}
						sprintf(t, "FILL %d %ld;", order_id, qty);
						write(trader->exchange_file, t, strlen(t));
						kill(trader->pid, SIGUSR1);

						p->sell_orders[i]->orders[j]->quantity -= qty;
						if (p->sell_orders[i]->orders[j]->quantity == 0){
							free(p->sell_orders[i]->orders[j]);
							for(int k = j; k < p->sell_orders[i]->order_num - 1; k++){
								p->sell_orders[i]->orders[k] = p->sell_orders[i]->orders[k + 1];
							}
							p->sell_orders[i]->order_num--;
							p->sell_orders[i]->orders = realloc(p->sell_orders[i]->orders, p->sell_orders[i]->order_num);
						}
						p->sell_orders[i]->total_amount -= qty;
						qty = 0;
						free(cpy);
						return 1;
					}else{
						long int f = (long int)(p->sell_orders[i]->orders[j]->quantity * p->sell_orders[i]->price * 0.01 + 0.5);
						printf("%s Match: Order %d [T%d], New Order %d [T%d], value: $%ld, fee: $%ld.\n", LOG_PREFIX, p->sell_orders[i]->orders[j]->order_id, 
						p->sell_orders[i]->orders[j]->trader->trader_id, order_id, trader->trader_id, p->sell_orders[i]->orders[j]->quantity * p->sell_orders[i]->price, f);

						fee += f;

						for(int k = 0; k < p->sell_orders[i]->orders[j]->trader->property_num; k++){
							if (strcmp(p->sell_orders[i]->orders[j]->trader->properties[k]->product_name, product) == 0){
								p->sell_orders[i]->orders[j]->trader->properties[k]->quantity -= p->sell_orders[i]->orders[j]->quantity;
								p->sell_orders[i]->orders[j]->trader->properties[k]->money_owned += (p->sell_orders[i]->orders[j]->quantity * p->sell_orders[i]->price);
							}
						}

						char t[30];

						if(p->sell_orders[i]->orders[j]->trader->disconnected == 0){
							sprintf(t, "FILL %d %ld;", p->sell_orders[i]->orders[j]->order_id, p->sell_orders[i]->orders[j]->quantity);
							write(p->sell_orders[i]->orders[j]->trader->exchange_file, t, strlen(t));
							kill(p->sell_orders[i]->orders[j]->trader->pid, SIGUSR1);
						}

						for(int k = 0; k < trader->property_num; k++){
							if (strcmp(trader->properties[k]->product_name, product) == 0){
								trader->properties[k]->quantity += p->sell_orders[i]->orders[j]->quantity;
								trader->properties[k]->money_owned -= (p->sell_orders[i]->orders[j]->quantity * p->sell_orders[i]->price + f);
							}
						}
						sprintf(t, "FILL %d %ld;", order_id, p->sell_orders[i]->orders[j]->quantity);
						write(trader->exchange_file, t, strlen(t));
						kill(trader->pid, SIGUSR1);

						qty -= p->sell_orders[i]->orders[j]->quantity;
						p->sell_orders[i]->total_amount -= p->sell_orders[i]->orders[j]->quantity;
						p->sell_orders[i]->orders[j]->quantity = 0;
						free(p->sell_orders[i]->orders[j]);
						for(int m = j; m < p->sell_orders[i]->order_num - 1; m++){
							p->sell_orders[i]->orders[m] = p->sell_orders[i]->orders[m + 1];
						}
						p->sell_orders[i]->order_num--;
						if(p->sell_orders[i]->order_num == 0){
							free_Order_lv(p->sell_orders[i]);
							for(int m = i; m < p->sell_num - 1; m++){
								p->sell_orders[m] = p->sell_orders[m + 1];
							}
							p->sell_num--;
							p->sell_orders = realloc(p->sell_orders, sizeof(Order_lv*) * p->sell_num);
							clear_lv = 1;
							break;
						}else{
							p->sell_orders[i]->orders = realloc(p->sell_orders[i]->orders, sizeof(Order*) * p->sell_orders[i]->order_num);
						}
						if(qty == 0){
							free(cpy);
							return 1;
						}
						j -= 1;
					}
				}
				if(qty == 0){
					free(cpy);
					return 1;
				}
				if (clear_lv == 1){
					i -= 1;
				}
			}
		}

		int find = 0;
		for(int i = 0; i < p->buy_num; i++){//add order if there is still remaing qty
			if(p->buy_orders[i]->price == price){
				find = 1;
				p->buy_orders[i]->order_num++;
				p->buy_orders[i]->orders = realloc(p->buy_orders[i]->orders, sizeof(Order*) * p->buy_orders[i]->order_num);
				p->buy_orders[i]->total_amount += qty;
				Order* o = malloc(sizeof(Order));
				o->order_id = order_id;
				o->quantity = qty;
				o->trader = trader;
				p->buy_orders[i]->orders[p->buy_orders[i]->order_num - 1] = o;
				break;
			}else if(p->buy_orders[i]->price < price){
				find = 1;
				p->buy_num++;
				p->buy_orders = realloc(p->buy_orders, sizeof(Order_lv*) * p->buy_num);
				for(int k = p->buy_num - 1; k >i; k--){
					p->buy_orders[k] = p->buy_orders[k - 1];
				}
				Order_lv* ol = malloc(sizeof(Order_lv));
				ol->total_amount = qty;
				ol->order_num = 1;
				ol->price = price;
				ol->orders = malloc(sizeof(Order*) * ol->order_num);
				Order* o = malloc(sizeof(Order));
				o->order_id = order_id;
				o->quantity = qty;
				o->trader = trader;
				ol->orders[0] = o;
				p->buy_orders[i] = ol;
				break;
			}
		}

		if(!find){
			p->buy_num++;
			p->buy_orders = realloc(p->buy_orders, sizeof(Order_lv*) * p->buy_num);
			Order_lv* ol = malloc(sizeof(Order_lv));
			ol->total_amount = qty;
			ol->order_num = 1;
			ol->price = price;
			ol->orders = malloc(sizeof(Order*) * ol->order_num);
			Order *o = malloc(sizeof(Order));
			o->order_id = order_id;
			o->quantity = qty;
			o->trader = trader;
			ol->orders[0] = o;
			p->buy_orders[p->buy_num - 1] = ol;
		}

		free(cpy);
		return 1;
	}else{
		free(cpy);
		return 0;
	}
}

int command_sell(Trader* trader, char* line, int line_len){
	if(!check_enough_argument(5, line, line_len)){
		return 0;
	}
	char* cpy = malloc(sizeof(char) * line_len);
	strcpy(cpy, line);
	strtok(cpy, " ");
	int order_id = atoi(strtok(NULL, " "));
	char *product = strtok(NULL, " ");
	long int qty = atoi(strtok(NULL, " "));
	long int price = atoi(strtok(NULL, " "));

	if(!check_valid(trader, order_id, qty, price)){
		free(cpy);
		return 0;
	}

	Product* p;
	int found = 0;
	for(int i = 0; i < orderbook->product_num; i++){
		if(strcmp(product, orderbook->products[i]->product_name) == 0){
			p = orderbook->products[i];
			found = 1;
			break;
		}
	}

	if (found){
		trader->owned_order++;

		char temperate[15];
		sprintf(temperate, "ACCEPTED %d;", order_id);
		write(trader->exchange_file, temperate, strlen(temperate));
		kill(trader->pid, SIGUSR1);

		char tmp[50];
		sprintf(tmp, "MARKET SELL %s %ld %ld;", product, qty, price);
		send_signal_to_all(tmp, strlen(tmp), trader->trader_id);

		// all of these long code could be optimized, just like what i did in command_amend, but I do not have time to do it :(
		for(int i = 0; i < p->buy_num; i++){//find is there any buy order could be matched
			if(p->buy_orders[i]->price >= price){
				int clear_lv = 0;
				for(int j = 0;j < p->buy_orders[i]->order_num; j++){//loop for all the order that have the same price
					if(p->buy_orders[i]->orders[j]->quantity > qty){
						long int f = (long int)(qty * p->buy_orders[i]->price * 0.01 + 0.5);
						printf("%s Match: Order %d [T%d], New Order %d [T%d], value: $%ld, fee: $%ld.\n", LOG_PREFIX, p->buy_orders[i]->orders[j]->order_id, 
						p->buy_orders[i]->orders[j]->trader->trader_id, order_id, trader->trader_id, qty * p->buy_orders[i]->price , f);
						fee += f;

						for(int k = 0; k < p->buy_orders[i]->orders[j]->trader->property_num; k++){
							if (strcmp(p->buy_orders[i]->orders[j]->trader->properties[k]->product_name, product) == 0){
								p->buy_orders[i]->orders[j]->trader->properties[k]->quantity += qty;
								p->buy_orders[i]->orders[j]->trader->properties[k]->money_owned -= (qty * p->buy_orders[i]->price);
							}
						}
						char t[30];

						if(p->buy_orders[i]->orders[j]->trader->disconnected == 0){
							sprintf(t, "FILL %d %ld;", p->buy_orders[i]->orders[j]->order_id, qty);
							write(p->buy_orders[i]->orders[j]->trader->exchange_file, t, strlen(t));
							kill(p->buy_orders[i]->orders[j]->trader->pid, SIGUSR1);
						}

						for(int k = 0; k < trader->property_num; k++){
							if (strcmp(trader->properties[k]->product_name, product) == 0){
								trader->properties[k]->quantity -= qty;
								trader->properties[k]->money_owned += (qty * p->buy_orders[i]->price - f);
							}
						}
						sprintf(t, "FILL %d %ld;", order_id, qty);
						write(trader->exchange_file, t, strlen(t));
						kill(trader->pid, SIGUSR1);

						p->buy_orders[i]->orders[j]->quantity -= qty;
						if (p->buy_orders[i]->orders[j]->quantity == 0){
							free(p->buy_orders[i]->orders[j]);
							for(int k = j; k < p->buy_orders[i]->order_num - 1; k++){
								p->buy_orders[i]->orders[k] = p->buy_orders[i]->orders[k + 1];
							}
							p->buy_orders[i]->order_num--;
							p->buy_orders[i]->orders = realloc(p->buy_orders[i]->orders, p->buy_orders[i]->order_num);
						}
						p->buy_orders[i]->total_amount -= qty;
						qty = 0;
						break;
					}else{
						long int f = (long int)(p->buy_orders[i]->orders[j]->quantity * p->buy_orders[i]->price * 0.01 + 0.5);
						printf("%s Match: Order %d [T%d], New Order %d [T%d], value: $%ld, fee: $%ld.\n", LOG_PREFIX, p->buy_orders[i]->orders[j]->order_id, 
						p->buy_orders[i]->orders[j]->trader->trader_id, order_id, trader->trader_id, p->buy_orders[i]->orders[j]->quantity * p->buy_orders[i]->price,
						f);

						fee += f;

						for(int k = 0; k < p->buy_orders[i]->orders[j]->trader->property_num; k++){
							if (strcmp(p->buy_orders[i]->orders[j]->trader->properties[k]->product_name, product) == 0){
								p->buy_orders[i]->orders[j]->trader->properties[k]->quantity += p->buy_orders[i]->orders[j]->quantity;
								p->buy_orders[i]->orders[j]->trader->properties[k]->money_owned -= (p->buy_orders[i]->orders[j]->quantity * p->buy_orders[i]->price);
							}
						}

						char t[30];

						if(p->buy_orders[i]->orders[j]->trader->disconnected == 0){
							sprintf(t, "FILL %d %ld;", p->buy_orders[i]->orders[j]->order_id, p->buy_orders[i]->orders[j]->quantity);
							write(p->buy_orders[i]->orders[j]->trader->exchange_file, t, strlen(t));
							kill(p->buy_orders[i]->orders[j]->trader->pid, SIGUSR1);
						}

						for(int k = 0; k < trader->property_num; k++){
							if (strcmp(trader->properties[k]->product_name, product) == 0){
								trader->properties[k]->quantity -= p->buy_orders[i]->orders[j]->quantity;
								trader->properties[k]->money_owned += (p->buy_orders[i]->orders[j]->quantity * p->buy_orders[i]->price - f);
							}
						}
						sprintf(t, "FILL %d %ld;", order_id, p->buy_orders[i]->orders[j]->quantity);
						write(trader->exchange_file, t, strlen(t));
						kill(trader->pid, SIGUSR1);

						qty -= p->buy_orders[i]->orders[j]->quantity;

						p->buy_orders[i]->total_amount -= p->buy_orders[i]->orders[j]->quantity;
						p->buy_orders[i]->orders[j]->quantity = 0;
						free(p->buy_orders[i]->orders[j]);
						for(int m = j; m < p->buy_orders[i]->order_num - 1; m++){
							p->buy_orders[i]->orders[m] = p->buy_orders[i]->orders[m + 1];
						}
						p->buy_orders[i]->order_num--;
						if(p->buy_orders[i]->order_num == 0){
							free_Order_lv(p->buy_orders[i]);
							for(int m = i; m < p->buy_num - 1; m++){
								p->buy_orders[m] = p->buy_orders[m + 1];
							}
							p->buy_num--;
							p->buy_orders = realloc(p->buy_orders, sizeof(Order_lv*) * p->buy_num);
							clear_lv = 1;
							break;
						}else{
							p->buy_orders[i]->orders = realloc(p->buy_orders[i]->orders, sizeof(Order*) * p->buy_orders[i]->order_num);
						}
						if(qty == 0){
							free(cpy);
							return 1;
						}
						j -= 1;
					}
				}
				if(qty == 0){
					free(cpy);
					return 1;
				}
				if (clear_lv == 1){
					i -= 1;
				}
			}
		}

		int find = 0;
		for(int i = 0; i < p->sell_num; i++){//add order if there is still remaing qty
			if(p->sell_orders[i]->price == price){
				find = 1;
				p->sell_orders[i]->order_num++;
				p->sell_orders[i]->orders = realloc(p->sell_orders[i]->orders, sizeof(Order*) * p->sell_orders[i]->order_num);
				p->sell_orders[i]->total_amount += qty;
				Order* o = malloc(sizeof(Order));
				o->order_id = order_id;
				o->quantity = qty;
				o->trader = trader;
				p->sell_orders[i]->orders[p->sell_orders[i]->order_num - 1] = o;
				break;
			}else if(p->sell_orders[i]->price < price){
				find = 1;
				p->sell_num++;
				p->sell_orders = realloc(p->sell_orders, sizeof(Order_lv*) * p->sell_num);
				for(int k = p->sell_num - 1; k >i; k--){
					p->sell_orders[k] = p->sell_orders[k - 1];
				}
				Order_lv* ol = malloc(sizeof(Order_lv));
				ol->total_amount = qty;
				ol->order_num = 1;
				ol->price = price;
				ol->orders = malloc(sizeof(Order*) * ol->order_num);
				Order* o = malloc(sizeof(Order));
				o->order_id = order_id;
				o->quantity = qty;
				o->trader = trader;
				ol->orders[0] = o;
				p->sell_orders[i] = ol;
				break;
			}
		}

		if(!find){
			p->sell_num++;
			p->sell_orders = realloc(p->sell_orders, sizeof(Order_lv*) * p->sell_num);
			Order_lv* ol = malloc(sizeof(Order_lv));
			ol->total_amount = qty;
			ol->order_num = 1;
			ol->price = price;
			ol->orders = malloc(sizeof(Order*) * ol->order_num);
			Order *o = malloc(sizeof(Order));
			o->order_id = order_id;
			o->quantity = qty;
			o->trader = trader;
			ol->orders[0] = o;
			p->sell_orders[p->sell_num - 1] = ol;
		}

		free(cpy);
		return 1;
	}else{
		free(cpy);
		return 0;
	}
}

int command_cancel(Trader* trader, char* line, int line_len){
	if(!check_enough_argument(2, line, line_len)){
		return 0;
	}
	char* cpy = malloc(sizeof(char) * line_len);
	strcpy(cpy, line);
	strtok(cpy, " ");
	int order_id = atoi(strtok(NULL, " "));

	if(order_id >= trader->owned_order){
		free(cpy);
		return 0;
	}
	for(int i = 0; i < orderbook->product_num; i++){//find the order
		Product* p = orderbook->products[i];
		for(int k = 0; k < p->buy_num; k++){
			Order_lv* ol = p->buy_orders[k];
			for(int j = 0; j < ol->order_num; j++){
				Order* o = ol->orders[j];
				if(o->order_id == order_id && o->trader->trader_id == trader->trader_id){
					char temperate[15];
					sprintf(temperate, "CANCELLED %d;", order_id);
					write(trader->exchange_file, temperate, strlen(temperate));
					kill(trader->pid, SIGUSR1);

					char tmp[50];
					sprintf(tmp, "MARKET BUY %s %d %d;", p->product_name, 0, 0);
					send_signal_to_all(tmp, strlen(tmp), trader->trader_id);

					ol->total_amount -= o->quantity;
					free(o);
					for(int m = j; m < ol->order_num - 1; m++){
						ol->orders[m] = ol->orders[m + 1];
					}
					ol->order_num--;
					if(ol->order_num == 0){
						free_Order_lv(ol);
						for(int m = k; m < p->buy_num - 1; m++){
							p->buy_orders[m] = p->buy_orders[m + 1];
						}
						p->buy_num--;
						p->buy_orders = realloc(p->buy_orders, sizeof(Order_lv*) * p->buy_num);
						free(cpy);
						return 1;
					}else{
						ol->orders = realloc(ol->orders, sizeof(Order*) * ol->order_num);
					}
					free(cpy);
					return 1;
				}
			}
		}

		for(int k = 0; k < p->sell_num; k++){
			Order_lv* ol = p->sell_orders[k];
			for(int j = 0; j < ol->order_num; j++){
				Order* o = ol->orders[j];
				if(o->order_id == order_id && o->trader->trader_id == trader->trader_id){
					char temperate[15];
					sprintf(temperate, "CANCELLED %d;", order_id);
					write(trader->exchange_file, temperate, strlen(temperate));
					kill(trader->pid, SIGUSR1);

					char tmp[50];
					sprintf(tmp, "MARKET SELL %s %d %d;", p->product_name, 0, 0);
					send_signal_to_all(tmp, strlen(tmp), trader->trader_id);

					ol->total_amount -= o->quantity;
					free(o);
					for(int m = j; m < ol->order_num - 1; m++){
						ol->orders[m] = ol->orders[m + 1];
					}
					ol->order_num--;
					if(ol->order_num == 0){
						free_Order_lv(ol);
						for(int m = k; m < p->sell_num - 1; m++){
							p->sell_orders[m] = p->sell_orders[m + 1];
						}
						p->sell_num--;
						p->sell_orders = realloc(p->sell_orders, sizeof(Order_lv*) * p->sell_num);
						free(cpy);
						return 1;
					}else{
						ol->orders = realloc(ol->orders, sizeof(Order*) * ol->order_num);
					}
					free(cpy);
					return 1;
				}
			}
		}
	}
	free(cpy);
	return 0;
}

int check_amend_valid(Trader* trader, int order_id, int qty, int price){
	if(order_id >= trader->owned_order || order_id < 0 || order_id > 999999){
		return 0;
	}

	if(qty < 1 || qty > 999999){
		return 0;
	}

	if(price < 1 || price > 999999){
		return 0;
	}
	return 1;
}

int command_amend(Trader* trader, char* line, int line_len){
	if(!check_enough_argument(4, line, line_len)){
		return 0;
	}
	char* cpy = malloc(sizeof(char) * line_len);
	strcpy(cpy, line);
	strtok(cpy, " ");
	int order_id = atoi(strtok(NULL, " "));
	long int qty = atoi(strtok(NULL, " "));
	long int price = atoi(strtok(NULL, " "));

	if(!check_amend_valid(trader, order_id, qty, price)){
		free(cpy);
		return 0;
	}

	int found = 0;
	Product* p;
	enum order_type{
		BUY = 0,
		SELL = 1
	}type;
	for(int i = 0; i < orderbook->product_num; i++){//find the order
		Product* temp_p = orderbook->products[i];
		for(int k = 0; k < temp_p->buy_num; k++){
			Order_lv* ol = temp_p->buy_orders[k];
			for(int j = 0; j < ol->order_num; j++){
				Order* o = ol->orders[j];
				if(ol->price == price && o->order_id == order_id && o->trader->trader_id == trader->trader_id){
					char temperate[15];
					sprintf(temperate, "AMENDED %d;", order_id);
					write(trader->exchange_file, temperate, strlen(temperate));
					kill(trader->pid, SIGUSR1);

					char tmp[50];
					sprintf(tmp, "MARKET BUY %s %ld %ld;", temp_p->product_name, qty, price);
					send_signal_to_all(tmp, strlen(tmp), trader->trader_id);

					int diff = qty - o->quantity;
					o->quantity = qty;
					ol->total_amount += diff;
					for(int m = j; m < ol->order_num - 1; m++){
						ol->orders[m] = ol->orders[m + 1];
					}
					ol->orders[ol->order_num - 1] = o;
					free(cpy);
					return 1;
				}else if(o->order_id == order_id && o->trader->trader_id == trader->trader_id){
					found = 1;
					ol->total_amount -= o->quantity;
					free(o);
					for(int m = j; m < ol->order_num - 1; m++){
						ol->orders[m] = ol->orders[m + 1];
					}
					ol->order_num--;
					if(ol->order_num == 0){
						free_Order_lv(ol);
						for(int m = k; m < temp_p->buy_num - 1; m++){
							temp_p->buy_orders[m] = temp_p->buy_orders[m + 1];
						}
						temp_p->buy_num--;
						temp_p->buy_orders = realloc(temp_p->buy_orders, sizeof(Order_lv*) * temp_p->buy_num);
					}else{
						ol->orders = realloc(ol->orders, sizeof(Order*) * ol->order_num);
					}
					p = orderbook->products[i];
					type = 0;
					break;
				}
			}
		}

		if(found){
			break;
		}

		for(int k = 0; k < temp_p->sell_num; k++){
			Order_lv* ol = temp_p->sell_orders[k];
			for(int j = 0; j < ol->order_num; j++){
				Order* o = ol->orders[j];
				if(ol->price == price && o->order_id == order_id && o->trader->trader_id == trader->trader_id){
					char temperate[15];
					sprintf(temperate, "AMENDED %d;", order_id);
					write(trader->exchange_file, temperate, strlen(temperate));
					kill(trader->pid, SIGUSR1);

					char tmp[50];
					sprintf(tmp, "MARKET SELL %s %ld %ld;", temp_p->product_name, qty, price);
					send_signal_to_all(tmp, strlen(tmp), trader->trader_id);

					int diff = qty - o->quantity;
					o->quantity = qty;
					ol->total_amount += diff;
					for(int m = j; m < ol->order_num - 1; m++){
						ol->orders[m] = ol->orders[m + 1];
					}
					ol->orders[ol->order_num - 1] = o;
					free(cpy);
					return 1;
				}else if(o->order_id == order_id && o->trader->trader_id == trader->trader_id){
					found = 1;
					ol->total_amount -= o->quantity;
					free(o);
					for(int m = j; m < ol->order_num - 1; m++){
						ol->orders[m] = ol->orders[m + 1];
					}
					ol->order_num--;
					if(ol->order_num == 0){
						free_Order_lv(ol);
						for(int m = k; m < temp_p->sell_num - 1; m++){
							temp_p->sell_orders[m] = temp_p->sell_orders[m + 1];
						}
						temp_p->sell_num--;
						temp_p->sell_orders = realloc(temp_p->sell_orders, sizeof(Order_lv*) * temp_p->sell_num);
					}else{
						ol->orders = realloc(ol->orders, sizeof(Order*) * ol->order_num);
					}
					p = orderbook->products[i];
					type = 1;
					break;
				}
			}
		}

		if(found){
			break;
		}
	}

	if(found){//do the same thing as we did in buy and sell
		char temperate[15];
		sprintf(temperate, "AMENDED %d;", order_id);
		write(trader->exchange_file, temperate, strlen(temperate));
		kill(trader->pid, SIGUSR1);
		
		if(qty == 0){
			free(cpy);
			return 1;
		}

		if(type == 0){
			char tmp[50];
			sprintf(tmp, "MARKET BUY %s %ld %ld;", p->product_name, qty, price);
			send_signal_to_all(tmp, strlen(tmp), trader->trader_id);
			for(int i = 0; i < p->sell_num; i++){
				if(p->sell_orders[i]->price <= price){
					int clear_lv = 0;
					for(int j = 0;j < p->sell_orders[i]->order_num; j++){
						if(p->sell_orders[i]->orders[j]->quantity > qty){
							long int f = (long int)(qty * p->sell_orders[i]->price * 0.01 + 0.5);
							printf("%s Match: Order %d [T%d], New Order %d [T%d], value: $%ld, fee: $%ld.\n", LOG_PREFIX, p->sell_orders[i]->orders[j]->order_id, 
							p->sell_orders[i]->orders[j]->trader->trader_id, order_id, trader->trader_id, qty * p->sell_orders[i]->price, f);

							fee += f;

							for(int k = 0; k < p->sell_orders[i]->orders[j]->trader->property_num; k++){
								if (strcmp(p->sell_orders[i]->orders[j]->trader->properties[k]->product_name, p->product_name) == 0){
									p->sell_orders[i]->orders[j]->trader->properties[k]->quantity -= qty;
									p->sell_orders[i]->orders[j]->trader->properties[k]->money_owned += (qty * p->sell_orders[i]->price);
								}
							}
							
							char t[30];

							if(p->sell_orders[i]->orders[j]->trader->disconnected == 0){
								sprintf(t, "FILL %d %ld;", p->sell_orders[i]->orders[j]->order_id, qty);
								write(p->sell_orders[i]->orders[j]->trader->exchange_file, t, strlen(t));
								kill(p->sell_orders[i]->orders[j]->trader->pid, SIGUSR1);
							}

							for(int k = 0; k < trader->property_num; k++){
								if (strcmp(trader->properties[k]->product_name, p->product_name) == 0){
									trader->properties[k]->quantity += qty;
									trader->properties[k]->money_owned -= (qty * p->sell_orders[i]->price + f);
								}
							}
							sprintf(t, "FILL %d %ld;", order_id, qty);
							write(trader->exchange_file, t, strlen(t));
							kill(trader->pid, SIGUSR1);

							p->sell_orders[i]->orders[j]->quantity -= qty;
							if (p->sell_orders[i]->orders[j]->quantity == 0){
								free(p->sell_orders[i]->orders[j]);
								for(int k = j; k < p->sell_orders[i]->order_num - 1; k++){
									p->sell_orders[i]->orders[k] = p->sell_orders[i]->orders[k + 1];
								}
								p->sell_orders[i]->order_num--;
								p->sell_orders[i]->orders = realloc(p->sell_orders[i]->orders, p->sell_orders[i]->order_num);
							}
							p->sell_orders[i]->total_amount -= qty;
							qty = 0;
							free(cpy);
							return 1;
						}else{
							long int f = (long int)(p->sell_orders[i]->orders[j]->quantity * p->sell_orders[i]->price * 0.01 + 0.5);
							printf("%s Match: Order %d [T%d], New Order %d [T%d], value: $%ld, fee: $%ld.\n", LOG_PREFIX, p->sell_orders[i]->orders[j]->order_id, 
							p->sell_orders[i]->orders[j]->trader->trader_id, order_id, trader->trader_id, p->sell_orders[i]->orders[j]->quantity * p->sell_orders[i]->price, f);

							fee += f;
							
							for(int k = 0; k < p->sell_orders[i]->orders[j]->trader->property_num; k++){
								if (strcmp(p->sell_orders[i]->orders[j]->trader->properties[k]->product_name, p->product_name) == 0){
									p->sell_orders[i]->orders[j]->trader->properties[k]->quantity -= p->sell_orders[i]->orders[j]->quantity;
									p->sell_orders[i]->orders[j]->trader->properties[k]->money_owned += (p->sell_orders[i]->orders[j]->quantity * p->sell_orders[i]->price);
								}
							}
							
							char t[30];

							if(p->sell_orders[i]->orders[j]->trader->disconnected == 0){
								sprintf(t, "FILL %d %ld;", p->sell_orders[i]->orders[j]->order_id, p->sell_orders[i]->orders[j]->quantity);
								write(p->sell_orders[i]->orders[j]->trader->exchange_file, t, strlen(t));
								kill(p->sell_orders[i]->orders[j]->trader->pid, SIGUSR1);
							}

							for(int k = 0; k < trader->property_num; k++){
								if (strcmp(trader->properties[k]->product_name, p->product_name) == 0){
									trader->properties[k]->quantity += p->sell_orders[i]->orders[j]->quantity;
									trader->properties[k]->money_owned -= (p->sell_orders[i]->orders[j]->quantity * p->sell_orders[i]->price + f);
								}
							}
							sprintf(t, "FILL %d %ld;", order_id, p->sell_orders[i]->orders[j]->quantity);
							write(trader->exchange_file, t, strlen(t));
							kill(trader->pid, SIGUSR1);

							qty -= p->sell_orders[i]->orders[j]->quantity;
							p->sell_orders[i]->total_amount -= p->sell_orders[i]->orders[j]->quantity;
							p->sell_orders[i]->orders[j]->quantity = 0;
							free(p->sell_orders[i]->orders[j]);
							for(int m = j; m < p->sell_orders[i]->order_num - 1; m++){
								p->sell_orders[i]->orders[m] = p->sell_orders[i]->orders[m + 1];
							}
							p->sell_orders[i]->order_num--;
							if(p->sell_orders[i]->order_num == 0){
								free_Order_lv(p->sell_orders[i]);
								for(int m = i; m < p->sell_num - 1; m++){
									p->sell_orders[m] = p->sell_orders[m + 1];
								}
								p->sell_num--;
								p->sell_orders = realloc(p->sell_orders, sizeof(Order_lv*) * p->sell_num);
								clear_lv = 1;
								break;
							}else{
								p->sell_orders[i]->orders = realloc(p->sell_orders[i]->orders, sizeof(Order*) * p->sell_orders[i]->order_num);
							}
							if(qty == 0){
								free(cpy);
								return 1;
							}
							j -= 1;
						}
					}
					if(qty == 0){
						free(cpy);
						return 1;
					}
					if (clear_lv == 1){
						i -= 1;
					}
				}
			}

			int find = 0;
			for(int i = 0; i < p->buy_num; i++){
				if(p->buy_orders[i]->price == price){
					find = 1;
					p->buy_orders[i]->order_num++;
					p->buy_orders[i]->orders = realloc(p->buy_orders[i]->orders, sizeof(Order*) * p->buy_orders[i]->order_num);
					p->buy_orders[i]->total_amount += qty;
					Order* o = malloc(sizeof(Order));
					o->order_id = order_id;
					o->quantity = qty;
					o->trader = trader;
					p->buy_orders[i]->orders[p->buy_orders[i]->order_num - 1] = o;
					break;
				}else if(p->buy_orders[i]->price < price){
					find = 1;
					p->buy_num++;
					p->buy_orders = realloc(p->buy_orders, sizeof(Order_lv*) * p->buy_num);
					for(int k = p->buy_num - 1; k >i; k--){
						p->buy_orders[k] = p->buy_orders[k - 1];
					}
					Order_lv* ol = malloc(sizeof(Order_lv));
					ol->total_amount = qty;
					ol->order_num = 1;
					ol->price = price;
					ol->orders = malloc(sizeof(Order*) * ol->order_num);
					Order* o = malloc(sizeof(Order));
					o->order_id = order_id;
					o->quantity = qty;
					o->trader = trader;
					ol->orders[0] = o;
					p->buy_orders[i] = ol;
					break;
				}
			}

			if(!find){
				p->buy_num++;
				p->buy_orders = realloc(p->buy_orders, sizeof(Order_lv*) * p->buy_num);
				Order_lv* ol = malloc(sizeof(Order_lv));
				ol->total_amount = qty;
				ol->order_num = 1;
				ol->price = price;
				ol->orders = malloc(sizeof(Order*) * ol->order_num);
				Order *o = malloc(sizeof(Order));
				o->order_id = order_id;
				o->quantity = qty;
				o->trader = trader;
				ol->orders[0] = o;
				p->buy_orders[p->buy_num - 1] = ol;
			}
		}else{
			char tmp[50];
			sprintf(tmp, "MARKET SELL %s %ld %ld;", p->product_name, qty, price);
			send_signal_to_all(tmp, strlen(tmp), trader->trader_id);
			for(int i = 0; i < p->buy_num; i++){
				if(p->buy_orders[i]->price >= price){
					int clear_lv = 0;
					for(int j = 0;j < p->buy_orders[i]->order_num; j++){
						if(p->buy_orders[i]->orders[j]->quantity > qty){
							long int f = (long int)(qty * p->buy_orders[i]->price * 0.01 + 0.5);
							printf("%s Match: Order %d [T%d], New Order %d [T%d], value: $%ld, fee: $%ld.\n", LOG_PREFIX, p->buy_orders[i]->orders[j]->order_id, 
							p->buy_orders[i]->orders[j]->trader->trader_id, order_id, trader->trader_id, qty * p->buy_orders[i]->price , f);
							fee += f;

							for(int k = 0; k < p->buy_orders[i]->orders[j]->trader->property_num; k++){
								if (strcmp(p->buy_orders[i]->orders[j]->trader->properties[k]->product_name, p->product_name) == 0){
									p->buy_orders[i]->orders[j]->trader->properties[k]->quantity += qty;
									p->buy_orders[i]->orders[j]->trader->properties[k]->money_owned -= (qty * p->buy_orders[i]->price);
								}
							}
							
							char t[30];

							if(p->buy_orders[i]->orders[j]->trader->disconnected == 0){
								sprintf(t, "FILL %d %ld;", p->buy_orders[i]->orders[j]->order_id, qty);
								write(p->buy_orders[i]->orders[j]->trader->exchange_file, t, strlen(t));
								kill(p->buy_orders[i]->orders[j]->trader->pid, SIGUSR1);
							}

							for(int k = 0; k < trader->property_num; k++){
								if (strcmp(trader->properties[k]->product_name, p->product_name) == 0){
									trader->properties[k]->quantity -= qty;
									trader->properties[k]->money_owned += (qty * p->buy_orders[i]->price - f);
								}
							}
							sprintf(t, "FILL %d %ld;", order_id, qty);
							write(trader->exchange_file, t, strlen(t));
							kill(trader->pid, SIGUSR1);

							p->buy_orders[i]->orders[j]->quantity -= qty;
							if (p->buy_orders[i]->orders[j]->quantity == 0){
								free(p->buy_orders[i]->orders[j]);
								for(int k = j; k < p->buy_orders[i]->order_num - 1; k++){
									p->buy_orders[i]->orders[k] = p->buy_orders[i]->orders[k + 1];
								}
								p->buy_orders[i]->order_num--;
								p->buy_orders[i]->orders = realloc(p->buy_orders[i]->orders, p->buy_orders[i]->order_num);
							}
							p->buy_orders[i]->total_amount -= qty;
							qty = 0;
							break;
						}else{
							long int f = (long int)(p->buy_orders[i]->orders[j]->quantity * p->buy_orders[i]->price * 0.01 + 0.5);
							printf("%s Match: Order %d [T%d], New Order %d [T%d], value: $%ld, fee: $%ld.\n", LOG_PREFIX, p->buy_orders[i]->orders[j]->order_id, 
							p->buy_orders[i]->orders[j]->trader->trader_id, order_id, trader->trader_id, p->buy_orders[i]->orders[j]->quantity * p->buy_orders[i]->price,
							f);
							fee += f;

							for(int k = 0; k < p->buy_orders[i]->orders[j]->trader->property_num; k++){
								if (strcmp(p->buy_orders[i]->orders[j]->trader->properties[k]->product_name, p->product_name) == 0){
									p->buy_orders[i]->orders[j]->trader->properties[k]->quantity += p->buy_orders[i]->orders[j]->quantity;
									p->buy_orders[i]->orders[j]->trader->properties[k]->money_owned -= (p->buy_orders[i]->orders[j]->quantity * p->buy_orders[i]->price);
								}
							}
							
							char t[30];
							
							if(p->buy_orders[i]->orders[j]->trader->disconnected == 0){
								sprintf(t, "FILL %d %ld;", p->buy_orders[i]->orders[j]->order_id, p->buy_orders[i]->orders[j]->quantity);
								write(p->buy_orders[i]->orders[j]->trader->exchange_file, t, strlen(t));
								kill(p->buy_orders[i]->orders[j]->trader->pid, SIGUSR1);
							}

							for(int k = 0; k < trader->property_num; k++){
								if (strcmp(trader->properties[k]->product_name, p->product_name) == 0){
									trader->properties[k]->quantity -= p->buy_orders[i]->orders[j]->quantity;
									trader->properties[k]->money_owned += (p->buy_orders[i]->orders[j]->quantity * p->buy_orders[i]->price - f);
								}
							}
							sprintf(t, "FILL %d %ld;", order_id, p->buy_orders[i]->orders[j]->quantity);
							write(trader->exchange_file, t, strlen(t));
							kill(trader->pid, SIGUSR1);

							qty -= p->buy_orders[i]->orders[j]->quantity;

							p->buy_orders[i]->total_amount -= p->buy_orders[i]->orders[j]->quantity;
							p->buy_orders[i]->orders[j]->quantity = 0;
							free(p->buy_orders[i]->orders[j]);
							for(int m = j; m < p->buy_orders[i]->order_num - 1; m++){
								p->buy_orders[i]->orders[m] = p->buy_orders[i]->orders[m + 1];
							}
							p->buy_orders[i]->order_num--;
							if(p->buy_orders[i]->order_num == 0){
								free_Order_lv(p->buy_orders[i]);
								for(int m = i; m < p->buy_num - 1; m++){
									p->buy_orders[m] = p->buy_orders[m + 1];
								}
								p->buy_num--;
								p->buy_orders = realloc(p->buy_orders, sizeof(Order_lv*) * p->buy_num);
								clear_lv = 1;
								break;
							}else{
								p->buy_orders[i]->orders = realloc(p->buy_orders[i]->orders, sizeof(Order*) * p->buy_orders[i]->order_num);
							}
							if(qty == 0){
								free(cpy);
								return 1;
							}
							j -= 1;
						}
					}
					if(qty == 0){
						free(cpy);
						return 1;
					}
					if (clear_lv == 1){
						i -= 1;
					}
				}
			}

			int find = 0;
			for(int i = 0; i < p->sell_num; i++){
				if(p->sell_orders[i]->price == price){
					find = 1;
					p->sell_orders[i]->order_num++;
					p->sell_orders[i]->orders = realloc(p->sell_orders[i]->orders, sizeof(Order*) * p->sell_orders[i]->order_num);
					p->sell_orders[i]->total_amount += qty;
					Order* o = malloc(sizeof(Order));
					o->order_id = order_id;
					o->quantity = qty;
					o->trader = trader;
					p->sell_orders[i]->orders[p->sell_orders[i]->order_num - 1] = o;
					break;
				}else if(p->sell_orders[i]->price < price){
					find = 1;
					p->sell_num++;
					p->sell_orders = realloc(p->sell_orders, sizeof(Order_lv*) * p->sell_num);
					for(int k = p->sell_num - 1; k >i; k--){
						p->sell_orders[k] = p->sell_orders[k - 1];
					}
					Order_lv* ol = malloc(sizeof(Order_lv));
					ol->total_amount = qty;
					ol->order_num = 1;
					ol->price = price;
					ol->orders = malloc(sizeof(Order*) * ol->order_num);
					Order* o = malloc(sizeof(Order));
					o->order_id = order_id;
					o->quantity = qty;
					o->trader = trader;
					ol->orders[0] = o;
					p->sell_orders[i] = ol;
					break;
				}
			}

			if(!find){
				p->sell_num++;
				p->sell_orders = realloc(p->sell_orders, sizeof(Order_lv*) * p->sell_num);
				Order_lv* ol = malloc(sizeof(Order_lv));
				ol->total_amount = qty;
				ol->order_num = 1;
				ol->price = price;
				ol->orders = malloc(sizeof(Order*) * ol->order_num);
				Order *o = malloc(sizeof(Order));
				o->order_id = order_id;
				o->quantity = qty;
				o->trader = trader;
				ol->orders[0] = o;
				p->sell_orders[p->sell_num - 1] = ol;
			}
		}
		
		free(cpy);
		return 1;
	}
	free(cpy);
	return 0;
}

void disconnect_handler(int signo, siginfo_t *si, void *data) {//do not free them since we need to change their properties after this
	for(int i = 0; i < trader_num; i++){
		if(trader_list[i]->pid == si->si_pid){
			printf("%s Trader %d disconnected\n", LOG_PREFIX, trader_list[i]->trader_id);
			trader_list[i]->disconnected = 1;
			char exchange_file[20];
			char trader_file[20];
			sprintf(exchange_file, FIFO_EXCHANGE, trader_list[i]->trader_id);
			sprintf(trader_file, FIFO_TRADER, trader_list[i]->trader_id);
			unlink(exchange_file);
			unlink(trader_file);
			disconneted_num++;
			break;
		}
	}
}

void print_position(){
	printf("%s\t--POSITIONS--\n", LOG_PREFIX);
	for(int i = 0; i < trader_num; i++){
		printf("%s\tTrader %d: ", LOG_PREFIX, trader_list[i]->trader_id);
		int j = 0;
		while(j < trader_list[i]->property_num - 1){
			printf("%s %ld ($%ld), ", trader_list[i]->properties[j]->product_name, trader_list[i]->properties[j]->quantity, trader_list[i]->properties[j]->money_owned);
			j++;
		}
		printf("%s %ld ($%ld)\n", trader_list[i]->properties[j]->product_name, trader_list[i]->properties[j]->quantity, trader_list[i]->properties[j]->money_owned);
	}
}

void print_orderbook(){
	printf("%s\t--ORDERBOOK--\n", LOG_PREFIX);
	for(int i = 0; i < orderbook->product_num; i++){
		printf("%s\tProduct: %s; Buy levels: %d; Sell levels: %d\n", LOG_PREFIX, orderbook->products[i]->product_name, orderbook->products[i]->buy_num, orderbook->products[i]->sell_num);
		int j = 0;
		int k = 0;
		while (j < orderbook->products[i]->sell_num || k < orderbook->products[i]->buy_num){
			if (j == orderbook->products[i]->sell_num){
				if(orderbook->products[i]->buy_orders[k]->order_num > 1){
					printf("%s\t\tBUY %ld @ $%ld (%d orders)\n", LOG_PREFIX, orderbook->products[i]->buy_orders[k]->total_amount, orderbook->products[i]->buy_orders[k]->price, orderbook->products[i]->buy_orders[k]->order_num);
				}else{
					printf("%s\t\tBUY %ld @ $%ld (%d order)\n", LOG_PREFIX, orderbook->products[i]->buy_orders[k]->total_amount, orderbook->products[i]->buy_orders[k]->price, orderbook->products[i]->buy_orders[k]->order_num);
				}
				k++;
			}else if(k == orderbook->products[i]->buy_num){
				if(orderbook->products[i]->sell_orders[j]->order_num > 1){
					printf("%s\t\tSELL %ld @ $%ld (%d orders)\n", LOG_PREFIX, orderbook->products[i]->sell_orders[j]->total_amount, orderbook->products[i]->sell_orders[j]->price, orderbook->products[i]->sell_orders[j]->order_num);
				}else{
					printf("%s\t\tSELL %ld @ $%ld (%d order)\n", LOG_PREFIX, orderbook->products[i]->sell_orders[j]->total_amount, orderbook->products[i]->sell_orders[j]->price, orderbook->products[i]->sell_orders[j]->order_num);
				}
				j++;
			}else if (orderbook->products[i]->sell_orders[j]->price >= orderbook->products[i]->buy_orders[k]->price){
				if(orderbook->products[i]->sell_orders[j]->order_num > 1){
					printf("%s\t\tSELL %ld @ $%ld (%d orders)\n", LOG_PREFIX, orderbook->products[i]->sell_orders[j]->total_amount, orderbook->products[i]->sell_orders[j]->price, orderbook->products[i]->sell_orders[j]->order_num);
				}else{
					printf("%s\t\tSELL %ld @ $%ld (%d order)\n", LOG_PREFIX, orderbook->products[i]->sell_orders[j]->total_amount, orderbook->products[i]->sell_orders[j]->price, orderbook->products[i]->sell_orders[j]->order_num);
				}
				j++;
			}else if (orderbook->products[i]->sell_orders[j]->price < orderbook->products[i]->buy_orders[k]->price){
				if(orderbook->products[i]->buy_orders[k]->order_num > 1){
					printf("%s\t\tBUY %ld @ $%ld (%d orders)\n", LOG_PREFIX, orderbook->products[i]->buy_orders[k]->total_amount, orderbook->products[i]->buy_orders[k]->price, orderbook->products[i]->buy_orders[k]->order_num);
				}else{
					printf("%s\t\tBUY %ld @ $%ld (%d order)\n", LOG_PREFIX, orderbook->products[i]->buy_orders[k]->total_amount, orderbook->products[i]->buy_orders[k]->price, orderbook->products[i]->buy_orders[k]->order_num);
				}
				k++;
			}
		}
	}
	print_position();
}

void sig_recieved(int signo, siginfo_t *si, void *data){
	for(int i = 0; i < trader_num; i++){
		if(trader_list[i]->pid == si->si_pid){
			Trader* trader = trader_list[i];
			char *temp = malloc(sizeof(char));
			int temp_num = 1; // there should be one more, because we need to restore the readed content
			int valid = 0;
			while(read(trader->trader_file, &(temp[temp_num - 1]), 1) == 1){
				if(temp[temp_num - 1] == ';' || temp[temp_num - 1] == '\0'){
					temp[temp_num - 1] = '\0';
					printf("%s [T%d] Parsing command: <%s>\n", LOG_PREFIX, trader->trader_id, temp);
					if(strncasecmp(temp, "BUY ", 4) == 0){
						if(command_buy(trader, temp, temp_num) == 1){
							valid = 1;
							print_orderbook();
							break;
						}
					}
					if(strncasecmp(temp, "SELL ", 5) == 0){
						if(command_sell(trader, temp, temp_num) == 1){
							valid = 1;
							print_orderbook();
							break;
						}
					}
					if(strncasecmp(temp, "AMEND ", 6) == 0){
						if(command_amend(trader, temp, temp_num) == 1){
							valid = 1;
							print_orderbook();
							break;
						}
					}
					if(strncasecmp(temp, "CANCEL ", 7) == 0){
						if(command_cancel(trader, temp, temp_num) == 1){
							valid = 1;
							print_orderbook();
							break;
						}
					}

					break;
				}
				temp_num++;
				temp = realloc(temp, sizeof(char) * temp_num);
			}
			free(temp);

			if(!valid){
				char msg[] = "INVALID;";
				write(trader->exchange_file, msg, strlen(msg));
				kill(trader->pid, SIGUSR1);
			}
			break;
		}
	}
}

int main(int argc, char **argv) {
	if (argc <= 2){
		exit(0);
	}
	printf("%s Starting\n", LOG_PREFIX);
	orderbook = malloc(sizeof(Orderbook));
	fee = 0;

	initialize_orderbook(argv[1]);

	printf("%s Trading %d products:", LOG_PREFIX, orderbook->product_num);
	int i = 0;
	while(i < orderbook->product_num){
		printf(" %s", orderbook->products[i]->product_name);
		i++;
	}
	printf("\n");

	trader_num = argc - 2;
	trader_list = malloc(sizeof(Trader*) * trader_num);
	disconneted_num = 0;

	int j = 0;
	while(j < trader_num){
		initialize_traders(argv[j + 2], j);
		j++;
	}
	sleep(1);//wait for traders to initialize themselves
	char open[] = "MARKET OPEN;";

	for(int i = 0; i < trader_num; i++){
		write(trader_list[i]->exchange_file, open, strlen(open));
		kill(trader_list[i]->pid, SIGUSR1);
	}

	struct sigaction disconnect_action;
	disconnect_action.sa_sigaction = disconnect_handler;
	disconnect_action.sa_flags |= SA_SIGINFO;

	struct sigaction got_sig;
	got_sig.sa_sigaction = sig_recieved;
	got_sig.sa_flags |= SA_SIGINFO;

	sigaction(SIGCHLD,&disconnect_action,NULL);

	while(disconneted_num < trader_num){
		sigaction(SIGUSR1, &got_sig, NULL);
		pause();
	}

	printf("%s Trading completed\n", LOG_PREFIX);
	printf("%s Exchange fees collected: $%ld\n", LOG_PREFIX, fee);

	free_orderbook();
	free_traders();

	return 0;
}