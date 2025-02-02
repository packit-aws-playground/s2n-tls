/*
 * Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License").
 * You may not use this file except in compliance with the License.
 * A copy of the License is located at
 *
 *  http://aws.amazon.com/apache2.0
 *
 * or in the "license" file accompanying this file. This file is distributed
 * on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either
 * express or implied. See the License for the specific language governing
 * permissions and limitations under the License.
 */

#include "s2n_test.h"
#include "testlib/s2n_testlib.h"
#include "tls/s2n_alerts.h"

#include "tls/s2n_quic_support.h"

#define ALERT_LEN (sizeof(uint16_t))

int s2n_flush(struct s2n_connection *conn, s2n_blocked_status *blocked);

int main(int argc, char **argv)
{
    BEGIN_TEST();

    /* Test s2n_error_get_alert */
    {
        uint8_t alert = 0;

        /* Test S2N_ERR_T_OK */
        EXPECT_FAILURE_WITH_ERRNO(s2n_error_get_alert(S2N_ERR_OK, &alert), S2N_ERR_NO_ALERT);

        /* Test S2N_ERR_T_CLOSED */
        EXPECT_FAILURE_WITH_ERRNO(s2n_error_get_alert(S2N_ERR_CLOSED, &alert), S2N_ERR_NO_ALERT);

        /* Test S2N_ERR_T_ALERT */
        EXPECT_FAILURE_WITH_ERRNO(s2n_error_get_alert(S2N_ERR_ALERT, &alert), S2N_ERR_NO_ALERT);

        /* Test S2N_ERR_T_BLOCKED */
        for (size_t i = S2N_ERR_T_BLOCKED_START; i < S2N_ERR_T_BLOCKED_END; i++) {
            EXPECT_FAILURE_WITH_ERRNO(s2n_error_get_alert(i, &alert), S2N_ERR_NO_ALERT);
        }

        /* Test S2N_ERR_T_USAGE */
        for (size_t i = S2N_ERR_T_USAGE_START; i < S2N_ERR_T_USAGE_END; i++) {
            EXPECT_FAILURE_WITH_ERRNO(s2n_error_get_alert(i, &alert), S2N_ERR_NO_ALERT);
        }

        /* Test S2N_ERR_T_PROTO */
        {
            /* Test all protocol errors are handled */
            int ret_val;
            for (size_t i = S2N_ERR_T_PROTO_START; i < S2N_ERR_T_PROTO_END; i++) {
                ret_val = s2n_error_get_alert(i, &alert);
                if (ret_val != S2N_SUCCESS && s2n_errno == S2N_ERR_UNIMPLEMENTED) {
                    fprintf(stdout, "\n\nNo alert mapping for protocol error %s\n\n", s2n_strerror_name(i));
                    FAIL_MSG("Missing alert mapping for protocol error.");
                }
            }

            /* Test some known mappings */
            {
                EXPECT_SUCCESS(s2n_error_get_alert(S2N_ERR_MISSING_EXTENSION, &alert));
                EXPECT_EQUAL(S2N_TLS_ALERT_MISSING_EXTENSION, alert);

                EXPECT_SUCCESS(s2n_error_get_alert(S2N_ERR_BAD_MESSAGE, &alert));
                EXPECT_EQUAL(S2N_TLS_ALERT_UNEXPECTED_MESSAGE, alert);
            }

            /* Test unknown mapping */
            EXPECT_FAILURE_WITH_ERRNO(s2n_error_get_alert(S2N_ERR_EARLY_DATA_TRIAL_DECRYPT, &alert), S2N_ERR_NO_ALERT);
        }

        /* Test S2N_ERR_T_IO */
        {
            EXPECT_SUCCESS(s2n_error_get_alert(S2N_ERR_IO, &alert));
            EXPECT_EQUAL(alert, S2N_TLS_ALERT_INTERNAL_ERROR);
        }

        /* Test S2N_ERR_T_INTERNAL */
        for (size_t i = S2N_ERR_T_INTERNAL_START; i < S2N_ERR_T_INTERNAL_END; i++) {
            EXPECT_SUCCESS(s2n_error_get_alert(i, &alert));
            EXPECT_EQUAL(alert, S2N_TLS_ALERT_INTERNAL_ERROR);
        }
    }

    /* Test S2N_TLS_ALERT_CLOSE_NOTIFY and close_notify_received */
    {
        const uint8_t close_notify_alert[] = {  2 /* AlertLevel = fatal */,
                                                0 /* AlertDescription = close_notify */ };

        const uint8_t not_close_notify_alert[] = {  2 /* AlertLevel = fatal */,
                                                   10 /* AlertDescription = unexpected_msg */ };


        /* Don't mark close_notify_received = true if we receive an alert other than close_notify alert */
        {
            struct s2n_connection *conn;
            EXPECT_NOT_NULL(conn = s2n_connection_new(S2N_CLIENT));

            /* Verify state prior to alert */
            EXPECT_FALSE(conn->close_notify_received);

            /* Write and process the alert */
            EXPECT_SUCCESS(s2n_stuffer_write_bytes(&conn->in, not_close_notify_alert, sizeof(not_close_notify_alert)));

            /* This fails due to the alert. This is ok since we are only testing that close_notify_received was set */
            EXPECT_FAILURE_WITH_ERRNO(s2n_process_alert_fragment(conn), S2N_ERR_ALERT);

            /* Verify state after alert */
            EXPECT_FALSE(conn->close_notify_received);

            EXPECT_SUCCESS(s2n_connection_free(conn));
        }

        /* Mark close_notify_received = true if we receive a close_notify alert */
        {
            struct s2n_connection *conn;
            EXPECT_NOT_NULL(conn = s2n_connection_new(S2N_CLIENT));

            /* Verify state prior to alert */
            EXPECT_FALSE(conn->close_notify_received);

            /* Write and process the alert */
            EXPECT_SUCCESS(s2n_stuffer_write_bytes(&conn->in, close_notify_alert, sizeof(close_notify_alert)));
            EXPECT_SUCCESS(s2n_process_alert_fragment(conn));

            /* Verify state after alert */
            EXPECT_TRUE(conn->close_notify_received);

            EXPECT_SUCCESS(s2n_connection_free(conn));
        }

    }

    /* Test s2n_process_alert_fragment */
    {
        /* Safety check */
        EXPECT_FAILURE_WITH_ERRNO(s2n_process_alert_fragment(NULL), S2N_ERR_NULL);

        /* Fails if alerts not supported */
        if (s2n_is_tls13_fully_supported()) {
            struct s2n_config *config;
            EXPECT_NOT_NULL(config = s2n_config_new());

            struct s2n_connection *conn;
            EXPECT_NOT_NULL(conn = s2n_connection_new(S2N_CLIENT));
            EXPECT_SUCCESS(s2n_connection_set_config(conn, config));

            /* Succeeds by default */
            EXPECT_SUCCESS(s2n_stuffer_write_uint16(&conn->in, 0));
            EXPECT_SUCCESS(s2n_process_alert_fragment(conn));

            /* Wipe error */
            EXPECT_SUCCESS(s2n_stuffer_wipe(&conn->alert_in));

            /* Fails when alerts not supported (when QUIC mode enabled) */
            EXPECT_SUCCESS(s2n_config_enable_quic(config));
            EXPECT_SUCCESS(s2n_stuffer_write_uint16(&conn->in, 0));
            EXPECT_FAILURE_WITH_ERRNO(s2n_process_alert_fragment(conn), S2N_ERR_BAD_MESSAGE);

            EXPECT_SUCCESS(s2n_connection_free(conn));
            EXPECT_SUCCESS(s2n_config_free(config));
        }

        /* Test warning behavior */
        {
            const uint8_t warning_alert[] = {  1 /* AlertLevel = warning */,
                                              70 /* AlertDescription = protocol_version (arbitrary value) */};

            const uint8_t user_canceled_alert[] = {  1 /* AlertLevel = warning */,
                                                    90 /* AlertDescription = user_canceled */ };

            /* Warnings treated as errors by default in TLS1.2 */
            {
                struct s2n_connection *conn;
                EXPECT_NOT_NULL(conn = s2n_connection_new(S2N_CLIENT));
                EXPECT_EQUAL(conn->config->alert_behavior, S2N_ALERT_FAIL_ON_WARNINGS);
                conn->actual_protocol_version = S2N_TLS12;

                EXPECT_SUCCESS(s2n_stuffer_write_bytes(&conn->in, warning_alert, sizeof(warning_alert)));

                EXPECT_FAILURE_WITH_ERRNO(s2n_process_alert_fragment(conn), S2N_ERR_ALERT);
                EXPECT_TRUE(conn->closed);

                EXPECT_SUCCESS(s2n_connection_free(conn));
            }

            /* Warnings treated as errors by default in TLS1.3 */
            {
                struct s2n_connection *conn;
                EXPECT_NOT_NULL(conn = s2n_connection_new(S2N_CLIENT));
                EXPECT_EQUAL(conn->config->alert_behavior, S2N_ALERT_FAIL_ON_WARNINGS);
                conn->actual_protocol_version = S2N_TLS13;

                EXPECT_SUCCESS(s2n_stuffer_write_bytes(&conn->in, warning_alert, sizeof(warning_alert)));

                EXPECT_FAILURE_WITH_ERRNO(s2n_process_alert_fragment(conn), S2N_ERR_ALERT);
                EXPECT_TRUE(conn->closed);

                EXPECT_SUCCESS(s2n_connection_free(conn));
            }

            /* Warnings ignored in TLS1.2 if alert_behavior == S2N_ALERT_IGNORE_WARNINGS */
            {
                struct s2n_config *config;
                EXPECT_NOT_NULL(config = s2n_config_new());
                EXPECT_SUCCESS(s2n_config_set_alert_behavior(config, S2N_ALERT_IGNORE_WARNINGS));

                struct s2n_connection *conn;
                EXPECT_NOT_NULL(conn = s2n_connection_new(S2N_CLIENT));
                EXPECT_SUCCESS(s2n_connection_set_config(conn, config));
                conn->actual_protocol_version = S2N_TLS12;

                EXPECT_SUCCESS(s2n_stuffer_write_bytes(&conn->in, warning_alert, sizeof(warning_alert)));

                EXPECT_SUCCESS(s2n_process_alert_fragment(conn));
                EXPECT_FALSE(conn->closed);

                EXPECT_SUCCESS(s2n_connection_free(conn));
                EXPECT_SUCCESS(s2n_config_free(config));
            }

            /* Warnings treated as errors in TLS1.3 if alert_behavior == S2N_ALERT_IGNORE_WARNINGS */
            {
                struct s2n_config *config;
                EXPECT_NOT_NULL(config = s2n_config_new());
                EXPECT_SUCCESS(s2n_config_set_alert_behavior(config, S2N_ALERT_IGNORE_WARNINGS));

                struct s2n_connection *conn;
                EXPECT_NOT_NULL(conn = s2n_connection_new(S2N_CLIENT));
                EXPECT_SUCCESS(s2n_connection_set_config(conn, config));
                conn->actual_protocol_version = S2N_TLS13;

                EXPECT_SUCCESS(s2n_stuffer_write_bytes(&conn->in, warning_alert, sizeof(warning_alert)));

                EXPECT_FAILURE_WITH_ERRNO(s2n_process_alert_fragment(conn), S2N_ERR_ALERT);
                EXPECT_TRUE(conn->closed);

                EXPECT_SUCCESS(s2n_connection_free(conn));
                EXPECT_SUCCESS(s2n_config_free(config));
            }

            /* user_canceled ignored in TLS1.3 by default */
            {
                struct s2n_config *config;
                EXPECT_NOT_NULL(config = s2n_config_new());

                struct s2n_connection *conn;
                EXPECT_NOT_NULL(conn = s2n_connection_new(S2N_CLIENT));
                EXPECT_SUCCESS(s2n_connection_set_config(conn, config));
                conn->actual_protocol_version = S2N_TLS13;

                EXPECT_SUCCESS(s2n_stuffer_write_bytes(&conn->in, user_canceled_alert, sizeof(user_canceled_alert)));

                EXPECT_SUCCESS(s2n_process_alert_fragment(conn));
                EXPECT_FALSE(conn->closed);

                EXPECT_SUCCESS(s2n_connection_free(conn));
                EXPECT_SUCCESS(s2n_config_free(config));
            }
        }
    }

    /* Test s2n_queue_writer_close_alert_warning */
    {
        /* Safety check */
        EXPECT_FAILURE_WITH_ERRNO(s2n_queue_writer_close_alert_warning(NULL), S2N_ERR_NULL);

        /* Does not send alert if alerts not supported */
        if (s2n_is_tls13_fully_supported()) {
            struct s2n_config *config;
            EXPECT_NOT_NULL(config = s2n_config_new());

            struct s2n_connection *conn;
            EXPECT_NOT_NULL(conn = s2n_connection_new(S2N_CLIENT));
            EXPECT_SUCCESS(s2n_connection_set_config(conn, config));

            EXPECT_EQUAL(s2n_stuffer_data_available(&conn->writer_alert_out), 0);

            /* Writes alert by default */
            EXPECT_SUCCESS(s2n_queue_writer_close_alert_warning(conn));
            EXPECT_EQUAL(s2n_stuffer_data_available(&conn->writer_alert_out), ALERT_LEN);

            /* Wipe error */
            EXPECT_SUCCESS(s2n_stuffer_wipe(&conn->writer_alert_out));

            /* Does not write alert when alerts not supported (when QUIC mode enabled) */
            EXPECT_SUCCESS(s2n_config_enable_quic(config));
            EXPECT_SUCCESS(s2n_queue_writer_close_alert_warning(conn));
            EXPECT_EQUAL(s2n_stuffer_data_available(&conn->writer_alert_out), 0);

            EXPECT_SUCCESS(s2n_connection_free(conn));
            EXPECT_SUCCESS(s2n_config_free(config));
        }
    }

    /* Test s2n_queue_reader_alert
     *      Since s2n_queue_reader_alert is static, we'll test it indirectly via s2n_queue_reader_handshake_failure_alert */
    {
        /* Safety check */
        EXPECT_FAILURE_WITH_ERRNO(s2n_queue_reader_handshake_failure_alert(NULL), S2N_ERR_NULL);

        /* Does not send alert if alerts not supported */
        if (s2n_is_tls13_fully_supported()) {
            struct s2n_config *config;
            EXPECT_NOT_NULL(config = s2n_config_new());

            struct s2n_connection *conn;
            EXPECT_NOT_NULL(conn = s2n_connection_new(S2N_CLIENT));
            EXPECT_SUCCESS(s2n_connection_set_config(conn, config));

            EXPECT_EQUAL(s2n_stuffer_data_available(&conn->reader_alert_out), 0);

            /* Writes alert by default */
            EXPECT_SUCCESS(s2n_queue_reader_handshake_failure_alert(conn));
            EXPECT_EQUAL(s2n_stuffer_data_available(&conn->reader_alert_out), ALERT_LEN);

            /* Wipe error */
            EXPECT_SUCCESS(s2n_stuffer_wipe(&conn->reader_alert_out));

            /* Does not write alert when alerts not supported (when QUIC mode enabled) */
            EXPECT_SUCCESS(s2n_config_enable_quic(config));
            EXPECT_SUCCESS(s2n_queue_reader_handshake_failure_alert(conn));
            EXPECT_EQUAL(s2n_stuffer_data_available(&conn->reader_alert_out), 0);

            EXPECT_SUCCESS(s2n_connection_free(conn));
            EXPECT_SUCCESS(s2n_config_free(config));
        }
    }

    /* Test s2n_alerts_close_if_fatal */
    {
        DEFER_CLEANUP(struct s2n_connection *conn = s2n_connection_new(S2N_CLIENT),
                s2n_connection_ptr_free);
        EXPECT_NOT_NULL(conn);
        EXPECT_OK(s2n_connection_set_secrets(conn));

        DEFER_CLEANUP(struct s2n_test_io_pair io_pair = { 0 }, s2n_io_pair_close);
        EXPECT_SUCCESS(s2n_io_pair_init_non_blocking(&io_pair));
        EXPECT_SUCCESS(s2n_connection_set_io_pair(conn, &io_pair));

        s2n_blocked_status blocked = S2N_NOT_BLOCKED;

        /* Sanity check: s2n_flush doesn't close the connection */
        conn->closing = false;
        EXPECT_SUCCESS(s2n_flush(conn, &blocked));
        EXPECT_FALSE(conn->closing);

        /* Test: a fatal reader alert closes the connection */
        conn->closing = false;
        EXPECT_SUCCESS(s2n_queue_reader_handshake_failure_alert(conn));
        EXPECT_SUCCESS(s2n_flush(conn, &blocked));
        EXPECT_TRUE(conn->closing);

        /* Test: a close_notify alert closes the connection
         * This is our only writer alert, and technically it's a warning.
         */
        conn->closing = false;
        EXPECT_SUCCESS(s2n_queue_writer_close_alert_warning(conn));
        EXPECT_SUCCESS(s2n_flush(conn, &blocked));
        EXPECT_TRUE(conn->closing);

        /* Test: a no_renegotiation alert does not close the connection */
        conn->closing = false;
        EXPECT_OK(s2n_queue_reader_no_renegotiation_alert(conn));
        EXPECT_SUCCESS(s2n_flush(conn, &blocked));
        EXPECT_FALSE(conn->closing);
    }

    END_TEST();
}
