#define _POSIX_C_SOURCE 200809L

#include <dds/dds.h>
#include <math.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#include "AgentState.h"

static void debug_log(int self_id, dds_entity_t writer, dds_entity_t reader) {
  static int tick = 0;
  tick++;
  if (tick % 10 == 0) {
    dds_publication_matched_status_t pub_status;
    dds_subscription_matched_status_t sub_status;

    int prc = dds_get_publication_matched_status(writer, &pub_status);
    int src = dds_get_subscription_matched_status(reader, &sub_status);

    printf("agent %d: pub current=%d total=%d | sub current=%d total=%d\n",
           self_id, pub_status.current_count, pub_status.total_count,
           sub_status.current_count, sub_status.total_count);
    fflush(stdout);
  }
}

static volatile sig_atomic_t g_stop = 0;

static void handle_sigint(int sig) {
  (void)sig;
  g_stop = 1;
}

static int64_t now_ms(void) {
  struct timespec ts;
  clock_gettime(CLOCK_REALTIME, &ts);
  return ((int64_t)ts.tv_sec * 1000) + (ts.tv_nsec / 1000000);
}

static double frand_range(double min, double max) {
  double r = (double)rand() / (double)RAND_MAX;
  return min + r * (max - min);
}

static void clamp_speed(double *vx, double *vy, double max_speed) {
  double mag = sqrt((*vx) * (*vx) + (*vy) * (*vy));
  if (mag > max_speed && mag > 0.0) {
    *vx = (*vx / mag) * max_speed;
    *vy = (*vy / mag) * max_speed;
  }
}

int main(int argc, char **argv) {
  signal(SIGINT, handle_sigint);

  if (argc < 2) {
    fprintf(stderr, "usage: %s AGENT_ID [x y]\n", argv[0]);
    return 1;
  }

  const int self_id = atoi(argv[1]);
  srand((unsigned int)(time(NULL) ^ (self_id * 7919)));

  double x = (argc >= 4) ? atof(argv[2]) : frand_range(-10.0, 10.0);
  double y = (argc >= 4) ? atof(argv[3]) : frand_range(-10.0, 10.0);
  double vx = frand_range(-0.3, 0.3);
  double vy = frand_range(-0.3, 0.3);

  dds_entity_t participant =
      dds_create_participant(DDS_DOMAIN_DEFAULT, NULL, NULL);
  if (participant < 0) {
    fprintf(stderr, "dds_create_participant failed: %s\n",
            dds_strretcode(-participant));
    return 1;
  }

  dds_entity_t topic = dds_create_topic(participant, &swarm_AgentState_desc,
                                        "swarm_agent_state", NULL, NULL);
  if (topic < 0) {
    fprintf(stderr, "dds_create_topic failed: %s\n", dds_strretcode(-topic));
    return 1;
  }

  dds_qos_t *writer_qos = dds_create_qos();
  dds_qset_reliability(writer_qos, DDS_RELIABILITY_BEST_EFFORT, 0);
  dds_qset_history(writer_qos, DDS_HISTORY_KEEP_LAST, 1);

  dds_entity_t writer = dds_create_writer(participant, topic, writer_qos, NULL);
  dds_delete_qos(writer_qos);
  if (writer < 0) {
    fprintf(stderr, "dds_create_writer failed: %s\n", dds_strretcode(-writer));
    return 1;
  }

  dds_qos_t *reader_qos = dds_create_qos();
  dds_qset_reliability(reader_qos, DDS_RELIABILITY_BEST_EFFORT, 0);
  dds_qset_history(reader_qos, DDS_HISTORY_KEEP_LAST, 16);

  dds_entity_t reader = dds_create_reader(participant, topic, reader_qos, NULL);
  dds_delete_qos(reader_qos);
  if (reader < 0) {
    fprintf(stderr, "dds_create_reader failed: %s\n", dds_strretcode(-reader));
    return 1;
  }
  printf("agent %d waiting for discovery...\n", self_id);
  fflush(stdout);
  sleep(2);

  enum { MAX_SAMPLES = 32 };
  void *samples[MAX_SAMPLES];
  dds_sample_info_t infos[MAX_SAMPLES];

  for (int i = 0; i < MAX_SAMPLES; i++) {
    samples[i] = swarm_AgentState__alloc();
  }

  printf("agent %d started at (%.2f, %.2f)\n", self_id, x, y);
  fflush(stdout);

  const double dt = 0.10;
  const double cohesion = 0.015;
  const double avoidance = 0.05;
  const double max_speed = 1.0;
  const double avoid_radius = 1.5;

  while (!g_stop) {

    // debug_log(self_id, writer, reader);

    int rc = dds_take(reader, samples, infos, MAX_SAMPLES, MAX_SAMPLES);
    if (rc < 0) {
      fprintf(stderr, "dds_take failed: %s\n", dds_strretcode(-rc));
      break;
    }

    double cx = 0.0, cy = 0.0;
    double ax = 0.0, ay = 0.0;
    int count = 0;

    for (int i = 0; i < rc; i++) {
      if (!infos[i].valid_data) {
        continue;
      }

      swarm_AgentState *other = (swarm_AgentState *)samples[i];

      if (other->id == self_id) {
        continue;
      }

      double dx = other->x - x;
      double dy = other->y - y;
      double dist2 = dx * dx + dy * dy;
      double dist = sqrt(dist2);

      cx += other->x;
      cy += other->y;
      count++;

      if (dist > 0.0001 && dist < avoid_radius) {
        ax -= dx / dist;
        ay -= dy / dist;
      }
    }
    fflush(stdout);

    if (count > 0) {
      cx /= count;
      cy /= count;
      vx += (cx - x) * cohesion;
      vy += (cy - y) * cohesion;
      vx += ax * avoidance;
      vy += ay * avoidance;
    }

    clamp_speed(&vx, &vy, max_speed);

    x += vx * dt;
    y += vy * dt;

    if (x > 20.0 || x < -20.0)
      vx = -vx;
    if (y > 20.0 || y < -20.0)
      vy = -vy;
    if (x > 20.0)
      x = 20.0;
    if (x < -20.0)
      x = -20.0;
    if (y > 20.0)
      y = 20.0;
    if (y < -20.0)
      y = -20.0;

    swarm_AgentState msg;
    msg.id = self_id;
    msg.x = x;
    msg.y = y;
    msg.vx = vx;
    msg.vy = vy;
    msg.ts_ms = now_ms();

    rc = dds_write(writer, &msg);
    if (rc < 0) {
      fprintf(stderr, "dds_write failed: %s\n", dds_strretcode(-rc));
      break;
    }

    struct timespec req;
    req.tv_sec = 0;
    req.tv_nsec = (long)(dt * 1000000000.0);
    nanosleep(&req, NULL);
  }

  for (int i = 0; i < MAX_SAMPLES; i++) {
    swarm_AgentState_free(samples[i], DDS_FREE_ALL);
  }

  dds_delete(participant);
  printf("agent %d stopped\n", self_id);
  return 0;
}
