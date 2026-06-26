#ifndef FINTECH_TRADING_H
#define FINTECH_TRADING_H

#include <stdbool.h>
#include <stdint.h>

/* L6: Order Matching Engine - Limit Order Book (LOB)
 * Price-time priority: best price first, then earliest order.
 * Reference: Harris "Trading and Exchanges" (2003)
 *
 * L8: Automated Market Maker (AMM) - Constant Product
 * x * y = k (constant product invariant)
 * Reference: Adams et al. "Uniswap v2 Core" (2020)
 */

#define ORDERBOOK_MAX_ORDERS 1024
#define AMM_MAX_POOLS 32

typedef enum {
    ORDER_BUY,
    ORDER_SELL,
    ORDER_SIDE_COUNT
} OrderSide;

typedef enum {
    ORDER_LIMIT,
    ORDER_MARKET,
    ORDER_STOP,
    ORDER_TYPE_COUNT
} OrderType;

typedef enum {
    ORDER_OPEN,
    ORDER_PARTIAL,
    ORDER_FILLED,
    ORDER_CANCELLED,
    ORDER_STATUS_COUNT
} OrderStatus;

typedef struct {
    int         id;
    OrderSide   side;
    OrderType   type;
    double      price;        /* limit price (0 for market orders) */
    double      quantity;
    double      filled;
    OrderStatus status;
    int64_t     timestamp;
    char        symbol[16];
    int         account_id;
} Order;

typedef struct {
    Order       orders[ORDERBOOK_MAX_ORDERS];
    int         order_count;
    int         next_order_id;
    double      best_bid;
    double      best_ask;
    double      last_price;
    double      total_volume;
    char        symbol[16];
} OrderBook;

typedef struct {
    int         trade_id;
    int         buy_order_id;
    int         sell_order_id;
    double      price;
    double      quantity;
    int64_t     timestamp;
} Trade;

#define TRADE_MAX_HISTORY 4096
typedef struct {
    Trade       trades[TRADE_MAX_HISTORY];
    int         trade_count;
} TradeHistory;

/* L8: AMM Pool - Constant Product Market Maker
 * Invariant: x * y = k
 * Price: p = y / x (amount of token Y per token X)
 * Swap: (x + dx)(y - dy) = k => dy = y * dx / (x + dx) */

typedef struct {
    int         pool_id;
    char        token_x[16];
    char        token_y[16];
    double      reserve_x;
    double      reserve_y;
    double      total_lp_tokens;   /* total liquidity provider tokens */
    double      fee_rate;           /* e.g., 0.003 = 0.3% */
    double      k_constant;         /* invariant: x * y */
} LiquidityPool;

typedef struct {
    double      amount_out;
    double      price_impact;       /* percentage price change */
    double      fee;
    double      effective_price;
} SwapResult;

/* Order Book API */
OrderBook* orderbook_create(const char* symbol);
int        orderbook_place_order(OrderBook* ob, OrderSide side, OrderType type,
                                  double price, double quantity, int account_id);
int        orderbook_cancel_order(OrderBook* ob, int order_id);
int        orderbook_match(OrderBook* ob, TradeHistory* th);
double     orderbook_get_best_bid(const OrderBook* ob);
double     orderbook_get_best_ask(const OrderBook* ob);
double     orderbook_get_spread(const OrderBook* ob);
double     orderbook_get_midprice(const OrderBook* ob);
void       orderbook_print(const OrderBook* ob);
void       orderbook_destroy(OrderBook* ob);

/* Trade History API */
TradeHistory* trade_history_create(void);
void          trade_history_add(TradeHistory* th, int buy_id, int sell_id,
                                 double price, double quantity);
double        trade_history_vwap(const TradeHistory* th);
void          trade_history_print(const TradeHistory* th);
void          trade_history_destroy(TradeHistory* th);

/* AMM API */
LiquidityPool* amm_create_pool(const char* token_x, const char* token_y,
                                double reserve_x, double reserve_y, double fee_rate);
SwapResult     amm_swap_x_for_y(LiquidityPool* pool, double dx);
SwapResult     amm_swap_y_for_x(LiquidityPool* pool, double dy);
double         amm_get_price_x(const LiquidityPool* pool);
double         amm_get_price_y(const LiquidityPool* pool);
double         amm_add_liquidity(LiquidityPool* pool, double dx, double dy);
double         amm_remove_liquidity(LiquidityPool* pool, double lp_tokens,
                                     double* rx_out, double* ry_out);
double         amm_impermanent_loss(double price_ratio);
void           amm_print_pool(const LiquidityPool* pool);
void           amm_destroy_pool(LiquidityPool* pool);

/* Trading Strategy API (L7) */
double signal_sma_crossover(const double* prices, int n,
                             int short_window, int long_window);
double signal_rsi(const double* prices, int n, int period);

#endif
