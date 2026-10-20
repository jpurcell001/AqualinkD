// Feed heat pump / chiller request and reply packets through devices_jandy.c and check the chiller button's
// LED state and chiller mode after every packet.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#define CONFIG_C
#include "config.h"

#include "aqualink.h"
#include "devices_jandy.h"
#include "iaqualink.h"
#include "json_messages.h"

// devices_jandy.o and aq_serial.o need these, none are reached by the heat pump packets.
int _rs_fd;
bool isAqualinkDStopping() { return false; }
#ifndef NEW_AQ_PROGRAMMER
void aq_programmer(program_type type, char *args, struct aqualinkdata *aq_data) {}
#else
void aq_programmer(program_type r_type, aqkey *button, int value, int value2, struct aqualinkdata *aq_data) {}
#endif
bool in_swg_programming_mode(struct aqualinkdata *aq_data) { return false; }
bool process_iAqualinkStatusPacket(unsigned char *packet, int length, struct aqualinkdata *aq_data) { return false; }
char *LED2text(aqledstate state) { return ""; }

static int failures = 0;
static struct aqualinkdata aqdata;
static aqkey chiller;
static aqled chiller_led;
static altlabel_detail chiller_detail;

static const char *ledname(aqledstate s)
{
  switch (s) {
    case ON: return "ON";
    case OFF: return "OFF";
    case FLASH: return "FLASH";
    case ENABLE: return "ENABLE";
    default: return "UNKNOWN";
  }
}

// Steps are request/reply status bytes: 'Q' 0xNN is the panel's 0x0c request, 'R' 0xNN the unit's 0x0d reply.
// expect_led / expect_cool hold the state after each step.
typedef struct { char dir; unsigned char status; aqledstate expect_led; bool expect_cool; } step;

static void run(const char *name, aqledstate start_led, bool start_cool, const step *steps, int nsteps, int max_changes)
{
  unsigned char req[] = {0x10,0x02,0x70,0x0c,0x00,0x00,0x00,0x00,0x00,0x10,0x03};
  unsigned char rep[] = {0x10,0x02,0x00,0x0d,0x00,0x00,0x00,0x00,0x10,0x03};
  int ok = 1, changes = 0;
  char trace[1024] = "";

  chiller_led.state = start_led;
  chiller_detail.in_alt_mode = start_cool;

  for (int i = 0; i < nsteps; i++) {
    aqledstate before = chiller_led.state;
    if (steps[i].dir == 'Q') {
      req[4] = steps[i].status;
      req[8] = (0x10 + 0x02 + 0x70 + 0x0c + req[4]) & 0xff;
      processPacketToHeatPump(req, sizeof(req), &aqdata);
    } else {
      rep[4] = steps[i].status;
      rep[7] = (0x10 + 0x02 + 0x00 + 0x0d + rep[4]) & 0xff;
      processPacketFromHeatPump(rep, sizeof(rep), &aqdata, 0x70);
    }
    if (chiller_led.state != before) changes++;
    if (chiller_led.state != steps[i].expect_led || chiller_detail.in_alt_mode != steps[i].expect_cool) ok = 0;
    snprintf(trace + strlen(trace), sizeof(trace) - strlen(trace), " %c%02x>%s/%s",
             steps[i].dir, steps[i].status, ledname(chiller_led.state), chiller_detail.in_alt_mode ? "cool" : "heat");
  }
  if (max_changes >= 0 && changes > max_changes) ok = 0;

  printf("%s  %s\n", ok ? "PASS" : "FAIL", name);
  if (!ok) {
    failures++;
    printf("      got:%s (%d LED changes)\n", trace, changes);
  }
}

#define S(...) (const step[]){__VA_ARGS__}, sizeof((const step[]){__VA_ARGS__}) / sizeof(step)

int main()
{
#ifdef AQ_MANAGER
  setLoggingPrms(LOG_WARNING, false, NULL);
#else
  setLoggingPrms(LOG_WARNING, false, NULL, NULL);
#endif

  chiller.led = &chiller_led;
  chiller.special_mask_ptr = &chiller_detail;
  aqdata.chiller_button = &chiller;

  run("chiller enabled, idle, panel asks cool: stays enabled in cool mode", ENABLE, true,
      S({'Q',0x29,ENABLE,true}, {'R',0x40,ENABLE,true}, {'Q',0x29,ENABLE,true}, {'R',0x40,ENABLE,true}), 0);

  run("chiller enabled, idle, panel says enabled: stays enabled in cool mode", ENABLE, true,
      S({'Q',0x01,ENABLE,true}, {'R',0x40,ENABLE,true}, {'Q',0x01,ENABLE,true}, {'R',0x40,ENABLE,true}), 0);

  run("chiller cooling: one change to ON, then steady", ENABLE, true,
      S({'Q',0x29,ENABLE,true}, {'R',0x68,ON,true}, {'Q',0x29,ON,true}, {'R',0x68,ON,true},
        {'Q',0x29,ON,true}, {'R',0x68,ON,true}), 1);

  run("chiller stops cooling: the unit's 0x40 reply turns ON into enabled", ON, true,
      S({'Q',0x29,ON,true}, {'R',0x68,ON,true}, {'Q',0x29,ON,true}, {'R',0x40,ENABLE,true}, {'Q',0x29,ENABLE,true}), 1);

  run("chiller off, panel asks cool: enabled in cool mode", OFF, false,
      S({'Q',0x29,ENABLE,true}, {'R',0x40,ENABLE,true}), 1);

  run("off, panel says off, unit idle: stays off", OFF, false,
      S({'Q',0x00,OFF,false}, {'R',0x40,OFF,false}, {'Q',0x00,OFF,false}, {'R',0x40,OFF,false}), 0);

  run("heat pump heating: ON in heat mode, steady", ENABLE, false,
      S({'Q',0x09,ENABLE,false}, {'R',0x48,ON,false}, {'Q',0x09,ON,false}, {'R',0x48,ON,false}), 1);

  run("heat pump stops heating: panel's enabled waits for the unit's 0x40", ON, false,
      S({'Q',0x09,ON,false}, {'R',0x48,ON,false}, {'Q',0x01,ON,false}, {'R',0x40,ENABLE,false}, {'Q',0x01,ENABLE,false}), 1);

  run("heat pump enabled from off by the panel", OFF, false,
      S({'Q',0x01,ENABLE,false}, {'R',0x40,ENABLE,false}), 1);

  printf("%s\n", failures ? "FAILURES" : "ALL PASS");
  return failures ? 1 : 0;
}
