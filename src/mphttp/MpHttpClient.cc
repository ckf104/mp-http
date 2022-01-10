#include "MpHttpClient.h"

void MpHttpClient::run(size_t start, size_t end) {
  if (tasks_.running == nullptr) {
    tasks_.running =
        new HttpClient(loop_, this, mp_task_, addr_in_, start, end);
  }
}

void MpHttpClient::reschedule() {
  // we should update the task queue and bandwidth before next step
  UpdateTask();
  rival_->UpdateTask();

  UpdateBandwidth();
  rival_->UpdateBandwidth();

  struct HttpClient *busy_client = rival_->tasks_.running;
  if (!busy_client) {
    MPHTTP_LOG(
        info,
        "MpHttpClient.reschedule(): busy client of the rival is nullptr\n");
    return;
  }

  size_t my_bw = bandwidth_;
  size_t other_bw = rival_->GetBandwidth();

  size_t my_length, other_end;

  size_t my_remaining = GetRemaining();
  size_t other_remaining = busy_client->GetRemaining();

  // we don't reschedule(or multipath) for small objects
  if (other_remaining <= 40000) {
    MPHTTP_LOG(
        info,
        "MpHttpClient.reschedule(): it is ok not to reschedule for rival\n");
    return;
  }

  if (my_bw == 0 || other_bw == 0) {
    MPHTTP_LOG(info, "MpHttpClient.reschedule(): initialize task");
    my_length = other_remaining / 2;
  } else {
    MPHTTP_LOG(info, "MpHttpClient.reschedule(): rescheduling bandwidth\n");
    my_length = (1.0 * other_remaining * my_bw +
                 1.0 * my_bw * other_bw * (rival_->GetRTT() - rtt_)) /
                (my_bw + other_bw);
  }

  if (my_length > 0) {
    other_end = busy_client->range.end;
    size_t new_end = other_end - my_length;
    busy_client->ResetRangeEnd(new_end);
    tasks_.next =
        new HttpClient(loop_, this, mp_task_, addr_in_, new_end, other_end);

    // if this is initially steal work from others, we need to adjust the
    // task queue
    UpdateTask();
  }
}