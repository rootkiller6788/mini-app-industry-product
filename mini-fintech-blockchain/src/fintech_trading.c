#include "fintech_trading.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* ================================================================
 * L6: Limit Order Book with Price-Time Priority Matching
 * ================================================================ */

OrderBook* orderbook_create(const char* symbol) {
    OrderBook* ob = (OrderBook*)calloc(1, sizeof(OrderBook));
    if (!ob) return NULL;
    if (symbol) snprintf(ob->symbol, sizeof(ob->symbol), "%s", symbol);
    ob->next_order_id = 1;
    return ob;
}

int orderbook_place_order(OrderBook* ob, OrderSide side, OrderType type,
                           double price, double quantity, int account_id) {
    if (!ob || ob->order_count >= ORDERBOOK_MAX_ORDERS || quantity <= 0.0)
        return -1;
    if (type == ORDER_LIMIT && price <= 0.0) return -1;

    Order* o = &ob->orders[ob->order_count];
    o->id = ob->next_order_id++;
    o->side = side;
    o->type = type;
    o->price = (type == ORDER_MARKET) ? 0.0 : price;
    o->quantity = quantity;
    o->filled = 0.0;
    o->status = ORDER_OPEN;
    o->timestamp = (int64_t)time(NULL);
    o->account_id = account_id;
    snprintf(o->symbol, sizeof(o->symbol), "%s", ob->symbol);
    ob->order_count++;
    return o->id;
}

int orderbook_cancel_order(OrderBook* ob, int order_id) {
    if (!ob) return -1;
    for (int i = 0; i < ob->order_count; i++) {
        if (ob->orders[i].id == order_id && ob->orders[i].status == ORDER_OPEN) {
            ob->orders[i].status = ORDER_CANCELLED;
            return 0;
        }
    }
    return -1;
}

/* Price-time priority: buy=highest price first, sell=lowest price first.
 * Within same price level, earliest timestamp has priority. */

static int find_best_buy_idx(const OrderBook* ob) {
    int best = -1;
    double best_price = -1.0;
    int64_t best_time = INT64_MAX;
    for (int i = 0; i < ob->order_count; i++) {
        const Order* o = &ob->orders[i];
        if (o->side != ORDER_BUY || o->status != ORDER_OPEN) continue;
        if (o->type == ORDER_MARKET) return i;
        if (o->price > best_price ||
            (o->price == best_price && o->timestamp < best_time)) {
            best_price = o->price;
            best_time = o->timestamp;
            best = i;
        }
    }
    return best;
}

static int find_best_sell_idx(const OrderBook* ob) {
    int best = -1;
    double best_price = 1e15;
    int64_t best_time = INT64_MAX;
    for (int i = 0; i < ob->order_count; i++) {
        const Order* o = &ob->orders[i];
        if (o->side != ORDER_SELL || o->status != ORDER_OPEN) continue;
        if (o->type == ORDER_MARKET) return i;
        if (o->price < best_price ||
            (o->price == best_price && o->timestamp < best_time)) {
            best_price = o->price;
            best_time = o->timestamp;
            best = i;
        }
    }
    return best;
}

int orderbook_match(OrderBook* ob, TradeHistory* th) {
    if (!ob) return 0;
    int trades = 0;

    while (1) {
        int bi = find_best_buy_idx(ob);
        int si = find_best_sell_idx(ob);
        if (bi < 0 || si < 0) break;

        Order* buy = &ob->orders[bi];
        Order* sell = &ob->orders[si];

        /* Determine match price */
        double match_price;
        if (buy->type == ORDER_MARKET && sell->type == ORDER_MARKET)
            match_price = ob->last_price > 0.0 ? ob->last_price : 100.0;
        else if (buy->type == ORDER_MARKET)
            match_price = sell->price;
        else if (sell->type == ORDER_MARKET)
            match_price = buy->price;
        else if (buy->price >= sell->price)
            match_price = sell->price;
        else
            break;

        double buy_rem = buy->quantity - buy->filled;
        double sell_rem = sell->quantity - sell->filled;
        double match_qty = (buy_rem < sell_rem) ? buy_rem : sell_rem;

        buy->filled += match_qty;
        sell->filled += match_qty;

        buy->status = (buy->filled >= buy->quantity * 0.9999)
                      ? ORDER_FILLED : ORDER_PARTIAL;
        sell->status = (sell->filled >= sell->quantity * 0.9999)
                       ? ORDER_FILLED : ORDER_PARTIAL;

        ob->last_price = match_price;
        ob->total_volume += match_qty;

        if (th) trade_history_add(th, buy->id, sell->id, match_price, match_qty);
        trades++;
    }

    ob->best_bid = orderbook_get_best_bid(ob);
    ob->best_ask = orderbook_get_best_ask(ob);
    return trades;
}

double orderbook_get_best_bid(const OrderBook* ob) {
    if (!ob) return 0.0;
    double best = 0.0;
    for (int i = 0; i < ob->order_count; i++) {
        const Order* o = &ob->orders[i];
        if (o->side == ORDER_BUY && o->status == ORDER_OPEN && o->price > best)
            best = o->price;
    }
    return best;
}

double orderbook_get_best_ask(const OrderBook* ob) {
    if (!ob) return 0.0;
    double best = 1e15;
    int found = 0;
    for (int i = 0; i < ob->order_count; i++) {
        const Order* o = &ob->orders[i];
        if (o->side == ORDER_SELL && o->status == ORDER_OPEN && o->price < best) {
            best = o->price;
            found = 1;
        }
    }
    return found ? best : 0.0;
}

double orderbook_get_spread(const OrderBook* ob) {
    double bid = orderbook_get_best_bid(ob);
    double ask = orderbook_get_best_ask(ob);
    if (bid <= 0.0 || ask <= 0.0) return 0.0;
    return ask - bid;
}

double orderbook_get_midprice(const OrderBook* ob) {
    double bid = orderbook_get_best_bid(ob);
    double ask = orderbook_get_best_ask(ob);
    if (bid <= 0.0 || ask <= 0.0) return 0.0;
    return (bid + ask) / 2.0;
}

void orderbook_print(const OrderBook* ob) {
    if (!ob) return;
    printf("OrderBook [%s]: %d orders Bid=%.2f Ask=%.2f Mid=%.2f Last=%.2f Vol=%.0f\\n",
           ob->symbol, ob->order_count, ob->best_bid, ob->best_ask,
           orderbook_get_midprice(ob), ob->last_price, ob->total_volume);
}

void orderbook_destroy(OrderBook* ob) { free(ob); }

/* ================================================================
 * L6: Trade History with VWAP calculation
 * ================================================================ */

TradeHistory* trade_history_create(void) {
    return (TradeHistory*)calloc(1, sizeof(TradeHistory));
}

void trade_history_add(TradeHistory* th, int buy_id, int sell_id,
                        double price, double quantity) {
    if (!th || th->trade_count >= TRADE_MAX_HISTORY) return;
    Trade* t = &th->trades[th->trade_count];
    t->trade_id = th->trade_count + 1;
    t->buy_order_id = buy_id;
    t->sell_order_id = sell_id;
    t->price = price;
    t->quantity = quantity;
    t->timestamp = (int64_t)time(NULL);
    th->trade_count++;
}

/* Volume-Weighted Average Price (VWAP):
 * VWAP = sum(P_i * Q_i) / sum(Q_i)
 * Used as execution quality benchmark. */

double trade_history_vwap(const TradeHistory* th) {
    if (!th || th->trade_count == 0) return 0.0;
    double total_pq = 0.0, total_q = 0.0;
    for (int i = 0; i < th->trade_count; i++) {
        total_pq += th->trades[i].price * th->trades[i].quantity;
        total_q += th->trades[i].quantity;
    }
    return (total_q > 0.0) ? total_pq / total_q : 0.0;
}

void trade_history_print(const TradeHistory* th) {
    if (!th) return;
    printf("Trades: %d VWAP=%.4f\\n", th->trade_count, trade_history_vwap(th));
    int show = (th->trade_count < 10) ? th->trade_count : 10;
    for (int i = th->trade_count - show; i < th->trade_count; i++)
        printf("  #%d: %.4f x %.2f\\n",
               th->trades[i].trade_id, th->trades[i].price, th->trades[i].quantity);
}

void trade_history_destroy(TradeHistory* th) { free(th); }

/* ================================================================
 * L8: Automated Market Maker (AMM) - Constant Product x*y=k
 *
 * Swap X for Y: (x + dx_eff) * (y - dy) = x * y
 *   dy = y * dx_eff / (x + dx_eff)  where dx_eff = dx * (1 - fee)
 *
 * Add Liquidity: provide dx of X and dy of Y proportionally.
 *   LP issued = total_LP * dx / x (minted in proportion to pool share)
 *
 * Remove Liquidity: burn LP tokens, withdraw proportional reserves.
 *   share = lp_burned / total_LP
 *   rx_out = share * x,  ry_out = share * y
 *
 * Impermanent Loss: loss vs HODL when price changes.
 *   IL = 2*sqrt(P_ratio) / (1 + P_ratio) - 1
 *
 * Reference: Adams, Zinsmeister & Robinson (2020) "Uniswap v2 Core"
 * ================================================================ */

LiquidityPool* amm_create_pool(const char* token_x, const char* token_y,
                                double reserve_x, double reserve_y,
                                double fee_rate) {
    if (reserve_x <= 0.0 || reserve_y <= 0.0) return NULL;
    LiquidityPool* pool = (LiquidityPool*)calloc(1, sizeof(LiquidityPool));
    if (!pool) return NULL;
    pool->pool_id = 1;
    snprintf(pool->token_x, sizeof(pool->token_x), "%s", token_x);
    snprintf(pool->token_y, sizeof(pool->token_y), "%s", token_y);
    pool->reserve_x = reserve_x;
    pool->reserve_y = reserve_y;
    pool->k_constant = reserve_x * reserve_y;
    pool->total_lp_tokens = sqrt(reserve_x * reserve_y);
    pool->fee_rate = (fee_rate > 0.0) ? fee_rate : 0.003;
    return pool;
}

SwapResult amm_swap_x_for_y(LiquidityPool* pool, double dx) {
    SwapResult result;
    memset(&result, 0, sizeof(result));
    if (!pool || dx <= 0.0) return result;

    double fee = dx * pool->fee_rate;
    double dx_eff = dx - fee;
    double x = pool->reserve_x;
    double y = pool->reserve_y;

    /* Constant product invariant */
    double dy = y * dx_eff / (x + dx_eff);
    double spot_price = y / x;
    double effective_price = dy / dx;

    result.amount_out = dy;
    result.fee = fee;
    result.effective_price = effective_price;
    result.price_impact = (effective_price / spot_price - 1.0) * 100.0;

    pool->reserve_x += dx_eff;
    pool->reserve_y -= dy;
    pool->k_constant = pool->reserve_x * pool->reserve_y;
    return result;
}

SwapResult amm_swap_y_for_x(LiquidityPool* pool, double dy) {
    SwapResult result;
    memset(&result, 0, sizeof(result));
    if (!pool || dy <= 0.0) return result;

    double fee = dy * pool->fee_rate;
    double dy_eff = dy - fee;
    double x = pool->reserve_x;
    double y = pool->reserve_y;

    double dx = x * dy_eff / (y + dy_eff);
    double spot_price = x / y;
    double effective_price = dx / dy;

    result.amount_out = dx;
    result.fee = fee;
    result.effective_price = effective_price;
    result.price_impact = (effective_price / spot_price - 1.0) * 100.0;

    pool->reserve_y += dy_eff;
    pool->reserve_x -= dx;
    pool->k_constant = pool->reserve_x * pool->reserve_y;
    return result;
}

double amm_get_price_x(const LiquidityPool* pool) {
    if (!pool || pool->reserve_x <= 0.0) return 0.0;
    return pool->reserve_y / pool->reserve_x;
}

double amm_get_price_y(const LiquidityPool* pool) {
    if (!pool || pool->reserve_y <= 0.0) return 0.0;
    return pool->reserve_x / pool->reserve_y;
}

double amm_add_liquidity(LiquidityPool* pool, double dx, double dy) {
    if (!pool || dx <= 0.0 || dy <= 0.0) return 0.0;
    double ratio = dx / pool->reserve_x;
    double expected_dy = pool->reserve_y * ratio;
    /* Verify proportional deposit within 1% tolerance */
    if (fabs(dy - expected_dy) / expected_dy > 0.01) return -1.0;

    double lp_issued = pool->total_lp_tokens * dx / pool->reserve_x;
    pool->reserve_x += dx;
    pool->reserve_y += dy;
    pool->total_lp_tokens += lp_issued;
    pool->k_constant = pool->reserve_x * pool->reserve_y;
    return lp_issued;
}

double amm_remove_liquidity(LiquidityPool* pool, double lp_tokens,
                             double* rx_out, double* ry_out) {
    if (!pool || lp_tokens <= 0.0 || lp_tokens > pool->total_lp_tokens)
        return -1.0;

    double share = lp_tokens / pool->total_lp_tokens;
    double rx = pool->reserve_x * share;
    double ry = pool->reserve_y * share;

    pool->reserve_x -= rx;
    pool->reserve_y -= ry;
    pool->total_lp_tokens -= lp_tokens;
    pool->k_constant = pool->reserve_x * pool->reserve_y;

    if (rx_out) *rx_out = rx;
    if (ry_out) *ry_out = ry;
    return share;
}

/* Impermanent Loss: value change vs holding tokens outside pool.
 * IL = 2*sqrt(r) / (1+r) - 1  where r = price_new / price_initial */

double amm_impermanent_loss(double price_ratio) {
    if (price_ratio <= 0.0) return -1.0;
    double sqrt_r = sqrt(price_ratio);
    return 2.0 * sqrt_r / (1.0 + price_ratio) - 1.0;
}

void amm_print_pool(const LiquidityPool* pool) {
    if (!pool) return;
    printf("AMM %s/%s: x=%.4f y=%.4f k=%.4f LP=%.4f Fee=%.2f%%\\n",
           pool->token_x, pool->token_y, pool->reserve_x, pool->reserve_y,
           pool->k_constant, pool->total_lp_tokens, pool->fee_rate * 100.0);
    printf("  1 %s = %.4f %s | 1 %s = %.4f %s\\n",
           pool->token_x, amm_get_price_x(pool), pool->token_y,
           pool->token_y, amm_get_price_y(pool), pool->token_x);
}

void amm_destroy_pool(LiquidityPool* pool) { free(pool); }

/* ================================================================
 * L7: Trading Signals
 *
 * SMA Crossover:
 *   Golden Cross: short SMA crosses above long SMA -> buy (+1)
 *   Death Cross: short SMA crosses below long SMA -> sell (-1)
 *
 * RSI (Relative Strength Index):
 *   RSI = 100 - 100/(1 + RS)  where RS = avg_gain / avg_loss
 *   Overbought: RSI > 70, Oversold: RSI < 30
 * ================================================================ */

double signal_sma_crossover(const double* prices, int n,
                             int short_window, int long_window) {
    if (!prices || n < long_window || short_window >= long_window) return 0.0;
    if (short_window < 2 || long_window < 2) return 0.0;

    double short_sum = 0.0, long_sum = 0.0;
    for (int i = n - short_window; i < n; i++) short_sum += prices[i];
    for (int i = n - long_window; i < n; i++) long_sum += prices[i];
    double short_sma = short_sum / short_window;
    double long_sma = long_sum / long_window;

    /* Previous period SMA for crossover detection */
    double prev_short_sum = 0.0, prev_long_sum = 0.0;
    for (int i = n - short_window - 1; i < n - 1; i++) prev_short_sum += prices[i];
    for (int i = n - long_window - 1; i < n - 1; i++) prev_long_sum += prices[i];
    double prev_short = prev_short_sum / short_window;
    double prev_long = prev_long_sum / long_window;

    if (prev_short <= prev_long && short_sma > long_sma)
        return 1.0;  /* Golden cross: buy */
    if (prev_short >= prev_long && short_sma < long_sma)
        return -1.0; /* Death cross: sell */
    return 0.0;
}

double signal_rsi(const double* prices, int n, int period) {
    if (!prices || n < period + 1 || period < 2) return 50.0;

    double avg_gain = 0.0, avg_loss = 0.0;

    /* Initial average using simple moving average of price changes */
    for (int i = n - period; i < n; i++) {
        double change = prices[i] - prices[i - 1];
        if (change > 0.0) avg_gain += change;
        else avg_loss += -change;
    }
    avg_gain /= period;
    avg_loss /= period;

    if (avg_loss < 1e-12) return 100.0;
    double rs = avg_gain / avg_loss;
    return 100.0 - 100.0 / (1.0 + rs);
}
