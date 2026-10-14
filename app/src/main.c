#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/logging/log.h>
#include <zephyr/zbus/zbus.h>
#include <stdint.h>

LOG_MODULE_REGISTER(sensor_app, LOG_LEVEL_INF);

struct sensor_sample
{
    uint32_t sequence;
    int32_t temperature_mC;
};

static void sensor_listener_cb(const struct zbus_channel *chan)
{
    const struct sensor_sample *sample = zbus_chan_const_msg(chan);

    if (sample != NULL)
    {
        LOG_INF("Display: sample=%u, temperature=%d mC",
                sample->sequence, sample->temperature_mC);
    }
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

K_THREAD_DEFINE(
    sensor_logger_tid,
    2048,
    sensor_logger,
    NULL, NULL, NULL,
    5, 0, 0);

int main(void)
{
    sensor_publisher();
    return 0;
}