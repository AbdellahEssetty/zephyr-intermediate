/*
 * temp_alarm - Error-path unit tests (homework skeleton)
 *
 * Task 2: fill in the stubs according to TEST_SPEC.md. Each stub calls
 * ztest_test_skip(), so the binary builds and runs cleanly until the test is
 * implemented.
 *
 * Run:
 *   west twister -T tests/temp_alarm -p native_sim
 */

#include <errno.h>
#include <zephyr/fff.h>
#include <zephyr/ztest.h>

#include "sensor_fake.h"
#include "temp_alarm.h"

#define DEFAULT_THRESHOLD 30

/* Same before() hook as in test_basic.c: a clean fake and an initialised module. */
static void before(void *fixture)
{
	ARG_UNUSED(fixture);

	sensor_fake_reset();
	sensor_fake_init();

	zassume_ok(temp_alarm_init(&fake_sensor_dev, DEFAULT_THRESHOLD),
		   "precondition: temp_alarm_init must succeed");
}

/*
 * ============================================================================
 * Test Suite: temp_alarm_sensor_errors
 *
 * Failures from the sensor driver, injected through the fakes.
 * ============================================================================
 */
ZTEST_SUITE(temp_alarm_sensor_errors, NULL, NULL, before, NULL, NULL);

ZTEST(temp_alarm_sensor_errors, test_fetch_error_propagates)
{
	/* TODO(l8-task2): a failed fetch is reported,
	 * and the module does not go on to read the channel.
	 * See TEST_SPEC.md "Suite temp_alarm_sensor_errors" #1.
	 */
	struct temp_alarm_status status;
	fake_sensor_sample_fetch_fake.return_val = -EIO;

	zassert_equal(temp_alarm_check(), -EIO, "fetch error must propagate");

	zassert_ok(temp_alarm_get_status(&status));

	zassert_equal(status.last_error, -EIO, "last_error must contain the fetch error");
	zassert_equal(fake_sensor_sample_fetch_fake.call_count, 1U);
	zassert_equal(fake_sensor_channel_get_fake.call_count, 0U);
}

ZTEST(temp_alarm_sensor_errors, test_channel_get_error_propagates)
{
	/* TODO(l8-task2): a failed channel read is reported.
	 * See TEST_SPEC.md "Suite temp_alarm_sensor_errors" #2.
	 */
	struct temp_alarm_status status;

	sensor_fake_set_temperature(40);

	fake_sensor_channel_get_fake.return_val = -EIO;

	zassert_equal(temp_alarm_check(), -EIO, "channel_get error must propagate");


	zassert_ok(temp_alarm_get_status(&status));

	zassert_equal(status.last_error, -EIO);
	zassert_equal(fake_sensor_sample_fetch_fake.call_count, 1U);
	zassert_equal(fake_sensor_channel_get_fake.call_count, 1U);
}

ZTEST(temp_alarm_sensor_errors, test_fail_then_recover)
{
	/* TODO(l8-task2): the first fetch fails and the second one succeeds.
	 * The module must recover on the second check.
	 * See TEST_SPEC.md "Suite temp_alarm_sensor_errors" #3.
	 */
	struct temp_alarm_status status;

	sensor_fake_set_temperature(40);


	fake_sensor_sample_fetch_fake.return_val = -EIO;

	zassert_equal(temp_alarm_check(), -EIO);

	zassert_ok(temp_alarm_get_status(&status));
	zassert_equal(status.last_error, -EIO);


	fake_sensor_sample_fetch_fake.return_val = 0;

	zassert_ok(temp_alarm_check());
	zassert_ok(temp_alarm_get_status(&status));

	zassert_equal(status.last_temp, 40);
	zassert_true(status.is_alarming);
	zassert_equal(status.alarm_count, 1U);
	zassert_equal(status.last_error, 0);

	zassert_equal(fake_sensor_sample_fetch_fake.call_count, 2U);
	zassert_equal(fake_sensor_channel_get_fake.call_count, 1U);
}

/*
 * ============================================================================
 * Test Suite: temp_alarm_invalid_input
 *
 * Arguments the public API must reject.
 * ============================================================================
 */
ZTEST_SUITE(temp_alarm_invalid_input, NULL, NULL, before, NULL, NULL);

ZTEST(temp_alarm_invalid_input, test_get_status_null)
{
	/* TODO(l8-task2): a NULL status pointer is rejected.
	 * See TEST_SPEC.md "Suite temp_alarm_invalid_input" #1.
	 */
		zassert_equal(temp_alarm_get_status(NULL), -EINVAL, "NULL status pointer must be rejected");
}

ZTEST(temp_alarm_invalid_input, test_init_without_device)
{
	/* TODO(l8-task2): init without a sensor device is rejected.
	 * See TEST_SPEC.md "Suite temp_alarm_invalid_input" #2.
	 */
	zassert_equal(temp_alarm_init(NULL, DEFAULT_THRESHOLD), -ENODEV, "NULL sensor device must be rejected");
}

ZTEST(temp_alarm_invalid_input, test_init_threshold_out_of_range)
{
	/* TODO(l8-task2): init rejects thresholds outside the allowed range.
	 * See TEST_SPEC.md "Suite temp_alarm_invalid_input" #3.
	 */
	zassert_equal(temp_alarm_init(&fake_sensor_dev, -1), -EINVAL, "Negative threshold must be rejected");

	zassert_equal(temp_alarm_init(&fake_sensor_dev, 101), -EINVAL, "Threshold above 100 must be rejected");
}

ZTEST(temp_alarm_invalid_input, test_set_threshold_out_of_range)
{
	/* TODO(l8-task2): set_threshold rejects values outside the allowed range,
	 * and keeps the old threshold.
	 * See TEST_SPEC.md "Suite temp_alarm_invalid_input" #4.
	 */
	struct temp_alarm_status status;

	zassert_equal(temp_alarm_set_threshold(-1), -EINVAL);
	zassert_equal(temp_alarm_set_threshold(101), -EINVAL);

	sensor_fake_set_temperature(40);

	zassert_ok(temp_alarm_check());
	zassert_ok(temp_alarm_get_status(&status));

	zassert_equal(status.last_temp, 40);
	zassert_true(status.is_alarming);
	zassert_equal(status.alarm_count, 1U);
}
