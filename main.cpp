#include "OrderBook.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <random>
#include <vector>
#include<condition_variable>

//The producer thread simulates a market data feed.
void market_data_producer(std::shared_ptr<OrderBook>book, std::atomic<bool>& running, std::condition_variable& cv, std::mutex& cv_mutex){
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<>price_dist(96.0,104.0);
    std::uniform_int_distribution<>quantity_dist(1,100);
    std::uniform_int_distribution<>action_dist(0,10);  //0-7: add, 8=10: cancel;

    long long order_id_counter = 1;
    std::vector<long long> active_orders;

    while(running){
        //Create a unique lock for the condition variable
        std::unique_lock<std::mutex>lock(cv_mutex);

        //Wait for 100ms OR until main()signals us to stop.
        //The lambda predicate [!running] prevents spurious wakeups.

        if(cv.wait_for(lock,std::chrono::microseconds(100),[&running]{return !running;})){
            break;
        }

        lock.unlock();

        double price = std::round(price_dist(gen) * 100.0)/100.0;
        unsigned int quantity = quantity_dist(gen);
        bool is_bid = (price<100.0);
        long long id = order_id_counter++;

        if(action_dist(gen)>2 || active_orders.empty()){
            //Add order
            book->add_order(id,price,quantity,is_bid);
            active_orders.push_back(id);
        }
        else{
            //Cancel order
            std::uniform_int_distribution<>cancel_dist(0,active_orders.size()-1);
            int idx_to_cancel = cancel_dist(gen);
            long long id_to_cancel = active_orders[idx_to_cancel];
            book->cancel_order(id_to_cancel);

            //Swap and pop for efficient removal
            std::swap(active_orders[idx_to_cancel],active_orders.back());
            active_orders.pop_back();
        }
    }
}

//The consumer thread simulates a trading strategy querying the book.
void strategy_consumer(std::shared_ptr<OrderBook>book, std::atomic<bool>& running, std::atomic<long long>& query_count){
    while(running){
        auto[best_bid,best_ask]=book->get_best_bid_ask();
        //In real strategy, we would use these values to make decisions.
        //for this project we just increment a counter.

        query_count++;
    }
}

int main(){
    //Use smart pointers to manage the lifetime of the OrderBook
    //and share it safely between threads.
    auto book = std::make_shared<OrderBook>();

    //Atomics to control the simulation.
    std::atomic<bool>running{true};
    std::atomic<long long>query_count{0};

    //Create a mutex and condition variable for signaling
    std:: mutex cv_mutex;
    std::condition_variable cv;

    std::cout<<" Starting Simulation..."<<endl;

    //Launch threads
    std::thread producer_thread(market_data_producer,book,std::ref(running),std::ref(cv),std::ref(cv_mutex));
    std::thread consumer_thread(strategy_consumer,book,std::ref(running),std::ref(query_count));

    //Let the simulation run for 5 seconds.
    std::this_thread::sleep_for(std::chrono::seconds(1));
    
    //Signal threads to stop
    std::cout<<endl<<"Stopping Simulation..."<<endl;
    {
        std::lock_guard<std::mutex>lock(cv_mutex);
        running = false;
    }
    //Notify the wiating thread(s)that the state has changed.
    cv.notify_all();
   
    //wait for threads to finish their current work and exit.
    producer_thread.join();
    consumer_thread.join();

    std::cout<<" Simulation Finished. "<<endl<<endl;
    std::cout<<"Final Order Book State: "<<endl;
    book->print_book();

    cout<<"Strategy Thread performed "<<query_count.load()<<" queries in 5 seconds."<<endl;
    
    return 0;
}