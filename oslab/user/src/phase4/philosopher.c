#include "philosopher.h"

// TODO: define some sem if you need
int chopsticks[PHI_NUM];
int mutex;

void init()
{
  // init some sem if you need
  mutex = sem_open(1);
  for (int i = 0; i < PHI_NUM; ++i)
    chopsticks[i] = sem_open(1);
}

void philosopher(int id)
{
  // implement philosopher, remember to call `eat` and `think`
  while (1)
  {
    think(id);
    P(mutex);
    P(chopsticks[id]);
    P(chopsticks[(id + 1) % 5]);
    V(mutex);
    eat(id);
    V(chopsticks[id]);
    V(chopsticks[(id + 1) % 5]);
  }
}
