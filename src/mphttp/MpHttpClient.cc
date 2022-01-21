#include "MpHttpClient.h"
#include "MpTask.h"

void MpHttpClient::run(size_t start, size_t end) {
  if (tasks_.running == nullptr) {
    struct HttpClient *client =
        new HttpClient(loop_, this, mp_task_, addr_in_, start, end);
    tasks_.running = client;
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
        "MpHttpClient.reschedule(): busy client of the rival is nullptr");
    return;
  }

  if (tasks_.next) {
    MPHTTP_LOG(debug, "MpHttpClient.reschedule(): Client Already scheduled");
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
        "MpHttpClient.reschedule(): it is ok not to reschedule for rival");
    return;
  }

  // is it an initial schedule?
  if (my_bw == 0 || other_bw == 0) {
    MPHTTP_LOG(info, "MpHttpClient.reschedule(): initialize task");
    my_length = other_remaining / 2;
  } else {
    //MPHTTP_LOG(debug, "my bandwidth = %zd", my_bw);
    //MPHTTP_LOG(debug, "rival bandwidth = %zd", other_bw);
    //MPHTTP_LOG(debug, "other remaining = %zd", other_remaining);
    //MPHTTP_LOG(debug, "rival rtt = %zd, my rtt = %zd", rival_->GetRTT(), rtt_);

    double my_dltime = 1.0 * my_remaining / my_bw;
    double other_dltime = 1.0 * other_remaining / other_bw;

    // delta = 10ms
    if (other_dltime - my_dltime <= 10.0) {
      MPHTTP_LOG(info, "MpHttpClient.reschedule() : "
                       "my_dltime - other_dltime < delta, no need to reschedule.");
      return;
    }


    my_length = (other_remaining * my_bw +
                 my_bw * other_bw * (rival_->GetRTT() - rtt_)) /
                (my_bw + other_bw);
  }

  if (my_length > 0 && my_length < other_remaining) {
    other_end = busy_client->range.end;
    size_t new_end = other_end - my_length;
    MPHTTP_LOG(info, "MpHttpClient : rival task %zd-%zd, my task %zd-%zd", 
                      busy_client->range.start, new_end, new_end + 1, other_end);
    busy_client->ResetRangeEnd(new_end);
    tasks_.next =
        new HttpClient(loop_, this, mp_task_, addr_in_, new_end + 1, other_end);
    UpdateTask();
  }
}