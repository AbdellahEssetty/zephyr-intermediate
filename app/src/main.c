#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(demo, LOG_LEVEL_DBG);

#define STACK_SIZE 1024
#define COUNT_TIMES 1000000

#define PRIO_A 7
#define PRIO_B 7

K_MUTEX_DEFINE(counter_mutex);

int flag_end = 0;
static uint32_t counter = 0;

void inc_counter()
{
    counter++;
}


void thread_a_fn(void *p1, void *p2, void *p3)
{
    for(uint32_t i = 0; i < COUNT_TIMES; i++) {
        k_mutex_lock(&counter_mutex, K_FOREVER);
        inc_counter();
        k_mutex_unlock(&counter_mutex);
    }
    flag_end++;
}

void thread_b_fn(void *p1, void *p2, void *p3)
{
    for(uint32_t i = 0; i < COUNT_TIMES; i++) {
        k_mutex_lock(&counter_mutex, K_FOREVER);
        inc_counter();
        k_mutex_unlock(&counter_mutex);
    }
    flag_end++;
}

K_THREAD_DEFINE(thread_a, STACK_SIZE, thread_a_fn,
                NULL, NULL, NULL, PRIO_A, 0, k_msec(50));
K_THREAD_DEFINE(thread_b, STACK_SIZE, thread_b_fn,
                NULL, NULL, NULL, PRIO_B, 0, k_msec(50));

int main(void)
{
    int64_t start_time = k_uptime_get();
    LOG_INF("Starting Application...");
    while(1)
    {
        k_sleep(K_MSEC(100));
        if(flag_end == 2)
        {
            int64_t end_time = k_uptime_get();
            int64_t elapsed_time = end_time - start_time;
            LOG_INF("counter value: %d", counter);
            LOG_INF("Elapsed time: %lld ms", elapsed_time);
            break;
        }
    }
    return 0;
}

