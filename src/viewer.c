#define _POSIX_C_SOURCE 200809L

#include <dds/dds.h>
#include <inttypes.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "AgentState.h"

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

typedef struct {
  int seen;
  double x;
  double y;
  double vx;
  double vy;
  int64_t ts_ms;
} agent_snapshot_t;

int main(void) {
  signal(SIGINT, handle_sigint);

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

  dds_qos_t *reader_qos = dds_create_qos();
  dds_qset_reliability(reader_qos, DDS_RELIABILITY_BEST_EFFORT, 0);
  dds_qset_history(reader_qos, DDS_HISTORY_KEEP_LAST, 64);

  dds_entity_t reader = dds_create_reader(participant, topic, reader_qos, NULL);
  dds_delete_qos(reader_qos);
  if (reader < 0) {
    fprintf(stderr, "dds_create_reader failed: %s\n", dds_strretcode(-reader));
    return 1;
  }

  enum { MAX_SAMPLES = 64, MAX_AGENTS = 256 };
  void *samples[MAX_SAMPLES];
  dds_sample_info_t infos[MAX_SAMPLES];
  agent_snapshot_t agents[MAX_AGENTS];

  memset(agents, 0, sizeof(agents));

  for (int i = 0; i < MAX_SAMPLES; i++) {
    samples[i] = swarm_AgentState__alloc();
  }

  printf("viewer started\n");

  while (!g_stop) {
    int rc = dds_take(reader, samples, infos, MAX_SAMPLES, MAX_SAMPLES);
    if (rc < 0) {
      fprintf(stderr, "dds_take failed: %s\n", dds_strretcode(-rc));
      break;
    }

    for (int i = 0; i < rc; i++) {
      if (!infos[i].valid_data) {
        continue;
      }

      swarm_AgentState *msg = (swarm_AgentState *)samples[i];
      if (msg->id < 0 || msg->id >= MAX_AGENTS) {
        continue;
      }

      agents[msg->id].seen = 1;
      agents[msg->id].x = msg->x;
      agents[msg->id].y = msg->y;
      agents[msg->id].vx = msg->vx;
      agents[msg->id].vy = msg->vy;
      agents[msg->id].ts_ms = msg->ts_ms;
    }

    printf("\033[2J\033[H");
    printf("DDS swarm viewer  |  Ctrl-C to quit\n");
    printf("--------------------------------------------------------------\n");
    printf("%-6s %-10s %-10s %-10s %-10s %-10s\n", "id", "x", "y", "vx", "vy",
           "age_ms");

    int64_t t = now_ms();
    for (int i = 0; i < MAX_AGENTS; i++) {
      if (!agents[i].seen) {
        continue;
      }
      printf("%-6d %-10.3f %-10.3f %-10.3f %-10.3f %-10" PRId64 "\n", i,
             agents[i].x, agents[i].y, agents[i].vx, agents[i].vy,
             t - agents[i].ts_ms);
    }

    fflush(stdout);
    struct timespec req;
    req.tv_sec = 0;
    req.tv_nsec = 200000000L;
    nanosleep(&req, NULL);
  }

  for (int i = 0; i < MAX_SAMPLES; i++) {
    swarm_AgentState_free(samples[i], DDS_FREE_ALL);
  }

  dds_delete(participant);
  printf("viewer stopped\n");
  return 0;
}
