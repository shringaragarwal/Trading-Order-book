#ifndef ORDER_BOOK_H
#define ORDER_BOOK_H

#include<map>
#include<atomic>
#include<memory>
#include<mutex>
#include<list>
#include<utility> //For std::pair
#include<functional> //For std::greater

using namespace std;

//Represents a single order in the book
struct Order{
    long long id;
    unsigned int quantity;
};

//Information needed for O(1)cancellation
struct OrderInfo{
    double price;
    bool is_bid;
    //An iterator to the order's position in the list at its price level
    list<Order>::iterator position;
};

class OrderBook{
    public:
        //Add an order to the book
        void add_order(long long order_id, double price, unsigned int quantity, bool is_bid);

        //Cancels an existing order from the book
        void cancel_order(long long order_id);

        //Atomically gets the best bid and ask prices. Lock free for readers.
        pair<double,double>get_best_bid_ask();

        //Helper to print the current state of the book.
        void print_book();

    private:
        //Updates the atomic best bid/ask prices. Must be called inside a lock.
        void update_best_prices();

        //Bids are stored in descending order of pricde.
        map<double,list<Order>,greater<double>>bids_;
        //Asks are stored in ascending order of price.
        map<double,list<Order>>asks_;

        //A map for fast O(log N) lookup of orders by ID for cancellation.
        map<long long, OrderInfo>all_orders_;

        //Atomics for the fast-path,lock-free queries.
        atomic<double>best_bid_{0.0};
        atomic<double>best_ask_{0.0};

        //A single mutex to protect access to the maps.
        mutex book_mutex_;
    
};

#endif // ORDER_BOOK_H