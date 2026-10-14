#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/logging/log.h>
#include <zephyr/zbus/zbus.h>
#include <stdint.h>


#include <zephyr/task_wdt/task_wdt.h>
#include <zephyr/sys/atomic.h>

#define QUEUE_SIZE       16
#define CONSUMER_SLEEP_MS 5000
#define WDT_TIMEOUT_MS   2000

// Semaphore to signal when the watchdog is ready (delay initialization until the watchdog is ready)
K_SEM_DEFINE(wdt_ready, 0, 1);

static int consumer_wdt_id;
static atomic_t watchdog_fired;

LOG_MODULE_REGISTER(sensor_app, LOG_LEVEL_INF);

struct sensor_sample
{
    uint32_t sequence;
    int32_t temperature_mC;
};

K_MSGQ_DEFINE(sensor_queue, sizeof(struct sensor_sample), QUEUE_SIZE, 4);

static void sensor_listener_cb(const struct zbus_channel *chan)
{
    const struct sensor_sample *sample = zbus_chan_const_msg(chan);

    if (sample != NULL)
    {
        LOG_INF("Display: sample=%u, temperature=%d mC",
                sample->sequence, sample->temperature_mC);
    }
}

static void consumer_wdt_callback(int channel_id, void *user_data)
{
    ARG_UNUSED(channel_id);
    ARG_UNUSED(user_data);

    atomic_set(&watchdog_fired, 1);
}

ZBUS_LISTENER_DEFINE(sensor_listener, sensor_listener_cb);

ZBUS_SUBSCRIBER_DEFINE(sensor_subscriber, 16);

ZBUS_CHAN_DEFINE(
    sensor_chan,
    struct sensor_sample,
    NULL,
    NULL,
    ZBUS_OBSERVERS(sensor_listener, sensor_subscriber),
    ZBUS_MSG_INIT(.sequence = 0, .temperature_mC = 0));


static void sensor_publisher(void)
{
    struct sensor_sample sample = {
        .sequence = 0,
        .temperature_mC = 25000,
    };

    while (1)
    {
        sample.sequence++;
        sample.temperature_mC = 25000 + (sample.sequence % 10) * 100;

        int ret = zbus_chan_pub(&sensor_chan, &sample, K_MSEC(50));

        if (ret != 0)
            LOG_ERR("Failed to publish sample: %d", ret);

        ret = k_msgq_put(&sensor_queue, &sample, K_NO_WAIT);

        if (ret != 0)
            LOG_ERR("Queue full: dropping sample %u", sample.sequence);

        k_msleep(100);
    }
}



static void sensor_logger(void)
{
    const struct zbus_channel *chan;

    while (1)
    {
        int ret = zbus_sub_wait(&sensor_subscriber, &chan, K_FOREVER);
        if (ret != 0)
        {
            LOG_ERR("Subscriber wait failed: %d", ret);
            continue;
        }

        struct sensor_sample sample;
        ret = zbus_chan_read(chan, &sample, K_MSEC(100));

        if (ret != 0)
        {
            LOG_ERR("Failed to read sensor channel: %d", ret);
            continue;
        }

        if (sample.sequence % 5 == 0)
            LOG_INF("Logger: sample=%u, temperature=%d mC", sample.sequence, sample.temperature_mC);
    }
}

static void sensor_consumer(void)
{
    k_sem_take(&wdt_ready, K_FOREVER);
    struct sensor_sample sample;

    consumer_wdt_id = task_wdt_add(
        WDT_TIMEOUT_MS,
        consumer_wdt_callback,
        NULL
    );

    if (consumer_wdt_id < 0) {
        LOG_ERR("Failed to register consumer watchdog");
        return;
    }

    while (1) {
        int ret = k_msgq_get(&sensor_queue, &sample, K_FOREVER);

        if (ret != 0) {
            continue;
        }

        LOG_INF("Consumer received sample %u", sample.sequence);

        k_sleep(K_MSEC(CONSUMER_SLEEP_MS));

        ret = task_wdt_feed(consumer_wdt_id);

        if (ret != 0) {
            LOG_ERR("Failed to feed task watchdog: %d", ret);
        }
    }
}



static void sensor_health_check(void)
{
    while (1) {
        uint32_t used = k_msgq_num_used_get(&sensor_queue);
        uint32_t percent = (used * 100U) / QUEUE_SIZE;

        if (percent >= 75U) {
            LOG_WRN("Queue occupancy: %u%% (%u/%u)",
                    percent, used, QUEUE_SIZE);
        }

        if (atomic_cas(&watchdog_fired, 1, 0)) {
            LOG_ERR("Task watchdog fired: consumer stalled");
        }

        k_sleep(K_MSEC(250));
    }
}



K_THREAD_DEFINE(
    sensor_logger_tid,
    2048,
    sensor_logger,
    NULL, NULL, NULL,
    5, 0, 0);
    
K_THREAD_DEFINE(
    sensor_consumer_tid,
    2048,
    sensor_consumer,
    NULL, NULL, NULL,
    5, 0, 0);

K_THREAD_DEFINE(
    sensor_health_check_tid,
    2048,
    sensor_health_check,
    NULL, NULL, NULL,
    5, 0, 0);

int main(void)
{
    int ret = task_wdt_init(NULL);

    if (ret != 0) {
        LOG_ERR("Task watchdog initialization failed: %d", ret);
        return ret;
    }

    LOG_INF("Task watchdog initialized");
    k_sem_give(&wdt_ready);

    sensor_publisher();
    return 0;
}