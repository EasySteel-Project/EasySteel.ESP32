#include <unity.h>
#include "offline_buffer.h"
#include "offline_buffer.cpp"
#include "outbox_storage.cpp"
#include "round_result.cpp"
#include "tcp_protocol.cpp"
#include "Arduino.h"
#include "mock_globals.cpp"

void setUp(void)
{
    g_mockMillis = 0;
    mockNvsReset();
    roundResultDiscard();
    TEST_ASSERT_TRUE(offlineBufferInit());
}

void tearDown(void) {}

static bool pushRun(
    const char *runId,
    unsigned long rawTimeMs,
    unsigned long timeMs,
    int missCount,
    int penaltyCount,
    const char *contextId = "",
    int targetCount = 5)
{
    return offlineBufferPush(
        runId,
        contextId,
        rawTimeMs,
        timeMs,
        missCount,
        penaltyCount,
        targetCount);
}

void test_buffer_initial_empty(void)
{
    TEST_ASSERT_EQUAL_UINT32(0, offlineBufferGetCount());
}

void test_buffer_push_single_and_multiple(void)
{
    pushRun("run-1", 1200, 1200, 0, 0);
    pushRun("run-2", 1500, 4500, 1, 1, "stage-2");
    TEST_ASSERT_EQUAL_UINT32(2, offlineBufferGetCount());
}

void test_buffer_overflow_capacity(void)
{
    for (int index = 0; index < 25; index++)
    {
        char runId[24];
        snprintf(runId, sizeof(runId), "run-%d", index);
        TEST_ASSERT_EQUAL(index < 20, pushRun(runId, 1000 + index * 10, 1000 + index * 10, 0, index % 2));
    }
    TEST_ASSERT_EQUAL_UINT32(20, offlineBufferGetCount());
    TEST_ASSERT_FALSE(offlineBufferCanAccept());
    TEST_ASSERT_FALSE(offlineBufferHasStorageError());
    TEST_ASSERT_TRUE(offlineBufferInit());
    for (int index = 0; index < 20; index++)
    {
        BufferedRunData run = {};
        char expected[24];
        snprintf(expected, sizeof(expected), "run-%d", index);
        TEST_ASSERT_TRUE(offlineBufferPeekOldestUnsent(run));
        TEST_ASSERT_EQUAL_STRING(expected, run.runId);
        TEST_ASSERT_TRUE(offlineBufferMarkOldestQueued(run.runId));
        TEST_ASSERT_TRUE(offlineBufferCompleteOldestSend(run.runId, 100));
        TEST_ASSERT_TRUE(offlineBufferAcknowledge(run.runId));
        offlineBufferTick(true);
    }
    TEST_ASSERT_TRUE(offlineBufferCanAccept());
}

void test_result_is_retained_after_completed_socket_send(void)
{
    pushRun("run-1", 2000, 2000, 0, 0);
    BufferedRunData run = {};
    TEST_ASSERT_TRUE(offlineBufferPeekOldestUnsent(run));
    TEST_ASSERT_EQUAL_STRING("run-1", run.runId);
    TEST_ASSERT_TRUE(offlineBufferMarkOldestQueued(run.runId));
    TEST_ASSERT_EQUAL_UINT32(1, offlineBufferGetCount());

    TEST_ASSERT_TRUE(offlineBufferCompleteOldestSend(run.runId, 100));
    TEST_ASSERT_EQUAL_UINT32(1, offlineBufferGetCount());
}

void test_ack_result_is_retained_until_matching_ack(void)
{
    pushRun("run-1", 2000, 5000, 1, 1);
    pushRun("run-2", 2500, 8500, 1, 2);

    BufferedRunData run = {};
    TEST_ASSERT_TRUE(offlineBufferPeekOldestUnsent(run));
    TEST_ASSERT_TRUE(offlineBufferMarkOldestQueued(run.runId));
    TEST_ASSERT_TRUE(offlineBufferCompleteOldestSend(run.runId, 100));
    TEST_ASSERT_EQUAL_UINT32(2, offlineBufferGetCount());
    TEST_ASSERT_FALSE(offlineBufferAcknowledge("run-2"));
    TEST_ASSERT_TRUE(offlineBufferAcknowledge("run-1"));
    offlineBufferTick(true);
    TEST_ASSERT_EQUAL_UINT32(1, offlineBufferGetCount());
}

void test_reconnect_makes_queued_result_available_again(void)
{
    pushRun("run-1", 2000, 2000, 0, 0);
    BufferedRunData run = {};
    TEST_ASSERT_TRUE(offlineBufferPeekOldestUnsent(run));
    TEST_ASSERT_TRUE(offlineBufferMarkOldestQueued(run.runId));
    TEST_ASSERT_FALSE(offlineBufferPeekOldestUnsent(run));

    offlineBufferMarkAllUnsent();
    TEST_ASSERT_TRUE(offlineBufferPeekOldestUnsent(run));
    TEST_ASSERT_EQUAL_STRING("run-1", run.runId);
}

void test_result_data_is_preserved_for_retransmission(void)
{
    pushRun("run-1", 1250, 4250, 1, 2, "match-17:stage-3");

    BufferedRunData run = {};
    TEST_ASSERT_TRUE(offlineBufferPeekOldestUnsent(run));
    TEST_ASSERT_EQUAL_STRING("run-1", run.runId);
    TEST_ASSERT_EQUAL_STRING("match-17:stage-3", run.contextId);
    TEST_ASSERT_EQUAL_UINT32(1250, run.rawTimeMs);
    TEST_ASSERT_EQUAL_UINT32(4250, run.timeMs);
    TEST_ASSERT_EQUAL_INT(1, run.missCount);
    TEST_ASSERT_EQUAL_INT(2, run.penaltyCount);
}

void test_ack_timeout_releases_result_and_late_ack_is_accepted(void)
{
    pushRun("run-1", 1250, 4250, 1, 1);
    BufferedRunData run = {};
    TEST_ASSERT_TRUE(offlineBufferPeekOldestUnsent(run));
    TEST_ASSERT_TRUE(offlineBufferMarkOldestQueued(run.runId));
    TEST_ASSERT_TRUE(offlineBufferCompleteOldestSend(run.runId, 100));

    TEST_ASSERT_FALSE(offlineBufferReleaseTimedOutAck(3099, 3000));
    TEST_ASSERT_TRUE(offlineBufferReleaseTimedOutAck(3100, 3000));
    TEST_ASSERT_TRUE(offlineBufferPeekOldestUnsent(run));
    TEST_ASSERT_TRUE(offlineBufferAcknowledge("run-1"));
    offlineBufferTick(true);
    TEST_ASSERT_EQUAL_UINT32(0, offlineBufferGetCount());
}

static size_t serializeBufferedRun(const BufferedRunData& run, char *payload, size_t capacity)
{
    return tcpProtocolSerializeEvent(
        payload, capacity, "EasySteel_6134", "Lane 1", "end", -1,
        run.timeMs, run.targetCount, run.runId, run.rawTimeMs,
        run.missCount, run.penaltyCount, run.contextId);
}

void test_retries_preserve_payload_after_target_configuration_changes(void)
{
    int configuredTargetCount = 5;
    TEST_ASSERT_TRUE(pushRun("run-1", 1250, 7250, 2, 2, "stage-1", configuredTargetCount));
    BufferedRunData run = {};
    TEST_ASSERT_TRUE(offlineBufferPeekOldestUnsent(run));
    TEST_ASSERT_EQUAL_INT(5, run.targetCount);

    char originalPayload[TCP_EVENT_MAX_BYTES] = {};
    const size_t originalLength = serializeBufferedRun(run, originalPayload, sizeof(originalPayload));
    TEST_ASSERT_GREATER_THAN_UINT32(0, originalLength);
    TEST_ASSERT_TRUE(offlineBufferMarkOldestQueued(run.runId));
    TEST_ASSERT_TRUE(offlineBufferCompleteOldestSend(run.runId, 100));

    // A later round uses fewer targets than the original result has misses.
    // Reading the current configuration would invalidate the original payload.
    configuredTargetCount = 2;
    TEST_ASSERT_TRUE(pushRun("run-2", 2000, 2000, 0, 0, "stage-2", configuredTargetCount));
    TEST_ASSERT_TRUE(offlineBufferReleaseTimedOutAck(3100, 3000));
    TEST_ASSERT_TRUE(offlineBufferPeekOldestUnsent(run));

    char retryPayload[TCP_EVENT_MAX_BYTES] = {};
    TEST_ASSERT_EQUAL_UINT32(originalLength, serializeBufferedRun(run, retryPayload, sizeof(retryPayload)));
    TEST_ASSERT_EQUAL_MEMORY(originalPayload, retryPayload, originalLength);
    TEST_ASSERT_TRUE(offlineBufferMarkOldestQueued(run.runId));
    TEST_ASSERT_TRUE(offlineBufferCompleteOldestSend(run.runId, 3200));

    offlineBufferMarkAllUnsent();
    TEST_ASSERT_TRUE(offlineBufferPeekOldestUnsent(run));
    TEST_ASSERT_EQUAL_UINT32(originalLength, serializeBufferedRun(run, retryPayload, sizeof(retryPayload)));
    TEST_ASSERT_EQUAL_MEMORY(originalPayload, retryPayload, originalLength);
    TEST_ASSERT_TRUE(offlineBufferAcknowledge(run.runId));
    offlineBufferTick(true);

    TEST_ASSERT_TRUE(offlineBufferPeekOldestUnsent(run));
    TEST_ASSERT_EQUAL_STRING("run-2", run.runId);
    TEST_ASSERT_EQUAL_INT(2, run.targetCount);
}

void test_ack_timeout_is_millis_rollover_safe(void)
{
    pushRun("run-1", 1000, 1000, 0, 0);
    BufferedRunData run = {};
    TEST_ASSERT_TRUE(offlineBufferPeekOldestUnsent(run));
    TEST_ASSERT_TRUE(offlineBufferMarkOldestQueued(run.runId));
    TEST_ASSERT_TRUE(offlineBufferCompleteOldestSend(run.runId, 0xfffffff0UL));

    TEST_ASSERT_FALSE(offlineBufferReleaseTimedOutAck(0x00000ba7UL, 3000));
    TEST_ASSERT_TRUE(offlineBufferReleaseTimedOutAck(0x00000ba8UL, 3000));
}

void test_reboot_restores_original_payload_and_requires_a_new_send_before_ack(void)
{
    TEST_ASSERT_TRUE(pushRun("run-original", 1250, 7250, 2, 2, "stage-original", 5));
    BufferedRunData run = {};
    TEST_ASSERT_TRUE(offlineBufferPeekOldestUnsent(run));
    char original[TCP_EVENT_MAX_BYTES] = {};
    const size_t length = serializeBufferedRun(run, original, sizeof(original));
    TEST_ASSERT_TRUE(offlineBufferMarkOldestQueued(run.runId));
    TEST_ASSERT_TRUE(offlineBufferCompleteOldestSend(run.runId, 100));

    TEST_ASSERT_TRUE(offlineBufferInit());
    TEST_ASSERT_FALSE(offlineBufferAcknowledge("run-original"));
    TEST_ASSERT_TRUE(offlineBufferPeekOldestUnsent(run));
    char restored[TCP_EVENT_MAX_BYTES] = {};
    TEST_ASSERT_EQUAL_UINT32(length, serializeBufferedRun(run, restored, sizeof(restored)));
    TEST_ASSERT_EQUAL_MEMORY(original, restored, length);
}

static void acknowledgeOldest()
{
    BufferedRunData run = {};
    TEST_ASSERT_TRUE(offlineBufferPeekOldestUnsent(run));
    TEST_ASSERT_TRUE(offlineBufferMarkOldestQueued(run.runId));
    TEST_ASSERT_TRUE(offlineBufferCompleteOldestSend(run.runId, 100));
    TEST_ASSERT_TRUE(offlineBufferAcknowledge(run.runId));
}

void test_ack_never_writes_flash_during_an_active_round(void)
{
    TEST_ASSERT_TRUE(pushRun("run-1", 1000, 1000, 0, 0));
    const unsigned int writes = mockNvs().writes;
    acknowledgeOldest();
    offlineBufferMarkAllUnsent();
    for (int tick = 0; tick < 100; tick++)
    {
        offlineBufferTick(false);
    }
    TEST_ASSERT_EQUAL_UINT32(writes, mockNvs().writes);
    TEST_ASSERT_EQUAL_UINT32(1, offlineBufferGetCount());
    BufferedRunData run = {};
    TEST_ASSERT_FALSE(offlineBufferPeekOldestUnsent(run));
    offlineBufferTick(true);
    TEST_ASSERT_EQUAL_UINT32(writes + 1, mockNvs().writes);
    TEST_ASSERT_EQUAL_UINT32(0, offlineBufferGetCount());
    TEST_ASSERT_TRUE(offlineBufferInit());
    TEST_ASSERT_EQUAL_UINT32(0, offlineBufferGetCount());
}

void test_power_loss_before_ack_cleanup_replays_instead_of_losing_result(void)
{
    TEST_ASSERT_TRUE(pushRun("run-1", 1000, 1000, 0, 0));
    acknowledgeOldest();
    offlineBufferTick(false);
    TEST_ASSERT_TRUE(offlineBufferInit());
    BufferedRunData run = {};
    TEST_ASSERT_TRUE(offlineBufferPeekOldestUnsent(run));
    TEST_ASSERT_EQUAL_STRING("run-1", run.runId);
}

static bool acceptPendingResult(const RoundResult& result)
{
    return pushRun("confirmed-new", result.rawTimeMs, result.timeMs, result.missCount, result.penaltyCount);
}

void test_failed_storage_preserves_pending_confirmation_and_existing_results(void)
{
    TEST_ASSERT_TRUE(pushRun("existing", 1000, 1000, 0, 0));
    roundResultPrepare(1250, 4250, 1, 1);
    mockNvs().failWrite = true;
    TEST_ASSERT_FALSE(roundResultConfirm(acceptPendingResult));
    TEST_ASSERT_TRUE(roundResultIsPending());
    TEST_ASSERT_TRUE(offlineBufferHasStorageError());
    TEST_ASSERT_FALSE(offlineBufferCanAccept());
    TEST_ASSERT_EQUAL_UINT32(1, offlineBufferGetCount());
    const unsigned int writes = mockNvs().writes;
    mockNvs().failWrite = false;
    TEST_ASSERT_FALSE(roundResultConfirm(acceptPendingResult));
    TEST_ASSERT_EQUAL_UINT32(writes, mockNvs().writes);
    TEST_ASSERT_TRUE(offlineBufferInit());
    BufferedRunData run = {};
    TEST_ASSERT_TRUE(offlineBufferPeekOldestUnsent(run));
    TEST_ASSERT_EQUAL_STRING("existing", run.runId);
    TEST_ASSERT_TRUE(roundResultConfirm(acceptPendingResult));
    TEST_ASSERT_FALSE(roundResultIsPending());
    TEST_ASSERT_FALSE(roundResultConfirm(acceptPendingResult));
    TEST_ASSERT_EQUAL_UINT32(2, offlineBufferGetCount());
}

void test_commit_failure_keeps_old_snapshot_and_blocks_admission(void)
{
    TEST_ASSERT_TRUE(pushRun("existing", 1000, 1000, 0, 0));
    mockNvs().failCommit = true;
    TEST_ASSERT_FALSE(pushRun("new", 2000, 2000, 0, 0));
    TEST_ASSERT_TRUE(offlineBufferHasStorageError());
    mockNvs().failCommit = false;
    TEST_ASSERT_TRUE(offlineBufferInit());
    TEST_ASSERT_EQUAL_UINT32(1, offlineBufferGetCount());
}

void test_uncertain_commit_is_recovered_without_overwriting_durable_result(void)
{
    TEST_ASSERT_TRUE(pushRun("existing", 1000, 1000, 0, 0));
    mockNvs().failCommit = true;
    mockNvs().commitDespiteFailure = true;
    TEST_ASSERT_FALSE(pushRun("new", 2000, 2000, 0, 0));
    TEST_ASSERT_FALSE(offlineBufferCanAccept());
    const unsigned int writes = mockNvs().writes;
    TEST_ASSERT_FALSE(pushRun("another", 2000, 2000, 0, 0));
    TEST_ASSERT_EQUAL_UINT32(writes, mockNvs().writes);
    mockNvs().failCommit = false;
    TEST_ASSERT_TRUE(offlineBufferInit());
    TEST_ASSERT_EQUAL_UINT32(2, offlineBufferGetCount());
    acknowledgeOldest();
    offlineBufferTick(true);
    BufferedRunData run = {};
    TEST_ASSERT_TRUE(offlineBufferPeekOldestUnsent(run));
    TEST_ASSERT_EQUAL_STRING("new", run.runId);
}

void test_failed_ack_cleanup_keeps_result_recoverable(void)
{
    TEST_ASSERT_TRUE(pushRun("run-1", 1000, 1000, 0, 0));
    acknowledgeOldest();
    mockNvs().failCommit = true;
    offlineBufferTick(true);
    TEST_ASSERT_TRUE(offlineBufferHasStorageError());
    TEST_ASSERT_EQUAL_UINT32(1, offlineBufferGetCount());
    mockNvs().failCommit = false;
    TEST_ASSERT_TRUE(offlineBufferInit());
    BufferedRunData run = {};
    TEST_ASSERT_TRUE(offlineBufferPeekOldestUnsent(run));
    TEST_ASSERT_EQUAL_STRING("run-1", run.runId);
}

void test_corrupt_or_incompatible_storage_is_not_silently_reset(void)
{
    TEST_ASSERT_TRUE(pushRun("run-1", 1000, 1000, 0, 0));
    const std::vector<uint8_t> valid = mockNvs().durable;
    const unsigned int writes = mockNvs().writes;
    const size_t offsets[] = {0, 4, 8, 12, 16, valid.size() - 1};
    for (size_t offset : offsets)
    {
        mockNvs().durable = valid;
        mockNvs().durable[offset] ^= 0x80;
        TEST_ASSERT_FALSE(offlineBufferInit());
        TEST_ASSERT_TRUE(offlineBufferHasStorageError());
        TEST_ASSERT_FALSE(offlineBufferCanAccept());
        TEST_ASSERT_FALSE(pushRun("replacement", 1000, 1000, 0, 0));
        offlineBufferTick(true);
        TEST_ASSERT_EQUAL_UINT32(writes, mockNvs().writes);
    }
    mockNvs().durable = valid;
    mockNvs().durable.pop_back();
    TEST_ASSERT_FALSE(offlineBufferInit());
    TEST_ASSERT_EQUAL_UINT32(writes, mockNvs().writes);
}

void test_read_and_open_failures_do_not_create_an_empty_outbox(void)
{
    TEST_ASSERT_TRUE(pushRun("run-1", 1000, 1000, 0, 0));
    const unsigned int writes = mockNvs().writes;
    mockNvs().failRead = true;
    TEST_ASSERT_FALSE(offlineBufferInit());
    TEST_ASSERT_FALSE(offlineBufferCanAccept());
    mockNvs().failRead = false;
    mockNvs().failOpen = true;
    TEST_ASSERT_FALSE(offlineBufferInit());
    TEST_ASSERT_EQUAL_UINT32(writes, mockNvs().writes);
    mockNvs().failOpen = false;
    TEST_ASSERT_TRUE(offlineBufferInit());
    TEST_ASSERT_EQUAL_UINT32(1, offlineBufferGetCount());
}

void test_ring_wrap_restores_fifo_order(void)
{
    for (int index = 0; index < 38; index++)
    {
        if (index >= 20)
        {
            acknowledgeOldest();
            offlineBufferTick(true);
        }
        char id[24];
        snprintf(id, sizeof(id), "run-%d", index);
        TEST_ASSERT_TRUE(pushRun(id, 1000, 1000, 0, 0));
    }
    TEST_ASSERT_TRUE(offlineBufferInit());
    for (int index = 18; index < 38; index++)
    {
        char id[24];
        snprintf(id, sizeof(id), "run-%d", index);
        BufferedRunData run = {};
        TEST_ASSERT_TRUE(offlineBufferPeekOldestUnsent(run));
        TEST_ASSERT_EQUAL_STRING(id, run.runId);
        acknowledgeOldest();
        offlineBufferTick(true);
    }
}

void test_buffer_clear_empties_and_persists_empty_snapshot(void)
{
    pushRun("run-1", 1000, 1000, 0, 0);
    pushRun("run-2", 2000, 2000, 0, 0);
    pushRun("run-3", 3000, 3000, 0, 0);
    TEST_ASSERT_EQUAL_UINT32(3, offlineBufferGetCount());

    TEST_ASSERT_TRUE(offlineBufferClear());
    TEST_ASSERT_EQUAL_UINT32(0, offlineBufferGetCount());
    TEST_ASSERT_TRUE(offlineBufferCanAccept());
    TEST_ASSERT_FALSE(offlineBufferHasStorageError());

    // Verify persisted state across re-init
    TEST_ASSERT_TRUE(offlineBufferInit());
    TEST_ASSERT_EQUAL_UINT32(0, offlineBufferGetCount());
    BufferedRunData run = {};
    TEST_ASSERT_FALSE(offlineBufferPeekOldestUnsent(run));
}

int main(int argc, char **argv)
{
    UNITY_BEGIN();
    RUN_TEST(test_buffer_initial_empty);
    RUN_TEST(test_buffer_push_single_and_multiple);
    RUN_TEST(test_buffer_overflow_capacity);
    RUN_TEST(test_result_is_retained_after_completed_socket_send);
    RUN_TEST(test_ack_result_is_retained_until_matching_ack);
    RUN_TEST(test_reconnect_makes_queued_result_available_again);
    RUN_TEST(test_result_data_is_preserved_for_retransmission);
    RUN_TEST(test_ack_timeout_releases_result_and_late_ack_is_accepted);
    RUN_TEST(test_retries_preserve_payload_after_target_configuration_changes);
    RUN_TEST(test_ack_timeout_is_millis_rollover_safe);
    RUN_TEST(test_reboot_restores_original_payload_and_requires_a_new_send_before_ack);
    RUN_TEST(test_ack_never_writes_flash_during_an_active_round);
    RUN_TEST(test_power_loss_before_ack_cleanup_replays_instead_of_losing_result);
    RUN_TEST(test_failed_storage_preserves_pending_confirmation_and_existing_results);
    RUN_TEST(test_commit_failure_keeps_old_snapshot_and_blocks_admission);
    RUN_TEST(test_uncertain_commit_is_recovered_without_overwriting_durable_result);
    RUN_TEST(test_failed_ack_cleanup_keeps_result_recoverable);
    RUN_TEST(test_corrupt_or_incompatible_storage_is_not_silently_reset);
    RUN_TEST(test_read_and_open_failures_do_not_create_an_empty_outbox);
    RUN_TEST(test_ring_wrap_restores_fifo_order);
    RUN_TEST(test_buffer_clear_empties_and_persists_empty_snapshot);
    return UNITY_END();
}
