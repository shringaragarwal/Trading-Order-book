#include "OrderBook.h"
#include<iostream>
#include<iomanip>  //For setprecision

using namespace std;

void OrderBook::add_order(long long order_id, double price, unsigned int quantity, bool is_bid){
    //Lock the mutex to ensure thread-safe across to the maps.
    lock_guard<mutex>lock(book_mutex_);

    if(is_bid){
        //Find or create the list of orders at this price level.
        auto& orders_at_price = bids_[price];

        //Add the new order to the front of the list.
        orders_at_price.push_front({order_id,quantity});

        //Store info for quick cancellation
        all_orders_[order_id]={price,is_bid,orders_at_price.begin()};
    }
    else{
        auto& orders_at_price = asks_[price];
        orders_at_price.push_front({order_id,quantity});
        all_orders_[order_id]={price,is_bid,orders_at_price.begin()};
    }

    //After modifying the book, update the cached best prices.
    update_best_prices();
}

void OrderBook::cancel_order(long long order_id){
    std::lock_guard<std::mutex>lock(book_mutex_);

    //Find the order in the cancellation map. O(log N)
    auto it = all_orders_.find(order_id);
    if(it == all_orders_.end()){
        //Order not found.
        return;
    }

    const auto& info = it->second;

    //Remove the order from the correct book (bids or asks). O(1)
    if(info.is_bid){
        auto& price_level = bids_.at(info.price);
        price_level.erase(info.position);
        if(price_level.empty()){
            bids_.erase(info.price);
        }
    }
    else{
        auto& price_level = asks_.at(info.price);
        price_level.erase(info.position);
        if(price_level.empty()){
            asks_.erase(info.price);
        }
    }

    //Remove the order from the cancellation map.
    all_orders_.erase(it);

    update_best_prices();
}

void OrderBook::update_best_prices(){

    //This function must be called from within a locked context.
    double new_best_bid = bids_.empty()?0.0:bids_.begin()->first;
    double new_best_ask = asks_.empty()?0.0:asks_.begin()->first;

    // Use memory order release to make sure that any prior writes
    // (like adding the order to the map) are visible to other threads
    // before they see the new best price.
    best_bid_.store(new_best_bid, std::memory_order_release);
    best_ask_.store(new_best_ask, std::memory_order_release);
}

std::pair<double,double> OrderBook::get_best_bid_ask(){

    //This is the lock free part.
    //Use memory_order_acquire to ensure that if we read the new best price,
    //we also see all memory writes that happened before it was updated.
    double bid = best_bid_.load(std::memory_order_acquire);
    double ask = best_ask_.load(std::memory_order_acquire);

    return {bid,ask};
}

void OrderBook::print_book(){
    std::lock_guard<std::mutex>guard(book_mutex_);
    std::cout<<"--- ORDER BOOK ---"<<endl;
    std::cout<<std::fixed<<std::setprecision(2);

    std::cout<<"ASKS:"<<endl;
    if(asks_.empty()){
        std::cout<<"empty"<<endl;
    }
    else{
        //Iterate in reverse to show latest ask at the "top" near the spread
        for(auto it = asks_.rbegin();it!=asks_.rend();++it){
            for(const auto& order  :it->second){
                std::cout<<"Price: "<<it->first<<" | Qty: "<<order.quantity<<" (ID: "<<order.id<<")"<<endl;
            }
        }
    }

    std::cout<<"------------"<<endl;

    std::cout<<"BIDS:"<<endl;
    if(bids_.empty()){
        std::cout<<"empty"<<endl;
    }
    else{
        for(const auto& [price, orders]: bids_){
            for(const auto& order:orders){
                std::cout<<" Price: "<<price<<" |Qty: "<<order.quantity<<" (ID: "<<order.id<<")"<<endl;
            }
        }
    }
    std::cout<<"------------"<<endl;
}