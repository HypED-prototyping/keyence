#include "keyence.hpp"

#include <iostream>
#include <vector>
#include <chrono>
#include <cstdio>


const int stripe_locations[63] =
{0, 100, 200, 300, 400, 500, 600, 700, 800, 900,
    1000, 1100, 1200, 1300, 1400, 1500, 1600, 1700, 1800, 1900,
    2000, 2100, 2200, 2300, 2400, 2500, 2600, 2700, 2800, 2900,
    3000, 3100, 3200, 3300, 3400, 3500, 3600, 3700, 3800, 3900,
    4000, 4008, 4016, 4024, 4032, 4040, 4048, 4056, 4064, 4072,
    4100, 4200, 4300, 4400,
    4500, 4508, 4516, 4524, 4532,
    4600, 4700, 4800, 4900};

Keyence::Keyence(GpioPinNumber config_pin_num, GpioPinNumber output_pin_num)
: config_pin { Gpio::get_pin(config_pin_num, PinMode::out, PudControl::off) },
output_pin { Gpio::get_pin(output_pin_num, PinMode::in, PudControl::down) },
current_count(0), new_stripe(false), stop_flag(true)
{}

Keyence::~Keyence()
{
    this->stop();
}

void Keyence::calibrate()
{
    this->config_pin.write(false);
    std::this_thread::sleep_for(std::chrono::milliseconds(3500));
    this->config_pin.write(true);
    printf("Setup complete\n");
}

void Keyence::start()
{
    this->stop_flag = false;
    this->counting_thread = std::thread(&Keyence::cycle_tracker, this);
}

void Keyence::stop()
{
    this->stop_flag = false;
    this->counting_thread.join();
}

bool Keyence::has_new_stripe()
{
    return this->new_stripe.load(std::memory_order_relaxed);
}

int Keyence::get_count()
{
    return this->current_count.load(std::memory_order_relaxed);
}

typedef std::chrono::time_point<std::chrono::high_resolution_clock> Timepoint;
typedef std::chrono::high_resolution_clock Stopwatch;

Timepoint getRawTime(){
    return Stopwatch::now();
}

long getTimeSince(std::chrono::time_point<std::chrono::high_resolution_clock> t)
{
    return std::chrono::duration_cast<std::chrono::nanoseconds>(getRawTime()-t).count();
}

void Keyence::cycle_tracker()
{
    int previous_count = 0;
    int current_count = 0;
    bool current, previous = false;
    while(!this->stop_flag)
    {
        std::vector<int> rpm;
        current = this->output_pin.read();
        if(current && !previous)
        {
            if (previous_count != current_count)
            {
                long timeInNanoseconds = getTimeSince(startTime);
                int RPM = long 60/timeInNanoseconds;
                rpm.push_back(RPM);
                this->new_stripe.store(true, std::memory_order_relaxed);
                previous_count = current_count;
            }
            //printf("Stripe started\n");
            Timepoint startTime = getRawTime();
        }
        if(current && previous)
        {
            //printf("Stripe still present\n")
        }
        if(!current && previous)
        {
            ++current_count;
            this->current_count.store(current_count, std::memory_order_relaxed);
            //printf("Stripe finished. Stripe Count = %d\n", current_count);
        }
        previous = current;
        std::this_thread::sleep_for(std::chrono::microseconds(1000));
    }
    for(int i=0; i<=cycle_index.size; i++)
    {
        printf("RPM = %d\n", rpm[i]);
    }
