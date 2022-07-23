#ifndef SPX_EXCHANGE_H
#define SPX_EXCHANGE_H

#include "spx_common.h"

#define LOG_PREFIX "[SPX]"

typedef struct Properties Properties;
typedef struct Trader Trader;
typedef struct Order Order;
typedef struct Order_lv Order_lv;
typedef struct Product Product;
typedef struct Orderbook Orderbook;

struct Properties{
    char* product_name;
    long int quantity;
    long int money_owned;
};

struct Trader{
    int trader_id;
    int disconnected;
    
    Properties** properties;
    int property_num;

    pid_t pid;
    int owned_order;

    int exchange_file;
    int trader_file;
};

struct Order{
    Trader* trader;
    long int quantity;
    int order_id;
};

struct Order_lv{
    long int price;
    Order** orders;
    int order_num;
    long int total_amount;
};

struct Product{
    char* product_name;
    int name_len;

    Order_lv** sell_orders;
    int sell_num;

    Order_lv** buy_orders;
    int buy_num;
};

struct Orderbook
{
    Product** products;
    int product_num;
};

#endif