
#include <stdint.h>

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/tracing/tracing.h>

LOG_MODULE_REGISTER(homework, LOG_LEVEL_INF);

/* Configuration */
#define STACK_SIZE              2048
#define CONTROL_PRIORITY        7
#define MAINTENANCE_PRIORITY    4

#define EVENT_PERIOD_MS         250
#define MAINTENANCE_LOAD_US     45000

#define CONTROL_DEADLINE_US     10000U
#define REPORT_INTERVAL         10U

struct control_event {
    uint32_t seq;
    uint32_t ready_us;
};

K_MSGQ_DEFINE(control_queue, sizeof(struct control_event), 4, 4);
K_SEM_DEFINE(maintenance_start, 0, 1);

/* Statistics: accessed only by the control thread. */
static uint32_t processed_count;
static uint32_t deadline_misses;
static uint32_t max_response_us;
static uint32_t consecutive_misses;

/* ================================================================
 * Timer expiry: create one control event.
 * Runs in interrupt context.
 * ================================================================ */
static void event_timer_expiry(struct k_timer *timer)
{
    ARG_UNUSED(timer);

    static uint32_t seq;

    struct control_event event = {
        .seq = seq++,
        .ready_us = k_cyc_to_us_floor32(k_cycle_get_32()),
    };

    /* Never wait in timer interrupt context. */
    int ret = k_msgq_put(&control_queue, &event, K_NO_WAIT);

    if (ret != 0) {
        /* Queue overflow: this event was not delivered. */
        return;
    }

    /* Wake the maintenance thread. */
    k_sem_give(&maintenance_start);

    /* Trace event creation. */
    sys_trace_named_event("control_ready", event.seq,
                          event.ready_us);
}

K_TIMER_DEFINE(event_timer, event_timer_expiry, NULL);

/* ================================================================
 * Control thread: measure response time and deadline misses.
 * ================================================================ */
static void control_fn(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);

    while (true) {
        struct control_event event;

        int ret = k_msgq_get(&control_queue, &event, K_FOREVER);

        if (ret != 0) {
            LOG_ERR("Control queue receive failed: %d", ret);
            continue;
        }

        uint32_t now_us =
            k_cyc_to_us_floor32(k_cycle_get_32());

        /*
         * Unsigned subtraction handles timer wraparound,
         * provided the elapsed time is within one counter wrap.
         */
        uint32_t response_us = now_us - event.ready_us;

        processed_count++;

        if (response_us > max_response_us) {
            max_response_us = response_us;
        }

        if (response_us > CONTROL_DEADLINE_US) {
            deadline_misses++;
            consecutive_misses++;

            /* Avoid printing a warning for every missed deadline. */
            if (consecutive_misses == 1U ||
                (consecutive_misses % REPORT_INTERVAL) == 0U) {
                LOG_WRN(
                    "[CONTROL] DEADLINE MISS seq=%u "
                    "response=%u us misses=%u",
                    event.seq, response_us, deadline_misses);
            }
        } else {
            consecutive_misses = 0U;
        }

        /* Trace completion for correlation with the CTF timeline. */
        sys_trace_named_event("control_done", event.seq,
                              response_us);

        /*
         * Print successful events only occasionally to reduce
         * UART logging overhead.
         */
        if ((processed_count % REPORT_INTERVAL) == 0U) {
            LOG_INF(
                "[SUMMARY] processed=%u misses=%u "
                "max_response=%u us last_seq=%u last_response=%u us",
                processed_count,
                deadline_misses,
                max_response_us,
                event.seq,
                response_us);
        }
    }
}

/* ================================================================
 * Background maintenance thread.
 * ================================================================ */
static void maintenance_fn(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);

    while (true) {
        k_sem_take(&maintenance_start, K_FOREVER);

        /* Simulate non-urgent background work. */
        k_busy_wait(MAINTENANCE_LOAD_US);
    }
}

K_THREAD_DEFINE(control, STACK_SIZE, control_fn,
                NULL, NULL, NULL,
                CONTROL_PRIORITY, 0, 0);

K_THREAD_DEFINE(maintenance, STACK_SIZE, maintenance_fn,
                NULL, NULL, NULL,
                MAINTENANCE_PRIORITY, 0, 0);

/* ================================================================
 * Main
 * ================================================================ */
int main(void)
{
    LOG_INF("=== L6 Homework: Runtime Investigation ===");
    LOG_INF("Response-time deadline: %u us", CONTROL_DEADLINE_US);
    LOG_INF("Event period: %u ms", EVENT_PERIOD_MS);
    LOG_INF("Maintenance workload: %u us", MAINTENANCE_LOAD_US);
    LOG_INF("Control priority: %d", CONTROL_PRIORITY);
    LOG_INF("Maintenance priority: %d", MAINTENANCE_PRIORITY);

    k_timer_start(&event_timer,
                  K_MSEC(500),
                  K_MSEC(EVENT_PERIOD_MS));

    return 0;
}
