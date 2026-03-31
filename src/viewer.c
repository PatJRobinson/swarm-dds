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

static void draw_map(agent_snapshot_t *agents, int64_t now) {
  enum { MAP_W = 61, MAP_H = 21, MAX_AGENTS = 256 };
  char grid[MAP_H][MAP_W + 1];

  const double world_min_x = -20.0;
  const double world_max_x = 20.0;
  const double world_min_y = -20.0;
  const double world_max_y = 20.0;

  for (int y = 0; y < MAP_H; y++) {
    for (int x = 0; x < MAP_W; x++) {
      grid[y][x] = '.';
    }
    grid[y][MAP_W] = '\0';
  }

  int axis_x =
      (int)((0.0 - world_min_x) / (world_max_x - world_min_x) * (MAP_W - 1));
  int axis_y =
      (int)((world_max_y - 0.0) / (world_max_y - world_min_y) * (MAP_H - 1));

  if (axis_x >= 0 && axis_x < MAP_W) {
    for (int y = 0; y < MAP_H; y++) {
      grid[y][axis_x] = '|';
    }
  }
  if (axis_y >= 0 && axis_y < MAP_H) {
    for (int x = 0; x < MAP_W; x++) {
      grid[axis_y][x] = '-';
    }
  }
  if (axis_x >= 0 && axis_x < MAP_W && axis_y >= 0 && axis_y < MAP_H) {
    grid[axis_y][axis_x] = '+';
  }

  for (int i = 0; i < MAX_AGENTS; i++) {
    if (!agents[i].seen) {
      continue;
    }

    int64_t age = now - agents[i].ts_ms;
    if (age > 2000) {
      continue;
    }

    double nx = (agents[i].x - world_min_x) / (world_max_x - world_min_x);
    double ny = (world_max_y - agents[i].y) / (world_max_y - world_min_y);

    int gx = (int)(nx * (MAP_W - 1));
    int gy = (int)(ny * (MAP_H - 1));

    if (gx < 0)
      gx = 0;
    if (gx >= MAP_W)
      gx = MAP_W - 1;
    if (gy < 0)
      gy = 0;
    if (gy >= MAP_H)
      gy = MAP_H - 1;

    grid[gy][gx] = (char)('0' + (i % 10));
  }

  printf("Map view (x,y in roughly [-20,20], stale >2s hidden)\n");
  for (int y = 0; y < MAP_H; y++) {
    printf("%s\n", grid[y]);
  }
}

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

    int64_t t = now_ms();

    printf("\033[2J\033[H");
    printf("DDS swarm viewer  |  Ctrl-C to quit\n");
    printf("==============================================================\n");
    draw_map(agents, t);
    printf("==============================================================\n");
    printf("%-6s %-10s %-10s %-10s %-10s %-10s\n", "id", "x", "y", "vx", "vy",
           "age_ms");

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
