/**
    Bollie Fader - (c) 2016 Thomas Ebeling https://ca9.eu

    This file is part of bolliefader.lv2

    bolliefader.lv2 is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    bolliedelay.lv2 is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Foobar.  If not, see <http://www.gnu.org/licenses/>.
*/

/**
* \file bollie-fader.c
* \author Bollie
* \date 31 Jan 2017
* \brief An LV2 fader plugin.
*/

#include <stdlib.h>
#include <math.h>

#include "lv2/lv2plug.in/ns/lv2core/lv2.h"

#define URI "https://ca9.eu/lv2/bolliefader"

/**
* Make a bool type available. ;)
*/
typedef enum { false, true } bool;


/**
* Enumeration of LV2 ports
*/
typedef enum {
    BGA_MUTE        = 0,
    BGA_VOLUME      = 1,
    BGA_INPUT_L     = 2,
    BGA_INPUT_R     = 3,
    BGA_OUTPUT_L    = 4,
    BGA_OUTPUT_R    = 5,
    BGA_CTL_DB_L    = 6,
    BGA_CTL_DB_R    = 7
} PortIdx;

/**
* Struct for THE BollieFader instance, the host is going to use.
*/
typedef struct {
    const float* ctl_mute;      ///< Mute toggle
    const float* ctl_volume;    ///< Volume
    const float* in_left;       ///< input0, left side
    const float* in_right;      ///< input1, right side
    float* out_left;            ///< output1, left side
    float* out_right;           ///< output2, right side
    float* ctl_db_l;            ///< control output of the current level
    float* ctl_db_r;            ///< control output of the current level
    int sample_rate;            ///< current sample rate
    float cur_volume;           ///< state variable for current volume
    float max_lvl_l;
    float max_lvl_r;
    double sample_cnt;
    double sample_interval;
} BollieFader;


/**
* Instantiates the plugin
* Allocates memory for the BollieFader object and returns a pointer as
* LV2Handle.
*/
static LV2_Handle instantiate(const LV2_Descriptor * descriptor, double rate,
    const char* bundle_path, const LV2_Feature* const* features) {
    
    BollieFader *self = (BollieFader*)calloc(1, sizeof(BollieFader));

    // Memorize sample rate for calculation
    self->sample_rate = rate;
    // Every 1/10 seconds update for level meter
    self->sample_interval = rate * 0.1;

    return (LV2_Handle)self;
}


/**
* Used by the host to connect the ports of this plugin.
* \param instance current LV2_Handle (will be cast to BollieFader*)
* \param port LV2 port index, maches the enum above.
* \param data Pointer to the actual port data.
*/
static void connect_port(LV2_Handle instance, uint32_t port, void *data) {
    BollieFader *self = (BollieFader*)instance;

    switch ((PortIdx)port) {
        case BGA_MUTE:
            self->ctl_mute = data;
            break;
        case BGA_VOLUME:
            self->ctl_volume = data;
            break;
        case BGA_INPUT_L:
            self->in_left = data;
            break;
        case BGA_INPUT_R:
            self->in_right = data;
            break;
        case BGA_OUTPUT_L:
            self->out_left = data;
            break;
        case BGA_OUTPUT_R:
            self->out_right = data;
            break;
        case BGA_CTL_DB_L:
            self->ctl_db_l = data;
            break;
        case BGA_CTL_DB_R:
            self->ctl_db_r = data;
            break;
    }
}
    

/**
* This has to reset all the internal states of the plugin
* \param instance pointer to current plugin instance
*/
static void activate(LV2_Handle instance) {
    BollieFader* self = (BollieFader*)instance;
    self->cur_volume = 0;
    self->max_lvl_l = 0;
    self->max_lvl_r = 0;
    self->sample_cnt = 0;
}



/**
* Main process function of the plugin.
* \param instance  handle of the current plugin
* \param n_samples number of samples in this current input block.
*/
static void run(LV2_Handle instance, uint32_t n_samples) {
    BollieFader* self = (BollieFader*)instance;

    float mute = *self->ctl_mute;
    float cur_vol = self->cur_volume;
    const float v = *self->ctl_volume;
    float target_vol = 1;
    if (mute != 0) {
        target_vol = 0;
    }
    else if (v < -90) {
        target_vol = 0;
    }
    else if (v <= 12) {
        target_vol = powf(10, (v/20));
    }

    float max_l = self->max_lvl_l;
    float max_r = self->max_lvl_r;
    // Loop over the block of audio we got
    for (unsigned int i = 0 ; i < n_samples ; ++i) {
        // Paraemter smoothing for volume
        cur_vol = target_vol * 0.01f + cur_vol * 0.99f;

        // Will it blend? ;)
        self->out_left[i] = self->in_left[i] * cur_vol;
        max_l = self->out_left[i] > max_l ? self->out_left[i] : max_l;
        self->out_right[i] = self->in_right[i] * cur_vol;
        max_r = self->out_right[i] > max_r ? self->out_right[i] : max_r;
    }
    self->max_lvl_l = max_l;
    self->max_lvl_r = max_r;
    self->sample_cnt += n_samples;

    if (self->sample_cnt > self->sample_interval) {
        *self->ctl_db_l = 20 * log(max_l);
        *self->ctl_db_r = 20 * log(max_r);
        self->sample_cnt = 0;
        self->max_lvl_l = 0;
        self->max_lvl_r = 0;
    }
    self->cur_volume = cur_vol;
}


/**
* Called, when the host deactivates the plugin.
*/
static void deactivate(LV2_Handle instance) {
}


/**
* Cleanup, freeing memory and stuff
*/
static void cleanup(LV2_Handle instance) {
    free(instance);
}


/**
* extension stuff for additional interfaces
*/
static const void* extension_data(const char* uri) {
    return NULL;
}


/**
* Descriptor linking our methods.
*/
static const LV2_Descriptor descriptor = {
    URI,
    instantiate,
    connect_port,
    activate,
    run,
    deactivate,
    cleanup,
    extension_data
};


/**
* Symbol export using the descriptor above
*/
LV2_SYMBOL_EXPORT const LV2_Descriptor* lv2_descriptor(uint32_t index) {
    switch (index) {
        case 0:  return &descriptor;
        default: return NULL;
    }
}
