#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(demo, LOG_LEVEL_DBG);

#define STACK_SIZE 1024

#define PRIO_A 7
#define PRIO_B 5
#define PRIO_C 3
#define PRIO_D -1

void thread_a_fn(void *p1, void *p2, void *p3)
{
    while (1) {
	LOG_INF("T_LOW running");
        k_msleep(300);
    }
}

void thread_b_fn(void *p1, void *p2, void *p3)
{
    while (1) {
	LOG_INF("T_MED running");
        k_msleep(200);
    }
}

void thread_c_fn(void *p1, void *p2, void *p3)
{
    while (1) {
        LOG_INF("T_HIGH running");
	k_msleep(100);
    }
}

void thread_d_fn(void *p1, void *p2, void *p3)
{
    uint8_t count = 0;
    while (1) {
	if(count++ >= 5) {
	    count = 0;
	    k_yield();
	    //k_msleep(100);
	} else {
	    LOG_INF("T_COOP running");
	}
    }
}
	

K_THREAD_DEFINE(thread_a, STACK_SIZE, thread_a_fn,
                NULL, NULL, NULL, PRIO_A, 0, 0);
K_THREAD_DEFINE(thread_b, STACK_SIZE, thread_b_fn,
                NULL, NULL, NULL, PRIO_B, 0, 0);
K_THREAD_DEFINE(thread_c, STACK_SIZE, thread_c_fn,
		NULL, NULL, NULL, PRIO_C, 0, 0);
K_THREAD_DEFINE(thread_d, STACK_SIZE, thread_d_fn,
		NULL, NULL, NULL, PRIO_D, 0, 0);

int main(void)
{
    return 0;
}

